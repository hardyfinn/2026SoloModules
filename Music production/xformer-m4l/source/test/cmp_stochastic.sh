#!/bin/bash
# Random Stochastic scenarios: firmware simulator vs the JavaScript port
cd "$(dirname "$0")/../.."
BIN=src/xformer/build-sim/src/tests/unit/sequencer/TestXfRef
pass=0; fail=0
for i in $(seq 1 ${N:-60}); do
  f=ref/st/s$i.txt
  r() { echo $(( RANDOM % ($1) )); }
  maybe() { [ $(r ${2:-2}) = 0 ] && echo "$1"; }
  {
    echo "engine stochastic"; echo "ticks 12000"
    [ $(r 4) != 0 ] && echo renew
    echo "rhythmMode $(r 2)"; echo "melodyMode $(r 2)"
    echo "scale $(r 14)"; echo "rootNote $(r 12)"; echo "scaleRotate $(r 4)"
    echo "complexity $(r 101)"; echo "contour $(( $(r 201) - 100 ))"; echo "range $(r 101)"
    echo "noteDuration $(r 8)"; echo "variation $(r 101)"; echo "rest $(r 40)"
    echo "gateLength $(r 101)"; echo "legatoProb $(r 40)"; echo "slide $(r 50)"
    maybe "repeatProb $(r 60)"
    maybe "burst $(r 101)"; echo "burstCount $(r 101)"; echo "burstRate $(r 101)"; echo "burstHold $(r 4)"
    maybe "sleep $(r 30)" 4; maybe "mutate $(r 101)" 3; maybe "jump $(r 101)" 3
    maybe "patienceRhythm $(r 101)" 3; maybe "patienceMelody $(r 101)" 3
    maybe "marblesSpread $(r 101)"; maybe "marblesBias $(r 101)"
    maybe "maskMelody $(r 101)" 3; maybe "tiltMelody $(r 101)" 3; maybe "maskRhythm $(r 101)" 3; maybe "tiltRhythm $(r 101)" 3
    maybe "feel $(r 101)" 3; maybe "rotate $(( $(r 17) - 8 ))" 3
    maybe "size $(( $(r 40) + 2 ))" 2; maybe "first $(r 6)" 4
    maybe "divisor $(echo 6 12 24 48 16 | cut -d' ' -f$(( $(r 5) + 1 )))"
    maybe "clockMult $(( $(r 101) + 50 ))" 4
    maybe "degreeRotation $(( $(r 9) - 4 ))" 3; maybe "maskRotation $(( $(r 9) - 4 ))" 3
    for d in 0 1 2 3 4 5 6; do maybe "degreeTicket $d $(( $(r 102) - 1 ))" 3; done
    for d in 0 1 2 3 4 5 6 7; do maybe "durationTicket $d $(( $(r 102) - 1 ))" 4; done
    echo "octave $(( $(r 3) - 1 ))"; echo "transpose $(( $(r 9) - 4 ))"
  } > $f
  XF_SCENARIO=$f XF_OUT=$f.fw $BIN >/dev/null 2>&1
  node m4l/test/trace.js stochastic $f $f.js || { echo "JS ERROR $f"; fail=$((fail+1)); continue; }
  if python3 m4l/test/cmp.py $f.fw $f.js 1; then pass=$((pass+1)); else fail=$((fail+1)); echo "FAIL $f"; fi
done
echo "pass $pass fail $fail"
