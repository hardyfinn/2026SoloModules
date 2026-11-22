# 2026 Solo Modules

Haer's home for new VCV Rack modules. Each folder is its own plugin (its own
`plugin.json`, version and build), so modules can be added and updated
without stepping on each other.

| Folder | Module | What it is |
|---|---|---|
| [Heathrow](Heathrow/) | Heathrow | Stereo phaser / flanger with through-zero, a second switchable engine and 6 memory slots |
| [Bore](Bore/) | Scanner, Bore Room | MRI scanner from the inside: real pulse sequences through three coils, pump and bore, plus the bore as a reverb |

Every push builds all plugins for Apple Silicon Macs (GitHub Actions). Branch
builds go to the **preview** release, `main` builds to **latest**.

Conventions: plugin versions stay 2.x (Rack 2 skips 3.x), control labels 3.2 mm,
jack labels 2.9 mm, Hær logo on the bottom rail (`res/haer_logo.py`).

## Licences

Each plugin folder carries its own `LICENSE.txt`. Our own modules are MIT, so anyone may port them to other formats, platforms or hardware as long as the copyright notice comes along. The exception is **Ectoplasm**, which contains the GPL-3 Ectocore firmware and must stay GPL-3.0-only.
