# 2026 Solo Modules

Haer's home for new VCV Rack modules. Each folder is its own plugin (its own
`plugin.json`, version and build), so modules can be added and updated
without stepping on each other.

| Folder | Module | What it is |
|---|---|---|
| [Heathrow](Heathrow/) | Heathrow | Stereo phaser / flanger with through-zero, a second switchable engine and 6 memory slots |
| [Peal](Peal/) | Peal, Bell Foundry | Change ringing sequencer (methods, calls, open handstroke) and a 12-bell modal voice to ring it |

Every push builds all plugins for Apple Silicon Macs (GitHub Actions). Branch
builds go to the **preview** release, `main` builds to **latest**.

Conventions: plugin versions stay 2.x (Rack 2 skips 3.x), control labels 3.2 mm,
jack labels 2.9 mm, Hær logo on the bottom rail (`res/haer_logo.py`).
