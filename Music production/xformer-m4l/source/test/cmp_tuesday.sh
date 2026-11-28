#!/bin/bash
# Random Algo scenarios: firmware simulator vs the JavaScript port
cd "$(dirname "$0")/../.."
BIN=src/xformer/build-sim/src/tests/unit/sequencer/TestXfRef
pass=0; fail=0
for i in $(seq 1 ${N:-60}); do
  f=ref/tu/s$i.txt
  r() { echo $(( RANDOM % ($1) )); }
  {
    echo "engine tuesday"; echo "ticks 6000"
    echo "algorithm $(r 16)"; echo "flow $(( $(r 16) + 1 ))"; echo "ornament $(( $(r 16) + 1 ))"
    echo "power $(r 17)"; echo "glide $([ $(r 2) = 0 ] && echo 0 || r 101)"; echo "trill $(r 101)"; echo "stepTrill $(r 101)"
    echo "gateLength $(( $(r 100) + 1 ))"; echo "gateOffset $(r 101)"; echo "scale $(r 14)"
    echo "loopLength $(r 30)"; echo "skew $(( $(r 17) - 8 ))"; echo "divisor $(echo 6 12 24 48 16 | cut -d' ' -f$(( $(r 5) + 1 )))"
    [ $(r 3) = 0 ] && echo "maskParameter $(r 16)" && echo "timeMode $(r 5)" && echo "maskProgression $(r 4)"
    [ $(r 3) = 0 ] && echo "start $(r 4)"
    [ $(r 3) = 0 ] && echo "rotate $(( $(r 9) - 4 ))"
    [ $(r 3) = 0 ] && echo "clockMult $(( $(r 150) + 50 ))"
    echo "octave $(( $(r 3) - 1 ))"; echo "transpose $(( $(r 9) - 4 ))"; echo "rootNote $(r 12)"; echo "scaleRotate $(r 7)"
  } > $f
  XF_SCENARIO=$f XF_OUT=$f.fw $BIN >/dev/null 2>&1
  node m4l/test/trace.js tuesday $f $f.js
  g=$(grep -q "^glide 0$" $f && echo 1 || echo 0)
  if python3 m4l/test/cmp.py $f.fw $f.js $g; then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL $f ($(grep algorithm $f))"; fi
done
echo "pass $pass fail $fail"
