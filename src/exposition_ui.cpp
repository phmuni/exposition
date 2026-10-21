#include "exposition.h"

#include <algorithm>

bool Exposition::handleUI(Vector2 pos, bool pressed, bool down) {
  bool overUI = false;

  if (m_showResPopup) {
    float cx = (float)GetScreenWidth() / 2.0f;

    Rectangle dropHeaderRec = {cx - 80, 16, 160, 32};

    if (CheckCollisionPointRec(pos, dropHeaderRec)) {
      overUI = true;

      if (pressed)
        m_showResDropdown = !m_showResDropdown;
    }

    if (m_showResDropdown) {
      float totalHeight = 32.0f + (float)m_resolutions.size() * 32.0f;

      Rectangle modalRec = {cx - 80, 16, 160, totalHeight};

      if (CheckCollisionPointRec(pos, modalRec)) {
        overUI = true;

        if (pressed) {
          for (size_t i = 0; i < m_resolutions.size(); i++) {
            Rectangle itemRec = {cx - 80, 48.0f + (float)i * 32.0f, 160, 32};

            if (CheckCollisionPointRec(pos, itemRec)) {
              m_selectedRes = (int)i;

              m_showResDropdown = false;

              m_showResPopup = false;

              m_canvas.clear();

              m_selectedIndex = 0;

              addCanva(m_resolutions[m_selectedRes].w,
                       m_resolutions[m_selectedRes].h);

              break;
            }
          }
        }
      }
    }

    if (pressed && !overUI) {
      m_showResPopup = false;
      m_showResDropdown = false;
    }

    if (m_showResPopup)
      overUI = true;
  }

  MouseCursor cursor = MOUSE_CURSOR_DEFAULT;

  constexpr Rectangle mainColorRec = {16, 56, 32, 32};

  if (CheckCollisionPointRec(pos, mainColorRec)) {
    overUI = true;
  }

  for (size_t i = 0; i < m_recentColors.size(); i++) {
    Rectangle recRec = {16.0f, 56.0f + (i + 1) * 36.0f, 32.0f, 32.0f};

    if (CheckCollisionPointRec(pos, recRec)) {
      overUI = true;
      cursor = MOUSE_CURSOR_POINTING_HAND;

      if (pressed) {
        Color picked = m_recentColors[i];

        m_brush.setColor(picked);
        syncPickerFromColor(picked);

        // with the picker open it stays open and follows the color
        endHexEdit(false);
      }
    }
  }

  float plusY = 56.0f + (m_recentColors.size() + 1) * 36.0f;

  Rectangle plusRec = {16.0f, plusY, 32.0f, 32.0f};

  if (CheckCollisionPointRec(pos, plusRec)) {
    overUI = true;
    cursor = MOUSE_CURSOR_POINTING_HAND;

    if (pressed) {
      m_advancedPickerOpen = !m_advancedPickerOpen;

      m_pickerActivePart = 0;

      if (m_advancedPickerOpen) {
        m_pickerOriginal = m_brush.getColor();
        syncPickerFromColor(m_pickerOriginal);
      } else {
        endHexEdit(true);
      }
    }
  }

  if (m_advancedPickerOpen) {
    PickerLayout L = pickerLayout();

    Rectangle hueHit = {L.hue.x - 6.0f, L.hue.y - 6.0f, L.hue.width + 12.0f,
                        L.hue.height + 12.0f};

    bool insidePanel = CheckCollisionPointRec(pos, L.panel);

    if (insidePanel)
      overUI = true;

    if (insidePanel) {
      if (CheckCollisionPointRec(pos, L.sv))
        cursor = MOUSE_CURSOR_CROSSHAIR;
      else if (CheckCollisionPointRec(pos, hueHit))
        cursor = MOUSE_CURSOR_RESIZE_EW;
      else if (CheckCollisionPointRec(pos, L.oldSwatch))
        cursor = MOUSE_CURSOR_POINTING_HAND;
      else if (CheckCollisionPointRec(pos, L.hex))
        cursor = MOUSE_CURSOR_IBEAM;
    }

    if (pressed && insidePanel) {
      if (CheckCollisionPointRec(pos, L.sv)) {
        endHexEdit(true);
        m_pickerActivePart = 2;
      } else if (CheckCollisionPointRec(pos, hueHit)) {
        endHexEdit(true);
        m_pickerActivePart = 1;
      } else if (CheckCollisionPointRec(pos, L.oldSwatch)) {
        endHexEdit(false);

        m_brush.setColor(m_pickerOriginal);
        syncPickerFromColor(m_pickerOriginal);
      } else if (CheckCollisionPointRec(pos, L.hex)) {
        if (!m_hexEditing)
          beginHexEdit();
      } else {
        endHexEdit(true);
      }
    }

    // Dragging keeps working even if the mouse leaves the panel.
    if (down && m_pickerActivePart != 0 && (m_uiConsumedPress || pressed)) {
      overUI = true;

      cursor = m_pickerActivePart == 2 ? MOUSE_CURSOR_CROSSHAIR
                                       : MOUSE_CURSOR_RESIZE_EW;

      updatePickerColor(pos, m_pickerActivePart);
    }
  }

  SetMouseCursor(cursor);

  return overUI;
}

void Exposition::renderUI() {
  constexpr float fontSize = 24.0f;
  constexpr float thickness = 1.0f;
  constexpr float radius = 16.0f;

  Vector2 centerLeft = {32.0f, 72.0f};

  Vector2 mouse =
      m_adjustingBrush ? Vector2{-1000.0f, -1000.0f} : GetMousePosition();

  DrawCircleV(centerLeft, radius + thickness * 1.5f, BLUE);

  DrawCircleV(centerLeft, radius - thickness, m_brush.getColor());

  for (size_t i = 0; i < m_recentColors.size(); i++) {
    Vector2 c = {32.0f, 72.0f + (i + 1) * 36.0f};

    if (CheckCollisionPointCircle(mouse, c, radius))
      DrawCircleV(c, radius + 2.0f, BLUE);

    DrawCircleV(c, radius, BLACK);

    DrawCircleV(c, radius - thickness, m_recentColors[i]);
  }

  float plusCenterY = 72.0f + (m_recentColors.size() + 1) * 36.0f;

  Vector2 plusCenter = {32.0f, plusCenterY};

  if (m_advancedPickerOpen ||
      CheckCollisionPointCircle(mouse, plusCenter, radius))
    DrawCircleV(plusCenter, radius + 2.0f, BLUE);

  DrawCircleV(plusCenter, radius, BLACK);

  DrawCircleV(plusCenter, radius - thickness,
              m_advancedPickerOpen ? Color{235, 235, 235, 255} : WHITE);

  // the "+" rotates 45° and becomes "×" while the picker is open
  if (m_advancedPickerOpen) {
    DrawLineEx({plusCenter.x - 5, plusCenter.y - 5},
               {plusCenter.x + 5, plusCenter.y + 5}, 2.0f, BLACK);

    DrawLineEx({plusCenter.x - 5, plusCenter.y + 5},
               {plusCenter.x + 5, plusCenter.y - 5}, 2.0f, BLACK);
  } else {
    DrawLineEx({plusCenter.x - 7, plusCenter.y},
               {plusCenter.x + 7, plusCenter.y}, 2.0f, BLACK);

    DrawLineEx({plusCenter.x, plusCenter.y - 7},
               {plusCenter.x, plusCenter.y + 7}, 2.0f, BLACK);
  }

  if (m_advancedPickerOpen)
    drawColorPicker(plusCenter);

  // fill mode indicator: drop with the current color next to the cursor
  if (IsKeyDown(KEY_G) && !m_lastOverUI && !m_adjustingBrush) {
    Vector2 m = GetMousePosition();
    Vector2 c = {m.x + 20.0f, m.y + 20.0f};

    DrawCircleV({c.x, c.y + 1.0f}, 12.0f, {0, 0, 0, 40});
    DrawCircleV(c, 11.0f, BLACK);
    DrawCircleV(c, 10.0f, WHITE);
    DrawCircleV(c, 7.0f, m_brush.getColor());
  }

  const char *fullText =
      TextFormat("%.0f | %.0f%% | %.0f%%", m_brush.getSize(),
                 m_brush.getSmoothing(), m_brush.getOpacity());

  Vector2 fullSize = MeasureTextEx(m_interFont, fullText, fontSize, 1.0f);

  const char *sizeText = TextFormat("%.0f", m_brush.getSize());

  Vector2 sizeDimensions =
      MeasureTextEx(m_interFont, sizeText, fontSize, 1.0f);

  int textX = (int)centerLeft.x - ((int)sizeDimensions.x / 2);

  if (textX + (int)fullSize.x > GetScreenWidth() - 16) {
    textX = GetScreenWidth() - 16 - (int)fullSize.x;
  }

  if (textX < 16)
    textX = 16;

  DrawTextEx(m_interFont, fullText, {(float)textX, 20.0f}, fontSize, 1.0f,
             BLACK);

  const char *zoom = TextFormat("%.1f", m_camera.zoom);

  Vector2 zoomSize = MeasureTextEx(m_interFont, zoom, fontSize, 1.0f);

  float rectRightX = (float)GetScreenWidth() - 48.0f;

  DrawTextEx(m_interFont, zoom,
             {rectRightX + 16.0f - zoomSize.x / 2.0f, 20.0f}, fontSize, 1.0f,
             BLACK);

  if (m_showResPopup) {
    float cx = (float)GetScreenWidth() / 2.0f;

    float modalRadius = 8.0f;

    float roundness = modalRadius / 80.0f;

    if (!m_showResDropdown) {
      Rectangle dropHeaderRec = {cx - 80, 16, 160, 32};

      DrawRectangleRounded(dropHeaderRec, 1.0f, 32, WHITE);

      DrawRectangleRoundedLinesEx(dropHeaderRec, 1.0f, 32, 1.0f, BLACK);

      const char *resText =
          TextFormat("%dx%d", m_resolutions[m_selectedRes].w,
                     m_resolutions[m_selectedRes].h);

      float resW = MeasureTextEx(m_interFont, resText, 20, 1).x;

      DrawTextEx(m_interFont, resText, {cx - resW / 2.0f - 10, 22}, 20, 1,
                 BLACK);

      DrawLineEx({cx + 50, 28}, {cx + 57.5f, 36}, 1.0f, BLACK);

      DrawLineEx({cx + 57.5f, 36}, {cx + 65, 28}, 1.0f, BLACK);
    } else {
      float totalHeight = 32.0f + (float)m_resolutions.size() * 32.0f;

      Rectangle modalRec = {cx - 80, 16, 160, totalHeight};

      DrawRectangleRounded(modalRec, roundness, 32, WHITE);

      DrawRectangleRoundedLinesEx(modalRec, roundness, 32, 1.0f, BLACK);

      const char *resText =
          TextFormat("%dx%d", m_resolutions[m_selectedRes].w,
                     m_resolutions[m_selectedRes].h);

      float resW = MeasureTextEx(m_interFont, resText, 20, 1).x;

      DrawTextEx(m_interFont, resText, {cx - resW / 2.0f - 10, 22}, 20, 1,
                 BLACK);

      DrawLineEx({cx + 50, 36}, {cx + 57.5f, 28}, 1.0f, BLACK);

      DrawLineEx({cx + 57.5f, 28}, {cx + 65, 36}, 1.0f, BLACK);

      DrawLineEx({cx - 80, 48}, {cx + 80, 48}, 1.0f, BLACK);

      Vector2 mousePos = GetMousePosition();

      for (size_t i = 0; i < m_resolutions.size(); i++) {
        Rectangle itemRec = {cx - 80, 48.0f + (float)i * 32.0f, 160, 32};

        if (CheckCollisionPointRec(mousePos, itemRec)) {
          if (i == m_resolutions.size() - 1) {
            float r = modalRadius;

            Color hoverColor = {235, 235, 235, 255};

            DrawRectangle((int)itemRec.x, (int)itemRec.y, (int)itemRec.width,
                          (int)(itemRec.height - r), hoverColor);

            DrawRectangle((int)(itemRec.x + r),
                          (int)(itemRec.y + itemRec.height - r),
                          (int)(itemRec.width - 2 * r), (int)r, hoverColor);

            DrawCircle((int)(itemRec.x + r),
                       (int)(itemRec.y + itemRec.height - r), r, hoverColor);

            DrawCircle((int)(itemRec.x + itemRec.width - r),
                       (int)(itemRec.y + itemRec.height - r), r, hoverColor);
          } else {
            DrawRectangleRec(itemRec, {235, 235, 235, 255});
          }
        }

        const char *itemText =
            TextFormat("%dx%d", m_resolutions[i].w, m_resolutions[i].h);

        float itemW = MeasureTextEx(m_interFont, itemText, 20, 1).x;

        DrawTextEx(m_interFont, itemText,
                   {cx - itemW / 2.0f, 48.0f + (float)i * 32.0f + 6}, 20, 1,
                   BLACK);
      }
    }
  }
}
