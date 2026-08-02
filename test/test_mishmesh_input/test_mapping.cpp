#include <gtest/gtest.h>
#include <mishmesh/core/InputMapping.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/InputSource.h>
#include "FakeDisplayDriver.h"
#include <deque>
#include <vector>

using namespace mishmesh;

namespace {
// Releases queued events on poll(), as a real source would when drained.
// `chq` is optional and only consulted for Char events (existing tests keep
// assigning `q` directly with a braced InputEvent list, so its type/usage
// stays unchanged; ch defaults to 0 when chq has nothing queued for a pop).
struct QueueSource : InputSource {
  std::deque<InputEvent> q;
  std::deque<char> chq;
  bool poll(InputReport& out) override {
    if (q.empty()) return false;
    out.event = q.front(); q.pop_front();
    if (!chq.empty()) { out.ch = chq.front(); chq.pop_front(); }
    else out.ch = 0;
    return true;
  }
};
// Records every event/char the host actually delivers.
struct RecordingApplet : Applet {
  std::vector<InputEvent> got;
  std::vector<char> gotChars;
  RecordingApplet() : Applet("rec") {}
  int onRender(Canvas&) override { return 500; }
  bool onInput(InputEvent ev) override { got.push_back(ev); return true; }
  bool onChar(char ch) override { gotChars.push_back(ch); return true; }
};
}  // namespace

TEST(MapGesture, MapsEachGestureToConfiguredEvent) {
  GestureMap m;
  m.click = InputEvent::NavDown;
  m.doubleClick = InputEvent::Select;
  m.longPress = InputEvent::Back;
  // tripleClick left as default (None)

  EXPECT_EQ(InputEvent::NavDown, mapGesture(m, Gesture::Click));
  EXPECT_EQ(InputEvent::Select,  mapGesture(m, Gesture::DoubleClick));
  EXPECT_EQ(InputEvent::Back,    mapGesture(m, Gesture::LongPress));
  EXPECT_EQ(InputEvent::None,    mapGesture(m, Gesture::TripleClick));
  EXPECT_EQ(InputEvent::None,    mapGesture(m, Gesture::None));
}

TEST(MapDirection, DefaultMapMatchesNavDirections) {
  DirectionalMap m;  // defaults: up->NavUp, ..., press->Select
  EXPECT_EQ(InputEvent::NavUp,    mapDirection(m, Direction::Up));
  EXPECT_EQ(InputEvent::NavDown,  mapDirection(m, Direction::Down));
  EXPECT_EQ(InputEvent::NavLeft,  mapDirection(m, Direction::Left));
  EXPECT_EQ(InputEvent::NavRight, mapDirection(m, Direction::Right));
  EXPECT_EQ(InputEvent::Select,   mapDirection(m, Direction::Press));
}

TEST(MapDirection, RespectsCustomPressEvent) {
  DirectionalMap m;
  m.press = InputEvent::Back;
  EXPECT_EQ(InputEvent::Back, mapDirection(m, Direction::Press));
}

TEST(InputDebounce, CoalescesBouncedRepeatButKeepsRealPresses) {
  FakeDisplayDriver d;
  RecordingApplet app;
  AppletContext ctx;
  AppletHost host(&d, ctx);
  QueueSource src;
  host.addSource(&src);
  host.setRoot(&app);

  // Two identical events in one drain (contact bounce) -> delivered once.
  src.q = {InputEvent::NavDown, InputEvent::NavDown};
  host.loop(0);
  EXPECT_EQ(1u, app.got.size());

  // Same event a few ms later (still bouncing) -> suppressed.
  src.q = {InputEvent::NavDown};
  host.loop(30);
  EXPECT_EQ(1u, app.got.size());

  // A distinct event is never suppressed.
  src.q = {InputEvent::Select};
  host.loop(35);
  EXPECT_EQ(2u, app.got.size());

  // The same event well past the window is a real second press.
  src.q = {InputEvent::Select};
  host.loop(200);
  EXPECT_EQ(3u, app.got.size());
}

TEST(MapCardKbByte, ArrowsMapToNavEvents) {
  char ch = 0;
  EXPECT_EQ(InputEvent::NavUp,    mapCardKbByte(0xB5, ch));
  EXPECT_EQ(InputEvent::NavDown,  mapCardKbByte(0xB6, ch));
  EXPECT_EQ(InputEvent::NavLeft,  mapCardKbByte(0xB4, ch));
  EXPECT_EQ(InputEvent::NavRight, mapCardKbByte(0xB7, ch));
}

TEST(MapCardKbByte, EnterAndEscMapToSelectAndBack) {
  char ch = 0;
  EXPECT_EQ(InputEvent::Select, mapCardKbByte(0x0D, ch));
  EXPECT_EQ(InputEvent::Back,   mapCardKbByte(0x1B, ch));
}

TEST(MapCardKbByte, FnEnterMapsToSelectLong) {
  char ch = 0;
  EXPECT_EQ(InputEvent::SelectLong, mapCardKbByte(0xA3, ch));
}

TEST(MapCardKbByte, IdleByteIsNone) {
  char ch = 0;
  EXPECT_EQ(InputEvent::None, mapCardKbByte(0x00, ch));
}

TEST(MapCardKbByte, PrintableAndBackspaceComeBackAsChar) {
  char ch = 0;
  EXPECT_EQ(InputEvent::Char, mapCardKbByte('a', ch));
  EXPECT_EQ('a', ch);

  ch = 0;
  EXPECT_EQ(InputEvent::Char, mapCardKbByte(0x08, ch));
  EXPECT_EQ(0x08, ch);
}

// Regression test for the bug the Char-event path in pumpInput() fixes: the
// bounce-coalescing window keys only on InputEvent, so two different
// characters (same InputEvent::Char) typed within INPUT_DEBOUNCE_MS used to
// look like the same key bouncing and the second was silently dropped.
TEST(InputDebounce, CharEventsBypassBounceCoalescing) {
  FakeDisplayDriver d;
  RecordingApplet app;
  AppletContext ctx;
  AppletHost host(&d, ctx);
  QueueSource src;
  host.addSource(&src);
  host.setRoot(&app);

  src.q = {InputEvent::Char};
  src.chq = {'a'};
  host.loop(0);

  // A different character just 5ms later - well inside INPUT_DEBOUNCE_MS -
  // must still be delivered, unlike a real repeated NavDown/Select.
  src.q = {InputEvent::Char};
  src.chq = {'b'};
  host.loop(5);

  ASSERT_EQ(2u, app.gotChars.size());
  EXPECT_EQ('a', app.gotChars[0]);
  EXPECT_EQ('b', app.gotChars[1]);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
