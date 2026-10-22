#pragma once

namespace frame_stage {
// FrameStageNotify (VClient018 slot 37) running original stage-2 handler 100900 (knife model changer).
bool install() noexcept;
// Original visual feature roots (ESP etc.), called from the menu paint before the menu frame.
void run_visuals() noexcept;
}
