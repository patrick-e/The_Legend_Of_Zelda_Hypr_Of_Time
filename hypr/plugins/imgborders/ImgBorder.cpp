#include "ImgBorder.hpp"
#include "ImgBorderPassElement.hpp"
#include "ImgUtils.hpp"
#include "globals.hpp"
#include <algorithm>
#include <cmath>
#include <desktop/view/window/Window.hpp>
#include <desktop/view/window/WindowPresentation.hpp>
#include <filesystem>
#include <format>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/render/decorations/DecorationPositioner.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <wordexp.h>

using namespace GL;

namespace {
constexpr float T_OPEN_END = 0.45f;
constexpr float T_HOLD_END = 3.15f;
constexpr float T_DISSOLVE_END = 4.25f;
constexpr int MAX_DISSOLVE_STAGES = 32;
} // namespace

CImgBorder::CImgBorder(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow) {
  m_pWindow = pWindow;
  m_animTimer = makeShared<CEventLoopTimer>(
      std::chrono::milliseconds(16),
      [this](SP<CEventLoopTimer> self, void *data) { onAnimTick(); }, this);
  if (g_pEventLoopManager)
    g_pEventLoopManager->addTimer(m_animTimer);
  updateConfig();
}

CImgBorder::~CImgBorder() {
  if (m_animTimer) {
    m_animTimer->cancel();
    if (g_pEventLoopManager)
      g_pEventLoopManager->removeTimer(m_animTimer);
    m_animTimer.reset();
  }
  clearTextures();
  std::erase(g_pGlobalState->borders, m_self);
}

void CImgBorder::clearTextures() {
  m_tl = {};
  m_tr = {};
  m_br = {};
  m_bl = {};
  m_t = {};
  m_r = {};
  m_b = {};
  m_l = {};
  m_titleTex = {};
}

void CImgBorder::onAnimTick() {
  if (m_animationFinished || !m_isEnabled || m_isHidden ||
      !validMapped(m_pWindow)) {
    return;
  }

  damageEntire();

  if (!m_animStarted) {
    if (m_animTimer)
      m_animTimer->updateTimeout(std::chrono::milliseconds(16));
    return;
  }

  float elapsed = std::chrono::duration<float>(
                      std::chrono::steady_clock::now() - m_startTime)
                      .count();

  if (elapsed < T_OPEN_END + 0.05f) {
    if (m_animTimer)
      m_animTimer->updateTimeout(std::chrono::milliseconds(16));
  } else if (elapsed < T_HOLD_END - 0.03f) {
    int waitMs =
        std::clamp((int)((T_HOLD_END - 0.02f - elapsed) * 1000.0f), 16, 150);
    if (m_animTimer)
      m_animTimer->updateTimeout(std::chrono::milliseconds(waitMs));
  } else if (elapsed < T_DISSOLVE_END + 0.05f) {
    if (m_animTimer)
      m_animTimer->updateTimeout(std::chrono::milliseconds(16));
  } else {
    // Only the app title disappears; the window borders remain permanently!
    m_animationFinished = true;
    m_titleTex = {};
    damageEntire();
    if (m_animTimer)
      m_animTimer->updateTimeout(std::nullopt);
  }
}

void CImgBorder::ensureBorderTextures() {
  if (!m_needsBorderReload && m_tl.tex != nullptr)
    return;

  m_needsBorderReload = false;
  auto slices = ImgUtils::getCachedBorderSlices(m_texPath, m_sizes);
  if (!slices.tl.tex) {
    m_isEnabled = false;
    return;
  }

  m_tl = std::move(slices.tl);
  m_t = std::move(slices.t);
  m_tr = std::move(slices.tr);
  m_r = std::move(slices.r);
  m_br = std::move(slices.br);
  m_b = std::move(slices.b);
  m_bl = std::move(slices.bl);
  m_l = std::move(slices.l);
}

void CImgBorder::ensureTitleTexture() {
  const auto PWINDOW = m_pWindow.lock();
  if (!PWINDOW)
    return;

  std::string appID = PWINDOW->metadata().appID();
  if (appID.empty())
    appID = PWINDOW->metadata().initialAppID();
  std::string winTitle = PWINDOW->metadata().title();
  if (winTitle.empty())
    winTitle = PWINDOW->metadata().initialTitle();

  if (m_titleTex.tex != nullptr && (!appID.empty() && appID == m_resolvedAppID))
    return;

  std::string resolved = ImgUtils::resolveDesktopAppName(appID, winTitle);
  if (m_titleTex.tex != nullptr && resolved == m_resolvedTitleText)
    return;

  m_resolvedAppID = appID;
  m_resolvedTitleText = resolved;
  m_titleTex = ImgUtils::renderDungeonTitle(m_resolvedTitleText);
}

SDecorationPositioningInfo CImgBorder::getPositioningInfo() {
  SDecorationPositioningInfo info;
  info.policy = DECORATION_POSITION_STICKY;
  info.edges = DECORATION_EDGE_LEFT | DECORATION_EDGE_RIGHT |
               DECORATION_EDGE_TOP | DECORATION_EDGE_BOTTOM;
  info.priority = 4999; // Must be lower than hyprbars
  if (m_isEnabled && !m_isHidden) {
    info.desiredExtents = {
        .topLeft = {(m_sizes[0] - m_insets[0]) * m_scale,
                    (m_sizes[2] - m_insets[2]) * m_scale},
        .bottomRight = {(m_sizes[1] - m_insets[1]) * m_scale,
                        (m_sizes[3] - m_insets[3]) * m_scale},
    };
  }
  info.reserved = true;
  return info;
}

void CImgBorder::onPositioningReply(const SDecorationPositioningReply &reply) {
  m_assignedBox = reply.assignedGeometry;
}

void CImgBorder::draw(Render::CRenderContext &ctx, PHLMONITOR, float const &a,
                      const Render::SWindowRenderPresentation &presentation) {
  if (!m_isEnabled || m_isHidden || !validMapped(m_pWindow) ||
      !m_pWindow->m_ruleApplicator->decorate().valueOrDefault())
    return;

  CImgBorderPassElement::SData data = {.deco = this, .a = a};
  g_pHyprRenderer->addPassElement(ctx, makeUnique<CImgBorderPassElement>(data));
}

eDecorationType CImgBorder::getDecorationType() { return DECORATION_CUSTOM; }

void CImgBorder::updateWindow(PHLWINDOW pWindow) { damageEntire(); }

void CImgBorder::damageEntire() {
  g_pHyprRenderer->damageBox(getGlobalBoundingBox());
}

eDecorationLayer CImgBorder::getDecorationLayer() {
  return DECORATION_LAYER_OVER;
}

uint64_t CImgBorder::getDecorationFlags() {
  return DECORATION_PART_OF_MAIN_WINDOW;
}

std::string CImgBorder::getDisplayName() { return DISPLAY_NAME; }

PHLWINDOWREF CImgBorder::getWindow() { return m_pWindow; }

bool CImgBorder::shouldBlur() { return m_shouldBlur; }

CBox CImgBorder::getGlobalBoundingBox() {
  const auto PWINDOW = m_pWindow.lock();
  if (!PWINDOW)
    return m_assignedBox;

  CBox box = m_assignedBox;
  box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(
      DECORATION_EDGE_TOP | DECORATION_EDGE_LEFT, PWINDOW));

  const auto PWORKSPACE = PWINDOW->m_workspace;
  const auto WORKSPACEOFFSET =
      PWORKSPACE &&
              !sc<bool>(PWINDOW->m_state & Desktop::View::WINDOW_STATE_PINNED)
          ? PWORKSPACE->m_renderOffset->value()
          : Vector2D();

  return box.translate(WORKSPACEOFFSET);
}

void CImgBorder::drawPass(Render::CRenderContext &ctx, const float &a) {
  auto pMonitor = ctx.m_data.pMonitor.lock();
  if (!pMonitor || !validMapped(m_pWindow))
    return;

  ensureBorderTextures();
  if (!m_isEnabled || !m_tl.tex)
    return;

  if (!m_animStarted) {
    m_animStarted = true;
    m_startTime = std::chrono::steady_clock::now();
    if (m_animTimer)
      m_animTimer->updateTimeout(std::chrono::milliseconds(16));
  }

  float elapsed = std::chrono::duration<float>(
                      std::chrono::steady_clock::now() - m_startTime)
                      .count();

  if (!m_animationFinished && elapsed >= T_DISSOLVE_END) {
    m_animationFinished = true;
    m_titleTex = {};
    if (m_animTimer)
      m_animTimer->updateTimeout(std::nullopt);
  }

  auto box = getGlobalBoundingBox()
                 .translate(m_pWindow->presentation().floatingOffset() -
                            pMonitor->m_position)
                 .scale(pMonitor->m_scale);

  const auto scale = m_scale * pMonitor->m_scale;

  const auto BORDER_LEFT = (float)m_sizes[0] * scale;
  const auto BORDER_RIGHT = (float)m_sizes[1] * scale;
  const auto BORDER_TOP = (float)m_sizes[2] * scale;
  const auto BORDER_BOTTOM = (float)m_sizes[3] * scale;

  const auto HEIGHT_MID = std::max(0.0, box.height - BORDER_TOP - BORDER_BOTTOM);
  const auto WIDTH_MID = std::max(0.0, box.width - BORDER_LEFT - BORDER_RIGHT);

  const auto wasUsingNearestNeighbour = ctx.m_data.useNearestNeighbor;
  const auto prevUVTL = ctx.m_data.primarySurfaceUVTopLeft;
  const auto prevUVBR = ctx.m_data.primarySurfaceUVBottomRight;

  ctx.m_data.useNearestNeighbor = !m_shouldSmooth;

  // 1. Always render the permanent ornate 9-slice window borders
  if (m_tl.tex != nullptr) {
    const CBox box_tl = {box.pos(), {BORDER_LEFT, BORDER_TOP}};
    g_pHyprRenderer->draw(ctx, {.tex = m_tl.tex,
                                .box = box_tl,
                                .a = a,
                                .damage = box_tl,
                                .blur = shouldBlur(),
                                .discardMode = DISCARD_ALPHA});
  }

  if (m_tr.tex != nullptr) {
    const CBox box_tr = {{box.x + box.width - BORDER_RIGHT, box.y},
                         {BORDER_RIGHT, BORDER_TOP}};
    g_pHyprRenderer->draw(ctx, {.tex = m_tr.tex,
                                .box = box_tr,
                                .a = a,
                                .damage = box_tr,
                                .blur = shouldBlur(),
                                .discardMode = DISCARD_ALPHA});
  }

  if (m_br.tex != nullptr) {
    const CBox box_br = {
        {box.x + box.width - BORDER_RIGHT, box.y + box.height - BORDER_BOTTOM},
        {BORDER_RIGHT, BORDER_BOTTOM}};
    g_pHyprRenderer->draw(ctx, {.tex = m_br.tex,
                                .box = box_br,
                                .a = a,
                                .damage = box_br,
                                .blur = shouldBlur(),
                                .discardMode = DISCARD_ALPHA});
  }

  if (m_bl.tex != nullptr) {
    const CBox box_bl = {{box.x, box.y + box.height - BORDER_BOTTOM},
                         {BORDER_LEFT, BORDER_BOTTOM}};
    g_pHyprRenderer->draw(ctx, {.tex = m_bl.tex,
                                .box = box_bl,
                                .a = a,
                                .damage = box_bl,
                                .blur = shouldBlur(),
                                .discardMode = DISCARD_ALPHA});
  }

  // Horizontal Edges (Top & Bottom)
  ctx.m_data.primarySurfaceUVTopLeft = {0., 0.};
  ctx.m_data.primarySurfaceUVBottomRight = {1., 1.};
  if (m_t.tex != nullptr && m_t.tex->m_size.x != 0 && scale != 0) {
    const auto spanX = WIDTH_MID / (m_t.tex->m_size.x * scale);
    ctx.m_data.primarySurfaceUVTopLeft.x = 0.5 - spanX * 0.5;
    ctx.m_data.primarySurfaceUVBottomRight.x = 0.5 + spanX * 0.5;
  }

  if (m_t.tex != nullptr) {
    const CBox box_t = {{box.x + BORDER_LEFT, box.y}, {WIDTH_MID, BORDER_TOP}};
    g_pHyprRenderer->draw(ctx, {.tex = m_t.tex,
                                .box = box_t,
                                .a = a,
                                .damage = box_t,
                                .blur = shouldBlur(),
                                .allowCustomUV = true,
                                .wrapX = WRAP_REPEAT,
                                .wrapY = WRAP_REPEAT,
                                .discardMode = DISCARD_ALPHA});
  }

  if (m_b.tex != nullptr) {
    const CBox box_b = {
        {box.x + BORDER_LEFT, box.y + box.height - BORDER_BOTTOM},
        {WIDTH_MID, BORDER_BOTTOM}};
    g_pHyprRenderer->draw(ctx, {.tex = m_b.tex,
                                .box = box_b,
                                .a = a,
                                .damage = box_b,
                                .blur = shouldBlur(),
                                .allowCustomUV = true,
                                .wrapX = WRAP_REPEAT,
                                .wrapY = WRAP_REPEAT,
                                .discardMode = DISCARD_ALPHA});
  }

  // Vertical Edges (Left & Right)
  ctx.m_data.primarySurfaceUVTopLeft = {0., 0.};
  ctx.m_data.primarySurfaceUVBottomRight = {1., 1.};
  if (m_l.tex != nullptr && m_l.tex->m_size.y != 0 && scale != 0) {
    const auto spanY = HEIGHT_MID / (m_l.tex->m_size.y * scale);
    ctx.m_data.primarySurfaceUVTopLeft.y = 0.5 - spanY * 0.5;
    ctx.m_data.primarySurfaceUVBottomRight.y = 0.5 + spanY * 0.5;
  }

  if (m_l.tex != nullptr) {
    const CBox box_l = {{box.x, box.y + BORDER_TOP}, {BORDER_LEFT, HEIGHT_MID}};
    g_pHyprRenderer->draw(ctx, {.tex = m_l.tex,
                                .box = box_l,
                                .a = a,
                                .damage = box_l,
                                .blur = shouldBlur(),
                                .allowCustomUV = true,
                                .wrapX = WRAP_REPEAT,
                                .wrapY = WRAP_REPEAT,
                                .discardMode = DISCARD_ALPHA});
  }

  if (m_r.tex != nullptr) {
    const CBox box_r = {{box.x + box.width - BORDER_RIGHT, box.y + BORDER_TOP},
                        {BORDER_RIGHT, HEIGHT_MID}};
    g_pHyprRenderer->draw(ctx, {.tex = m_r.tex,
                                .box = box_r,
                                .a = a,
                                .damage = box_r,
                                .blur = shouldBlur(),
                                .allowCustomUV = true,
                                .wrapX = WRAP_REPEAT,
                                .wrapY = WRAP_REPEAT,
                                .discardMode = DISCARD_ALPHA});
  }

  // 2. Dungeon App Name Title (centered horizontally on the window, visible ~3s then organically dissolves)
  if (!m_animationFinished) {
    ensureTitleTexture();

    int titleStage = 0;
    if (elapsed >= T_HOLD_END) {
      float pOut = std::clamp(
          (elapsed - T_HOLD_END) / (T_DISSOLVE_END - T_HOLD_END), 0.0f, 1.0f);
      titleStage = std::clamp((int)std::ceil(pOut * MAX_DISSOLVE_STAGES), 1,
                              MAX_DISSOLVE_STAGES);
    }
    ImgUtils::setDissolveStage(m_titleTex, titleStage, MAX_DISSOLVE_STAGES);

    if (m_titleTex.tex != nullptr && m_titleTex.width > 0 &&
        m_titleTex.height > 0) {
      ctx.m_data.primarySurfaceUVTopLeft = {0., 0.};
      ctx.m_data.primarySurfaceUVBottomRight = {1., 1.};

      double titleScale = scale * 0.88;
      double titleW = m_titleTex.width * titleScale;
      double titleH = m_titleTex.height * titleScale;

      double maxTitleW =
          std::max(80.0, box.width - BORDER_LEFT * 1.2 - BORDER_RIGHT * 1.2);
      if (titleW > maxTitleW) {
        double shrink = maxTitleW / titleW;
        titleW *= shrink;
        titleH *= shrink;
      }

      // Centered horizontally on the window (aligned with the center of the bottom border ornament)
      double titleX = box.x + (box.width - titleW) * 0.5;
      double titleY =
          std::max(box.y + BORDER_TOP * 0.5,
                   box.y + box.height - BORDER_BOTTOM * 0.65 - titleH);

      float titleAlpha =
          a * ((elapsed < T_OPEN_END)
                   ? std::clamp(elapsed / T_OPEN_END, 0.0f, 1.0f)
                   : 1.0f);

      if (titleAlpha > 0.01f) {
        const CBox box_title = {{titleX, titleY}, {titleW, titleH}};
        g_pHyprRenderer->draw(ctx, {.tex = m_titleTex.tex,
                                    .box = box_title,
                                    .a = titleAlpha,
                                    .damage = box_title,
                                    .blur = false,
                                    .discardMode = DISCARD_ALPHA});
      }
    }
  }

  // Restore previous values
  ctx.m_data.useNearestNeighbor = wasUsingNearestNeighbour;
  ctx.m_data.primarySurfaceUVTopLeft = prevUVTL;
  ctx.m_data.primarySurfaceUVBottomRight = prevUVBR;
}

void CImgBorder::updateConfig() {
  m_isEnabled = true;

  auto texSrc = g_pGlobalState->config.image->value();
  if (texSrc.empty()) {
    texSrc = "~/.config/hypr/assets/zelda_border_ornate.png";
  }
  wordexp_t p;
  wordexp(texSrc.c_str(), &p, 0);
  std::string texSrcExpanded = "";
  for (size_t i = 0; i < p.we_wordc; i++)
    texSrcExpanded.append(p.we_wordv[i]);
  wordfree(&p);
  if (!std::filesystem::exists(texSrcExpanded)) {
    m_isEnabled = false;
    return;
  }
  m_texPath = texSrcExpanded;

  const auto sizes = g_pGlobalState->config.sizes->value();
  if (sizes.m_top <= 0 && sizes.m_right <= 0 && sizes.m_bottom <= 0 &&
      sizes.m_left <= 0) {
    m_sizes[0] = m_sizes[1] = m_sizes[2] = m_sizes[3] = 74;
  } else {
    m_sizes[0] = sizes.m_left;
    m_sizes[1] = sizes.m_right;
    m_sizes[2] = sizes.m_top;
    m_sizes[3] = sizes.m_bottom;
  }

  const auto insets = g_pGlobalState->config.insets->value();
  if (insets.m_top == 0 && insets.m_right == 0 && insets.m_bottom == 0 &&
      insets.m_left == 0 && sizes.m_top == 0) {
    m_insets[0] = m_insets[1] = m_insets[2] = m_insets[3] = 64;
  } else {
    m_insets[0] = insets.m_left;
    m_insets[1] = insets.m_right;
    m_insets[2] = insets.m_top;
    m_insets[3] = insets.m_bottom;
  }

  m_scale = g_pGlobalState->config.scale->value();
  m_shouldSmooth = g_pGlobalState->config.smooth->value();
  m_shouldBlur = g_pGlobalState->config.blur->value();

  clearTextures();
  m_needsBorderReload = true;

  m_animStarted = false;
  m_animationFinished = false;
  if (m_animTimer)
    m_animTimer->updateTimeout(std::chrono::milliseconds(16));

  g_pDecorationPositioner->repositionDeco(this);
  damageEntire();
}

void CImgBorder::updateRules() {
  const auto PWINDOW = m_pWindow.lock();
  if (!PWINDOW)
    return;
  auto &rules = PWINDOW->m_ruleApplicator->m_otherProps.props;

  auto prevIsHidden = m_isHidden;
  m_isHidden = false;

  if (rules.contains(g_pGlobalState->noImgBordersRuleIdx))
    m_isHidden = truthy(rules.at(g_pGlobalState->noImgBordersRuleIdx)->effect);

  if (prevIsHidden != m_isHidden)
    g_pDecorationPositioner->repositionDeco(this);
}
