#pragma once

#include "brush.h"
#include "canva.h"
#include "raylib.h"
#include <string>
#include <vector>

// Size (in pixels) of a painting canvas.
struct Resolution {
  int w;
  int h;
};

// Application: input (mouse/keyboard), camera, interface and color picker.
//
// The implementation is split into three files:
//   exposition.cpp         - lifecycle, camera, brush/fill and shortcuts
//   exposition_picker.cpp  - color picker (HSV + hex): logic and drawing
//   exposition_ui.cpp      - interface hit-testing and UI rendering
class Exposition {
  int m_selectedIndex{0};

  Brush m_brush;

  Camera2D m_camera = {{0, 0}, {0, 0}, 0.0f, 1.0f};

  Font m_interFont;

  std::vector<Canva> m_canvas;
  std::vector<Vector2> m_drawHistory;

  bool m_wasDrawing{false};
  bool m_wasPanning{false};

  bool m_advancedPickerOpen{false};
  std::vector<Color> m_recentColors;

  bool m_uiConsumedPress{false};

  // fill (hold G): the click that filled must not turn into a stroke
  bool m_fillSwallowsPress{false};
  bool m_lastOverUI{false};
  static constexpr int kFillTolerance = 12;
  int m_pickerActivePart{0};

  // HSV is the source of truth while the picker is open; this way the cursor
  // doesn't "jump" due to 8-bit rounding and the hue isn't lost when the
  // color passes through gray or black.
  float m_pickerHue{0.0f};
  float m_pickerSat{0.0f};
  float m_pickerVal{0.0f};
  bool m_pickerHueInitialized{false};
  Color m_pickerOriginal{0, 0, 0, 255};

  bool m_hexEditing{false};
  bool m_hexFresh{false};
  std::string m_hexBuffer;
  float m_hexCaretTime{0.0f};

  std::vector<Resolution> m_resolutions = {
      {1080, 1080}, {2048, 2048}, {1920, 1080}, {3840, 2160}, {2480, 3508},
      {2048, 1536}, {1080, 1920}, {3000, 2000}, {1200, 1800}, {2560, 1080}};

  bool m_showResPopup{false};
  bool m_showResDropdown{false};
  int m_selectedRes{2};

  const float m_DRAG_SENSITIVITY = 0.2f;

  Vector2 m_panLastPos{0, 0};

  float m_sizeDeltaAccum{0};
  float m_smoothDeltaAccum{0};
  float m_zoomAccum{0};
  float m_opacityAccum{0};

  bool m_adjustingBrush{false};
  Vector2 m_brushAdjustmentMousePos{0, 0};

  float m_dpiScale{1.0f};

  struct PickerLayout {
    Rectangle panel, sv, hue, oldSwatch, newSwatch, hex;
  };

  // exposition.cpp
  void addRecentColor(Color c);
  void resetCamera();
  void handleCamera(Vector2 mouse);
  void updateBrushAdjustment();
  void handleMouse(Vector2 mouse);
  void handleShortcuts();

  // exposition_picker.cpp
  static PickerLayout pickerLayout();
  void syncPickerFromColor(Color c);
  void applyPickerColor();
  void updatePickerColor(Vector2 pos, int activePart);
  void beginHexEdit();
  void endHexEdit(bool commit);
  void updateHexInput();
  void drawColorPicker(Vector2 plusCenter);

  // exposition_ui.cpp
  bool handleUI(Vector2 pos, bool pressed, bool down);

public:
  Exposition();
  ~Exposition();

  void addCanva(int w, int h);

  void handleInput();

  void renderCanvas();
  void renderUI();
};
