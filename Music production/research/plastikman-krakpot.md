# Plastikman – "Krakpot" (1993): how it was made, and how to get there in your rig

## Quick facts
- **Release:** Plastikman single, *Krakpot* b/w *Elektrostatik*, 13 Dec 1993 (Plus 8 / NovaMute). Later the opening track (11:13) of the *Recycled Plastik* EP (1994). It is **not** on *Sheet One*, although it comes from the same studio (UTK, Windsor, Ontario) and the same period. ([Wikipedia: Recycled Plastik](https://en.wikipedia.org/wiki/Recycled_Plastik), [Bandcamp](https://richiehawtin.bandcamp.com/track/krakpot))

## What is documented
I couldn't find any track-by-track breakdown of Krakpot itself. Hawtin's only on-record comment about it is that it's one of the "pretty out-there tracks" (with Elektrostatik) that "took a long time to develop" ([Sound On Sound, Classic Tracks: Consumed](https://www.soundonsound.com/techniques/classic-tracks-plastikman-consumed)). What *is* documented is his rig and method in 1993–94:

| Ingredient | Source |
|---|---|
| **Roland TB-303s**, bought cheap ("they were 50 bucks"); the 303 was the core voice of that era | SOS; [RBMA lecture 2014](https://www.redbullmusicacademy.com/lectures/richie-hawtin-pt-1-2014/) |
| **Yamaha SPX90**: "The SPX had a dirty, great flange." | SOS |
| **ART Multiverb** (gated reverb on nearly every clap) | SOS |
| **Ensoniq DP/4**: "my main effects processor for the Plastikman albums"; on Sheet One its delays were fed back on themselves (Helikopter) | RBMA; [MusicRadar, Classic album: Sheet One](https://www.musicradar.com/news/tech/classic-album-richie-hawtin-on-plastikmans-sheet-one-633433) |
| **TR-606, 808, 909, SH-101, Pro One**; the 909 often used as a trigger/one-note sequencer, shuffle around setting 3 | MusicRadar; SOS |
| **No aux returns.** Effects came back on mixer channels so there were "always slight feedback loops"; delays on reverbs and reverbs on delays | MusicRadar |
| **Mixer:** Allen & Heath GS3000 (until ~1995) | SOS |
| **Method:** machines left running, synced by trigger/CV rather than MIDI, everything played live at the desk and recorded straight to 2-track DAT, then edited on a PC. Spastik was one ~28-minute take with a few edits | SOS; RBMA |

## What is inferred (my reading, not confirmed)
- **Flanger *and* filter are both likely, and they blur into each other.** The squelch is the 303's resonant filter and accent being ridden by hand. The swooshing, metallic, "hollow" character is most plausibly the SPX90 flange (the one effect he explicitly praises from that time) and/or very short DP/4 delays with feedback. A short delay with feedback is a comb filter, which is exactly what a flanger is, so you can't tell them apart by ear in the end result.
- **The "dirty" part matters.** The SPX90 is a 1985 digital unit with lo-fi converters, so its flange is grainier and more metallic than a smooth analogue one. Expect some aliasing grit.
- **The arrangement is the mixer.** At 11 minutes with almost no new material, the track evolves through slow rides of cutoff, resonance, effect sends, flange rate/depth and feedback. That fits his documented "press start and record" method, and it's a direct answer to loop fatigue: commit to one loop and perform the changes.
- I haven't been able to listen to the track here, so check these guesses against your own ears.

## Recreating it in your rig

### Option A: disting NT + modules (closest to his hardware method)
**Signal flow:** Metropolix → Mangrove (or a disting NT VCO) → Vult Freak → disting NT flanger/short delay → mixer with feedback → Live via NT USB.
1. **Sequence:** Metropolix, 16 steps, root-heavy line with a few octave jumps; use its slide and accent (accent gate → Freak resonance or VCA level). Clock from Live via the NT (CV Tools), as already planned.
2. **Voice:** Mangrove square out (formant around noon) → **Vult Freak** as the 303-style filter. Start with cutoff fairly low and resonance high, and a short Zadar decay (~150–300 ms) on cutoff. Ride the cutoff by hand.
3. **Flange (SPX90 stand-in):** on the disting NT, use a delay algorithm with **time 1–8 ms**, **feedback 60–85%**, **mix 50%**, and modulate the delay time with a slow LFO (Pam's channel, triangle at 1/8 to 1/32 note rate, or free at 0.05–0.3 Hz). If your NT firmware has a dedicated flanger or chorus algorithm, start there instead. Add a bitcrush or sample-rate-reduction stage before it for the "dirty" SPX grit.
4. **Hawtin-style feedback:** send the flanger output into a second NT delay (dotted 1/8 or 1/16 triplet, feedback ~40%) and then a reverb, and bring a little of the reverb back into the delay input via a mixer channel. Cold Mac can crossfade dry against wet as a single performance knob.
5. **Performance:** record a long take (10+ minutes) into Live, riding Freak cutoff, flange feedback and the delay-return level. Edit afterwards.

**Variations:** swap Freak for Three Sisters in LP mode (cleaner, more "Roland") and use the Freak only on the delay return for nastier repeats. Or let Marbles nudge the flanger LFO rate so the sweep never repeats exactly, which is a small step towards the Autechre side you like.

### Option B: Ableton only (fastest to try)
**Track 1 (acid):** any 303 emulation (Roland TB-303 plugin, ABL3, or Analog with a single saw/square → 24 dB LP, short filter envelope, high resonance) → **Auto Filter** if you want extra drive → **Redux** (sample-rate down a touch, bit depth ~12) → **Phaser-Flanger** in Flanger mode (delay ~2–5 ms, feedback 60–80%, LFO 0.1–0.3 Hz synced or free) → Utility.
**Returns:** Return A = **Echo** (1/16 dotted, feedback ~45%) followed by **Hybrid Reverb**. Return B = Hybrid Reverb alone. Turn up Return A's send into Return B and a little of Return B's send back into Return A for his "delays on reverbs, reverbs on delays" loop. Keep a Limiter after Return A because it can run away.
**Performance:** map cutoff, resonance, flange feedback and both return sends to a controller (or the Hapax), arm an Audio track recording the master output, and play one long pass.
**Max for Live helper:** drop the stock **LFO** device on the acid track and map it to the flanger's delay time at very low rate and depth so the sweep drifts against the 4-bar loop instead of locking to it.

### Option C: VCV Rack sketch
VCV/Fundamental **SEQ-3** (or any acid sequencer) → **VCO** square → **Vult Freak** if you have the Vult collection (otherwise VCV VCF, resonance high) → **ADSR** on cutoff → **Surge XT FX** module set to Flanger (or VCV **Delay** with time near minimum, feedback high, and an LFO into the time CV) → a second VCV Delay into a reverb (e.g. Valley Plateau) with a mixer channel feeding some reverb back into the delay. Route to Live through VCV's Audio module or the VCV Rack Pro plugin.

## Where to go next
- Try Option B tonight to find the sweet spot of flange feedback against filter resonance, then move it onto the NT and Freak for the real hardware feel.
- Listen to *Krakpot* next to *Spastik* and *Sheet One*: the shared ingredient is long, single takes with effects in feedback loops, not new parts.
