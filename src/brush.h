#pragma once

#include "canva.h"
#include "raylib.h"
#include <vector>

// Brush: turns the mouse path into stamps ("dabs") with soft coverage and
// composites them onto the Canva.
class Brush {
  const float m_minSize = 1.0f;
  const float m_maxSize = 100.0f;

  float m_size = 3.0f;
  float m_smoothing = 50.0f;
  float m_opacity = 100.0f;
  Color m_color = BLACK;

  float m_remainderDist = 0.0f;

  // Distance traveled along the stroke up to the start of the current
  // segment. Acts as a "stamp" to tell whether two dabs belong to the same
  // pass (consecutive dabs, spaced by `spacing`) or to different passes
  // that cross or come back over the same place.
  float m_arc = 0.0f;

  int m_maskWidth = 0;
  int m_maskHeight = 0;

  // Coverage (0..1) of the current pass, of the union of older passes, and
  // the stamp of the last dab that touched each pixel. The global opacity
  // is applied only at composite time, so it remains a ceiling for the
  // whole stroke, as before.
  std::vector<float> m_recent;
  std::vector<float> m_old;
  std::vector<float> m_stamp;

  int m_dirtyMinX = 0, m_dirtyMinY = 0, m_dirtyMaxX = -1, m_dirtyMaxY = -1;

  void clearStrokeBuffers();
  void paintDot(Canva &canvas, float cx, float cy, float stamp);
  Vector2 catmullRom(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float t);
  void walkAndPaint(Canva &canvas, Vector2 from, Vector2 to);

public:
  void beginStroke(Canva &canvas, Vector2 pos);

  void endStroke() { clearStrokeBuffers(); }

  void drawSplineSegment(Canva &canvas, Vector2 p0, Vector2 p1, Vector2 p2,
                         Vector2 p3);

  void adjustSize(float delta);
  void adjustSmoothing(float delta);
  void adjustOpacity(float delta);

  void setColor(Color c) { m_color = c; }

  float getSize() const { return m_size; }

  float getSmoothing() const { return m_smoothing; }

  float getOpacity() const { return m_opacity; }

  Color getColor() const { return m_color; }
};
