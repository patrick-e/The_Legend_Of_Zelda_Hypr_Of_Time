#pragma once

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/rule/windowRule/WindowRule.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>

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
  PHLWINDOWREF m_pWindow;

  CBox m_assignedBox;

  bool m_isEnabled;
  bool m_isHidden;
  int m_sizes[4];
  int m_insets[4];
  float m_scale;
  bool m_shouldSmooth;
  bool m_shouldBlur;

  SP<ITexture> m_tex_tl;
  SP<ITexture> m_tex_tr;
  SP<ITexture> m_tex_br;
  SP<ITexture> m_tex_bl;
  SP<ITexture> m_tex_t;
  SP<ITexture> m_tex_r;
  SP<ITexture> m_tex_b;
  SP<ITexture> m_tex_l;
};
