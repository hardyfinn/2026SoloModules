MTRX-1K for Mac (Apple Silicon)
===============================

MTRX-1K is by GPR Music Project (github.com/gprmusicproject/mtrx-1k), free
software under the GNU GPL v3. This is an unofficial Mac build of their
source, made without changes to the instrument. GPR Music Project only test
it on Windows.

Install
-------
1. Double-click "Install MTRX-1K.command".
   If macOS says it can't be opened: right-click it, choose Open, then Open.
   It copies the AU and VST3 plugins into ~/Library/Audio/Plug-Ins and the
   app into ~/Applications, then opens the firmware folder.

2. Put your own copy of the Matrix-1000 firmware in that folder:
     ~/Library/Application Support/GPR Music Project/MTRX-1K/ROM
   - the 32 KB system EPROM, firmware version 1.20 (required)
   - the 64 KB patch EPROM, for the factory banks 2 to 9 (optional)
   Any file names work; they are recognised by size and content.
   No firmware is included: it is not free to share.

3. Start Live (or the MTRX-1K app). In Live it is under
   Plug-Ins > GPR Music Project > MTRX-1K. The first start calibrates
   the voices and reads the factory banks, which takes a little while.

Source: https://github.com/gprmusicproject/mtrx-1k
