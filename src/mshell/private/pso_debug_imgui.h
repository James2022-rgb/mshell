#pragma once

// public project headers -------------------------------
#include "mnexus/public/render_pipeline_state_snapshot.h"

namespace mshell {

/// Renders a detailed view of a render pipeline state snapshot using Dear ImGui.
void ShowSnapshotDetail(mnexus::RenderPipelineStateSnapshot const& snapshot);

} // namespace mshell
