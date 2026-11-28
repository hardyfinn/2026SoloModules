#!/bin/bash
# Random Note-track scenarios: firmware simulator vs the JavaScript port
cd "$(dirname "$0")/../.."
BIN=src/xformer/build-sim/src/tests/unit/sequencer/TestXfRef
pass=0; fail=0
for i in $(seq 1 ${N:-60}); do
  f=ref/no/s$i.txt
  r() { echo $(( RANDOM % ($1) )); }
  maybe() { [ $(r ${2:-2}) = 0 ] && echo "$1"; }
  {
    echo "engine note"; echo "ticks 12000"; echo "playMode 0"; echo "slideTime 0"
    echo "scale $(r 14)"; echo "rootNote $(r 12)"; maybe "scaleRotate $(r 4)" 3
    last=$(( $(r 16) + 1 )); echo "lastStep $last"; maybe "firstStep $(r 4)" 3
    maybe "runMode $(r 6)" 2
    maybe "divisor $(echo 6 12 24 48 16 | cut -d' ' -f$(( $(r 5) + 1 )))"
    maybe "clockMult $(( $(r 101) + 50 ))" 4
    maybe "resetMeasure $(( $(r 3) + 1 ))" 4
    maybe "rotate $(( $(r 9) - 4 ))" 4
    maybe "gateProbabilityBias $(( $(r 9) - 4 ))" 4; maybe "retriggerProbabilityBias $(( $(r 9) - 4 ))" 4
    maybe "lengthBias $(( $(r 9) - 4 ))" 4; maybe "noteProbabilityBias $(( $(r 9) - 4 ))" 4
    echo "octave $(( $(r 3) - 1 ))"; echo "transpose $(( $(r 9) - 4 ))"
    echo "accEnabled $(r 2)"; echo "accMode $(r 2)"; echo "accDirection $(r 3)"; echo "accOrder $(r 4)"
    echo "accTrigger $(r 3)"; echo "accMin $(( $(r 5) - 2 ))"; echo "accMax $(( $(r 10) + 2 ))"; echo "accStep $(( $(r 3) + 1 ))"
    for s in $(seq 0 17); do
      echo "step $s gate $(( $(r 3) > 0 ))"; echo "step $s note $(( $(r 25) - 12 ))"
      maybe "step $s gateProbability $(r 8)" 4; maybe "step $s gateOffset $(r 8)" 5; maybe "step $s slide 1" 5
      maybe "step $s retrigger $(r 4)" 4; maybe "step $s retriggerProbability $(r 8)" 5
      maybe "step $s length $(r 8)" 2; maybe "step $s lengthVariationRange $(( $(r 15) - 7 ))" 5
      maybe "step $s lengthVariationProbability $(r 8)" 5; maybe "step $s noteVariationRange $(( $(r 13) - 6 ))" 4
      maybe "step $s noteVariationProbability $(r 8)" 4; maybe "step $s condition $(r 77)" 6
      maybe "step $s accumulator $(( $(r 15) - 7 ))" 3; maybe "step $s pulseCount $(r 4)" 4; maybe "step $s gateMode $(r 4)" 4
    done
  } > $f
  XF_SCENARIO=$f XF_OUT=$f.fw $BIN >/dev/null 2>&1
  node m4l/test/trace.js note $f $f.js || { echo "JS ERROR $f"; fail=$((fail+1)); continue; }
  if python3 m4l/test/cmp.py $f.fw $f.js 1; then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL $f"; fi
done
echo "pass $pass fail $fail"
