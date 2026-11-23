# Peal + Bell Foundry

Two modules in one plugin (Peal 2.0.0).

**Peal** is a change ringing sequencer. Every clock pulse rings one bell. When
every bell has rung once (a row), the order changes by the rules of a ringing
method, so the pattern keeps moving but always comes back round.

- 4 to 12 bells (Minimus to Maximus)
- Methods: Plain Hunt, Plain Bob, Little Bob, Grandsire, Stedman, Cambridge
  Surprise, and Wander (rewrites its own lead now and then)
- GO starts the method at the next handstroke; press again for "That's all"
  (back to rounds)
- BOB and SINGLE calls by button, trigger, or the CALLS chance knob
- GAP: rest pulses after each backstroke row (the open handstroke lead)
- Outs: a gate per bell, poly gates, poly bell pitches (TUNES), the row as a
  poly chord (ROW), pitch and place of the bell just rung, and STRIKE, HAND,
  CHANGE, LEAD, ROUNDS, CALL triggers
- Pitch: scale knob, or a poly chord into TUNE (tenor first)

**Bell Foundry** is twelve modal bells to play from Peal: patch GATES to GATES
and TUNES to V/OCT.

Build: `make dist` with the Rack SDK. Offline checks + demo WAV: `make test`
(every method's plain course is checked to come round at the right length
without repeating a row). Panels: `python3 res/make_panel.py`. Preview picture:
`python3 tools/preview.py panel-preview.png`.
