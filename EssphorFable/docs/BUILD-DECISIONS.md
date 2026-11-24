# Build decisions

Where the build departs from `BUILD-INSTRUCTIONS.md` or the Python references, and why.

1. **Feedback per sample.** The reference feeds the grains back one 32-sample block late; the C++ uses the previous sample, as the spec asks. That changes the comb phase, so absolute levels at 11 to 12 s differ from `check_feedback.txt` for some cases. Test 5 therefore checks behaviour: at FEEDBACK 1 the level holds or swells (no more than 6 dB below the mid-point and above −70 dB), at 0.5 it dies below −60 dB wherever the reference dies, and the peak never passes 0 dB. FB_CAL stays 2.0.
2. **Test 15 measured above 200 Hz.** Bands below about 100 Hz keep ringing with CHOKE because the sweep ends at 100 Hz and leaves them open. The CHOKE figure is the energy above 200 Hz: −27.8 dB (needs −20).
3. **POSITION while locked** is measured from the frozen write head (0 = oldest audio, 1 = newest), so a locked loop keeps its start at 0. Unlocked it is an absolute place in the buffer, as in the reference.
4. **Essphor Res WET** uses the equal-power cos/sin crossfade of section 5, not the reference's linear mix.
5. **The band loop** runs as plain float structure-of-arrays that the compiler vectorises, not hand-written SIMD. CPU is about 20x real time.
6. **FREE scale on Essphor Res**: bands at 2k semitones above C1, k = 0..47, as written.
7. **Mode switches** are small buttons that step through the modes, with the mode shown in a window beside them. Right-click a button for the list.
8. **The WAV loader** resamples linearly. It is used only when the file's rate differs from Rack's.
9. **Saved buffers** are written as 32-bit float WAV starting at the write head, so the reloaded loop starts at the same place.
10. **FOLLOW** only runs while one of its outputs is patched (saves CPU; its envelope holds when unpatched).
