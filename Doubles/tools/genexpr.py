"""Expand the gen~ template and (for tests) translate it to JavaScript.

expand()  -> GenExpr text for the codebox inside the device.
to_js()   -> a JavaScript module that runs the same code sample by sample in node,
             used by ref/sim.js to test the engine without Max.
"""
import re, os

HERE = os.path.dirname(os.path.abspath(__file__))
TMPL = os.path.join(HERE, '..', 'src', 'doubles_core.gen.tmpl')
KEYWORDS = {'if', 'else', 'for', 'while', 'return', 'Param', 'History', 'Buffer', 'Data'}
BUILTINS = {'floor', 'ceil', 'abs', 'min', 'max', 'exp', 'cos', 'sin', 'tanh', 'sqrt', 'pow', 'sign',
            'wrap', 'noise', 'dim', 'peek', 'poke', 'samplerate', 'pi', 'twopi',
            'in1', 'in2', 'in3', 'out1', 'out2', 'gauss'}


def _expand_decks(src):
    out = []
    block = None
    for line in src.split('\n'):
        if line.strip() == '#DECK':
            block = []
            continue
        if line.strip() == '#ENDDECK':
            text = '\n'.join(block)
            for d in (0, 1):
                out.append(text.replace('$', str(d)).replace('{D}', str(d)))
            block = None
            continue
        (block if block is not None else out).append(line)
    return '\n'.join(out)


def parse(src):
    params, hist, bufs, datas = {}, {}, {}, {}
    for m in re.finditer(r'^Param (\w+)\(([^,)]+)', src, re.M):
        params[m.group(1)] = float(m.group(2))
    for m in re.finditer(r'^History (\w+)\(([^)]*)\)', src, re.M):
        hist[m.group(1)] = float(m.group(2) or 0)
    for m in re.finditer(r'^Buffer (\w+)\("([^"]+)"\)', src, re.M):
        bufs[m.group(1)] = m.group(2)
    for m in re.finditer(r'^Data (\w+)\((\d+)\)', src, re.M):
        datas[m.group(1)] = int(m.group(2))
    return params, hist, bufs, datas


def _split(src):
    """declarations / functions / main body"""
    lines = src.split('\n')
    decl, funcs, body = [], [], []
    i = 0
    while i < len(lines):
        ln = lines[i]
        s = ln.strip()
        if re.match(r'^(Param|History|Buffer|Data) ', s):
            decl.append(ln)
        elif re.match(r'^\w+\(\)\s*\{$', s):          # function definition
            f = [ln]
            depth = 1
            i += 1
            while depth:
                f.append(lines[i])
                depth += lines[i].count('{') - lines[i].count('}')
                i += 1
            funcs.append('\n'.join(f))
            continue
        else:
            body.append(ln)
        i += 1
    return decl, funcs, '\n'.join(body)


def _strip_comments(code):
    return re.sub(r'//[^\n]*', '', code)


def locals_of(body, params, hist, bufs, datas):
    code = _strip_comments(body)
    names = []
    for m in re.finditer(r'\b([A-Za-z_]\w*)\s*(?:=(?!=)|\+=)', code):
        n = m.group(1)
        if n in params or n in hist or n in bufs or n in datas or n in BUILTINS or n in KEYWORDS:
            continue
        if n in ('out1', 'out2'):
            continue
        if n not in names:
            names.append(n)
    return names


def expand():
    src = _expand_decks(open(TMPL).read())
    params, hist, bufs, datas = parse(src)
    decl, funcs, body = _split(src)
    locs = locals_of(body, params, hist, bufs, datas)
    init = '// every local starts at zero, so none is read before it is set\n'
    row = []
    for n in locs:
        row.append('%s = 0;' % n)
        if len(row) == 8:
            init += ' '.join(row) + '\n'
            row = []
    if row:
        init += ' '.join(row) + '\n'
    head = src[:src.index(decl[0])] if decl else ''
    out = head + '\n'.join(decl) + '\n\n' + '\n\n'.join(funcs) + '\n\n' + init + body.strip() + '\n'
    return out


def to_js(gen):
    params, hist, bufs, datas = parse(gen)
    decl, funcs, body = _split(gen)
    locs = locals_of(body, params, hist, bufs, datas)

    def sub(code):
        code = _strip_comments(code)

        def rep(m):
            n = m.group(0)
            if n in params:
                return 'P.' + n
            if n in hist:
                return 'H.' + n
            if n in bufs or n in datas:
                return 'BUF.' + n
            return n
        code = re.sub(r'\b[A-Za-z_]\w*\b', rep, code)
        code = code.replace('samplerate', 'SRATE')
        return code

    js = ['// generated from doubles_core.gen.tmpl by tools/genexpr.py: do not edit',
          'function makeCore(BUF, SRATE, rnd) {',
          '  var pi = Math.PI, twopi = 2 * Math.PI;',
          '  var floor = Math.floor, ceil = Math.ceil, abs = Math.abs, exp = Math.exp, cos = Math.cos,',
          '      tanh = Math.tanh, sqrt = Math.sqrt, pow = Math.pow, sign = Math.sign;',
          '  function min(a, b) { return a < b ? a : b; }',
          '  function max(a, b) { return a > b ? a : b; }',
          '  function wrap(x, lo, hi) { var r = hi - lo; var y = (x - lo) % r; if (y < 0) y += r; return y + lo; }',
          '  function noise() { return rnd() * 2 - 1; }',
          '  function dim(b) { return b.frames; }',
          '  function peek(b, i, ch) { i = Math.floor(i); if (i < 0 || i >= b.frames) return 0; return b.ch[ch | 0][i]; }',
          '  function poke(b, v, i, ch) { i = Math.floor(i); if (i < 0 || i >= b.frames) return; b.ch[(ch | 0)][i] = v; }',
          '  var P = %s;' % ('{' + ', '.join('%s: %r' % (k, v) for k, v in params.items()) + '}'),
          '  var H = %s;' % ('{' + ', '.join('%s: %r' % (k, v) for k, v in hist.items()) + '}'),
          ]
    for f in funcs:
        js.append('  function ' + sub(f).strip())
    js.append('  function step(in1, in2, in3) {')
    js.append('    var out1 = 0, out2 = 0, ' + ', '.join('%s = 0' % n for n in locs) + ';')
    js.append(sub(body))
    js.append('    return [out1, out2];')
    js.append('  }')
    js.append('  return {P: P, H: H, step: step};')
    js.append('}')
    js.append('if (typeof module !== "undefined") module.exports = makeCore;')
    out = '\n'.join(js)
    out = out.replace('+= 1)', '+= 1)')
    return out


if __name__ == '__main__':
    import sys
    g = expand()
    open(os.path.join(HERE, '..', 'build', 'doubles_core.genexpr'), 'w').write(g)
    open(os.path.join(HERE, '..', 'ref', 'core_sim.js'), 'w').write(to_js(g))
    print('genexpr lines', g.count('\n'))
