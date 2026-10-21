#include "exposition.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace {

// Converts a color to "RRGGBB" (without the '#').
std::string hexOf(Color c) {
  char buf[8];

  snprintf(buf, sizeof(buf), "%02X%02X%02X", c.r, c.g, c.b);

  return buf;
}

// Parses "RGB" or "RRGGBB" (without the '#'). Returns false if invalid.
bool parseHex(const std::string &s, Color &out) {
  std::string h = s;

  if (h.size() == 3) {
    std::string e;

    for (char ch : h) {
      e += ch;
      e += ch;
    }

    h = e;
  }

  if (h.size() != 6)
    return false;

  unsigned v = 0;

  for (char ch : h) {
    if (!isxdigit((unsigned char)ch))
      return false;

    v = v * 16 + (isdigit((unsigned char)ch)
                      ? (unsigned)(ch - '0')
                      : (unsigned)(tolower((unsigned char)ch) - 'a' + 10));
  }

  out = {(unsigned char)((v >> 16) & 255), (unsigned char)((v >> 8) & 255),
         (unsigned char)(v & 255), 255};

  return true;
}

float roundnessFor(Rectangle r, float radius) {
  return radius / (std::min(r.width, r.height) / 2.0f);
}

void drawPickerHandle(Vector2 c, float r, Color fill) {
  DrawCircleV({c.x, c.y + 1.0f}, r + 2.0f, {0, 0, 0, 40});
  DrawCircleV(c, r + 1.5f, {0, 0, 0, 150});
  DrawCircleV(c, r, WHITE);
  DrawCircleV(c, r - 2.5f, fill);
}

} // namespace

Exposition::PickerLayout Exposition::pickerLayout() {
  constexpr float pad = 14.0f;
  constexpr float width = 244.0f;

  PickerLayout L;

  L.panel = {64.0f, 56.0f, width, 264.0f};
  L.sv = {L.panel.x + pad, L.panel.y + pad, width - 2.0f * pad, 160.0f};
  L.hue = {L.sv.x, L.sv.y + L.sv.height + 12.0f, L.sv.width, 16.0f};

  float rowY = L.hue.y + L.hue.height + 14.0f;

  L.oldSwatch = {L.sv.x, rowY, 26.0f, 34.0f};
  L.newSwatch = {L.sv.x + 28.0f, rowY, 26.0f, 34.0f};
  L.hex = {L.sv.x + 64.0f, rowY, L.sv.width - 64.0f, 34.0f};

  return L;
}

void Exposition::syncPickerFromColor(Color c) {
  Vector3 hsv = ColorToHSV(c);

  // For gray/black the hue is undefined: keep the last one, don't zero it.
  if (hsv.y > 0.001f && hsv.z > 0.001f)
    m_pickerHue = hsv.x;

  m_pickerSat = hsv.y;
  m_pickerVal = hsv.z;
  m_pickerHueInitialized = true;
}

void Exposition::applyPickerColor() {
  m_brush.setColor(ColorFromHSV(m_pickerHue, m_pickerSat, m_pickerVal));
}

void Exposition::updatePickerColor(Vector2 pos, int activePart) {
  PickerLayout L = pickerLayout();

  if (activePart == 1) {
    float r = L.hue.height / 2.0f;

    float t = (pos.x - (L.hue.x + r)) / (L.hue.width - 2.0f * r);

    t = std::max(0.0f, std::min(1.0f, t));

    m_pickerHue = t * 359.999f;
  } else if (activePart == 2) {
    float s = (pos.x - L.sv.x) / L.sv.width;
    float v = (pos.y - L.sv.y) / L.sv.height;

    m_pickerSat = std::max(0.0f, std::min(1.0f, s));
    m_pickerVal = 1.0f - std::max(0.0f, std::min(1.0f, v));
  } else {
    return;
  }

  applyPickerColor();
}

void Exposition::beginHexEdit() {
  m_hexEditing = true;
  m_hexFresh = true;
  m_hexBuffer = hexOf(m_brush.getColor());
  m_hexCaretTime = 0.0f;

  // discard characters typed before the field gained focus
  while (GetCharPressed() > 0) {
  }
}

void Exposition::endHexEdit(bool commit) {
  if (!m_hexEditing)
    return;

  if (commit) {
    Color c;

    if (parseHex(m_hexBuffer, c)) {
      m_brush.setColor(c);
      syncPickerFromColor(c);
    }
  }

  m_hexEditing = false;
}

void Exposition::updateHexInput() {
  if (!m_hexEditing)
    return;

  m_hexCaretTime += GetFrameTime();

  bool modifier = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER) ||
                  IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

  if (modifier && IsKeyPressed(KEY_V)) {
    const char *clip = GetClipboardText();

    if (clip) {
      std::string pasted;

      for (const char *p = clip; *p; p++) {
        if (isxdigit((unsigned char)*p))
          pasted += (char)toupper((unsigned char)*p);
      }

      if (!pasted.empty()) {
        m_hexBuffer = pasted.substr(0, 6);
        m_hexFresh = false;
      }
    }
  }

  int ch;

  while ((ch = GetCharPressed()) > 0) {
    if (ch < 128 && isxdigit(ch)) {
      if (m_hexFresh) {
        m_hexBuffer.clear();
        m_hexFresh = false;
      }

      if (m_hexBuffer.size() < 6)
        m_hexBuffer += (char)toupper(ch);
    }
  }

  if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
    if (m_hexFresh) {
      m_hexBuffer.clear();
      m_hexFresh = false;
    } else if (!m_hexBuffer.empty()) {
      m_hexBuffer.pop_back();
    }
  }

  // apply live as soon as all 6 digits are in
  Color c;

  if (m_hexBuffer.size() == 6 && parseHex(m_hexBuffer, c)) {
    m_brush.setColor(c);
    syncPickerFromColor(c);
  }

  if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
    endHexEdit(true);
}

void Exposition::drawColorPicker(Vector2 plusCenter) {
  PickerLayout L = pickerLayout();

  // The displayed color is the one the brush actually uses; HSV only
  // positions the cursors (avoids the hex drifting by 1 due to rounding).
  Color current = m_brush.getColor();
  Color pure = ColorFromHSV(m_pickerHue, 1.0f, 1.0f);

  // soft shadow
  for (int i = 5; i >= 1; i--) {
    Rectangle s = {L.panel.x - i, L.panel.y - i + 4.0f,
                   L.panel.width + 2.0f * i, L.panel.height + 2.0f * i};

    DrawRectangleRounded(s, roundnessFor(s, 12.0f + i), 24, {0, 0, 0, 9});
  }

  float panelRound = roundnessFor(L.panel, 12.0f);

  DrawRectangleRounded(L.panel, panelRound, 32, WHITE);
  DrawRectangleRoundedLinesEx(L.panel, panelRound, 32, 1.0f, BLACK);

  // little arrow pointing at the button that opened the picker
  float cy =
      std::max(L.panel.y + 22.0f,
               std::min(L.panel.y + L.panel.height - 22.0f, plusCenter.y));

  Vector2 tip = {L.panel.x - 8.0f, cy};
  Vector2 top = {L.panel.x + 1.0f, cy - 8.0f};
  Vector2 bottom = {L.panel.x + 1.0f, cy + 8.0f};

  DrawTriangle(tip, bottom, top, WHITE);
  DrawLineEx(tip, top, 1.0f, BLACK);
  DrawLineEx(tip, bottom, 1.0f, BLACK);
  DrawLineEx({L.panel.x, cy - 7.0f}, {L.panel.x, cy + 7.0f}, 2.5f, WHITE);

  // saturation (x) and brightness (y) square
  DrawRectangleGradientH((int)L.sv.x, (int)L.sv.y, (int)L.sv.width,
                         (int)L.sv.height, WHITE, pure);

  DrawRectangleGradientV((int)L.sv.x, (int)L.sv.y, (int)L.sv.width,
                         (int)L.sv.height, BLANK, BLACK);

  DrawRectangleLinesEx(L.sv, 1.0f, BLACK);

  Vector2 svPos = {L.sv.x + m_pickerSat * L.sv.width,
                   L.sv.y + (1.0f - m_pickerVal) * L.sv.height};

  drawPickerHandle(svPos, 7.0f, current);

  // pill-shaped hue slider
  float hr = L.hue.height / 2.0f;
  float hcy = L.hue.y + hr;
  float hx0 = L.hue.x + hr;
  float hx1 = L.hue.x + L.hue.width - hr;
  float seg = (hx1 - hx0) / 6.0f;

  Color red = ColorFromHSV(0.0f, 1.0f, 1.0f);

  DrawCircleV({hx0, hcy}, hr, red);
  DrawCircleV({hx1, hcy}, hr, red);

  for (int i = 0; i < 6; i++) {
    Color a = ColorFromHSV(i * 60.0f, 1.0f, 1.0f);
    Color b = ColorFromHSV((i + 1) * 60.0f, 1.0f, 1.0f);

    DrawRectangleGradientEx({hx0 + seg * i, L.hue.y, seg, L.hue.height}, a, a,
                            b, b);
  }

  DrawRectangleRoundedLinesEx(L.hue, 1.0f, 24, 1.0f, BLACK);

  float hueX = hx0 + (m_pickerHue / 360.0f) * (hx1 - hx0);

  drawPickerHandle({hueX, hcy}, 9.0f, pure);

  // preview: color from when the picker opened (click to revert) and the
  // current one
  float chipRound = roundnessFor(L.oldSwatch, 6.0f);

  DrawRectangleRounded(L.oldSwatch, chipRound, 12, m_pickerOriginal);
  DrawRectangleRoundedLinesEx(L.oldSwatch, chipRound, 12, 1.0f, BLACK);

  DrawRectangleRounded(L.newSwatch, chipRound, 12, current);
  DrawRectangleRoundedLinesEx(L.newSwatch, chipRound, 12, 1.0f, BLACK);

  // hex field
  float hexRound = roundnessFor(L.hex, 8.0f);

  DrawRectangleRounded(L.hex, hexRound, 16, {245, 245, 245, 255});

  if (m_hexEditing)
    DrawRectangleRoundedLinesEx(L.hex, hexRound, 16, 2.0f, BLUE);
  else
    DrawRectangleRoundedLinesEx(L.hex, hexRound, 16, 1.0f, BLACK);

  std::string digits = m_hexEditing ? m_hexBuffer : hexOf(current);

  constexpr float hexFont = 17.0f;

  float textY = L.hex.y + (L.hex.height - hexFont) / 2.0f;
  float textX = L.hex.x + 12.0f;

  DrawTextEx(m_interFont, "#", {textX, textY}, hexFont, 1.0f, GRAY);

  float digitsX =
      textX + MeasureTextEx(m_interFont, "#", hexFont, 1.0f).x + 2.0f;
  float digitsW = MeasureTextEx(m_interFont, digits.c_str(), hexFont, 1.0f).x;

  // whole text selected right when entering the field
  if (m_hexEditing && m_hexFresh && !digits.empty())
    DrawRectangle((int)digitsX - 2, (int)textY - 1, (int)digitsW + 4,
                  (int)hexFont + 2, {180, 210, 255, 255});

  DrawTextEx(m_interFont, digits.c_str(), {digitsX, textY}, hexFont, 1.0f,
             BLACK);

  if (m_hexEditing && !m_hexFresh && fmodf(m_hexCaretTime, 1.0f) < 0.5f)
    DrawLineEx({digitsX + digitsW + 1.0f, textY},
               {digitsX + digitsW + 1.0f, textY + hexFont}, 1.5f, BLACK);
}
