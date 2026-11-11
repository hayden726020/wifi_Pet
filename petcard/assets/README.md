# Pet sprites

All animation frames are transparent 64×64 PNGs. Play frames 1–4 in order and loop them; render with nearest-neighbor scaling on the 128×128 display.

| Prefix | State | Frames |
| --- | --- | --- |
| `happy_` | 开心 | 1–4 |
| `hungry_` | 饥饿 | 1–4 |
| `sleepy_` | 困倦 | 1–4 |
| `grumpy_` | 生气/闹脾气 | 1–4 |
| `sleep_` | 睡觉 | 1–4 |

`pet_emotions_64_sheet.png` contains the 20 frames arranged as five rows in the order above, with four frames per row. `pet_idle_64.png` is the idle sprite and `pet_idle_master.png` is its high-resolution source.

The 20 emotion frames were cleaned to remove low-alpha colored fringe pixels before being resized to 64×64. The device RGBA copies are generated from these PNGs with `python prepare_sprites.py`.
