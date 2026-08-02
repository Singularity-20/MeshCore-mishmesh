#pragma once

#include <stdint.h>
#include <mishmesh/core/InputEvent.h>

namespace mishmesh {

// Mirrors MomentaryButton's BUTTON_EVENT_* codes, but kept Arduino-free so the
// mappers below stay unit-testable on the host.
enum class Gesture : uint8_t {
  None = 0,
  Click = 1,
  LongPress = 2,
  DoubleClick = 3,
  TripleClick = 4,
};

struct GestureMap {
  InputEvent click = InputEvent::None;
  InputEvent longPress = InputEvent::None;
  InputEvent doubleClick = InputEvent::None;
  InputEvent tripleClick = InputEvent::None;
};

inline InputEvent mapGesture(const GestureMap& m, Gesture g) {
  switch (g) {
    case Gesture::Click:       return m.click;
    case Gesture::LongPress:   return m.longPress;
    case Gesture::DoubleClick: return m.doubleClick;
    case Gesture::TripleClick: return m.tripleClick;
    default:                   return InputEvent::None;
  }
}

enum class Direction : uint8_t { Up, Down, Left, Right, Press };

struct DirectionalMap {
  InputEvent up = InputEvent::NavUp;
  InputEvent down = InputEvent::NavDown;
  InputEvent left = InputEvent::NavLeft;
  InputEvent right = InputEvent::NavRight;
  InputEvent press = InputEvent::Select;        // center click
  InputEvent pressLong = InputEvent::SelectLong; // center long-press
};

inline InputEvent mapDirection(const DirectionalMap& m, Direction d) {
  switch (d) {
    case Direction::Up:    return m.up;
    case Direction::Down:  return m.down;
    case Direction::Left:  return m.left;
    case Direction::Right: return m.right;
    case Direction::Press: return m.press;
    default:               return InputEvent::None;
  }
}

// Classifies one raw byte from a DFRobot-protocol CardKB register: 0 = idle
// (None), arrows/Enter/Esc map onto the same semantic nav events every other
// InputSource uses (so CardKB drives existing screens for free), anything
// else (printables, Backspace) comes back as Char with `outCh` set for the
// caller to interpret in context.
// NOTE: arrow byte values (0xB4-0xB7) are the documented DFRobot SEN0160
// codes; TODO verify against the actual module before relying on nav.
inline InputEvent mapCardKbByte(uint8_t b, char& outCh) {
  switch (b) {
    case 0x00: return InputEvent::None;
    case 0xB5: return InputEvent::NavUp;     // CardKB up arrow
    case 0xB6: return InputEvent::NavDown;   // CardKB down arrow
    case 0xB4: return InputEvent::NavLeft;   // CardKB left arrow
    case 0xB7: return InputEvent::NavRight;  // CardKB right arrow
    case 0x0D: return InputEvent::Select;    // Enter
    case 0x1B: return InputEvent::Back;      // Esc
    default:
      outCh = (char)b;
      return InputEvent::Char;
  }
}

}  // namespace mishmesh
