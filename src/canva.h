#pragma once

#include "raylib.h"
#include <vector>

// Painting canvas: keeps the pixels on the CPU, keeps the GPU texture in sync
// and manages the undo/redo history.
class Canva {
  int m_width, m_height;

  bool m_dirty;
  std::vector<Color> m_pixels;
  Texture2D m_texture;

  std::vector<std::vector<Color>> m_undoStack;
  std::vector<std::vector<Color>> m_redoStack;
  const size_t m_maxHistory = 20;

public:
  Canva(int x, int y);

  Canva(Canva &&other) noexcept;
  Canva &operator=(Canva &&other) noexcept;

  ~Canva();

  int getWidth() const { return m_width; }

  int getHeight() const { return m_height; }

  void saveState();
  void undo();
  void redo();

  void setPixelStroke(int x, int y, Color color, float alpha);

  // Fill that respects the softness of strokes.
  //
  // 1. The "background" region is the (4-connected) flood fill of the
  //    clicked color, with a small tolerance.
  // 2. A stroke fade pixel is P = background + a * (S - background), where S
  //    is the stroke color and `a` the coverage. Starting from the region
  //    border, the fade is walked outward (always rising in distance from
  //    the background) and, for each pixel, S is estimated from the
  //    strongest stretch that continues along the same background -> S line.
  // 3. Knowing `a`, the pixel becomes lerp(new color, S, a): the stroke fade
  //    blends with the new color instead of leaving a halo of the old one.
  bool floodFill(int sx, int sy, Color fill, float alpha, int tolerance);

  void setAllPixels(Color color);

  void draw();

  void copyToClipboard();
};
