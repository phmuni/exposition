#include "exposition.h"

#include <algorithm>
#include <cmath>

namespace {

// The font lives in assets/, next to bin/. The path is resolved from the
// executable (not the current directory) so it works from anywhere.
Font loadInterFont() {
  const char *appDir = GetApplicationDirectory();

  const char *candidates[] = {
      TextFormat("%s/../assets/Inter.ttf", appDir),
      "assets/Inter.ttf",
  };

  for (const char *path : candidates) {
    if (FileExists(path))
      return LoadFont(path);
  }

  TraceLog(LOG_WARNING, "Inter.ttf not found in assets/; using the default "
                        "raylib font.");

  return GetFontDefault();
}

} // namespace

Exposition::Exposition() {
  m_canvas.reserve(16);

  m_dpiScale = GetWindowScaleDPI().x;

  m_interFont = loadInterFont();

  SetTextureFilter(m_interFont.texture, TEXTURE_FILTER_BILINEAR);
}

Exposition::~Exposition() {
  if (m_adjustingBrush)
    EnableCursor();

  UnloadFont(m_interFont);
}

void Exposition::addCanva(int w, int h) {
  m_canvas.emplace_back(w, h);
  resetCamera();
}

void Exposition::addRecentColor(Color c) {
  if (!m_recentColors.empty() && m_recentColors[0].r == c.r &&
      m_recentColors[0].g == c.g && m_recentColors[0].b == c.b) {
    return;
  }

  m_recentColors.insert(m_recentColors.begin(), c);

  if (m_recentColors.size() > 4)
    m_recentColors.pop_back();
}

void Exposition::resetCamera() {
  if (m_canvas.empty())
    return;

  float fitScale = std::min(((float)GetScreenWidth() - 100.0f) /
                                m_canvas[m_selectedIndex].getWidth(),
                            ((float)GetScreenHeight() - 100.0f) /
                                m_canvas[m_selectedIndex].getHeight());

  m_camera.zoom = fitScale;

  m_camera.target = {(float)m_canvas[m_selectedIndex].getWidth() / 2.0f,
                     (float)m_canvas[m_selectedIndex].getHeight() / 2.0f};

  m_camera.offset = {(float)GetScreenWidth() / 2.0f,
                     (float)GetScreenHeight() / 2.0f};

  m_camera.rotation = 0.0f;
}

void Exposition::handleCamera(Vector2 mouse) {
  bool isModifierHeld =
      IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER) ||
      IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

  bool middleDown = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);

  bool spaceDown = IsKeyDown(KEY_SPACE);

  float wheel = GetMouseWheelMove();

  if (IsKeyDown(KEY_R) && !isModifierHeld && !m_adjustingBrush) {
    resetCamera();
  }

  if ((middleDown || spaceDown) && m_wasPanning && !m_adjustingBrush) {
    m_camera.offset.x += mouse.x - m_panLastPos.x;

    m_camera.offset.y += mouse.y - m_panLastPos.y;
  }

  m_wasPanning = (middleDown || spaceDown) && !m_adjustingBrush;

  m_panLastPos = mouse;

  if (wheel != 0 && !m_adjustingBrush) {
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsKeyDown(KEY_LEFT_ALT)) {
      m_opacityAccum += wheel * -5.0f;

      float step = 5.0f;

      if (fabsf(m_opacityAccum) >= step) {
        float delta = floorf(fabsf(m_opacityAccum) / step) * step;

        if (m_opacityAccum < 0)
          delta = -delta;

        m_brush.adjustOpacity(delta);
        m_opacityAccum -= delta;
      }
    } else {
      m_zoomAccum += wheel * 0.1f;

      float step = 0.1f;

      if (fabsf(m_zoomAccum) >= step) {
        float delta = floorf(fabsf(m_zoomAccum) / step) * step;

        if (m_zoomAccum < 0)
          delta = -delta;

        Vector2 worldPos = GetScreenToWorld2D(mouse, m_camera);

        m_camera.zoom += delta;

        m_camera.zoom = roundf(m_camera.zoom * 10.0f) / 10.0f;

        if (m_camera.zoom < 0.1f)
          m_camera.zoom = 0.1f;

        Vector2 screenPos = GetWorldToScreen2D(worldPos, m_camera);

        m_camera.offset.x += mouse.x - screenPos.x;

        m_camera.offset.y += mouse.y - screenPos.y;

        m_zoomAccum -= delta;
      }
    }
  }
}

void Exposition::updateBrushAdjustment() {
  bool rightDown = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

  bool rightPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

  bool rightReleased = IsMouseButtonReleased(MOUSE_BUTTON_RIGHT);

  if (rightPressed && !m_adjustingBrush) {
    m_adjustingBrush = true;
    m_brushAdjustmentMousePos = GetMousePosition();

    m_sizeDeltaAccum = 0.0f;
    m_smoothDeltaAccum = 0.0f;

    DisableCursor();
  }

  if (m_adjustingBrush && rightDown) {
    Vector2 delta = GetMouseDelta();

    m_sizeDeltaAccum += -delta.y * m_DRAG_SENSITIVITY;

    m_smoothDeltaAccum += delta.x * m_DRAG_SENSITIVITY;

    int sizeDelta = (int)std::truncf(m_sizeDeltaAccum);

    m_sizeDeltaAccum -= sizeDelta;

    int smoothDelta = (int)std::truncf(m_smoothDeltaAccum);

    m_smoothDeltaAccum -= smoothDelta;

    m_brush.adjustSize((float)sizeDelta);

    m_brush.adjustSmoothing((float)smoothDelta);
  }

  if (m_adjustingBrush && (rightReleased || !rightDown)) {
    m_adjustingBrush = false;

    EnableCursor();

    SetMousePosition((int)m_brushAdjustmentMousePos.x,
                     (int)m_brushAdjustmentMousePos.y);

    m_sizeDeltaAccum = 0.0f;
    m_smoothDeltaAccum = 0.0f;
  }
}

void Exposition::handleMouse(Vector2 mouse) {
  if (m_adjustingBrush)
    return;

  Vector2 canvaPos = GetScreenToWorld2D(mouse, m_camera);

  // Holding G turns the brush into a bucket: the click fills, not draws.
  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && IsKeyDown(KEY_G) &&
      !IsKeyDown(KEY_SPACE) && !m_wasDrawing) {
    Color c = m_brush.getColor();

    float alpha = (c.a / 255.0f) * (m_brush.getOpacity() / 100.0f);

    if (m_canvas[m_selectedIndex].floodFill((int)floorf(canvaPos.x),
                                            (int)floorf(canvaPos.y), c, alpha,
                                            kFillTolerance))
      addRecentColor(c);

    m_fillSwallowsPress = true;
  }

  if (m_fillSwallowsPress) {
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
      return;

    m_fillSwallowsPress = false;
  }

  bool isLeftDown =
      IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !IsKeyDown(KEY_SPACE);

  if (isLeftDown) {
    if (!m_wasDrawing) {
      m_canvas[m_selectedIndex].saveState();

      m_brush.beginStroke(m_canvas[m_selectedIndex], canvaPos);

      m_drawHistory.clear();

      m_drawHistory.push_back(canvaPos);
    } else {
      Vector2 last = m_drawHistory.back();

      float dx = canvaPos.x - last.x;

      float dy = canvaPos.y - last.y;

      float dist = sqrtf(dx * dx + dy * dy);

      if (dist >= 2.0f) {
        m_drawHistory.push_back(canvaPos);

        size_t n = m_drawHistory.size();

        if (n >= 4) {
          m_brush.drawSplineSegment(
              m_canvas[m_selectedIndex], m_drawHistory[n - 4],
              m_drawHistory[n - 3], m_drawHistory[n - 2],
              m_drawHistory[n - 1]);
        } else if (n == 3) {
          m_brush.drawSplineSegment(m_canvas[m_selectedIndex],
                                    m_drawHistory[0], m_drawHistory[0],
                                    m_drawHistory[1], m_drawHistory[2]);
        } else if (n == 2) {
          m_brush.drawSplineSegment(m_canvas[m_selectedIndex],
                                    m_drawHistory[0], m_drawHistory[0],
                                    m_drawHistory[1], m_drawHistory[1]);
        }
      }
    }
  } else if (m_wasDrawing) {
    size_t n = m_drawHistory.size();

    if (n >= 4) {
      m_brush.drawSplineSegment(m_canvas[m_selectedIndex],
                                m_drawHistory[n - 3], m_drawHistory[n - 2],
                                m_drawHistory[n - 1], m_drawHistory[n - 1]);
    } else if (n == 3) {
      m_brush.drawSplineSegment(m_canvas[m_selectedIndex], m_drawHistory[0],
                                m_drawHistory[1], m_drawHistory[2],
                                m_drawHistory[2]);
    } else if (n == 2) {
      m_brush.drawSplineSegment(m_canvas[m_selectedIndex], m_drawHistory[0],
                                m_drawHistory[0], m_drawHistory[1],
                                m_drawHistory[1]);
    }

    m_brush.endStroke();
    m_drawHistory.clear();

    addRecentColor(m_brush.getColor());
  }

  m_wasDrawing = isLeftDown;
}

void Exposition::handleShortcuts() {
  bool isModifierHeld =
      IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER) ||
      IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

  bool isShiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

  if (!isModifierHeld)
    return;

  if (IsKeyPressed(KEY_R)) {
    m_showResPopup = !m_showResPopup;

    m_showResDropdown = false;
  } else if (IsKeyPressed(KEY_S)) {
    if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_canvas.size()) {
      m_canvas[m_selectedIndex].copyToClipboard();
    }
  } else if (IsKeyPressed(KEY_Z)) {
    if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_canvas.size()) {
      if (isShiftHeld)
        m_canvas[m_selectedIndex].redo();
      else
        m_canvas[m_selectedIndex].undo();
    }
  } else if (IsKeyPressed(KEY_Y)) {
    if (m_selectedIndex >= 0 && m_selectedIndex < (int)m_canvas.size()) {
      m_canvas[m_selectedIndex].redo();
    }
  }
}

void Exposition::handleInput() {
  if (m_selectedIndex >= (int)m_canvas.size())
    return;

  Vector2 mouse = GetMousePosition();

  updateBrushAdjustment();

  updateHexInput();

  // While typing the hex, keys like R/S/Z must not trigger shortcuts.
  if (!m_adjustingBrush && !m_hexEditing) {
    mouse = GetMousePosition();

    handleCamera(mouse);
    handleShortcuts();
  }

  bool pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

  bool down = IsMouseButtonDown(MOUSE_LEFT_BUTTON);

  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
    m_uiConsumedPress = false;
    m_pickerActivePart = 0;
  }

  if (m_adjustingBrush)
    return;

  bool isOverUI = handleUI(mouse, pressed, down);

  m_lastOverUI = isOverUI;

  if (pressed && !isOverUI && m_advancedPickerOpen) {
    endHexEdit(true);

    m_advancedPickerOpen = false;

    m_pickerActivePart = 0;
  }

  if (pressed && isOverUI)
    m_uiConsumedPress = true;

  if (!m_uiConsumedPress) {
    handleMouse(mouse);
  } else if (m_wasDrawing) {
    handleMouse(mouse);
  }
}

void Exposition::renderCanvas() {
  Camera2D cam = m_camera;

  cam.zoom *= m_dpiScale;

  cam.offset.x *= m_dpiScale;

  cam.offset.y *= m_dpiScale;

  BeginMode2D(cam);

  for (auto &canva : m_canvas)
    canva.draw();

  EndMode2D();
}
