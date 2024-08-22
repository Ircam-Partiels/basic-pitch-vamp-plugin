#pragma once

#include "bpvp_model.h"
#include <array>
#include <cmath>
#include <functional>
#include <optional>
#include <vector>

namespace Bpvp
{
    struct Note
    {
        double start;
        double end;
        float pitch;
        float amplitude;
    };

    std::vector<Note> getNotes(std::vector<std::array<float, modelNumNotes>> const& frames, std::vector<std::array<float, modelNumNotes>> const& onsets, bool inferOnsets, size_t voiceIndex, float frameEnergyThreshold, float onsetEnergyThreshold, double minNoteDuration, long maxFramesBelowThreshold, std::optional<float> const minFreq = {}, std::optional<float> const maxFreq = {}, bool melodiaTrick = true);
} // namespace Bpvp
