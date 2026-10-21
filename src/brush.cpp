#include "brush.h"

#include <algorithm>
#include <cmath>

namespace {

float smoothstep01(float t) {
  t = std::max(0.0f, std::min(1.0f, t));

  return t * t * (3.0f - 2.0f * t);
}

} // namespace

void Brush::clearStrokeBuffers() {
  if (m_dirtyMaxX >= m_dirtyMinX && m_dirtyMaxY >= m_dirtyMinY) {
    size_t n = (size_t)(m_dirtyMaxX - m_dirtyMinX + 1);

    for (int y = m_dirtyMinY; y <= m_dirtyMaxY; y++) {
      size_t off = (size_t)y * m_maskWidth + m_dirtyMinX;

      std::fill_n(m_recent.begin() + off, n, 0.0f);
      std::fill_n(m_old.begin() + off, n, 0.0f);
      std::fill_n(m_stamp.begin() + off, n, 0.0f);
    }
  }

  m_dirtyMinX = m_maskWidth;
  m_dirtyMinY = m_maskHeight;
  m_dirtyMaxX = -1;
  m_dirtyMaxY = -1;
}

void Brush::paintDot(Canva &canvas, float cx, float cy, float stamp) {
  float radius = m_size / 2.0f;

  if (radius < 0.5f)
    radius = 0.5f;

  float smoothness = m_smoothing / 100.0f;
  float core_r = radius * (1.0f - smoothness);

  float globalAlpha = (m_color.a / 255.0f) * (m_opacity / 100.0f);

  // Consecutive dabs of the same pass are `spacing` apart along the stroke;
  // if a pixel is only touched again after a much larger gap, the brush
  // left and came back, i.e. it is another pass. The transition between
  // "same pass" and "other pass" is gradual (between gapMin and gapMax) so
  // no visible seam appears where the stroke makes a tight turn.
  float spacing = std::max(0.5f, m_size * 0.05f);
  float gapMin = spacing * 3.0f;
  float gapMax = std::max(m_size * 2.0f, gapMin + spacing);

  int startX = std::max(0, (int)floorf(cx - radius));
  int endX = std::min(m_maskWidth - 1, (int)ceilf(cx + radius));
  int startY = std::max(0, (int)floorf(cy - radius));
  int endY = std::min(m_maskHeight - 1, (int)ceilf(cy + radius));

  if (startX > endX || startY > endY)
    return;

  m_dirtyMinX = std::min(m_dirtyMinX, startX);
  m_dirtyMinY = std::min(m_dirtyMinY, startY);
  m_dirtyMaxX = std::max(m_dirtyMaxX, endX);
  m_dirtyMaxY = std::max(m_dirtyMaxY, endY);

  for (int y = startY; y <= endY; y++) {
    for (int x = startX; x <= endX; x++) {
      float dx = ((float)x + 0.5f) - cx;
      float dy = ((float)y + 0.5f) - cy;

      float dist = sqrtf(dx * dx + dy * dy);

      if (dist >= radius)
        continue;

      float coverage = 1.0f;

      if (smoothness > 0.0f && dist > core_r) {
        float factor = 1.0f - ((dist - core_r) / (radius - core_r));

        coverage = factor * factor;
      }

      if (coverage <= 0.0f)
        continue;

      size_t idx = (size_t)y * m_maskWidth + x;

      float &recent = m_recent[idx];
      float &old = m_old[idx];
      float &last = m_stamp[idx];

      // How much of what was already in this pixel belongs to a previous
      // pass: 0 = same pass, 1 = completely new pass. The "previous" part
      // is merged into the union and only the rest still counts as the
      // current pass.
      if (recent > 0.0f) {
        float flush =
            smoothstep01((stamp - last - gapMin) / (gapMax - gapMin));

        if (flush > 0.0f) {
          // Redistributes between "previous" and "current" without changing
          // the pixel's total coverage:
          // (1-old')(1-recent') == (1-old)(1-recent).
          float keep = recent * (1.0f - flush);
          float denom = 1.0f - keep;

          // With recent == 1 (brush core) and a tiny flush, `keep` rounds
          // to 1 and the math becomes 0/0 = NaN, which reached the canvas
          // as a black pixel. In that case the coverage is already ~1 and
          // `old` can stay as it is.
          if (denom > 1e-4f) {
            old = 1.0f - (1.0f - old) * (1.0f - recent) / denom;
            old = std::max(0.0f, std::min(1.0f, old));
          }

          recent = keep;
        }
      }

      last = stamp;

      // Within the same pass the maximum applies (clean edge, independent
      // of spacing); between passes the probabilistic union applies, which
      // is smooth and leaves no crease where the edges meet.
      if (coverage > recent) {
        recent = coverage;

        float total = 1.0f - (1.0f - old) * (1.0f - recent);

        total = std::max(0.0f, std::min(1.0f, total));

        canvas.setPixelStroke(x, y, m_color, total * globalAlpha);
      }
    }
  }
}

Vector2 Brush::catmullRom(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3,
                        float t) {
  float t2 = t * t;
  float t3 = t2 * t;

  return {0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                  (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                  (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),

          0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                  (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                  (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3)};
}

void Brush::walkAndPaint(Canva &canvas, Vector2 from, Vector2 to) {
  float dx = to.x - from.x;
  float dy = to.y - from.y;

  float dist = sqrtf(dx * dx + dy * dy);

  if (dist <= 0.0f)
    return;

  float spacing = std::max(0.5f, m_size * 0.05f);

  float current = m_remainderDist;

  while (current <= dist) {
    float factor = current / dist;

    paintDot(canvas, from.x + dx * factor, from.y + dy * factor,
             m_arc + current);

    current += spacing;
  }

  m_remainderDist = current - dist;
  m_arc += dist;
}

void Brush::beginStroke(Canva &canvas, Vector2 pos) {
  int w = canvas.getWidth();
  int h = canvas.getHeight();

  // The buffers are allocated once per canvas size and afterwards only the
  // dirty region is cleared, instead of reallocating the whole canvas on
  // every stroke.
  if (w != m_maskWidth || h != m_maskHeight || m_recent.empty()) {
    m_maskWidth = w;
    m_maskHeight = h;

    m_recent.assign((size_t)w * h, 0.0f);
    m_old.assign((size_t)w * h, 0.0f);
    m_stamp.assign((size_t)w * h, 0.0f);

    m_dirtyMinX = w;
    m_dirtyMinY = h;
    m_dirtyMaxX = -1;
    m_dirtyMaxY = -1;
  } else {
    clearStrokeBuffers();
  }

  float spacing = std::max(0.5f, m_size * 0.05f);

  m_remainderDist = spacing;
  m_arc = 0.0f;

  paintDot(canvas, pos.x, pos.y, 0.0f);
}

void Brush::drawSplineSegment(Canva &canvas, Vector2 p0, Vector2 p1,
                              Vector2 p2, Vector2 p3) {
  float dx = p2.x - p1.x;
  float dy = p2.y - p1.y;

  float distSegment = sqrtf(dx * dx + dy * dy);

  int steps = std::max(5, (int)(distSegment / 2.0f));

  if (steps > 50)
    steps = 50;

  Vector2 prevPos = p1;

  for (int i = 1; i <= steps; i++) {
    float t = (float)i / (float)steps;

    Vector2 nextPos = catmullRom(p0, p1, p2, p3, t);

    walkAndPaint(canvas, prevPos, nextPos);

    prevPos = nextPos;
  }
}

void Brush::adjustSize(float delta) {
  m_size += delta;

  if (m_size < m_minSize)
    m_size = m_minSize;

  if (m_size > m_maxSize)
    m_size = m_maxSize;
}

void Brush::adjustSmoothing(float delta) {
  m_smoothing += delta;

  if (m_smoothing < 0.0f)
    m_smoothing = 0.0f;

  if (m_smoothing > 100.0f)
    m_smoothing = 100.0f;
}

void Brush::adjustOpacity(float delta) {
  m_opacity += delta;

  if (m_opacity < 0.0f)
    m_opacity = 0.0f;

  if (m_opacity > 100.0f)
    m_opacity = 100.0f;
}
