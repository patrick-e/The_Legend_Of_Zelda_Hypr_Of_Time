#include "ImgBorderPassElement.hpp"
#include "ImgBorder.hpp"
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#include <render/Renderer.hpp>

using namespace Render::GL;

CImgBorderPassElement::CImgBorderPassElement(
    const CImgBorderPassElement::SData &data_) {
  data = data_;
}
CImgBorderPassElement::~CImgBorderPassElement() {}

std::vector<UP<IPassElement>>
CImgBorderPassElement::draw(Render::CRenderContext &ctx) {
  data.deco->drawPass(ctx, data.a);
  return {};
}

bool CImgBorderPassElement::needsLiveBlur(Render::CRenderContext &ctx) {
  return data.deco->shouldBlur();
}

bool CImgBorderPassElement::needsPrecomputeBlur(Render::CRenderContext &ctx) {
  return data.deco->shouldBlur();
}

const char *CImgBorderPassElement::passName() { return PASS_NAME; }

ePassElementType CImgBorderPassElement::type() { return EK_CUSTOM; }

std::optional<CBox>
CImgBorderPassElement::boundingBox(Render::CRenderContext &ctx) {
  return data.deco->getGlobalBoundingBox().translate(
      -ctx.m_data.pMonitor->m_position);
}
