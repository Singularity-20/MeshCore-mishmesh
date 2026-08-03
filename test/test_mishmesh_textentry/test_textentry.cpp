#include <gtest/gtest.h>
#include <string>
#include <cstring>
#include <mishmesh/applets/TextEntryApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/text/Fonts.h>
#include "FakeDisplayDriver.h"

using namespace mishmesh;

// Start an applet (runs onStart) and return the host so tests can drive input.
struct Harness {
  FakeDisplayDriver d;
  AppletContext ctx;
  AppletHost host;
  Harness(TextEntryApplet* t) : host(&d, ctx) { host.setRoot(t); }
};

TEST(TextEntry, TypingInsertsAtCursor) {
  TextEntryApplet t; Harness h(&t);
  t.onChar('h'); t.onChar('i');
  EXPECT_STREQ("hi", t.text());
  EXPECT_EQ(2, t.cursor());
}

TEST(TextEntry, BackspaceDeletesBeforeCursor) {
  TextEntryApplet t; Harness h(&t);
  t.onChar('h'); t.onChar('i');
  t.onChar(8);   // backspace
  EXPECT_STREQ("h", t.text());
  EXPECT_EQ(1, t.cursor());
}

TEST(TextEntry, BackspaceOnEmptyIsNoop) {
  TextEntryApplet t; Harness h(&t);
  t.onChar(8);
  EXPECT_STREQ("", t.text());
  EXPECT_EQ(0, t.cursor());
}

TEST(TextEntry, NavLeftRightMovesCursorWithoutEditing) {
  TextEntryApplet t; Harness h(&t);
  t.onChar('h'); t.onChar('i');       // "hi", cursor at 2
  t.onInput(InputEvent::NavLeft);
  EXPECT_EQ(1, t.cursor());
  t.onChar('!');                       // insert mid-string -> "h!i"
  EXPECT_STREQ("h!i", t.text());
  t.onInput(InputEvent::NavRight);
  t.onInput(InputEvent::NavRight);
  EXPECT_EQ(3, t.cursor());            // clamped at length
  t.onInput(InputEvent::NavRight);
  EXPECT_EQ(3, t.cursor());
}

namespace {
struct ConfirmCapture {
  std::string text;
  int calls = 0;
  static void cb(void* ctx, const char* txt) {
    auto* self = static_cast<ConfirmCapture*>(ctx);
    self->text = txt ? txt : "";
    self->calls++;
  }
};
}  // namespace

TEST(TextEntry, SelectConfirmsAndFiresCallback) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static TextEntryApplet root; host.setRoot(&root);

  TextEntryApplet t;
  char buf[KeypadApplet::KP_MAX + 1] = {0};
  ConfirmCapture cap;
  t.configure(buf, KeypadApplet::KP_MAX, "Message", &ConfirmCapture::cb, &cap);
  host.push(&t);
  EXPECT_EQ(2, host.depth());

  t.onChar('h'); t.onChar('i');
  t.onInput(InputEvent::Select);

  EXPECT_EQ(1, cap.calls);
  EXPECT_EQ("hi", cap.text);
  EXPECT_EQ(1, host.depth());   // popped back to root
}

TEST(TextEntry, ConfiguredSeedsWorkingBufferFromSource) {
  TextEntryApplet t;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  t.configure(buf, KeypadApplet::KP_MAX, "T");
  EXPECT_STREQ("hi", t.text());
  EXPECT_EQ(2, t.cursor());
}

TEST(TextEntry, EditsDoNotTouchSourceBeforeConfirm) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static TextEntryApplet root; host.setRoot(&root);
  TextEntryApplet t;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  t.configure(buf, KeypadApplet::KP_MAX, "T");
  host.push(&t);
  t.onChar('!');
  EXPECT_STREQ("hi!", t.text());
  EXPECT_STREQ("hi", buf);       // source untouched until confirm
}

TEST(TextEntry, BackWhenCleanPopsWithoutDialog) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static TextEntryApplet root; host.setRoot(&root);
  TextEntryApplet t;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  t.configure(buf, KeypadApplet::KP_MAX, "T");
  host.push(&t);
  host.dispatch(InputEvent::Back);
  EXPECT_EQ(1, host.depth());    // popped, no discard dialog
}

TEST(TextEntry, BackWhenDirtyOpensDiscardConfirm) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static TextEntryApplet root; host.setRoot(&root);
  TextEntryApplet t;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  t.configure(buf, KeypadApplet::KP_MAX, "T");
  host.push(&t);
  t.onChar('!');                  // dirty
  host.dispatch(InputEvent::Back);
  EXPECT_EQ(2, host.depth());     // NOT popped: discard dialog is up
}

TEST(TextEntry, DiscardConfirmedPopsWithoutCommitting) {
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static TextEntryApplet root; host.setRoot(&root);
  TextEntryApplet t;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  t.configure(buf, KeypadApplet::KP_MAX, "T");
  host.push(&t);
  t.onChar('!');
  host.dispatch(InputEvent::Back);     // opens discard confirm, default sel = Confirm
  host.dispatch(InputEvent::Select);   // confirm discard
  EXPECT_EQ(1, host.depth());          // popped
  EXPECT_STREQ("hi", buf);             // source untouched
}

TEST(TextEntryRender, NonEmptyTextIsInsetFromLeftEdge) {
  TextEntryApplet t; Harness h(&t);
  t.onChar('h'); t.onChar('i');
  Canvas c(&h.d, 0);   // now=0 -> blink-off half of the cycle, so no caret rect to filter out
  t.onRender(c);
  ASSERT_FALSE(h.d.fills.empty());
  for (auto& r : h.d.fills) EXPECT_GE(r.x, 2);   // padX
}

TEST(TextEntryRender, CaretDrawnAsBarOnBlinkOnFrame) {
  TextEntryApplet t; Harness h(&t);
  t.onChar('h'); t.onChar('i');
  const mf_font_s* f = fontBody();

  Canvas measure(&h.d, 0);
  int cx, cy;
  measure.measureWrappedCursor(f, measure.width() - 4, "hi", 2, cx, cy);  // width - 2*padX

  Canvas c(&h.d, 500);   // (500/500)%2 == 1 -> blink-on
  t.onRender(c);

  bool found = false;
  for (auto& r : h.d.fills) {
    if (r.x == 2 + cx && r.y == 1 + cy && r.w == 1 && r.h == c.fontHeight(f)) { found = true; break; }
  }
  EXPECT_TRUE(found);
}

TEST(TextEntryRender, CharCountHiddenByDefault) {
  TextEntryApplet t;
  char dst[8]; strcpy(dst, "hi");
  t.configure(dst, 5, "Msg");   // showCharCount defaults to false
  FakeDisplayDriver d;
  Canvas c(&d, 0);
  t.onRender(c);

  FakeDisplayDriver ref;
  Canvas rc(&ref, 0);
  const mf_font_s* cf = fontCaption();
  rc.drawText(cf, rc.width() - 2, rc.height() - 1 - rc.lineHeight(cf), "2/5",
              DisplayDriver::LIGHT, TextAlign::Right);
  ASSERT_FALSE(ref.fills.empty());   // sanity: the reference glyphs actually draw something

  for (auto& rr : ref.fills)
    for (auto& dr : d.fills)
      EXPECT_FALSE(dr.x == rr.x && dr.y == rr.y && dr.w == rr.w && dr.h == rr.h);
}

TEST(TextEntryRender, CharCountShownMatchesExpectedGlyphs) {
  TextEntryApplet t;
  char dst[8]; strcpy(dst, "hi");
  t.configure(dst, 5, "Msg", nullptr, nullptr, true);
  FakeDisplayDriver d;
  Canvas c(&d, 0);
  t.onRender(c);

  FakeDisplayDriver ref;
  Canvas rc(&ref, 0);
  const mf_font_s* cf = fontCaption();
  rc.drawText(cf, rc.width() - 2, rc.height() - 1 - rc.lineHeight(cf), "2/5",
              DisplayDriver::LIGHT, TextAlign::Right);
  ASSERT_FALSE(ref.fills.empty());

  for (auto& rr : ref.fills) {
    bool found = false;
    for (auto& dr : d.fills)
      if (dr.x == rr.x && dr.y == rr.y && dr.w == rr.w && dr.h == rr.h) { found = true; break; }
    EXPECT_TRUE(found);
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
