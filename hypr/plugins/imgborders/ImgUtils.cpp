#include "ImgUtils.hpp"
#include <GLES3/gl32.h>
#include <algorithm>
#include <cairo/cairo.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <pango/pangocairo.h>
#include <render/Renderer.hpp>
#include <sstream>
#include <unordered_map>

namespace {

static inline float hash2D(int x, int y) {
  int n = x * 374761393 + y * 668265263;
  n = (n ^ (n >> 13)) * 1274126177;
  return ((n ^ (n >> 16)) & 0x7fffffff) / float(0x7fffffff);
}

static float smoothNoise(float x, float y) {
  int ix = (int)std::floor(x);
  int iy = (int)std::floor(y);
  float fx = x - ix;
  float fy = y - iy;
  float u = fx * fx * (3.0f - 2.0f * fx);
  float v = fy * fy * (3.0f - 2.0f * fy);
  float a = hash2D(ix, iy);
  float b = hash2D(ix + 1, iy);
  float c = hash2D(ix, iy + 1);
  float d = hash2D(ix + 1, iy + 1);
  return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}

static float fbmNoise(float x, float y) {
  float val = 0.0f;
  float amp = 0.52f;
  float freq = 1.0f;
  for (int i = 0; i < 4; ++i) {
    val += amp * smoothNoise(x * freq, y * freq);
    freq *= 2.07f;
    amp *= 0.48f;
  }
  return std::clamp(val, 0.0f, 1.0f);
}

static std::vector<std::string> utf8CharsUpper(const std::string &word) {
  std::vector<std::string> out;
  gchar *up = g_utf8_strup(word.c_str(), -1);
  const char *p = up;
  while (*p) {
    const char *next = g_utf8_next_char(p);
    out.emplace_back(p, next - p);
    p = next;
  }
  g_free(up);
  return out;
}

static bool isSmallConnectorWord(const std::string &w) {
  gchar *low = g_utf8_strdown(w.c_str(), -1);
  std::string s(low);
  g_free(low);
  return (s == "da" || s == "de" || s == "do" || s == "das" || s == "dos" ||
          s == "e" || s == "em" || s == "na" || s == "no" || s == "of" ||
          s == "the" || s == "and" || s == "for" || s == "in" || s == "on" ||
          s == "to");
}

static SP<ITexture> createGLTextureFromBGRA(int width, int height,
                                            const uint8_t *data) {
  SP<ITexture> tex = g_pHyprRenderer->createTexture();
  tex->allocate({(double)width, (double)height});

  glBindTexture(GL_TEXTURE_2D, tex->m_texID);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, data);
  return tex;
}

static float computePieceResist(int x, int y, int w, int h,
                                ImgUtils::eBorderPieceType pieceType) {
  using namespace ImgUtils;
  switch (pieceType) {
  case PIECE_TOP:
  case PIECE_BOTTOM: {
    float seedY = (pieceType == PIECE_TOP) ? 3.7f : 41.2f;
    float u = float(x) / std::max(1, w);
    float distCenter = std::abs(u - 0.5f) * 2.0f; // 0 at center ornament, 1 near corners
    float n = fbmNoise(x * 0.028f + 5.1f, y * 0.045f + seedY);
    return n * 0.75f + distCenter * 0.25f;
  }
  case PIECE_LEFT:
  case PIECE_RIGHT: {
    float seedX = (pieceType == PIECE_LEFT) ? 11.4f : 67.8f;
    float v = float(y) / std::max(1, h);
    float distCenter = std::abs(v - 0.5f) * 2.0f;
    float n = fbmNoise(x * 0.045f + seedX, y * 0.028f + 19.3f);
    return n * 0.75f + distCenter * 0.25f;
  }
  case PIECE_TL:
  case PIECE_BL: {
    float seedY = (pieceType == PIECE_BL) ? 88.0f : 12.0f;
    float bias = (pieceType == PIECE_BL) ? 0.30f : 0.25f;
    float n = fbmNoise(x * 0.05f + 2.3f, y * 0.05f + seedY);
    return n * 0.76f + bias;
  }
  case PIECE_TR:
  case PIECE_BR: {
    float seedY = (pieceType == PIECE_BR) ? 94.0f : 24.0f;
    float n = fbmNoise(x * 0.05f + 31.7f, y * 0.05f + seedY);
    return n * 0.76f + 0.18f;
  }
  case PIECE_TITLE:
  default: {
    float n = fbmNoise(x * 0.044f + 17.3f, y * 0.054f + 9.1f);
    float sweep = (float(x) / std::max(1, w)) * 0.34f;
    return n * 0.70f + sweep;
  }
  }
}

static std::string toLowerAscii(std::string s) {
  for (char &c : s)
    c = (char)std::tolower((unsigned char)c);
  return s;
}

static std::string trimStr(const std::string &s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos)
    return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

static bool isPortuguesePreferred() {
  const char *home = std::getenv("HOME");
  if (home) {
    std::ifstream f(std::string(home) + "/.config/hypr/scripts/.navi_lang");
    if (f) {
      std::string lang;
      std::getline(f, lang);
      lang = trimStr(lang);
      if (lang == "pt" || lang == "pt-br" || lang == "pt_BR")
        return true;
      if (lang == "en")
        return false;
    }
  }
  const char *envLang = std::getenv("LANG");
  if (envLang && std::string(envLang).find("pt") != std::string::npos)
    return true;
  return false;
}

static ImgUtils::SDissolveTexture
sliceTemplateFromSurface(cairo_surface_t *srcSurf, CBox box,
                         ImgUtils::eBorderPieceType pieceType) {
  ImgUtils::SDissolveTexture dt;
  if (!srcSurf)
    return dt;

  int srcW = cairo_image_surface_get_width(srcSurf);
  int srcH = cairo_image_surface_get_height(srcSurf);
  int srcStride = cairo_image_surface_get_stride(srcSurf);
  const uint8_t *srcData = cairo_image_surface_get_data(srcSurf);

  int bx = std::clamp((int)std::round(box.x), 0, std::max(0, srcW - 1));
  int by = std::clamp((int)std::round(box.y), 0, std::max(0, srcH - 1));
  int bw = std::clamp((int)std::round(box.width), 1, srcW - bx);
  int bh = std::clamp((int)std::round(box.height), 1, srcH - by);

  dt.width = bw;
  dt.height = bh;
  dt.workBuffer.assign(bw * bh * 4, 0);

  for (int y = 0; y < bh; ++y) {
    const uint8_t *srcRow = srcData + (by + y) * srcStride + bx * 4;
    uint8_t *dstRow = dt.workBuffer.data() + y * bw * 4;
    std::memcpy(dstRow, srcRow, bw * 4);
    for (int x = 0; x < bw; ++x) {
      uint8_t a = dstRow[x * 4 + 3];
      if (a > 0) {
        uint32_t off = (uint32_t)((y * bw + x) * 4);
        float resist = computePieceResist(x, y, bw, bh, pieceType);
        dt.activePixels.push_back(
            {off, dstRow[x * 4 + 0], dstRow[x * 4 + 1], dstRow[x * 4 + 2], a,
             resist});
      }
    }
  }
  dt.currentStage = 0;
  return dt;
}

static ImgUtils::SDissolveTexture
instantiateSliceGL(const ImgUtils::SDissolveTexture &tmpl) {
  ImgUtils::SDissolveTexture out = tmpl;
  if (out.width > 0 && out.height > 0 && !out.workBuffer.empty()) {
    out.tex = createGLTextureFromBGRA(out.width, out.height,
                                      out.workBuffer.data());
    out.currentStage = 0;
  }
  return out;
}

} // namespace

ImgUtils::SBorderSliceSet
ImgUtils::getCachedBorderSlices(const std::string &fullPath,
                                const int sizes[4]) {
  static std::string cachedPath;
  static int cachedSizes[4] = {-1, -1, -1, -1};
  static SBorderSliceSet cachedTemplates;

  if (cachedPath != fullPath || cachedSizes[0] != sizes[0] ||
      cachedSizes[1] != sizes[1] || cachedSizes[2] != sizes[2] ||
      cachedSizes[3] != sizes[3]) {
    cachedTemplates = {};
    if (!std::filesystem::exists(fullPath)) {
      Log::logger->log(Log::ERR, "ImgUtils failed to load {}", fullPath);
      return {};
    }
    cairo_surface_t *srcSurf =
        cairo_image_surface_create_from_png(fullPath.c_str());
    if (!srcSurf || cairo_surface_status(srcSurf) != CAIRO_STATUS_SUCCESS) {
      if (srcSurf)
        cairo_surface_destroy(srcSurf);
      return {};
    }

    const float surfW = (float)cairo_image_surface_get_width(srcSurf);
    const float surfH = (float)cairo_image_surface_get_height(srcSurf);
    const float bl = (float)sizes[0];
    const float br = (float)sizes[1];
    const float bt = (float)sizes[2];
    const float bb = (float)sizes[3];
    const float wMid = surfW - bl - br;
    const float hMid = surfH - bt - bb;

    cachedTemplates.tl =
        sliceTemplateFromSurface(srcSurf, {{0., 0.}, {bl, bt}}, PIECE_TL);
    cachedTemplates.t =
        sliceTemplateFromSurface(srcSurf, {{bl, 0.}, {wMid, bt}}, PIECE_TOP);
    cachedTemplates.tr = sliceTemplateFromSurface(
        srcSurf, {{surfW - br, 0.}, {br, bt}}, PIECE_TR);
    cachedTemplates.r = sliceTemplateFromSurface(
        srcSurf, {{surfW - br, bt}, {br, hMid}}, PIECE_RIGHT);
    cachedTemplates.br = sliceTemplateFromSurface(
        srcSurf, {{surfW - br, surfH - bb}, {br, bb}}, PIECE_BR);
    cachedTemplates.b = sliceTemplateFromSurface(
        srcSurf, {{bl, surfH - bb}, {wMid, bb}}, PIECE_BOTTOM);
    cachedTemplates.bl = sliceTemplateFromSurface(
        srcSurf, {{0., surfH - bb}, {bl, bb}}, PIECE_BL);
    cachedTemplates.l =
        sliceTemplateFromSurface(srcSurf, {{0., bt}, {bl, hMid}}, PIECE_LEFT);

    cairo_surface_destroy(srcSurf);
    cachedPath = fullPath;
    for (int i = 0; i < 4; ++i)
      cachedSizes[i] = sizes[i];
  }

  SBorderSliceSet inst;
  inst.tl = instantiateSliceGL(cachedTemplates.tl);
  inst.t = instantiateSliceGL(cachedTemplates.t);
  inst.tr = instantiateSliceGL(cachedTemplates.tr);
  inst.r = instantiateSliceGL(cachedTemplates.r);
  inst.br = instantiateSliceGL(cachedTemplates.br);
  inst.b = instantiateSliceGL(cachedTemplates.b);
  inst.bl = instantiateSliceGL(cachedTemplates.bl);
  inst.l = instantiateSliceGL(cachedTemplates.l);
  return inst;
}

ImgUtils::SDissolveTexture
ImgUtils::renderDungeonTitle(const std::string &appName) {
  std::istringstream iss(appName);
  std::vector<std::string> words;
  std::string w;
  while (iss >> w)
    words.push_back(w);
  if (words.empty())
    words.push_back("HYPRLAND");

  const int largePt = 50;
  const int smallPt = 37;
  const double condScaleX = 0.88;
  const double letterSpacingPx = 5.5;
  const double wordSpacingPx = 20.0;

  cairo_surface_t *tmpSurf =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 16, 16);
  cairo_t *tmpCr = cairo_create(tmpSurf);
  PangoLayout *layout = pango_cairo_create_layout(tmpCr);

  PangoFontDescription *fontLarge = pango_font_description_from_string(
      "Noto Serif Condensed ExtraBold, Noto Serif Bold, DejaVu Serif Bold");
  pango_font_description_set_weight(fontLarge, PANGO_WEIGHT_ULTRABOLD);
  pango_font_description_set_stretch(fontLarge, PANGO_STRETCH_CONDENSED);
  pango_font_description_set_absolute_size(fontLarge, largePt * PANGO_SCALE);

  PangoFontDescription *fontSmall = pango_font_description_copy(fontLarge);
  pango_font_description_set_absolute_size(fontSmall, smallPt * PANGO_SCALE);

  struct GlyphItem {
    std::string ch;
    bool isLarge;
    double width;
    double advanceAfter;
  };
  std::vector<GlyphItem> glyphs;

  double totalW = 0.0;
  double maxH = 96.0;

  for (size_t wi = 0; wi < words.size(); ++wi) {
    auto chars = utf8CharsUpper(words[wi]);
    bool connector = (wi > 0 && isSmallConnectorWord(words[wi]));
    for (size_t ci = 0; ci < chars.size(); ++ci) {
      bool isLarge = (ci == 0 && !connector);
      pango_layout_set_font_description(layout,
                                        isLarge ? fontLarge : fontSmall);
      pango_layout_set_text(layout, chars[ci].c_str(), -1);
      PangoRectangle inkRect, logRect;
      pango_layout_get_pixel_extents(layout, &inkRect, &logRect);
      double gw = logRect.width * condScaleX;
      double adv =
          gw + ((ci + 1 < chars.size())
                    ? letterSpacingPx
                    : (wi + 1 < words.size() ? wordSpacingPx : 0.0));
      glyphs.push_back({chars[ci], isLarge, gw, adv});
      totalW += adv;
    }
  }

  g_object_unref(layout);
  cairo_destroy(tmpCr);
  cairo_surface_destroy(tmpSurf);

  int padX = 32;
  int padY = 24;
  int surfW = std::max(120, (int)std::ceil(totalW) + padX * 2);
  int surfH = (int)maxH + padY * 2;

  cairo_surface_t *glyphSurf =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, surfW, surfH);
  cairo_t *gcr = cairo_create(glyphSurf);
  PangoLayout *glayout = pango_cairo_create_layout(gcr);

  double curX = padX;
  double baseLineY = padY + 62.0;

  struct GlyphBox {
    int x0, x1;
    bool isLarge;
    std::string ch;
  };
  std::vector<GlyphBox> gboxes;

  cairo_set_source_rgba(gcr, 1.0, 1.0, 1.0, 1.0);
  for (const auto &g : glyphs) {
    pango_layout_set_font_description(glayout,
                                      g.isLarge ? fontLarge : fontSmall);
    pango_layout_set_text(glayout, g.ch.c_str(), -1);
    int baseline = pango_layout_get_baseline(glayout) / PANGO_SCALE;
    double drawY = baseLineY - baseline + (g.isLarge ? 3.0 : 0.0);

    cairo_save(gcr);
    cairo_translate(gcr, curX, drawY);
    cairo_scale(gcr, condScaleX, 1.0);
    pango_cairo_layout_path(gcr, glayout);
    cairo_set_line_join(gcr, CAIRO_LINE_JOIN_MITER);
    cairo_set_miter_limit(gcr, 3.0);
    cairo_set_line_width(gcr, g.isLarge ? 1.8 : 1.4);
    cairo_stroke_preserve(gcr);
    cairo_fill(gcr);
    cairo_restore(gcr);

    int gx0 = std::max(0, (int)std::floor(curX - 2));
    int gx1 = std::min(surfW - 1, (int)std::ceil(curX + g.width + 3));
    gboxes.push_back({gx0, gx1, g.isLarge, g.ch});
    curX += g.advanceAfter;
  }

  pango_font_description_free(fontLarge);
  pango_font_description_free(fontSmall);
  g_object_unref(glayout);
  cairo_destroy(gcr);
  cairo_surface_flush(glyphSurf);

  uint8_t *gdata = cairo_image_surface_get_data(glyphSurf);
  int gstride = cairo_image_surface_get_stride(glyphSurf);

  std::vector<float> carveMask(surfW * surfH, 0.0f);
  std::vector<float> spurAdd(surfW * surfH, 0.0f);

  for (const auto &gb : gboxes) {
    int minStemW = gb.isLarge ? 6 : 5;
    int maxStemW = gb.isLarge ? 20 : 16;

    for (int side = 0; side < 2; ++side) {
      struct StemRow {
        int y;
        float xc;
        int xLeft;
        int w;
      };
      std::vector<StemRow> rows;
      for (int y = 0; y < surfH; ++y) {
        struct Run {
          int x0, w;
        };
        std::vector<Run> runs;
        int runStart = -1;
        for (int x = gb.x0; x <= gb.x1; ++x) {
          uint8_t a = gdata[y * gstride + x * 4 + 3];
          if (a > 160) {
            if (runStart < 0)
              runStart = x;
          } else if (runStart >= 0) {
            int rw = x - runStart;
            if (rw >= minStemW && rw <= maxStemW)
              runs.push_back({runStart, rw});
            runStart = -1;
          }
        }
        if (runStart >= 0) {
          int rw = gb.x1 + 1 - runStart;
          if (rw >= minStemW && rw <= maxStemW)
            runs.push_back({runStart, rw});
        }
        if ((int)runs.size() > side) {
          auto &r = runs[side];
          rows.push_back({y, r.x0 + r.w * 0.5f - 0.5f, r.x0, r.w});
        }
      }

      int bestStart = 0, bestLen = 0, curStart = 0;
      for (size_t i = 1; i < rows.size(); ++i) {
        if (rows[i].y == rows[i - 1].y + 1 &&
            std::abs(rows[i].xc - rows[i - 1].xc) <= 1.6f) {
          int len = (int)i - curStart + 1;
          if (len > bestLen) {
            bestLen = len;
            bestStart = curStart;
          }
        } else {
          curStart = (int)i;
        }
      }

      if (bestLen >= 10) {
        int yTop = rows[bestStart].y;
        int yBot = rows[bestStart + bestLen - 1].y;
        float yMid = 0.5f * (yTop + yBot);
        float halfH = 0.5f * (yBot - yTop);

        float minXc = 1e9f, maxXc = -1e9f;
        for (int i = bestStart; i < bestStart + bestLen; ++i) {
          minXc = std::min(minXc, rows[i].xc);
          maxXc = std::max(maxXc, rows[i].xc);
        }
        bool isCurved = (maxXc - minXc) > 2.2f;
        if (side == 1 && isCurved && gb.ch != "A")
          continue;

        for (int i = bestStart; i < bestStart + bestLen; ++i) {
          int y = rows[i].y;
          float xc = rows[i].xc;
          float dy = std::abs(y - yMid);
          float normDy = dy / std::max(1.0f, halfH);

          if (normDy < 0.72f) {
            float diamondHalfW =
                (gb.isLarge ? 1.85f : 1.45f) *
                std::max(0.0f, 1.0f - dy / (gb.isLarge ? 5.0f : 4.0f));
            float grooveHalfW =
                isCurved ? 0.0f
                         : (gb.isLarge ? 0.95f : 0.78f) * (1.0f - normDy * 0.65f);
            float cutHalfW = std::max(diamondHalfW, grooveHalfW);

            if (cutHalfW > 0.1f) {
              for (int x = (int)std::floor(xc - cutHalfW - 1);
                   x <= (int)std::ceil(xc + cutHalfW + 1); ++x) {
                if (x >= 0 && x < surfW) {
                  float dx = std::abs(x - xc);
                  float cov = std::clamp(cutHalfW + 0.45f - dx, 0.0f, 1.0f);
                  carveMask[y * surfW + x] =
                      std::max(carveMask[y * surfW + x], cov * 0.92f);
                }
              }
            }
          }

          if (!isCurved && side == 0 && gb.isLarge) {
            for (float spurOffset : {-4.5f, 4.5f}) {
              float sdy = std::abs((y - yMid) - spurOffset);
              if (sdy <= 2.2f) {
                float spurW = (1.0f - sdy / 2.2f) * 2.4f;
                int xEdge = rows[i].xLeft;
                for (int sx = xEdge - (int)std::ceil(spurW) - 1; sx <= xEdge;
                     ++sx) {
                  if (sx >= 0 && sx < surfW) {
                    float dist = xEdge - sx;
                    float scov = std::clamp(spurW + 0.3f - dist, 0.0f, 1.0f);
                    spurAdd[y * surfW + sx] =
                        std::max(spurAdd[y * surfW + sx], scov);
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  std::vector<float> rawAlpha(surfW * surfH, 0.0f);
  int minInkX = surfW, maxInkX = 0;
  for (int y = 0; y < surfH; ++y) {
    for (int x = 0; x < surfW; ++x) {
      float baseA = gdata[y * gstride + x * 4 + 3] / 255.0f;
      baseA = std::min(1.0f, baseA + spurAdd[y * surfW + x]);
      float carvedA = baseA * (1.0f - carveMask[y * surfW + x]);
      float a = std::clamp(carvedA, 0.0f, 1.0f);
      rawAlpha[y * surfW + x] = a;
      if (a > 0.05f) {
        minInkX = std::min(minInkX, x);
        maxInkX = std::max(maxInkX, x);
      }
    }
  }

  cairo_surface_destroy(glyphSurf);

  if (minInkX > maxInkX) {
    minInkX = 0;
    maxInkX = surfW - 1;
  }

  // Symmetrically pad left and right around the exact visual ink bounds [minInkX, maxInkX]
  const int symPadX = 24;
  const int inkW = maxInkX - minInkX + 1;
  const int outW = inkW + symPadX * 2;
  std::vector<float> finalAlpha(outW * surfH, 0.0f);
  for (int y = 0; y < surfH; ++y) {
    for (int x = 0; x < inkW; ++x) {
      finalAlpha[y * outW + (x + symPadX)] = rawAlpha[y * surfW + (minInkX + x)];
    }
  }
  surfW = outW;

  SDissolveTexture dt;
  dt.width = surfW;
  dt.height = surfH;
  dt.workBuffer.assign(surfW * surfH * 4, 0);

  std::vector<float> shadowA(surfW * surfH, 0.0f);
  const int blurR = 5;
  for (int y = blurR; y < surfH - blurR; ++y) {
    for (int x = blurR; x < surfW - blurR; ++x) {
      float sum = 0.0f, wsum = 0.0f;
      for (int dy = -blurR; dy <= blurR; ++dy) {
        for (int dx = -blurR; dx <= blurR; ++dx) {
          float w = std::exp(-(dx * dx + dy * dy) / 8.0f);
          sum += finalAlpha[(y + dy) * surfW + (x + dx)] * w;
          wsum += w;
        }
      }
      shadowA[y * surfW + x] = sum / wsum;
    }
  }

  for (int y = 0; y < surfH; ++y) {
    float tGrad = std::clamp((y - padY) / 56.0f, 0.0f, 1.0f);
    float fgR = (247.0f * (1.0f - tGrad) + 228.0f * tGrad) / 255.0f;
    float fgG = (244.0f * (1.0f - tGrad) + 219.0f * tGrad) / 255.0f;
    float fgB = (228.0f * (1.0f - tGrad) + 185.0f * tGrad) / 255.0f;

    for (int x = 0; x < surfW; ++x) {
      float fgA = finalAlpha[y * surfW + x];
      int sy = std::max(0, y - 2);
      int sx = std::max(0, x - 1);
      float shA = std::min(1.0f, shadowA[sy * surfW + sx] * 0.85f);
      float shR = 0.04f, shG = 0.07f, shB = 0.05f;

      float outA = fgA + shA * (1.0f - fgA);
      uint8_t aByte = (uint8_t)std::clamp(outA * 255.0f, 0.0f, 255.0f);
      if (aByte == 0)
        continue;

      float r = (fgR * fgA + shR * shA * (1.0f - fgA));
      float g = (fgG * fgA + shG * shA * (1.0f - fgA));
      float b = (fgB * fgA + shB * shA * (1.0f - fgA));

      uint8_t bByte = (uint8_t)std::clamp(b * 255.0f, 0.0f, 255.0f);
      uint8_t gByte = (uint8_t)std::clamp(g * 255.0f, 0.0f, 255.0f);
      uint8_t rByte = (uint8_t)std::clamp(r * 255.0f, 0.0f, 255.0f);

      uint32_t off = (uint32_t)((y * surfW + x) * 4);
      dt.workBuffer[off + 0] = bByte;
      dt.workBuffer[off + 1] = gByte;
      dt.workBuffer[off + 2] = rByte;
      dt.workBuffer[off + 3] = aByte;

      float resist = computePieceResist(x, y, surfW, surfH, PIECE_TITLE);
      dt.activePixels.push_back({off, bByte, gByte, rByte, aByte, resist});
    }
  }

  dt.tex = createGLTextureFromBGRA(surfW, surfH, dt.workBuffer.data());
  dt.currentStage = 0;
  return dt;
}

void ImgUtils::setDissolveStage(SDissolveTexture &dt, int stage,
                                int maxStages) {
  if (!dt.tex || dt.width <= 0 || dt.height <= 0 || dt.currentStage == stage)
    return;

  if (stage <= 0) {
    for (const auto &px : dt.activePixels) {
      uint8_t *dst = dt.workBuffer.data() + px.byteOffset;
      dst[0] = px.b;
      dst[1] = px.g;
      dst[2] = px.r;
      dst[3] = px.a;
    }
    dt.currentStage = 0;
  } else {
    float progress = std::clamp(float(stage) / float(maxStages), 0.0f, 1.0f);
    float threshold = progress * 1.18f - 0.05f;
    const float edge = 0.09f;

    for (const auto &px : dt.activePixels) {
      float keep = std::clamp((px.resist - threshold) / edge, 0.0f, 1.0f);
      keep = keep * keep * (3.0f - 2.0f * keep);
      uint8_t *dst = dt.workBuffer.data() + px.byteOffset;
      dst[0] = (uint8_t)(px.b * keep);
      dst[1] = (uint8_t)(px.g * keep);
      dst[2] = (uint8_t)(px.r * keep);
      dst[3] = (uint8_t)(px.a * keep);
    }
    dt.currentStage = stage;
  }

  glBindTexture(GL_TEXTURE_2D, dt.tex->m_texID);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, dt.width, dt.height, GL_RGBA,
                  GL_UNSIGNED_BYTE, dt.workBuffer.data());
}

std::string ImgUtils::resolveDesktopAppName(const std::string &appID,
                                            const std::string &fallbackTitle) {
  static bool initialized = false;
  static std::unordered_map<std::string, std::string> desktopMap;

  if (!initialized) {
    initialized = true;
    bool usePt = isPortuguesePreferred();
    std::vector<std::string> dirs = {"/usr/share/applications",
                                     "/var/lib/flatpak/exports/share/applications"};
    const char *home = std::getenv("HOME");
    if (home)
      dirs.push_back(std::string(home) + "/.local/share/applications");

    for (const auto &dir : dirs) {
      std::error_code ec;
      if (!std::filesystem::exists(dir, ec))
        continue;
      for (const auto &entry :
           std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec) ||
            entry.path().extension() != ".desktop")
          continue;

        std::ifstream in(entry.path());
        if (!in)
          continue;

        std::string line;
        bool inMainGroup = false;
        std::string nameDefault, namePt, wmClass;
        while (std::getline(in, line)) {
          line = trimStr(line);
          if (line.empty() || line[0] == '#')
            continue;
          if (line[0] == '[') {
            inMainGroup = (line == "[Desktop Entry]");
            continue;
          }
          if (!inMainGroup)
            continue;

          if (line.rfind("Name=", 0) == 0 && nameDefault.empty())
            nameDefault = trimStr(line.substr(5));
          else if ((line.rfind("Name[pt_BR]=", 0) == 0 ||
                    line.rfind("Name[pt]=", 0) == 0) &&
                   namePt.empty()) {
            size_t eq = line.find('=');
            if (eq != std::string::npos)
              namePt = trimStr(line.substr(eq + 1));
          } else if (line.rfind("StartupWMClass=", 0) == 0)
            wmClass = trimStr(line.substr(15));
        }

        std::string chosen = (usePt && !namePt.empty()) ? namePt : nameDefault;
        if (chosen.empty())
          continue;

        std::string stem = toLowerAscii(entry.path().stem().string());
        desktopMap[stem] = chosen;
        size_t lastDot = stem.rfind('.');
        if (lastDot != std::string::npos && lastDot + 1 < stem.size()) {
          std::string shortStem = stem.substr(lastDot + 1);
          if (!desktopMap.contains(shortStem))
            desktopMap[shortStem] = chosen;
        }
        if (!wmClass.empty())
          desktopMap[toLowerAscii(wmClass)] = chosen;
      }
    }
  }

  std::string raw = trimStr(appID);
  if (!raw.empty()) {
    std::string low = toLowerAscii(raw);
    if (desktopMap.contains(low))
      return desktopMap[low];

    size_t lastDot = low.rfind('.');
    if (lastDot != std::string::npos && lastDot + 1 < low.size()) {
      std::string shortKey = low.substr(lastDot + 1);
      if (desktopMap.contains(shortKey))
        return desktopMap[shortKey];
      raw = raw.substr(lastDot + 1);
    }

    for (char &c : raw) {
      if (c == '-' || c == '_')
        c = ' ';
    }
    return raw;
  }

  std::string fb = trimStr(fallbackTitle);
  if (!fb.empty()) {
    size_t dash = fb.find(" - ");
    if (dash != std::string::npos && dash + 3 < fb.size())
      fb = fb.substr(dash + 3);
    if (fb.size() > 28)
      fb = fb.substr(0, 28);
    return fb;
  }
  return "Hyprland";
}
