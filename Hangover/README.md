# Hangover

A handpan (hang drum) for VCV Rack, made by modal synthesis, with the real
instrument's dirt left in. 42 HP, stereo out.

**The instrument.** A ding in the middle and up to 8 tone fields. Every field
rings with a fundamental, an octave and a compound fifth plus three very
quiet shell modes. The air inside the shell is a Helmholtz resonator that speaks
through the **gu** (the port underneath): the deep bass boom.

- **SCALE** (Kurd, Integral, Celtic, Amara, Hijaz, Pygmy, Equinox, Sabye, Aegean) and **KEY** (the ding's note).
- **DECAY**, **FORCE** (velocity when VEL is unpatched), **TOUCH** (soft pad to hard slap), **STRIKE** (centre of the field = fundamental, edge = octave + fifth).
- **MUTE** palm damping, **GU** bass level, **ROLL** speed for thumb rolls.

**The physics** (2.2): fitted to recordings of a real D Kurd hang. Each field
is carried by its fundamental, octave and compound fifth; the octave is about
as loud as the fundamental, and the octave and fifth ring 1.5 to 2 times longer
than it, so notes end as a thin shimmer. The ding speaks mostly through its
octave. Every field is voiced a little differently. The finger is a mass on a
soft, lossy pad landing on the moving steel (a dull thud, nothing above 2 kHz);
a conservative 1:2:3 shell nonlinearity makes the octave bloom after hard
hits; the cavity air is coupled both ways to every fundamental; fields share
energy through a symmetric spring through the shell (BLEED).

**The dirt** (each knob, scaled by the big **DIRT** knob: middle = as set, right = double):

- **TUNING**: fields and their octaves / fifths drift off true; hard hits start a touch sharp.
- **BEAT**: every partial is a pair of close modes that beat slowly.
- **RATTLE**: a loose part on a spring clatters against its stops, and one loose field buzzes, on hard hits.
- **BLEED**: the shell knocks the neighbouring fields and they ring in sympathy.
- **AIR**: breath through the gu, turbulence and a little room tone.
- **SKIN**: finger and palm noise, the odd fingernail.
- **SLOP**: late, uneven, bounced and grazed strikes.
- **ROOM**: a small room around the instrument.
- **NEW HANG** re-rolls this instrument's imperfections (saved with the patch).

**Inputs.** DING and 1-8 triggers strike single fields (ascending); GU slaps
beside the port. Poly **GATE + V/OCT** strike the nearest field (so Peal or any
sequencer can play it), poly **VEL** 0-10 V. **MUTE** gate puts a palm down,
**ROLL** gate rolls on the last field played. CV for DECAY, TOUCH, STRIKE, MUTE,
GU and DIRT (10 V = full range).

`make test` renders a demo WAV offline. Panel: `python3 res/make_panel.py`.
