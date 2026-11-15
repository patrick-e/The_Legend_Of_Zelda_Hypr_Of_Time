#pragma once

#include <cairo/cairo.h>
#include <cstdint>
#include <hyprland/src/render/Texture.hpp>
#include <string>
#include <vector>

using namespace Render;

namespace ImgUtils {

enum eBorderPieceType : uint8_t {
  PIECE_TL = 0,
  PIECE_TR,
  PIECE_BR,
  PIECE_BL,
  PIECE_TOP,
  PIECE_BOTTOM,
  PIECE_LEFT,
  PIECE_RIGHT,
  PIECE_TITLE,
};

struct SActivePixel {
  uint32_t byteOffset;
  uint8_t b, g, r, a;
  float resist;
};

struct SDissolveTexture {
  SP<ITexture> tex;
  int width = 0;
  int height = 0;
  std::vector<uint8_t> workBuffer;
  std::vector<SActivePixel> activePixels;
  int currentStage = -1;
};

struct SBorderSliceSet {
  SDissolveTexture tl, t, tr, r, br, b, bl, l;
};

SBorderSliceSet getCachedBorderSlices(const std::string &fullPath,
                                      const int sizes[4]);

SDissolveTexture renderDungeonTitle(const std::string &appName);

void setDissolveStage(SDissolveTexture &dt, int stage, int maxStages);

std::string resolveDesktopAppName(const std::string &appID,
                                  const std::string &fallbackTitle);

} // namespace ImgUtils
