#pragma once

#include "ImgUtils.hpp"
#include "globals.hpp"
#include <chrono>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/rule/windowRule/WindowRule.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>
#include <string>

using namespace Render;

static const auto DISPLAY_NAME = "Image borders";

class CImgBorder : public IHyprWindowDecoration {
public:
  CImgBorder(PHLWINDOW);
  virtual ~CImgBorder();

  virtual SDecorationPositioningInfo getPositioningInfo() override;
  virtual void
  onPositioningReply(const SDecorationPositioningReply &reply) override;
  virtual void
  draw(Render::CRenderContext &ctx, PHLMONITOR, float const &a,
       const Render::SWindowRenderPresentation &presentation) override;
  virtual eDecorationType getDecorationType() override;
  virtual void updateWindow(PHLWINDOW) override;
  virtual void damageEntire() override;
  virtual eDecorationLayer getDecorationLayer() override;
  virtual uint64_t getDecorationFlags() override;
  virtual std::string getDisplayName() override;

  PHLWINDOWREF getWindow();

  bool shouldBlur();

  CBox getGlobalBoundingBox();

  void drawPass(Render::CRenderContext &ctx, const float &a);

  void updateConfig();
  void updateRules();

  WP<CImgBorder> m_self;

private:
  void onAnimTick();
  void clearTextures();
  void ensureBorderTextures();
  void ensureTitleTexture();

  PHLWINDOWREF m_pWindow;

  CBox m_assignedBox;

  bool m_isEnabled = false;
  bool m_isHidden = false;
  bool m_needsBorderReload = true;
  std::string m_texPath;
  int m_sizes[4] = {74, 74, 74, 74};
  int m_insets[4] = {64, 64, 64, 64};
  float m_scale = 1.0f;
  bool m_shouldSmooth = true;
  bool m_shouldBlur = false;

  bool m_animStarted = false;
  bool m_animationFinished = false;
  std::chrono::steady_clock::time_point m_startTime;
  SP<CEventLoopTimer> m_animTimer;

  std::string m_resolvedAppID;
  std::string m_resolvedTitleText;

  ImgUtils::SDissolveTexture m_tl;
  ImgUtils::SDissolveTexture m_tr;
  ImgUtils::SDissolveTexture m_br;
  ImgUtils::SDissolveTexture m_bl;
  ImgUtils::SDissolveTexture m_t;
  ImgUtils::SDissolveTexture m_r;
  ImgUtils::SDissolveTexture m_b;
  ImgUtils::SDissolveTexture m_l;
  ImgUtils::SDissolveTexture m_titleTex;
};
