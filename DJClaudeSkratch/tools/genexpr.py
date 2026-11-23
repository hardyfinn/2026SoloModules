"""Expand the gen~ template and (for tests) translate it to JavaScript.

expand()  -> GenExpr text for the codebox inside the device.
to_js()   -> a JavaScript module that runs the same code sample by sample in node,
             used by ref/sim.js to test the engine without Max.

Template: lines between '#FOR name a b c' and '#ENDFOR' are written out once per value,
with {name} replaced. Blocks nest.
"""
import re, os

HERE = os.path.dirname(os.path.abspath(__file__))
TMPL = os.path.join(HERE, '..', 'src', 'skratch_core.gen.tmpl')
KEYWORDS = {'if', 'else', 'for', 'while', 'return', 'Param', 'History', 'Buffer', 'Data'}
BUILTINS = {'floor', 'ceil', 'abs', 'min', 'max', 'exp', 'cos', 'sin', 'tanh', 'sqrt', 'pow', 'sign',
            'wrap', 'noise', 'dim', 'peek', 'poke', 'samplerate', 'pi', 'twopi',
            'in1', 'in2', 'in3', 'out1', 'out2'}


def _expand_for(lines):
    out, i = [], 0
    while i < len(lines):
        m = re.match(r'^\s*#FOR (\w+) (.+)$', lines[i])
        if not m:
            out.append(lines[i]); i += 1; continue
        name, vals = m.group(1), m.group(2).split()
        depth, j = 1, i + 1
        while depth:
            if re.match(r'^\s*#FOR ', lines[j]): depth += 1
            elif lines[j].strip() == '#ENDFOR': depth -= 1
            j += 1
        body = _expand_for(lines[i + 1:j - 1])
        for v in vals:
            out += [ln.replace('{%s}' % name, v) for ln in body]
        i = j
    return out


def parse(src):
    params, hist, bufs = {}, {}, {}
    for m in re.finditer(r'^Param (\w+)\(([^,)]+)', src, re.M):
        params[m.group(1)] = float(m.group(2))
    for m in re.finditer(r'^History (\w+)\(([^)]*)\)', src, re.M):
        hist[m.group(1)] = float(m.group(2) or 0)
    for m in re.finditer(r'^Buffer (\w+)\("([^"]+)"\)', src, re.M):
        bufs[m.group(1)] = m.group(2)
    return params, hist, bufs


def _split(src):
    decl, body = [], []
    for ln in src.split('\n'):
        (decl if re.match(r'^(Param|History|Buffer|Data) ', ln.strip()) else body).append(ln)
    return decl, '\n'.join(body)


def _strip_comments(code):
    return re.sub(r'//[^\n]*', '', code)


def locals_of(body, params, hist, bufs):
    code = _strip_comments(body)
    names = []
    for m in re.finditer(r'\b([A-Za-z_]\w*)\s*(?:=(?!=)|\+=)', code):
        n = m.group(1)
        if n in params or n in hist or n in bufs or n in BUILTINS or n in KEYWORDS or n in ('out1', 'out2'):
            continue
        if n not in names:
            names.append(n)
    return names


def expand():
    src = '\n'.join(_expand_for(open(TMPL).read().split('\n')))
    params, hist, bufs = parse(src)
    for n in list(params) + list(hist):
        for d in '0123456789':
            if (n + d) in params or (n + d) in hist:
                pass
    decl, body = _split(src)
    locs = locals_of(body, params, hist, bufs)
    clash = [n for n in locs if n in params or n in hist]
    assert not clash, clash
    init = '// every local starts at zero, so none is read before it is set\n'
    row = []
    for n in locs:
        row.append('%s = 0;' % n)
        if len(row) == 8:
            init += ' '.join(row) + '\n'; row = []
    if row:
        init += ' '.join(row) + '\n'
    head = src[:src.index(decl[0])]
    return head + '\n'.join(d.strip() for d in decl) + '\n\n' + init + body.replace(head, '', 1).strip() + '\n'


def to_js(gen):
    params, hist, bufs = parse(gen)
    decl, body = _split(gen)
    locs = locals_of(body, params, hist, bufs)

    def sub(code):
        code = _strip_comments(code)

        def rep(m):
            n = m.group(0)
            if n in params: return 'P.' + n
            if n in hist: return 'H.' + n
            if n in bufs: return 'BUF.' + n
            return n
        return re.sub(r'\b[A-Za-z_]\w*\b', rep, code).replace('samplerate', 'SRATE')

    js = ['// generated from skratch_core.gen.tmpl by tools/genexpr.py: do not edit',
          'function makeCore(BUF, SRATE) {',
          '  var pi = Math.PI, twopi = 2 * Math.PI;',
          '  var floor = Math.floor, ceil = Math.ceil, abs = Math.abs, exp = Math.exp, cos = Math.cos, sin = Math.sin,',
          '      tanh = Math.tanh, sqrt = Math.sqrt, pow = Math.pow, sign = Math.sign;',
          '  function min(a, b) { return a < b ? a : b; }',
          '  function max(a, b) { return a > b ? a : b; }',
          '  function dim(b) { return b.frames; }',
          '  function peek(b, i, ch) { i = Math.floor(i); if (i < 0 || i >= b.frames) return 0; return b.ch[ch | 0][i]; }',
          '  function poke(b, v, i, ch) { i = Math.floor(i); if (i < 0 || i >= b.frames) return; b.ch[(ch | 0)][i] = v; }',
          '  var P = {%s};' % ', '.join('%s: %r' % kv for kv in params.items()),
          '  var H = {%s};' % ', '.join('%s: %r' % kv for kv in hist.items()),
          '  function step(in1) {',
          '    var out1 = 0, out2 = 0, ' + ', '.join('%s = 0' % n for n in locs) + ';',
          sub(body),
          '    return [out1, out2];',
          '  }',
          '  return {P: P, H: H, step: step};',
          '}',
          'if (typeof module !== "undefined") module.exports = makeCore;']
    return '\n'.join(js)


if __name__ == '__main__':
    g = expand()
    open(os.path.join(HERE, '..', 'build', 'skratch_core.genexpr'), 'w').write(g)
    open(os.path.join(HERE, '..', 'ref', 'core_sim.js'), 'w').write(to_js(g))
    print('genexpr lines', g.count('\n'))
