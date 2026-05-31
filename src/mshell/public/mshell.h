#pragma once

// c++ headers ------------------------------------------
#include <cstdint>
#include <memory>

// public project headers -------------------------------
#include "mbase/public/platform.h"
#include "mbase/public/access.h"
#include "mnexus/public/types.h"

// forward declarations ---------------------------------
#if MBASE_PLATFORM_WINDOWS || MBASE_PLATFORM_LINUX
typedef union SDL_Event SDL_Event;
#endif

#if MBASE_PLATFORM_ANDROID
struct AAssetManager;
struct AInputEvent;
struct GameActivityMotionEvent;
#endif

namespace mnexus {
  class INexus;
  class IDevice;
  class ICommandList;
}

namespace mshell {

/// Per-platform window handles passed to `IShell::OnSurfaceRecreated`.
/// Matches the previous `wentos2::PlatformSurfaceInfo` layout.
struct PlatformSurfaceInfo final {
  uint64_t instance_handle = 0;  // HINSTANCE / JNIEnv*
  uint64_t display_handle  = 0;  // X11 Display / GameActivity object
  uint64_t window_handle   = 0;  // HWND / X11 Window / ANativeWindow*
  /// `SDL_Window*`. Desktop only.
  uint64_t imgui_sdl_window = 0;
};

struct PlatformEvent final {
#if MBASE_PLATFORM_WINDOWS || MBASE_PLATFORM_LINUX
  SDL_Event* sdl_event = nullptr;
#endif
};

/// Surfaced to the frame after the shell has finished bringing up
/// mnexus + ImGui. Frames typically grab `device` and `nexus` here for
/// later use; both pointers stay valid until `OnDetach`.
struct AttachContext final {
  mnexus::IDevice* device = nullptr;
  mnexus::INexus*  nexus  = nullptr;
};

struct NewFrameContext final {
  mnexus::IDevice* device = nullptr;
  mnexus::INexus*  nexus  = nullptr;
  float delta_time = 0.0f;
};

struct RenderContext final {
  mnexus::IDevice*      device       = nullptr;
  mnexus::INexus*       nexus        = nullptr;
  mnexus::ICommandList* command_list = nullptr;
  mnexus::TextureHandle swapchain_texture;
};

/// What to draw each frame. The shell owns the window, mnexus device,
/// ImGui setup, and the per-frame lifecycle; the frame just decides
/// what to render on top. Pure-virtual `OnNewFrame` + `OnRender`.
class IFrame {
public:
  virtual ~IFrame() = default;
  MBASE_DISALLOW_COPY_MOVE(IFrame);

  /// Called once after the shell has created mnexus + ImGui. Safe to
  /// allocate GPU resources, compile shaders, register fonts, etc.
  virtual void OnAttach(AttachContext const&) {}

  /// Called once during shell teardown. Frame should release the GPU
  /// resources it allocated in `OnAttach`.
  virtual void OnDetach() {}

  /// Called whenever the swapchain surface is recreated (window
  /// resize, display change, HDR toggle). Frame should re-query the
  /// surface format if it cares.
  virtual void OnSurfaceChanged() {}

  /// Called for every platform event the shell sees (SDL drop, key,
  /// resize, ...). Default: ignore.
  virtual void OnEvent(PlatformEvent const&) {}

  /// Called once per frame AFTER `ImGui::NewFrame`. Frame's ImGui
  /// calls go here.
  virtual void OnNewFrame(NewFrameContext const&) = 0;

  /// Called once per frame to record the swapchain pass. ImGui is
  /// rendered by the shell AFTER this returns, so the frame's content
  /// is the base layer and ImGui composites on top.
  virtual void OnRender(RenderContext const&) = 0;

protected:
  IFrame() = default;
};

struct ShellCreateDesc final {
#if MBASE_PLATFORM_ANDROID
  ::AAssetManager* android_asset_manager = nullptr;
  /// `JNIEnv*`.
  uint64_t jni_env = 0;
  /// `jobject` of the GameActivity.
  uint64_t game_activity_object = 0;
  /// `ANativeActivity::internalDataPath`, e.g. `/data/data/<package>/files`.
  char const* android_internal_data_path = nullptr;
#endif

  /// The frame the shell hosts. Ownership transfers to the shell.
  std::unique_ptr<IFrame> frame;
};

class IShell {
public:
  virtual ~IShell() = default;
  MBASE_DISALLOW_COPY_MOVE(IShell);

  [[nodiscard]] static std::unique_ptr<IShell> Create(ShellCreateDesc desc);

  virtual void OnFinalize() = 0;

  virtual void OnDisplayChanged() = 0;
  virtual void OnSurfaceDestroyed() = 0;
  virtual void OnSurfaceRecreated(PlatformSurfaceInfo const&) = 0;

  virtual void ProcessPlatformEvent(PlatformEvent const&) = 0;
#if MBASE_PLATFORM_ANDROID
  virtual void ProcessAndroidMotionEvents(uint32_t count, GameActivityMotionEvent const* events) = 0;
  virtual void ProcessAndroidInputEvent(AInputEvent const* event) = 0;
#endif

  virtual void OnNewFrame() = 0;
  virtual void OnTick() = 0;

protected:
  IShell() = default;
};

} // namespace mshell
