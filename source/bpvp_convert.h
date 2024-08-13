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

    std::vector<Note> getNotes(std::vector<float> const& frames, std::vector<float> const& onsets, size_t numFrames, size_t numNotes, bool inferOnsets, float frameEnergyThreshold, float onsetEnergyThreshold, long minNoteLength, long maxFramesBelowThreshold = 11, std::optional<float> const maxFreq = {}, std::optional<float> const minFreq = {}, bool melodiaTrick = true);
} // namespace Bpvp
