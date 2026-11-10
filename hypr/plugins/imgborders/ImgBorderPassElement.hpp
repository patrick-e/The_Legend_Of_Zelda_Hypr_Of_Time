#pragma once

#include <hyprland/src/render/Texture.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#include <optional>

// Class defined elsewhere
class CImgBorder;

static const char *PASS_NAME = "CImgBorderPassElement";

class CImgBorderPassElement : public IPassElement {
public:
  struct SData {
    CImgBorder *deco = nullptr;
    float a = 1.F;
  };

  CImgBorderPassElement(const SData &data_);
  virtual ~CImgBorderPassElement();

  virtual std::vector<UP<IPassElement>>
  draw(Render::CRenderContext &ctx) override;
  virtual bool needsLiveBlur(Render::CRenderContext &ctx) override;
  virtual bool needsPrecomputeBlur(Render::CRenderContext &ctx) override;
  virtual const char *passName() override;
  virtual ePassElementType type() override;
  virtual std::optional<CBox> boundingBox(Render::CRenderContext &ctx) override;

private:
  SData data;
};
