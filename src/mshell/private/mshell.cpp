// TU header --------------------------------------------
#include "mshell/public/mshell.h"

// c++ system headers -----------------------------------
#include <chrono>
#include <memory>

// external headers -------------------------------------
#if MBASE_PLATFORM_WINDOWS || MBASE_PLATFORM_LINUX
# define SDL_MAIN_HANDLED
# include <SDL2/SDL.h>
#endif

#if MBASE_PLATFORM_ANDROID
# include <android/asset_manager.h>
# include <android/native_window.h>
# include <android/input.h>
#endif

#include "imgui.h"
#if MBASE_PLATFORM_WINDOWS || MBASE_PLATFORM_LINUX
# include "backends/imgui_impl_sdl2.h"
#elif MBASE_PLATFORM_ANDROID
# include "backends/imgui_impl_android.h"
#endif

// public project headers -------------------------------
#include "masset/public/masset.h"
#include "mbase/public/assert.h"
#include "mbase/public/log.h"
#include "mnexus/public/mnexus.h"
#include "mslang_proxy/public/mslang_proxy.h"

// project headers --------------------------------------
#include "mshell/public/imgui_renderer.h"

namespace mshell {

class ShellImpl final : public IShell {
public:
  ShellImpl(mnexus::INexus* nexus, std::unique_ptr<IFrame> frame)
    : nexus_(nexus)
    , frame_(std::move(frame)) {
    MBASE_ASSERT(nexus_ != nullptr);
    MBASE_ASSERT(frame_ != nullptr);
  }

  ~ShellImpl() override = default;
  MBASE_DISALLOW_COPY_MOVE(ShellImpl);

  void OnFinalize() override {
    if (frame_ != nullptr) {
      frame_->OnDetach();
      frame_.reset();
    }
    if (imgui_initialized_) {
#if MBASE_PLATFORM_WINDOWS || MBASE_PLATFORM_LINUX
      ImGui_ImplSDL2_Shutdown();
#elif MBASE_PLATFORM_ANDROID
      ImGui_ImplAndroid_Shutdown();
#endif
      if (imgui_renderer_ != nullptr) {
        imgui_renderer_->Finalize(nexus_->GetDevice());
        imgui_renderer_.reset();
      }
      ImGui::DestroyContext();
      imgui_initialized_ = false;
    }
    if (nexus_ != nullptr) {
      nexus_->Destroy();
      nexus_ = nullptr;
    }
  }

  void OnDisplayChanged() override {
    if (nexus_ != nullptr) nexus_->OnDisplayChanged();
  }

  void OnSurfaceDestroyed() override {
    if (nexus_ != nullptr) nexus_->OnSurfaceDestroyed();
  }

  void OnSurfaceRecreated(PlatformSurfaceInfo const& info) override {
    if (nexus_ == nullptr) return;

    mnexus::SurfaceSourceDesc source{};
    source.instance_handle = info.instance_handle;
    source.display_handle  = info.display_handle;
    source.window_handle   = info.window_handle;
    nexus_->OnSurfaceRecreated(source);

    if (!imgui_initialized_) {
      ImGui::CreateContext();

#if MBASE_PLATFORM_WINDOWS || MBASE_PLATFORM_LINUX
      if (info.imgui_sdl_window != 0) {
        SDL_Window* w = reinterpret_cast<SDL_Window*>(info.imgui_sdl_window);
        ImGui_ImplSDL2_InitForOther(w);
      }
#elif MBASE_PLATFORM_ANDROID
      ANativeWindow* aw = reinterpret_cast<ANativeWindow*>(info.window_handle);
      ImGui_ImplAndroid_Init(aw);
#endif

      // Load a Japanese-capable font BEFORE the renderer bakes the
      // font atlas. Meiryo ships with every modern Japanese-locale
      // Windows install; silently fall back to the default font when
      // absent. Apps that want a different font can post-process the
      // atlas in their `OnAttach`.
#if MBASE_PLATFORM_WINDOWS
      {
        ImGuiIO& io = ImGui::GetIO();
        ImFont* const jp_font = io.Fonts->AddFontFromFileTTF(
          "C:\\Windows\\Fonts\\meiryo.ttc",
          15.0f,
          nullptr,
          io.Fonts->GetGlyphRangesJapanese());
        if (jp_font == nullptr) {
          MBASE_LOG_WARN("mshell: meiryo.ttc load failed; non-ASCII text will not render");
        }
      }
#endif

      imgui_renderer_ = mshell::ImguiRenderer::Create();
      imgui_renderer_->Initialize(nexus_->GetDevice());

      imgui_initialized_ = true;

      // First-time attach happens after both nexus and ImGui are up.
      if (!frame_attached_ && frame_ != nullptr) {
        frame_->OnAttach(AttachContext{
          .device = nexus_->GetDevice(),
          .nexus  = nexus_,
        });
        frame_attached_ = true;
      }
    } else {
      if (frame_ != nullptr) frame_->OnSurfaceChanged();
    }
  }

  void ProcessPlatformEvent(PlatformEvent const& event) override {
#if MBASE_PLATFORM_WINDOWS || MBASE_PLATFORM_LINUX
    if (imgui_initialized_) {
      ImGui_ImplSDL2_ProcessEvent(reinterpret_cast<::SDL_Event const*>(event.sdl_event));
    }
#endif
    if (frame_ != nullptr) frame_->OnEvent(event);
  }

#if MBASE_PLATFORM_ANDROID
  void ProcessAndroidMotionEvents(uint32_t count, GameActivityMotionEvent const* events) override {
    ImGui_ProcessGameActivityMotionEvents(count, events);
  }
  void ProcessAndroidInputEvent(AInputEvent const* /*event*/) override {
    // Currently a no-op; left as an extension hook for IME / hard-key
    // forwarding once an app needs it.
  }
#endif

  void OnNewFrame() override {
    if (!imgui_initialized_ || frame_ == nullptr || !frame_attached_) return;

#if MBASE_PLATFORM_WINDOWS || MBASE_PLATFORM_LINUX
    ImGui_ImplSDL2_NewFrame();
#elif MBASE_PLATFORM_ANDROID
    ImGui_ImplAndroid_NewFrame();
#endif
    ImGui::NewFrame();

    auto const now = std::chrono::steady_clock::now();
    float const dt = std::chrono::duration<float>(now - last_tick_time_).count();
    last_tick_time_ = now;

    frame_->OnNewFrame(NewFrameContext{
      .device     = nexus_->GetDevice(),
      .nexus      = nexus_,
      .delta_time = dt,
    });
  }

  void OnTick() override {
    if (!imgui_initialized_ || frame_ == nullptr || !frame_attached_) return;

    nexus_->OnPresentPrologue();

    mnexus::IDevice* const device = nexus_->GetDevice();
    mnexus::TextureHandle const swapchain_tex = device->GetSwapchainTexture();
    mnexus::ICommandList* const command_list = device->CreateCommandList({});

    frame_->OnRender(RenderContext{
      .device            = device,
      .nexus             = nexus_,
      .command_list      = command_list,
      .swapchain_texture = swapchain_tex,
    });

    // Render ImGui overlay on top of whatever the frame drew.
    if (imgui_renderer_ != nullptr) {
      imgui_renderer_->Tick();
      ImGui::Render();
      ImDrawData* const draw_data = ImGui::GetDrawData();
      imgui_renderer_->UpdateGeometryBuffers(device, draw_data);
      imgui_renderer_->Render(command_list, swapchain_tex, draw_data);
    }

    command_list->End();
    device->QueueSubmitCommandList({}, command_list);

    nexus_->OnPresentEpilogue();
  }

private:
  mnexus::INexus* nexus_ = nullptr;
  std::unique_ptr<IFrame> frame_;
  bool frame_attached_ = false;

  std::unique_ptr<mshell::ImguiRenderer> imgui_renderer_;
  bool imgui_initialized_ = false;

  std::chrono::steady_clock::time_point last_tick_time_ = std::chrono::steady_clock::now();
};

std::unique_ptr<IShell> IShell::Create(ShellCreateDesc desc) {
  MBASE_ASSERT(desc.frame != nullptr);

#if MBASE_PLATFORM_ANDROID
  masset::IAssetManager::Get()->SetAndroidAssetManager(desc.android_asset_manager);
  masset::IAssetManager::Get()->SetAndroidContextObject(desc.jni_env, desc.game_activity_object);
  if (desc.android_internal_data_path != nullptr) {
    mslang_proxy::InitializeSlangCache(desc.android_internal_data_path);
  } else {
    MBASE_LOG_WARN("mshell: android_internal_data_path is null; Slang cache disabled.");
  }
#elif MBASE_PLATFORM_WINDOWS || MBASE_PLATFORM_LINUX
  mslang_proxy::InitializeSlangCache(".");
#endif

  std::span<mnexus::BackendType const> available_backends = mnexus::INexus::EnumerateBackends();
  for (mnexus::BackendType backend : available_backends) {
    MBASE_LOG_INFO("mshell: available backend: {}", mnexus::ToString(backend));
  }

  mnexus::NexusDesc nexus_desc{};
  nexus_desc.backend_type = mnexus::BackendType::kVulkan;
  nexus_desc.app_name     = "mshell";
  mnexus::INexus* nexus = mnexus::INexus::Create(nexus_desc);

  return std::make_unique<ShellImpl>(nexus, std::move(desc.frame));
}

} // namespace mshell
