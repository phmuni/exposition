#include "canva.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

Canva::Canva(int x, int y)
    : m_width(x), m_height(y), m_dirty(true) {
  m_pixels.resize(x * y, WHITE);

  Image img = {.data = m_pixels.data(),
               .width = x,
               .height = y,
               .mipmaps = 1,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};

  m_texture = LoadTextureFromImage(img);
  SetTextureFilter(m_texture, TEXTURE_FILTER_POINT);
}

Canva::Canva(Canva &&other) noexcept
    : m_width(other.m_width), m_height(other.m_height), m_dirty(other.m_dirty),
      m_pixels(std::move(other.m_pixels)), m_texture(other.m_texture),
      m_undoStack(std::move(other.m_undoStack)),
      m_redoStack(std::move(other.m_redoStack)) {
  other.m_texture.id = 0;
}

Canva &Canva::operator=(Canva &&other) noexcept {
  if (this != &other) {
    if (m_texture.id != 0)
      UnloadTexture(m_texture);

    m_width = other.m_width;
    m_height = other.m_height;
    m_pixels = std::move(other.m_pixels);
    m_texture = other.m_texture;
    m_dirty = other.m_dirty;
    m_undoStack = std::move(other.m_undoStack);
    m_redoStack = std::move(other.m_redoStack);

    other.m_texture.id = 0;
  }

  return *this;
}

Canva::~Canva() {
  if (m_texture.id != 0)
    UnloadTexture(m_texture);
}

void Canva::saveState() {
  m_undoStack.push_back(m_pixels);

  if (m_undoStack.size() > m_maxHistory)
    m_undoStack.erase(m_undoStack.begin());

  m_redoStack.clear();
}

void Canva::undo() {
  if (m_undoStack.empty())
    return;

  m_redoStack.push_back(m_pixels);
  m_pixels = m_undoStack.back();
  m_undoStack.pop_back();
  m_dirty = true;
}

void Canva::redo() {
  if (m_redoStack.empty())
    return;

  m_undoStack.push_back(m_pixels);
  m_pixels = m_redoStack.back();
  m_redoStack.pop_back();
  m_dirty = true;
}

void Canva::setPixelStroke(int x, int y, Color color, float alpha) {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height)
    return;

  Color dest = m_pixels[y * m_width + x];

  if (!m_undoStack.empty())
    dest = m_undoStack.back()[y * m_width + x];

  // NaN or values outside [0,1] must never become a color: `!(alpha >= 0)`
  // also catches NaN.
  if (!(alpha >= 0.0f))
    alpha = 0.0f;
  else if (alpha > 1.0f)
    alpha = 1.0f;

  float invAlpha = 1.0f - alpha;

  unsigned char r =
      (unsigned char)((color.r * alpha) + (dest.r * invAlpha) + 0.5f);
  unsigned char g =
      (unsigned char)((color.g * alpha) + (dest.g * invAlpha) + 0.5f);
  unsigned char b =
      (unsigned char)((color.b * alpha) + (dest.b * invAlpha) + 0.5f);

  m_pixels[y * m_width + x] = {r, g, b, 255};
  m_dirty = true;
}

bool Canva::floodFill(int sx, int sy, Color fill, float alpha,
                      int tolerance) {
  if (sx < 0 || sx >= m_width || sy < 0 || sy >= m_height || alpha <= 0.0f)
    return false;

  const int W = m_width;
  const int H = m_height;
  const size_t total = (size_t)W * H;
  const Color seed = m_pixels[(size_t)sy * W + sx];

  auto cheb = [](Color a, Color b) {
    return std::max({std::abs((int)a.r - (int)b.r),
                     std::abs((int)a.g - (int)b.g),
                     std::abs((int)a.b - (int)b.b)});
  };

  if (cheb(seed, fill) <= 1)
    return false;

  auto distSeed = [&](Color c) {
    float dr = (float)c.r - seed.r;
    float dg = (float)c.g - seed.g;
    float db = (float)c.b - seed.b;

    return sqrtf(dr * dr + dg * dg + db * db);
  };

  // C is a plausible "stroke color" for P if P lies on the segment
  // background -> C (with a small slack for 8-bit rounding).
  auto onSegment = [&](Color p, Color c) {
    float vr = (float)c.r - seed.r, vg = (float)c.g - seed.g,
          vb = (float)c.b - seed.b;
    float wr = (float)p.r - seed.r, wg = (float)p.g - seed.g,
          wb = (float)p.b - seed.b;

    float vv = vr * vr + vg * vg + vb * vb;

    if (vv < 1.0f)
      return false;

    float t = (wr * vr + wg * vg + wb * vb) / vv;

    if (t < -0.05f || t > 1.05f)
      return false;

    float er = wr - t * vr, eg = wg - t * vg, eb = wb - t * vb;

    return er * er + eg * eg + eb * eb <= 7.0f * 7.0f;
  };

  std::vector<uint8_t> state(total, 0); // 1 = background, 2 = fade
  std::vector<int> region;

  int start = sy * W + sx;

  region.push_back(start);
  state[start] = 1;

  static const int dx4[4] = {1, -1, 0, 0};
  static const int dy4[4] = {0, 0, 1, -1};
  static const int dx8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
  static const int dy8[8] = {0, 0, 1, -1, 1, -1, 1, -1};

  for (size_t head = 0; head < region.size(); head++) {
    int i = region[head];
    int x = i % W;
    int y = i / W;

    for (int k = 0; k < 4; k++) {
      int nx = x + dx4[k];
      int ny = y + dy4[k];

      if (nx < 0 || nx >= W || ny < 0 || ny >= H)
        continue;

      size_t n = (size_t)ny * W + nx;

      if (state[n] == 0 && cheb(m_pixels[n], seed) <= tolerance) {
        state[n] = 1;
        region.push_back((int)n);
      }
    }
  }

  // fade: breadth-first search starting from the whole region border
  constexpr int maxDepth = 56;
  constexpr float slack = 3.0f;

  std::vector<int> fIdx;
  std::vector<int> fParent;
  std::vector<uint8_t> fDepth;
  std::vector<Color> fS;
  std::vector<float> fPeak;   // max distance to the background on the path
  std::vector<uint8_t> fFlat; // consecutive steps without really rising
  std::vector<float> fRef;    // distance where the "flat" sequence began

  for (int i : region) {
    int x = i % W;
    int y = i / W;

    for (int k = 0; k < 8; k++) {
      int nx = x + dx8[k];
      int ny = y + dy8[k];

      if (nx < 0 || nx >= W || ny < 0 || ny >= H)
        continue;

      size_t n = (size_t)ny * W + nx;

      // Also includes pixels almost the background color that only touch
      // the region diagonally (step of a slanted edge); without this they
      // would be left as specks of the old color stuck to the stroke.
      if (state[n] == 0) {
        state[n] = 2;

        fIdx.push_back((int)n);
        fParent.push_back(-1);
        fDepth.push_back(1);
        fS.push_back(m_pixels[n]);
        fPeak.push_back(distSeed(m_pixels[n]));
        fFlat.push_back(0);
        fRef.push_back(fPeak.back());
      }
    }
  }

  for (size_t head = 0; head < fIdx.size(); head++) {
    if (fDepth[head] >= maxDepth)
      continue;

    int i = fIdx[head];
    int x = i % W;
    int y = i / W;
    float peak = fPeak[head];
    int flat = fFlat[head];
    float ref = fRef[head];
    float dq = distSeed(m_pixels[i]);

    for (int k = 0; k < 8; k++) {
      int nx = x + dx8[k];
      int ny = y + dy8[k];

      if (nx < 0 || nx >= W || ny < 0 || ny >= H)
        continue;

      size_t n = (size_t)ny * W + nx;

      if (state[n] != 0 || cheb(m_pixels[n], seed) <= tolerance)
        continue;

      // Comparing with the path's peak (and not just the previous pixel)
      // prevents crossing the stroke and slowly descending the fade on the
      // other side, which belongs to another region.
      float dn = distSeed(m_pixels[n]);

      if (dn < peak - slack)
        continue;

      // A single-color stroke fade always keeps the same direction away
      // from the background. If the direction changes, the pixel belongs
      // to another stroke (or a mix of two); crossing into it would make
      // the fill invade the other side, e.g. after crossing a thin line.
      if (dq > 20.0f && dn > 20.0f) {
        Color pq = m_pixels[i];
        Color pn = m_pixels[n];

        float dot = ((float)pq.r - seed.r) * ((float)pn.r - seed.r) +
                    ((float)pq.g - seed.g) * ((float)pn.g - seed.g) +
                    ((float)pq.b - seed.b) * ((float)pn.b - seed.b);

        if (dot < 0.96f * dq * dn)
          continue;
      }

      // In the stroke core the distance stops rising; a few such steps
      // are enough to know the peak was reached, and continuing would only
      // make the fill walk along the stroke into sides that aren't its own.
      // "Really rising" means gaining at least 4 levels since the start of
      // the flat sequence; smaller oscillations (rounding, the background
      // showing under the edge of another stroke) don't restart the count.
      bool rose = dn >= ref + 4.0f;

      int nextFlat = rose ? 0 : flat + 1;

      if (nextFlat > 3)
        continue;

      state[n] = 2;

      fIdx.push_back((int)n);
      fParent.push_back((int)head);
      fDepth.push_back((uint8_t)(fDepth[head] + 1));
      fS.push_back(m_pixels[n]);
      fPeak.push_back(std::max(peak, dn));
      fFlat.push_back((uint8_t)nextFlat);
      fRef.push_back(rose ? dn : ref);
    }
  }

  // from outermost to innermost: each pixel inherits the strongest stroke
  // color that continues along the same background -> stroke line
  for (size_t k = fIdx.size(); k-- > 0;) {
    int p = fParent[k];

    if (p < 0)
      continue;

    Color cand = fS[k];

    if (distSeed(cand) > distSeed(fS[p]) + 0.5f &&
        onSegment(m_pixels[fIdx[p]], cand))
      fS[p] = cand;
  }

  // Nodes born adjacent to the region (roots) are not children of one
  // another; let the stroke color also flow between neighbors, so a light
  // border pixel finds the stroke next to it.
  {
    std::vector<int> nodeOf(total, -1);

    for (size_t k = 0; k < fIdx.size(); k++)
      nodeOf[fIdx[k]] = (int)k;

    for (int pass = 0; pass < 8; pass++) {
      bool changed = false;

      for (size_t k = 0; k < fIdx.size(); k++) {
        int x = fIdx[k] % W;
        int y = fIdx[k] / W;
        Color pk = m_pixels[fIdx[k]];

        for (int d = 0; d < 8; d++) {
          int nx = x + dx8[d];
          int ny = y + dy8[d];

          if (nx < 0 || nx >= W || ny < 0 || ny >= H)
            continue;

          int j = nodeOf[(size_t)ny * W + nx];

          if (j < 0)
            continue;

          Color cand = fS[j];

          if (distSeed(cand) > distSeed(fS[k]) + 0.5f &&
              onSegment(pk, cand)) {
            fS[k] = cand;
            changed = true;
          }
        }
      }

      if (!changed)
        break;
    }
  }

  saveState();

  auto blend = [&](size_t i, float r, float g, float b) {
    Color old = m_pixels[i];

    auto mix = [&](unsigned char o, float v) {
      float out =
          (float)o + (std::max(0.0f, std::min(255.0f, v)) - o) * alpha;

      return (unsigned char)(out + 0.5f);
    };

    m_pixels[i] = {mix(old.r, r), mix(old.g, g), mix(old.b, b), 255};
  };

  for (int i : region)
    blend((size_t)i, fill.r, fill.g, fill.b);

  for (size_t k = 0; k < fIdx.size(); k++) {
    Color s = fS[k];
    Color p = m_pixels[fIdx[k]];

    float vr = (float)s.r - seed.r, vg = (float)s.g - seed.g,
          vb = (float)s.b - seed.b;
    float vv = vr * vr + vg * vg + vb * vb;

    if (vv < 1.0f) {
      // no stroke ahead: it is background left isolated on the diagonal
      blend((size_t)fIdx[k], fill.r, fill.g, fill.b);

      continue;
    }

    float a = (((float)p.r - seed.r) * vr + ((float)p.g - seed.g) * vg +
               ((float)p.b - seed.b) * vb) /
              vv;

    a = std::max(0.0f, std::min(1.0f, a));

    // Pixel already in the stroke core (got here after passing the peak):
    // treat it as pure stroke. Without this, the almost-black core on the
    // side outside the region picked up a tint of the new color through
    // rounding.
    if (fFlat[k] > 0 && a > 0.9f)
      a = 1.0f;

    float keep = 1.0f - a;

    blend((size_t)fIdx[k], p.r + keep * ((float)fill.r - seed.r),
          p.g + keep * ((float)fill.g - seed.g),
          p.b + keep * ((float)fill.b - seed.b));
  }

  m_dirty = true;

  return true;
}

void Canva::setAllPixels(Color color) {
  for (Color &c : m_pixels)
    c = color;

  m_dirty = true;
}

void Canva::draw() {
  if (m_dirty) {
    UpdateTexture(m_texture, m_pixels.data());
    m_dirty = false;
  }

  DrawTexture(m_texture, 0, 0, WHITE);
}

void Canva::copyToClipboard() {
  static const char *tempPath = "/tmp/raylib_canvas_temp.png";

  Image img = {.data = m_pixels.data(),
               .width = m_width,
               .height = m_height,
               .mipmaps = 1,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};

  ExportImage(img, tempPath);

#ifdef __APPLE__
  char command[512];

  snprintf(command, sizeof(command),
           "osascript -e 'set the clipboard to (read (POSIX file \"%s\") as "
           "TIFF picture)'",
           tempPath);

  system(command);

  if (remove(tempPath) == 0)
    TraceLog(LOG_INFO, "SUCCESS: Image Copied to Clipboard!");
  else
    TraceLog(LOG_WARNING,
             "Warning: Image Copied but temporary file was not deleted!");
#else
  TraceLog(LOG_WARNING, "Clipboard only supported on MacOS.");
#endif
}
