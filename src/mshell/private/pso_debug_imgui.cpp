// TU header --------------------------------------------
#include "pso_debug_imgui.h"

// c++ headers ------------------------------------------
#include <string>

// external headers -------------------------------------
#include "imgui.h"

// public project headers -------------------------------
#include "mnexus/public/mnexus.h"

namespace mshell {

void ShowSnapshotDetail(mnexus::RenderPipelineStateSnapshot const& s) {
  ImGui::Text("Program: 0x%016llX", static_cast<unsigned long long>(s.program.Get()));

  ImGui::SeparatorText("Rasterization");
  ImGui::Text("Topology:    %.*s",
    static_cast<int>(mnexus::ToString(s.primitive_topology).size()),
    mnexus::ToString(s.primitive_topology).data());
  ImGui::Text("PolygonMode: %.*s",
    static_cast<int>(mnexus::ToString(s.polygon_mode).size()),
    mnexus::ToString(s.polygon_mode).data());
  ImGui::Text("CullMode:    %.*s",
    static_cast<int>(mnexus::ToString(s.cull_mode).size()),
    mnexus::ToString(s.cull_mode).data());
  ImGui::Text("FrontFace:   %.*s",
    static_cast<int>(mnexus::ToString(s.front_face).size()),
    mnexus::ToString(s.front_face).data());

  ImGui::SeparatorText("Depth / Stencil");
  ImGui::Text("DepthTest:    %s", s.depth_test_enabled ? "true" : "false");
  ImGui::Text("DepthWrite:   %s", s.depth_write_enabled ? "true" : "false");
  ImGui::Text("DepthCompare: %.*s",
    static_cast<int>(mnexus::ToString(s.depth_compare_op).size()),
    mnexus::ToString(s.depth_compare_op).data());
  ImGui::Text("StencilTest:  %s", s.stencil_test_enabled ? "true" : "false");
  if (s.stencil_test_enabled) {
    ImGui::Text("  Front: fail=%.*s pass=%.*s depthFail=%.*s compare=%.*s",
      static_cast<int>(mnexus::ToString(s.stencil_front_fail_op).size()),
      mnexus::ToString(s.stencil_front_fail_op).data(),
      static_cast<int>(mnexus::ToString(s.stencil_front_pass_op).size()),
      mnexus::ToString(s.stencil_front_pass_op).data(),
      static_cast<int>(mnexus::ToString(s.stencil_front_depth_fail_op).size()),
      mnexus::ToString(s.stencil_front_depth_fail_op).data(),
      static_cast<int>(mnexus::ToString(s.stencil_front_compare_op).size()),
      mnexus::ToString(s.stencil_front_compare_op).data());
    ImGui::Text("  Back:  fail=%.*s pass=%.*s depthFail=%.*s compare=%.*s",
      static_cast<int>(mnexus::ToString(s.stencil_back_fail_op).size()),
      mnexus::ToString(s.stencil_back_fail_op).data(),
      static_cast<int>(mnexus::ToString(s.stencil_back_pass_op).size()),
      mnexus::ToString(s.stencil_back_pass_op).data(),
      static_cast<int>(mnexus::ToString(s.stencil_back_depth_fail_op).size()),
      mnexus::ToString(s.stencil_back_depth_fail_op).data(),
      static_cast<int>(mnexus::ToString(s.stencil_back_compare_op).size()),
      mnexus::ToString(s.stencil_back_compare_op).data());
  }

  ImGui::SeparatorText("Blend");
  for (size_t i = 0; i < s.attachments.size(); ++i) {
    auto const& a = s.attachments[i];
    std::string mask_str = mnexus::ToString(a.write_mask);
    if (a.blend_enabled) {
      ImGui::Text("[%zu] ON  src=%.*s dst=%.*s op=%.*s | srcA=%.*s dstA=%.*s opA=%.*s  mask=%s",
        i,
        static_cast<int>(mnexus::ToString(a.src_color).size()),
        mnexus::ToString(a.src_color).data(),
        static_cast<int>(mnexus::ToString(a.dst_color).size()),
        mnexus::ToString(a.dst_color).data(),
        static_cast<int>(mnexus::ToString(a.color_op).size()),
        mnexus::ToString(a.color_op).data(),
        static_cast<int>(mnexus::ToString(a.src_alpha).size()),
        mnexus::ToString(a.src_alpha).data(),
        static_cast<int>(mnexus::ToString(a.dst_alpha).size()),
        mnexus::ToString(a.dst_alpha).data(),
        static_cast<int>(mnexus::ToString(a.alpha_op).size()),
        mnexus::ToString(a.alpha_op).data(),
        mask_str.c_str());
    } else {
      ImGui::Text("[%zu] OFF  mask=%s", i, mask_str.c_str());
    }
  }

  ImGui::SeparatorText("Vertex Input");
  for (size_t i = 0; i < s.vertex_bindings.size(); ++i) {
    auto const& b = s.vertex_bindings[i];
    ImGui::Text("Binding %u: stride=%u step=%.*s",
      b.binding, b.stride,
      static_cast<int>(mnexus::ToString(b.step_mode).size()),
      mnexus::ToString(b.step_mode).data());
  }
  for (size_t i = 0; i < s.vertex_attributes.size(); ++i) {
    auto const& a = s.vertex_attributes[i];
    auto fmt_sv = mnexus::ToString(static_cast<MnFormat>(a.format));
    ImGui::Text("  loc=%u bind=%u fmt=%.*s off=%u",
      a.location, a.binding,
      static_cast<int>(fmt_sv.size()), fmt_sv.data(),
      a.offset);
  }

  ImGui::SeparatorText("Render Target");
  for (size_t i = 0; i < s.color_formats.size(); ++i) {
    auto fmt_sv = mnexus::ToString(static_cast<MnFormat>(s.color_formats[i]));
    ImGui::Text("Color[%zu]: %.*s", i, static_cast<int>(fmt_sv.size()), fmt_sv.data());
  }
  {
    auto fmt_sv = mnexus::ToString(static_cast<MnFormat>(s.depth_stencil_format));
    ImGui::Text("DepthStencil: %.*s", static_cast<int>(fmt_sv.size()), fmt_sv.data());
  }
  ImGui::Text("SampleCount: %u", s.sample_count);
}

} // namespace mshell
