# Fork changes

Changes in this fork ([Singularity-20/MeshCore-mishmesh](https://github.com/Singularity-20/MeshCore-mishmesh))
on top of upstream [mishmesh](https://github.com/burakcan/MeshCore-mishmesh).
This is not a full feature list - see the main [README](README.md) for that,
and [CHANGELOG.md](CHANGELOG.md) for upstream mishmesh's own versioned
release notes. This file only tracks what's changed in this fork, most
recent first.

## Fixed: text-entry caret misplaced by emoji in existing text

`Canvas::measureWrappedCursor` (used by `TextEntryApplet`'s caret and by the
character-counter/wrap logic) located the wrapped line under the cursor by
treating mcufont's `mf_wordwrap` line-dispatch `count` as a byte count. It's
actually a *character* count - identical to bytes for plain ASCII, which hid
this for a long time, but wrong the moment any multi-byte UTF-8 codepoint
(an emoji) appears in the text, since the cursor index is always a byte
offset. Once the two counts diverged, the cursor's byte offset never matched
any dispatched line, so the caret fell back to `(0, one line down)` and
stayed there no matter what was typed.

Reported as: renaming a contact whose name contains an emoji (e.g. synced in
from a device that has the emoji atlas installed, even if this build
doesn't) showed the caret stuck one line below the name, not moving as you
typed. Would have hit any `TextEntryApplet` field given emoji content, not
just contact rename. Fixed by computing each line's actual byte length by
walking `count` codepoints forward instead of assuming `count` bytes.
Hardware-confirmed.

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
- **Fn+Enter for "hold" actions** (e.g. hold-to-reset on the clock screens).
  The module has no way to report how long a key was physically held - it
  only ever sends one discrete byte per press, confirmed on hardware (see
  the 2048 fix below) - so there's no signal to time a long-press from the
  way the physical button does. Fn+Enter is mapped to the same `SelectLong`
  event instead: the module's own firmware treats Fn as a modifier with its
  own hangtime (press Fn, then Enter - they don't need to overlap) and folds
  it into a single distinct byte (`0xA3`) sent once, which fits the existing
  one-byte-per-keystroke model with no new plumbing. Byte value confirmed
  against the module's own firmware source, not just guessed.

## Text-entry screen (TextEntryApplet) UI improvements

Ongoing polish pass on the CardKB open text box (see "Open text box instead
of the flip-phone keypad" above), each piece hardware-tested individually
unless noted:

- **Inset margins and a real caret bar.** Text no longer sits flush against
  the screen edges, and the blinking cursor is a drawn 1px bar at its own
  pixel position instead of a `'_'`/`' '` character spliced into the string
  each blink. Added `Canvas::measureWrappedCursor`, which maps a character
  index to its pixel position inside word-wrapped text by walking mcufont's
  own wrap pass - the shared primitive every item below builds on. Also
  fixed a pre-existing bug where the old woven-marker cursor could make an
  entire trailing line blink once the field scrolled past ~line 6, since the
  marker character's width didn't match the real character it replaced.
- **Character counter.** Message compose (only) shows a live "n/160" counter
  bottom-right while typing; every other free-text field (renames, settings,
  hex fields) is unaffected.
- **Fixed: discard-confirm dialog was invisible.** `onInput` already entered
  the modal "Discard changes?" state on Esc when the buffer was dirty, but
  `onRender` never actually drew it (unlike the equivalent path in the
  flip-phone keypad), so the dialog opened invisibly while still silently
  swallowing input - a second Esc would cancel it with no visible feedback.
  Confirmed fixed on hardware.
- **Emoji picker (Tab key).** Tab opens a modal emoji grid (reusing the
  existing emoji catalog/atlas and a shared grid widget), Enter inserts the
  selected glyph, Esc cancels; a "tab: emoji" hint shows bottom-left whenever
  the catalog is non-empty. Cursor movement and backspace are now
  codepoint-aware so an inserted (multi-byte UTF-8) emoji moves/deletes as
  one unit instead of corrupting on a partial byte. **Not yet hardware
  verified** - the licensed emoji atlas (`mishmesh/text/emoji-local/`) isn't
  installed in the environment this was built in, so `emojiCatalogCount()`
  is always 0 there and the picker/hint never actually render. Covered by
  native unit tests against a synthetic fake catalog instead (open/close,
  insert, cancel, codepoint-aware nav/backspace, paging). Needs a real
  on-device pass once the atlas is available.
- **Reworded field titles; persistent header tried and reverted.** The
  ~18 field titles (e.g. "Join hashtag") were reworded into fuller
  descriptive phrases ("Enter hashtag channel name") shown as the
  empty-state placeholder. A persistent always-on header (visible even
  while editing, not just pre-typing) was also tried, but reverted after
  hardware testing: once the field scrolled, wrapped text ran straight
  through the header row (no clip existed between them), and the
  always-on look wasn't well liked regardless. Kept the reworded
  placeholder text; dropped the persistent-header rendering change.
- **Thin border.** A 1px frame around the whole screen, square corners,
  drawn last so it overlays content - matching the existing `Card`/`Modal`
  convention. Confirmed on hardware, no interaction issues with the
  counter, emoji hint, caret, or scrolling.

## Fixed: 2048 didn't respond to CardKB arrow keys

Root cause: the CardKB module's key register reads back `0` again immediately after being
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
