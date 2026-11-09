#include "ImgBorder.hpp"
#include "ImgBorderPassElement.hpp"
#include "ImgUtils.hpp"
#include "globals.hpp"
#include <desktop/view/window/Window.hpp>
#include <desktop/view/window/WindowPresentation.hpp>
#include <filesystem>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/render/decorations/DecorationPositioner.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <wordexp.h>

using namespace GL;

CImgBorder::CImgBorder(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow) {
  m_pWindow = pWindow;
  updateConfig();
}

CImgBorder::~CImgBorder() { std::erase(g_pGlobalState->borders, m_self); }

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
  if (!m_isEnabled || m_isHidden || //
      !validMapped(m_pWindow) ||    //
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

  auto box = getGlobalBoundingBox()
                 .translate(m_pWindow->presentation().floatingOffset() -
                            pMonitor->m_position)
                 .scale(pMonitor->m_scale);

  // For debugging
  // g_pHyprOpenGL->renderRect(box, CHyprColor{1.0, 0.0, 0.0, 0.5});
  // return;

  // Render the textures
  // ------------

  const auto scale = m_scale * pMonitor->m_scale;

  const auto BORDER_LEFT = (float)m_sizes[0] * scale;
  const auto BORDER_RIGHT = (float)m_sizes[1] * scale;
  const auto BORDER_TOP = (float)m_sizes[2] * scale;
  const auto BORDER_BOTTOM = (float)m_sizes[3] * scale;

  const auto HEIGHT_MID = box.height - BORDER_TOP - BORDER_BOTTOM;
  const auto WIDTH_MID = box.width - BORDER_LEFT - BORDER_RIGHT;

  // Save previous values

  const auto wasUsingNearestNeighbour = ctx.m_data.useNearestNeighbor;
  const auto prevUVTL = ctx.m_data.primarySurfaceUVTopLeft;
  const auto prevUVBR = ctx.m_data.primarySurfaceUVBottomRight;

  ctx.m_data.useNearestNeighbor = !m_shouldSmooth;

  // Corners

  if (m_tex_tl != nullptr) {
    const CBox box_tl = {box.pos(), {BORDER_LEFT, BORDER_TOP}};
    g_pHyprRenderer->draw(ctx, {.tex = m_tex_tl,
                                .box = box_tl,
                                .a = a,
                                .damage = box_tl,
                                .blur = shouldBlur(),
                                .discardMode = DISCARD_ALPHA});
  }

  if (m_tex_tr != nullptr) {
    const CBox box_tr = {{box.x + box.width - BORDER_RIGHT, box.y},
                         {BORDER_RIGHT, BORDER_TOP}};
    g_pHyprRenderer->draw(ctx, {.tex = m_tex_tr,
                                .box = box_tr,
                                .a = a,
                                .damage = box_tr,
                                .blur = shouldBlur(),
                                .discardMode = DISCARD_ALPHA});
  }

  if (m_tex_br != nullptr) {
    const CBox box_br = {
        {box.x + box.width - BORDER_RIGHT, box.y + box.height - BORDER_BOTTOM},
        {BORDER_RIGHT, BORDER_BOTTOM}};
    g_pHyprRenderer->draw(ctx, {.tex = m_tex_br,
                                .box = box_br,
                                .a = a,
                                .damage = box_br,
                                .blur = shouldBlur(),
                                .discardMode = DISCARD_ALPHA});
  }

  if (m_tex_bl != nullptr) {
    const CBox box_bl = {{box.x, box.y + box.height - BORDER_BOTTOM},
                         {BORDER_LEFT, BORDER_BOTTOM}};
    g_pHyprRenderer->draw(ctx, {.tex = m_tex_bl,
                                .box = box_bl,
                                .a = a,
                                .damage = box_bl,
                                .blur = shouldBlur(),
                                .discardMode = DISCARD_ALPHA});
  }

  // Edges

  ctx.m_data.primarySurfaceUVTopLeft = {0., 0.};
  ctx.m_data.primarySurfaceUVBottomRight = {1., 1.};
  if (m_tex_t != nullptr && m_tex_t->m_size.x != 0 && scale != 0) {
    const auto spanX = WIDTH_MID / (m_tex_t->m_size.x * scale);
    ctx.m_data.primarySurfaceUVTopLeft.x = 0.5 - spanX * 0.5;
    ctx.m_data.primarySurfaceUVBottomRight.x = 0.5 + spanX * 0.5;
  }

  if (m_tex_t != nullptr) {
    const CBox box_t = {{box.x + BORDER_LEFT, box.y}, {WIDTH_MID, BORDER_TOP}};
    g_pHyprRenderer->draw(ctx, {.tex = m_tex_t,
                                .box = box_t,
                                .a = a,
                                .damage = box_t,
                                .blur = shouldBlur(),
                                .allowCustomUV = true,
                                .wrapX = WRAP_REPEAT,
                                .wrapY = WRAP_REPEAT,
                                .discardMode = DISCARD_ALPHA});
  }

  if (m_tex_b != nullptr) {
    const CBox box_b = {
        {box.x + BORDER_LEFT, box.y + box.height - BORDER_BOTTOM},
        {WIDTH_MID, BORDER_BOTTOM}};
    g_pHyprRenderer->draw(ctx, {.tex = m_tex_b,
                                .box = box_b,
                                .a = a,
                                .damage = box_b,
                                .blur = shouldBlur(),
                                .allowCustomUV = true,
                                .wrapX = WRAP_REPEAT,
                                .wrapY = WRAP_REPEAT,
                                .discardMode = DISCARD_ALPHA});
  }

  ctx.m_data.primarySurfaceUVTopLeft = {0., 0.};
  ctx.m_data.primarySurfaceUVBottomRight = {1., 1.};
  if (m_tex_l != nullptr && m_tex_l->m_size.y != 0 && scale != 0) {
    const auto spanY = HEIGHT_MID / (m_tex_l->m_size.y * scale);
    ctx.m_data.primarySurfaceUVTopLeft.y = 0.5 - spanY * 0.5;
    ctx.m_data.primarySurfaceUVBottomRight.y = 0.5 + spanY * 0.5;
  }

  if (m_tex_l != nullptr) {
    const CBox box_l = {{box.x, box.y + BORDER_TOP}, {BORDER_LEFT, HEIGHT_MID}};
    g_pHyprRenderer->draw(ctx, {.tex = m_tex_l,
                                .box = box_l,
                                .a = a,
                                .damage = box_l,
                                .blur = shouldBlur(),
                                .allowCustomUV = true,
                                .wrapX = WRAP_REPEAT,
                                .wrapY = WRAP_REPEAT,
                                .discardMode = DISCARD_ALPHA});
  }

  if (m_tex_r != nullptr) {
    const CBox box_r = {{box.x + box.width - BORDER_RIGHT, box.y + BORDER_TOP},
                        {BORDER_RIGHT, HEIGHT_MID}};
    g_pHyprRenderer->draw(ctx, {.tex = m_tex_r,
                                .box = box_r,
                                .a = a,
                                .damage = box_r,
                                .blur = shouldBlur(),
                                .allowCustomUV = true,
                                .wrapX = WRAP_REPEAT,
                                .wrapY = WRAP_REPEAT,
                                .discardMode = DISCARD_ALPHA});
  }

  // Restore previous values

  ctx.m_data.useNearestNeighbor = wasUsingNearestNeighbour;

  ctx.m_data.primarySurfaceUVTopLeft = prevUVTL;
  ctx.m_data.primarySurfaceUVBottomRight = prevUVBR;
}

void CImgBorder::updateConfig() {
  // Read config
  // ------------

  m_isEnabled = true;

  // image
  const auto texSrc = g_pGlobalState->config.image->value();
  if (texSrc.empty()) {
    m_isEnabled = false;
    return;
  }
  wordexp_t p;
  wordexp(texSrc.c_str(), &p, 0);
  std::string texSrcExpanded = "";
  for (size_t i = 0; i < p.we_wordc; i++)
    texSrcExpanded.append(p.we_wordv[i]);
  wordfree(&p);
  if (!std::filesystem::exists(texSrcExpanded)) {
    HyprlandAPI::addNotification(
        PHANDLE,
        std::format("[imgborders] image at \"{}\" doesn't exist",
                    texSrcExpanded),
        CHyprColor{1.0, 0.1, 0.1, 1.0}, 5000);
    m_isEnabled = false;
    return;
  }

  // sizes
  const auto sizes = g_pGlobalState->config.sizes->value();
  if (sizes.m_top < 0 || sizes.m_right < 0 || sizes.m_bottom < 0 ||
      sizes.m_left < 0) {
    HyprlandAPI::addNotification(PHANDLE,
                                 "[imgborders] invalid sizes in config",
                                 CHyprColor{1.0, 0.1, 0.1, 1.0}, 5000);
    m_isEnabled = false;
    return;
  }
  m_sizes[0] = sizes.m_left;
  m_sizes[1] = sizes.m_right;
  m_sizes[2] = sizes.m_top;
  m_sizes[3] = sizes.m_bottom;

  // insets
  const auto insets = g_pGlobalState->config.insets->value();
  m_insets[0] = insets.m_left;
  m_insets[1] = insets.m_right;
  m_insets[2] = insets.m_top;
  m_insets[3] = insets.m_bottom;

  // scale
  m_scale = g_pGlobalState->config.scale->value();

  // smooth
  m_shouldSmooth = g_pGlobalState->config.smooth->value();

  // blur
  m_shouldBlur = g_pGlobalState->config.blur->value();

  // Create textures
  // ------------

  // But first delete old textures
  m_tex_tl = nullptr;
  m_tex_tr = nullptr;
  m_tex_br = nullptr;
  m_tex_bl = nullptr;
  m_tex_l = nullptr;
  m_tex_t = nullptr;
  m_tex_b = nullptr;
  m_tex_r = nullptr;

  auto tex = ImgUtils::load(texSrcExpanded);

  const auto BORDER_LEFT = (float)m_sizes[0];
  const auto BORDER_RIGHT = (float)m_sizes[1];
  const auto BORDER_TOP = (float)m_sizes[2];
  const auto BORDER_BOTTOM = (float)m_sizes[3];

  const auto WIDTH_MID = tex->m_size.x - BORDER_LEFT - BORDER_RIGHT;
  const auto HEIGHT_MID = tex->m_size.y - BORDER_TOP - BORDER_BOTTOM;

  m_tex_tl = ImgUtils::sliceTexture(tex, {{0., 0.}, {BORDER_LEFT, BORDER_TOP}});

  m_tex_t =
      ImgUtils::sliceTexture(tex, {{BORDER_LEFT, 0.}, {WIDTH_MID, BORDER_TOP}});

  m_tex_tr = ImgUtils::sliceTexture(
      tex, {{tex->m_size.x - BORDER_RIGHT, 0.}, {BORDER_RIGHT, BORDER_TOP}});

  m_tex_r =
      ImgUtils::sliceTexture(tex, {{tex->m_size.x - BORDER_RIGHT, BORDER_TOP},
                                   {BORDER_RIGHT, HEIGHT_MID}});

  m_tex_br = ImgUtils::sliceTexture(
      tex, {{tex->m_size.x - BORDER_RIGHT, tex->m_size.y - BORDER_BOTTOM},
            {BORDER_RIGHT, BORDER_BOTTOM}});

  m_tex_b =
      ImgUtils::sliceTexture(tex, {{BORDER_LEFT, tex->m_size.y - BORDER_BOTTOM},
                                   {WIDTH_MID, BORDER_BOTTOM}});

  m_tex_bl = ImgUtils::sliceTexture(
      tex, {{0., tex->m_size.y - BORDER_BOTTOM}, {BORDER_LEFT, BORDER_BOTTOM}});

  m_tex_l = ImgUtils::sliceTexture(
      tex, {{0., BORDER_TOP}, {BORDER_LEFT, HEIGHT_MID}});

  g_pDecorationPositioner->repositionDeco(this);
}

void CImgBorder::updateRules() {
  const auto PWINDOW = m_pWindow.lock();
  auto &rules = PWINDOW->m_ruleApplicator->m_otherProps.props;

  auto prevIsHidden = m_isHidden;

  m_isHidden = false;

  if (rules.contains(g_pGlobalState->noImgBordersRuleIdx))
    m_isHidden = truthy(rules.at(g_pGlobalState->noImgBordersRuleIdx)->effect);

  if (prevIsHidden != m_isHidden)
    g_pDecorationPositioner->repositionDeco(this);
}
