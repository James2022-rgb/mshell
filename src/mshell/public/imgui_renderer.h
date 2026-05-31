#pragma once

// c++ headers ------------------------------------------
#include <memory>

// public project headers -------------------------------
#include "mbase/public/access.h"

#include "mnexus/public/types.h"

// forward declarations ---------------------------------
struct ImDrawData;

namespace mnexus {
  class IDevice;
  class ICommandList;
}

namespace mshell {

class ImguiRenderer {
public:
  virtual ~ImguiRenderer() = default;
  MBASE_DISALLOW_COPY_MOVE(ImguiRenderer);

  static std::unique_ptr<ImguiRenderer> Create();

  /// `claim_font_atlas`: when true (the default), this instance uploads
  /// the ImGui font atlas as a GPU texture and assigns it as the
  /// active `io.Fonts->TexID`. Pass false for *secondary* instances
  /// (e.g. a playground that wants to render its own ImDrawData to an
  /// offscreen RT) to avoid clobbering the primary instance's font
  /// texture / TexID -- the secondary's `Render` still works because
  /// each `ImDrawCmd` carries its own `TextureId` (the primary's),
  /// and `BindSampledTexture` will resolve that handle.
  virtual void Initialize(mnexus::IDevice* device, bool claim_font_atlas = true) = 0;
  virtual void Finalize(mnexus::IDevice* device) = 0;

  virtual void Tick() = 0;

  virtual void UpdateGeometryBuffers(
    mnexus::IDevice* device,
    ImDrawData const* draw_data
  ) = 0;

  virtual void Render(
    mnexus::ICommandList* command_list,
    mnexus::TextureHandle render_target,
    ImDrawData const* draw_data
  ) = 0;

protected:
  ImguiRenderer() = default;
};

} // namespace wentos2
