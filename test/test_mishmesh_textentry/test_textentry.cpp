#include <gtest/gtest.h>
#include <string>
#include <cstring>
#include <mishmesh/applets/TextEntryApplet.h>
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/core/Canvas.h>
#include <mishmesh/core/EmojiCatalog.h>
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

TEST(TextEntryRender, DiscardConfirmDialogIsActuallyDrawn) {
  // Regression test: onInput correctly enters the discard-confirm modal state
  // (covered by TextEntry.BackWhenDirtyOpensDiscardConfirm), but that alone
  // isn't enough - onRender must also draw it, or the dialog is invisible on
  // hardware while still silently swallowing input (KeypadApplet's onRender
  // has the equivalent `if (_confirming) { _confirm.draw(...); return; }`).
  setEmojiCatalog(nullptr, 0);   // deterministic regardless of test order
  FakeDisplayDriver d; AppletContext ctx; AppletHost host(&d, ctx);
  static TextEntryApplet root; host.setRoot(&root);
  TextEntryApplet t;
  char buf[KeypadApplet::KP_MAX + 1]; strcpy(buf, "hi");
  t.configure(buf, KeypadApplet::KP_MAX, "T");
  host.push(&t);
  t.onChar('!');                  // dirty
  host.dispatch(InputEvent::Back);   // opens discard confirm
  Canvas c(&d, 0);
  t.onRender(c);
  EXPECT_FALSE(d.fills.empty() && d.rects.empty());   // dialog scrim/box actually drew something
}

TEST(TextEntryRender, NonEmptyTextIsInsetFromLeftEdge) {
  setEmojiCatalog(nullptr, 0);   // deterministic regardless of test order
  TextEntryApplet t; Harness h(&t);
  t.onChar('h'); t.onChar('i');
  Canvas c(&h.d, 0);   // now=0 -> blink-off half of the cycle, so no caret rect to filter out
  t.onRender(c);
  ASSERT_FALSE(h.d.fills.empty());
  for (auto& r : h.d.fills) EXPECT_GE(r.x, 2);   // padX
}

TEST(TextEntryRender, CaretDrawnAsBarOnBlinkOnFrame) {
  setEmojiCatalog(nullptr, 0);   // deterministic regardless of test order
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
  setEmojiCatalog(nullptr, 0);   // deterministic regardless of test order
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
  setEmojiCatalog(nullptr, 0);   // deterministic regardless of test order
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

TEST(TextEntryEmoji, TabWithEmptyCatalogDoesNotOpenPicker) {
  setEmojiCatalog(nullptr, 0);
  TextEntryApplet t; Harness h(&t);
  t.onChar(9);      // Tab, catalog empty -> no-op
  t.onChar('x');    // if the picker had wrongly opened, typing would be swallowed
  EXPECT_STREQ("x", t.text());
}

TEST(TextEntryEmoji, TabOpensPickerSelectInsertsEmojiAndCloses) {
  static const uint32_t kCat[] = { 0x1F600 };
  setEmojiCatalog(kCat, 1);
  TextEntryApplet t; Harness h(&t);
  t.onChar(9);                         // open picker
  t.onInput(InputEvent::Select);       // insert the only cell (0,0)
  char expected[5];
  int n = KeypadApplet::utf8Encode(0x1F600, expected);
  EXPECT_STREQ(expected, t.text());
  EXPECT_EQ(n, t.cursor());
  t.onChar('x');                       // picker closed -> normal typing resumes
  EXPECT_EQ(n + 1, (int)t.length());
  setEmojiCatalog(nullptr, 0);
}

TEST(TextEntryEmoji, BackWhilePickingCancelsWithoutInserting) {
  static const uint32_t kCat[] = { 0x1F600 };
  setEmojiCatalog(kCat, 1);
  TextEntryApplet t; Harness h(&t);
  t.onChar('h');
  t.onChar(9);                    // open picker
  t.onInput(InputEvent::Back);    // cancel
  EXPECT_STREQ("h", t.text());    // unchanged
  t.onChar('i');                  // picker closed -> typing resumes
  EXPECT_STREQ("hi", t.text());
  setEmojiCatalog(nullptr, 0);
}

TEST(TextEntryEmoji, NavAndBackspaceTreatEmojiAsOneUnit) {
  static const uint32_t kCat[] = { 0x1F600 };   // 4-byte UTF-8 codepoint
  setEmojiCatalog(kCat, 1);
  TextEntryApplet t; Harness h(&t);
  t.onChar('a');
  t.onChar(9);
  t.onInput(InputEvent::Select);       // "a<emoji>"
  t.onChar('b');                       // "a<emoji>b"

  char emoji[5];
  int n = KeypadApplet::utf8Encode(0x1F600, emoji);
  std::string expected = std::string("a") + emoji + "b";
  EXPECT_EQ(expected, t.text());

  uint16_t afterB = t.cursor();        // 1 + n + 1
  t.onInput(InputEvent::NavLeft);      // step back over 'b'
  EXPECT_EQ(afterB - 1, t.cursor());
  t.onInput(InputEvent::NavLeft);      // step back over the whole emoji, not one byte
  EXPECT_EQ(1, t.cursor());
  t.onInput(InputEvent::NavRight);     // step forward over the whole emoji
  EXPECT_EQ(1 + n, t.cursor());

  t.onChar(8);                         // backspace removes the whole emoji, not one byte
  EXPECT_STREQ("ab", t.text());
  setEmojiCatalog(nullptr, 0);
}

TEST(TextEntryEmoji, PagingRevealsCellsBeyondFirstPage) {
  static uint32_t kCat[13];
  for (int i = 0; i < 13; i++) kCat[i] = 0x1F600 + i;   // 13 -> 2 pages (12 + 1)
  setEmojiCatalog(kCat, 13);
  TextEntryApplet t; Harness h(&t);
  t.onChar(9);                              // open picker, page 1, focus (0,0)
  for (int i = 0; i < 3; i++) t.onInput(InputEvent::NavRight);   // -> (0,3), last col
  t.onInput(InputEvent::NavRight);          // at the right edge -> pages instead of wrapping
  // Paging keeps the grid's focus position (same as KeypadApplet's emoji grid) -
  // it doesn't reset to (0,0), so step back to column 0 on the new page.
  for (int i = 0; i < 3; i++) t.onInput(InputEvent::NavLeft);
  t.onInput(InputEvent::Select);            // select (0,0) of page 2 -> the 13th codepoint
  char expected[5];
  KeypadApplet::utf8Encode(kCat[12], expected);
  EXPECT_STREQ(expected, t.text());
  setEmojiCatalog(nullptr, 0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
