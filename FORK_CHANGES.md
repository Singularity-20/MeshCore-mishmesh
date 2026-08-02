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

## Fixed: 2048 didn't respond to CardKB arrow keys

Root cause (confirmed on hardware, not the original I2C-latency guess): the
CardKB module's key register reads back `0` again immediately after being
read over I2C, even while the key is still physically held down - there is no
continuous "held" byte to observe, only a one-shot pulse per press. That's
fine for menu navigation (which reacts to discrete press events), but 2048
runs on an Arduboy compatibility bridge that polls a live held-button snapshot
once per frame, and `CardKbSource` wasn't populating that snapshot at all
(`heldMask()` was left at its default of "always 0").

Fixed by having `CardKbSource` stretch each observed press into a short
(150ms) synthetic hold window, long enough for the frame-polled bridge to
sample a true value before it lapses back to `0` - which conveniently also
satisfies the game's own debounce, which needs to see a release before the
next move can register. One tile-move per key press, matching how most
keyboard-driven tile games behave (not continuous scrolling while held).

**Known issues found during hardware testing, not yet root-caused:**
- The clock app's long-press "hold" actions (e.g. hold to reset a
  stopwatch/timer) don't fire. Not yet investigated.
