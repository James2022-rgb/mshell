// TU header --------------------------------------------
#include "mshell/public/imgui_renderer.h"

// c++ headers ------------------------------------------
#include <array>
#include <vector>

// external headers -------------------------------------
#include "imgui.h"

// public project headers -------------------------------
#include "mbase/public/assert.h"
#include "mbase/public/log.h"

#include "mnexus/public/mnexus.h"
#include "mnexus/public/render_state_event_log.h"

#include "mslang_proxy/public/mslang_proxy.h"

// project headers --------------------------------------
#include "slang_cache_asset.h"
#include "pso_debug_imgui.h"

namespace mshell {

namespace {

char const* TagToString(mnexus::RenderStateEventTag tag) {
  using enum mnexus::RenderStateEventTag;
  switch (tag) {
    case kBeginRenderPass:    return "BeginRenderPass";
    case kEndRenderPass:      return "EndRenderPass";
    case kSetProgram:         return "SetProgram";
    case kSetVertexInputLayout: return "SetVertexInputLayout";
    case kSetPrimitiveTopology: return "SetPrimitiveTopology";
    case kSetPolygonMode:     return "SetPolygonMode";
    case kSetCullMode:        return "SetCullMode";
    case kSetFrontFace:       return "SetFrontFace";
    case kSetDepthTestEnabled:  return "SetDepthTestEnabled";
    case kSetDepthWriteEnabled: return "SetDepthWriteEnabled";
    case kSetDepthCompareOp:  return "SetDepthCompareOp";
    case kSetStencilTestEnabled: return "SetStencilTestEnabled";
    case kSetStencilFrontOps: return "SetStencilFrontOps";
    case kSetStencilBackOps:  return "SetStencilBackOps";
    case kSetBlendEnabled:    return "SetBlendEnabled";
    case kSetBlendFactors:    return "SetBlendFactors";
    case kSetColorWriteMask:  return "SetColorWriteMask";
    case kPsoResolved:        return "PsoResolved";
    case kDraw:               return "Draw";
    case kDrawIndexed:        return "DrawIndexed";
  }
  return "Unknown";
}

struct DrawListBuffers final {
  mnexus::BufferHandle vertex_buffer;
  mnexus::BufferHandle index_buffer;
  uint32_t vertex_buffer_size = 0;
  uint32_t index_buffer_size = 0;
};

} // anonymous namespace

class ImguiRendererImpl final : public ImguiRenderer {
public:
  ImguiRendererImpl() = default;
  ~ImguiRendererImpl() override = default;
  MBASE_DISALLOW_COPY_MOVE(ImguiRendererImpl);

  void Tick() override {
    if (!ImGui::Begin("PSO Event Log")) {
      ImGui::End();
      return;
    }

    ImGui::Text("Events: %zu", last_frame_events_.size());

    if (last_frame_events_.empty()) {
      ImGui::TextDisabled("No events recorded yet.");
      ImGui::End();
      return;
    }

    // Event table.
    ImGuiTableFlags const table_flags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;

    float const table_height = ImGui::GetTextLineHeightWithSpacing() *
      static_cast<float>(std::min<size_t>(last_frame_events_.size() + 1, 12));

    if (ImGui::BeginTable("##events", 4, table_flags, ImVec2(0.0f, table_height))) {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("#",         ImGuiTableColumnFlags_WidthFixed, 30.0f);
      ImGui::TableSetupColumn("Event",     ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("PSO Hash",  ImGuiTableColumnFlags_WidthFixed, 130.0f);
      ImGui::TableSetupColumn("Cache Hit", ImGuiTableColumnFlags_WidthFixed, 70.0f);
      ImGui::TableHeadersRow();

      for (size_t i = 0; i < last_frame_events_.size(); ++i) {
        auto const& ev = last_frame_events_[i];

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%zu", i);

        ImGui::TableNextColumn();
        bool const selected = (selected_event_ == static_cast<int>(i));
        char label[64];
        snprintf(label, sizeof(label), "%s##%zu", TagToString(ev.tag), i);
        if (ImGui::Selectable(label, selected,
              ImGuiSelectableFlags_SpanAllColumns)) {
          selected_event_ = static_cast<int>(i);
        }

        ImGui::TableNextColumn();
        if (ev.tag == mnexus::RenderStateEventTag::kPsoResolved) {
          ImGui::Text("0x%016zX", ev.pso_hash);
        }

        ImGui::TableNextColumn();
        if (ev.tag == mnexus::RenderStateEventTag::kPsoResolved) {
          if (ev.cache_hit) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Hit");
          } else {
            ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "Miss");
          }
        }
      }

      ImGui::EndTable();
    }

    // Detail pane for selected event.
    if (selected_event_ >= 0 &&
        selected_event_ < static_cast<int>(last_frame_events_.size())) {
      ImGui::Separator();
      ImGui::Text("State Snapshot (#%d: %s)", selected_event_,
        TagToString(last_frame_events_[selected_event_].tag));

      if (ImGui::BeginChild("##detail", ImVec2(0, 0), ImGuiChildFlags_None,
            ImGuiWindowFlags_HorizontalScrollbar)) {
        ShowSnapshotDetail(last_frame_events_[selected_event_].state);
      }
      ImGui::EndChild();
    }

    ImGui::End();
  }

  void Initialize(mnexus::IDevice* device, bool claim_font_atlas = true) override {
    device_ = device;
    clip_space_y_down_ = device->GetClipSpaceConvention().y_direction == mnexus::ClipSpaceYDirection::kDown;
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    // Upload font atlas texture only when this instance owns the
    // primary ImGui state. Secondary instances reuse whatever
    // TexID the primary already set on io.Fonts.
    if (claim_font_atlas) {
      unsigned char* pixels;
      int width, height;
      io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

      font_texture_ = device->CreateTexture(mnexus::TextureDesc {
        .usage = mnexus::TextureUsageFlagBits::kSampled | mnexus::TextureUsageFlagBits::kTransferDst,
        .format = mnexus::Format::kR8G8B8A8_UNORM,
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
      });

      uint32_t const data_size = static_cast<uint32_t>(width * height * 4);
      mnexus::BufferHandle staging = device->CreateBuffer(mnexus::BufferDesc {
        .usage = mnexus::BufferUsageFlagBits::kTransferSrc | mnexus::BufferUsageFlagBits::kTransferDst,
        .size_in_bytes = data_size,
      });

      device->QueueWriteBuffer({}, staging, 0, pixels, data_size);

      mnexus::ICommandList* cmd = device->CreateCommandList(mnexus::CommandListDesc {});

      mnexus::TextureSubresourceRange const font_range =
        mnexus::TextureSubresourceRange::SingleSubresourceColor(0, 0);

      cmd->TextureBarrier(
        font_texture_, font_range,
        mnexus::ResourceBarrierStageFlagBits::kTransfer,
        mnexus::ResourceBarrierState::kTransferDst
      );
      cmd->CopyBufferToTexture(
        staging, 0,
        font_texture_,
        font_range,
        mnexus::Extent3d { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 }
      );
      // Transition back to read-only so subsequent BindSampledTexture calls
      // see the texture in the expected state.
      cmd->TextureBarrier(
        font_texture_, font_range,
        mnexus::ResourceBarrierStageFlagBits::kFragmentShader,
        mnexus::ResourceBarrierState::kReadOnly
      );
      cmd->End();

      mnexus::IntraQueueSubmissionId submit_id = device->QueueSubmitCommandList({}, cmd);
      device->QueueWaitIdle({}, submit_id);

      device->DestroyBuffer(staging);
    }

    // Create font sampler.
    font_sampler_ = device->CreateSampler(mnexus::SamplerDesc {
      .min_filter = mnexus::Filter::kLinear,
      .mag_filter = mnexus::Filter::kLinear,
      .mipmap_filter = mnexus::Filter::kLinear,
    });

    if (claim_font_atlas) {
      io.Fonts->SetTexID(static_cast<ImTextureID>(font_texture_.Get()));
    }

    // Compile shader via batch API with caching.
    {
      char const* entry_points[] = { "vertex_main", "fragment_main" };

      AssetSlangCodeProvider code_provider;
      AssetSlangIncludeHandler include_handler;

      mslang_proxy::CompileBatchResult batch = mslang_proxy::CompileSlangToSpirvBatch(
        "", "imgui",
        &code_provider, &include_handler,
        "GLSL_150",
        std::span<char const* const>(entry_points, 2)
      );

      if (batch.spirv[0].has_value()) {
        vs_handle_ = device->CreateShaderModule(mnexus::ShaderModuleDesc {
          .source_language = mnexus::ShaderSourceLanguage::kSpirV,
          .code_ptr = reinterpret_cast<uint64_t>(batch.spirv[0]->data()),
          .code_size_in_bytes = static_cast<uint32_t>(batch.spirv[0]->size() * sizeof(uint32_t)),
        });
      }
      if (batch.spirv[1].has_value()) {
        fs_handle_ = device->CreateShaderModule(mnexus::ShaderModuleDesc {
          .source_language = mnexus::ShaderSourceLanguage::kSpirV,
          .code_ptr = reinterpret_cast<uint64_t>(batch.spirv[1]->data()),
          .code_size_in_bytes = static_cast<uint32_t>(batch.spirv[1]->size() * sizeof(uint32_t)),
        });
      }
    }

    if (!vs_handle_.IsValid() || !fs_handle_.IsValid()) {
      MBASE_LOG_ERROR("Failed to create imgui shader modules");
      return;
    }

    std::array<mnexus::ShaderModuleHandle, 2> shader_modules = { vs_handle_, fs_handle_ };
    program_handle_ = device->CreateProgram(
      mnexus::ProgramDesc {
        .shader_modules = shader_modules,
      }
    );

    if (!program_handle_.IsValid()) {
      MBASE_LOG_ERROR("Failed to create imgui program");
      return;
    }

    // Create uniform buffer for the view-projection matrix (float4x4 = 64 bytes).
    uniform_buffer_ = device->CreateBuffer(
      mnexus::BufferDesc {
        .usage = mnexus::BufferUsageFlagBits::kUniform,
        .size_in_bytes = 64,
      }
    );

    MBASE_LOG_INFO("ImguiRenderer initialized");
  }

  void Finalize(mnexus::IDevice* device) override {
    for (auto& dl : draw_list_buffers_) {
      if (dl.vertex_buffer.IsValid()) {
        device->DestroyBuffer(dl.vertex_buffer);
      }
      if (dl.index_buffer.IsValid()) {
        device->DestroyBuffer(dl.index_buffer);
      }
    }
    draw_list_buffers_.clear();

    if (font_sampler_.IsValid()) {
      device->DestroySampler(font_sampler_);
    }
    if (font_texture_.IsValid()) {
      device->DestroyTexture(font_texture_);
    }
    if (uniform_buffer_.IsValid()) {
      device->DestroyBuffer(uniform_buffer_);
    }
    if (program_handle_.IsValid()) {
      device->DestroyProgram(program_handle_);
    }
    if (fs_handle_.IsValid()) {
      device->DestroyShaderModule(fs_handle_);
    }
    if (vs_handle_.IsValid()) {
      device->DestroyShaderModule(vs_handle_);
    }
  }

  void UpdateGeometryBuffers(
    mnexus::IDevice* device,
    ImDrawData const* draw_data
  ) override {
    if (draw_data == nullptr || draw_data->CmdListsCount == 0) {
      return;
    }

    // Ensure we have enough draw list buffer entries.
    if (draw_list_buffers_.size() < static_cast<size_t>(draw_data->CmdListsCount)) {
      draw_list_buffers_.resize(draw_data->CmdListsCount);
    }

    for (int n = 0; n < draw_data->CmdListsCount; n++) {
      ImDrawList const* draw_list = draw_data->CmdLists[n];
      DrawListBuffers& buffers = draw_list_buffers_[n];

      uint32_t const vtx_size_raw = static_cast<uint32_t>(draw_list->VtxBuffer.Size) * sizeof(ImDrawVert);
      uint32_t const idx_size_raw = static_cast<uint32_t>(draw_list->IdxBuffer.Size) * sizeof(ImDrawIdx);

      // QueueWriteBuffer requires size to be a multiple of 4 bytes.
      uint32_t const vtx_size = (vtx_size_raw + 3u) & ~3u;
      uint32_t const idx_size = (idx_size_raw + 3u) & ~3u;

      if (vtx_size == 0 || idx_size == 0) {
        continue;
      }

      // Recreate vertex buffer if too small.
      if (buffers.vertex_buffer_size < vtx_size) {
        if (buffers.vertex_buffer.IsValid()) {
          device->DestroyBuffer(buffers.vertex_buffer);
        }
        buffers.vertex_buffer = device->CreateBuffer(
          mnexus::BufferDesc {
            .usage = mnexus::BufferUsageFlagBits::kVertex | mnexus::BufferUsageFlagBits::kTransferDst,
            .size_in_bytes = vtx_size,
          }
        );
        buffers.vertex_buffer_size = vtx_size;
      }

      // Recreate index buffer if too small.
      if (buffers.index_buffer_size < idx_size) {
        if (buffers.index_buffer.IsValid()) {
          device->DestroyBuffer(buffers.index_buffer);
        }
        buffers.index_buffer = device->CreateBuffer(
          mnexus::BufferDesc {
            .usage = mnexus::BufferUsageFlagBits::kIndex | mnexus::BufferUsageFlagBits::kTransferDst,
            .size_in_bytes = idx_size,
          }
        );
        buffers.index_buffer_size = idx_size;
      }

      // Upload data.
      device->QueueWriteBuffer(
        {}, buffers.vertex_buffer, 0,
        draw_list->VtxBuffer.Data, vtx_size
      );
      device->QueueWriteBuffer(
        {}, buffers.index_buffer, 0,
        draw_list->IdxBuffer.Data, idx_size
      );
    }
  }

  void Render(
    mnexus::ICommandList* command_list,
    mnexus::TextureHandle render_target,
    ImDrawData const* draw_data
  ) override {
    if (draw_data == nullptr || draw_data->CmdListsCount == 0) {
      return;
    }
    if (!program_handle_.IsValid()) {
      return;
    }

    // Enable PSO event log for this render pass.
    auto& event_log = command_list->GetStateEventLog();
    event_log.SetEnabled(true);
    event_log.Clear();

    // Build orthographic projection matrix.
    float const L = draw_data->DisplayPos.x;
    float const R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float const T = draw_data->DisplayPos.y;
    float const B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    // The base matrix targets Y-up clip space (WebGPU/D3D/OpenGL): screen
    // y=T (top) maps to NDC y=+1, screen y=B (bottom) maps to NDC y=-1.
    // For Y-down clip space (Vulkan), we negate the Y row so that
    // top->-1 and bottom->+1.
    float const y_sign = clip_space_y_down_ ? -1.0f : 1.0f;

    // Row-major float4x4 matching Slang's `mul(matrix, vector)` convention.
    float const view_projection[16] = {
      2.0f / (R - L),       0.0f,                          0.0f, (R + L) / (L - R),
      0.0f,                 y_sign * 2.0f / (T - B),       0.0f, y_sign * (T + B) / (B - T),
      0.0f,                 0.0f,                          0.5f, 0.5f,
      0.0f,                 0.0f,                          0.0f, 1.0f,
    };

    device_->QueueWriteBuffer(
      {}, uniform_buffer_, 0,
      view_projection, sizeof(view_projection)
    );

    command_list->PushDebugGroup("ImGui Render");

    // Begin render pass (load existing content, so we overlay on top of previous rendering).
    mnexus::TextureSubresourceRange const render_target_range =
      mnexus::TextureSubresourceRange::SingleSubresourceColor(0, 0);

    command_list->TextureBarrier(
      render_target, render_target_range,
      mnexus::ResourceBarrierStageFlagBits::kColorAttachmentOutput,
      mnexus::ResourceBarrierState::kAttachment
    );

    mnexus::ColorAttachmentDesc color_attachment {
      .texture = render_target,
      .subresource_range = render_target_range,
      .load_op = mnexus::LoadOp::kLoad,
      .store_op = mnexus::StoreOp::kStore,
    };

    command_list->BeginRenderPass(
      mnexus::RenderPassDesc {
        .color_attachments = color_attachment,
      }
    );

    // Bind program and set render state.
    command_list->BindRenderProgram(program_handle_);

    command_list->SetPrimitiveTopology(mnexus::PrimitiveTopology::kTriangleList);
    command_list->SetPolygonMode(mnexus::PolygonMode::kFill);
    command_list->SetCullMode(mnexus::CullMode::kNone);
    command_list->SetDepthTestEnabled(false);
    command_list->SetDepthWriteEnabled(false);

    // Alpha blending: srcAlpha, oneMinusSrcAlpha for color; one, oneMinusSrcAlpha for alpha.
    command_list->SetBlendEnabled(0, true);
    command_list->SetBlendFactors(
      0,
      mnexus::BlendFactor::kSrcAlpha,
      mnexus::BlendFactor::kOneMinusSrcAlpha,
      mnexus::BlendOp::kAdd,
      mnexus::BlendFactor::kOne,
      mnexus::BlendFactor::kOneMinusSrcAlpha,
      mnexus::BlendOp::kAdd
    );

    // Vertex input layout: ImDrawVert = { float2 pos, float2 uv, uint32 col }.
    static_assert(sizeof(ImDrawVert) == 20, "ImDrawVert size mismatch");
    static_assert(sizeof(ImDrawIdx) == sizeof(uint16_t), "ImDrawIdx size mismatch");

    mnexus::VertexInputBindingDesc binding {
      .binding = 0,
      .stride = sizeof(ImDrawVert),
      .step_mode = mnexus::VertexStepMode::kVertex,
    };
    std::array<mnexus::VertexInputAttributeDesc, 3> attributes = {{
      { .location = 0, .binding = 0, .format = mnexus::Format::kR32G32_SFLOAT,    .offset = offsetof(ImDrawVert, pos) },
      { .location = 1, .binding = 0, .format = mnexus::Format::kR32G32_SFLOAT,    .offset = offsetof(ImDrawVert, uv) },
      { .location = 2, .binding = 0, .format = mnexus::Format::kR8G8B8A8_UNORM,   .offset = offsetof(ImDrawVert, col) },
    }};

    command_list->SetVertexInputLayout(binding, attributes);

    // Bind uniform buffer (view-projection matrix).
    command_list->BindUniformBuffer(
      mnexus::BindingId { .group = 0, .binding = 0, .array_element = 0 },
      uniform_buffer_, 0, 64
    );

    // Bind font sampler (shared across all draw commands).
    command_list->BindSampler(
      mnexus::BindingId { .group = 0, .binding = 2, .array_element = 0 },
      font_sampler_
    );

    // Set viewport.
    command_list->SetViewport(
      0.0f, 0.0f,
      draw_data->DisplaySize.x * draw_data->FramebufferScale.x,
      draw_data->DisplaySize.y * draw_data->FramebufferScale.y,
      0.0f, 1.0f
    );

    // Draw each draw list.
    float const clip_off_x = draw_data->DisplayPos.x;
    float const clip_off_y = draw_data->DisplayPos.y;
    float const clip_scale_x = draw_data->FramebufferScale.x;
    float const clip_scale_y = draw_data->FramebufferScale.y;

    mnexus::TextureHandle bound_texture;

    for (int n = 0; n < draw_data->CmdListsCount; n++) {
      ImDrawList const* draw_list = draw_data->CmdLists[n];
      DrawListBuffers const& buffers = draw_list_buffers_[n];

      if (!buffers.vertex_buffer.IsValid() || !buffers.index_buffer.IsValid()) {
        continue;
      }

      command_list->BindVertexBuffer(0, buffers.vertex_buffer, 0);
      command_list->BindIndexBuffer(buffers.index_buffer, 0, mnexus::IndexType::kUint16);

      for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++) {
        ImDrawCmd const& draw_cmd = draw_list->CmdBuffer[cmd_i];

        if (draw_cmd.UserCallback != nullptr) {
          draw_cmd.UserCallback(draw_list, &draw_cmd);
          continue;
        }

        // Bind texture (skip redundant binds).
        mnexus::TextureHandle tex(static_cast<uint64_t>(draw_cmd.GetTexID()));
        if (tex != bound_texture) {
          bound_texture = tex;
          command_list->BindSampledTexture(
            mnexus::BindingId { .group = 0, .binding = 1, .array_element = 0 },
            tex,
            mnexus::TextureSubresourceRange::SingleSubresourceColor(0, 0)
          );
        }

        // Clip rectangle.
        float const clip_min_x = (draw_cmd.ClipRect.x - clip_off_x) * clip_scale_x;
        float const clip_min_y = (draw_cmd.ClipRect.y - clip_off_y) * clip_scale_y;
        float const clip_max_x = (draw_cmd.ClipRect.z - clip_off_x) * clip_scale_x;
        float const clip_max_y = (draw_cmd.ClipRect.w - clip_off_y) * clip_scale_y;

        if (clip_max_x <= clip_min_x || clip_max_y <= clip_min_y) {
          continue;
        }

        command_list->SetScissor(
          static_cast<int32_t>(clip_min_x),
          static_cast<int32_t>(clip_min_y),
          static_cast<uint32_t>(clip_max_x - clip_min_x),
          static_cast<uint32_t>(clip_max_y - clip_min_y)
        );

        command_list->DrawIndexed(
          draw_cmd.ElemCount,
          1,
          draw_cmd.IdxOffset,
          static_cast<int32_t>(draw_cmd.VtxOffset),
          0
        );
      }
    }

    command_list->EndRenderPass();
    command_list->PopDebugGroup();

    // Collect events for display in next frame's Tick().
    last_frame_events_.clear();
    last_frame_events_.reserve(event_log.GetCount());
    for (uint32_t i = 0; i < event_log.GetCount(); ++i) {
      last_frame_events_.push_back(event_log.GetEvent(i));
    }
    event_log.SetEnabled(false);
  }

private:
  mnexus::IDevice* device_ = nullptr;
  bool clip_space_y_down_ = false;

  mnexus::ShaderModuleHandle vs_handle_;
  mnexus::ShaderModuleHandle fs_handle_;
  mnexus::ProgramHandle program_handle_;
  mnexus::BufferHandle uniform_buffer_;
  mnexus::TextureHandle font_texture_;
  mnexus::SamplerHandle font_sampler_;

  std::vector<DrawListBuffers> draw_list_buffers_;

  // PSO event log state.
  std::vector<mnexus::RenderStateEvent> last_frame_events_;
  int selected_event_ = -1;
};

std::unique_ptr<ImguiRenderer> ImguiRenderer::Create() {
  return std::make_unique<ImguiRendererImpl>();
}

} // namespace mshell
