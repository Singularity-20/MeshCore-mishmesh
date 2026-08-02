# Fork changes

Changes in this fork ([Singularity-20/MeshCore-mishmesh](https://github.com/Singularity-20/MeshCore-mishmesh))
on top of upstream [mishmesh](https://github.com/burakcan/MeshCore-mishmesh).
This is not a full feature list - see the main [README](README.md) for that,
and [CHANGELOG.md](CHANGELOG.md) for upstream mishmesh's own versioned
release notes. This file only tracks what's changed in this fork, most
recent first.

## CardKB (I2C keyboard) support

Adds a [CardKB](https://www.dfrobot.com/product-2496.html)-protocol I2C
keyboard as an input device on the Wio Tracker L1, alongside the existing
joystick - you don't need one to keep using the other.

- **Runtime-detected, not a build flag.** The firmware probes the I2C bus at
  boot; the same binary works whether or not a CardKB is physically attached.
  Nothing changes for anyone without one.
- **Navigation everywhere, for free.** Arrow keys, Enter, and Esc map onto the
  same directional/select/back events the joystick already uses, so CardKB
  drives every existing screen without any per-screen changes.
- **Open text box instead of the flip-phone keypad.** When a CardKB is
  present, every screen that previously opened the Nokia-3310-style multi-tap
  keypad (messages, contact rename, channel create/join, repeater
  settings/identity/radio fields, device name, quick replies, room/repeater
  login, the admin CLI line, and more) opens a normal open text field instead.
  With no CardKB attached, every one of those screens behaves exactly as
  before.

**Known issues found during hardware testing, not yet root-caused:**
- The 2048 game's arrow keys don't move tiles.
- The clock app's long-press "hold" actions (e.g. hold to reset a
  stopwatch/timer) don't fire.

Neither depends on code this feature touched, as far as static review shows.
Leading hypothesis: the CardKB input source does a real I2C read every main
loop pass (not just while typing), and that added latency may be disrupting
frame- or long-press-timing elsewhere - not yet confirmed against a
no-CardKB-attached repro of the same bugs.
