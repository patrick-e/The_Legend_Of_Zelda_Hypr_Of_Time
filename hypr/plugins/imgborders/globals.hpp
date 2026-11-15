#pragma once

#include <hyprland/src/config/values/types/BoolValue.hpp>
#include <hyprland/src/config/values/types/CssGapValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

// Plugin API handle
inline HANDLE PHANDLE = nullptr;

// Class defined elsewhere
class CImgBorder;

struct SGlobalState {
  std::vector<WP<CImgBorder>> borders;

  struct {
    SP<Config::Values::CStringValue> image;
    SP<Config::Values::CCssGapValue> sizes;
    SP<Config::Values::CCssGapValue> insets;
    SP<Config::Values::CFloatValue> scale;
    SP<Config::Values::CBoolValue> smooth;
    SP<Config::Values::CBoolValue> blur;
  } config;

  uint32_t noImgBordersRuleIdx = 0;
};
inline UP<SGlobalState> g_pGlobalState;
