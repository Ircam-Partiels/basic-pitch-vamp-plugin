#pragma once

#include "bpvp.h"
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

    // Returns the time in seconds of a frame of the unwrapped model output
    // (each model block contributes modelNumValidFrames frames and the blocks are spaced by modelBlockHop samples)
    double frameToSeconds(size_t frame);

    // Port of basic_pitch.note_creation.output_to_notes_polyphonic, the overlapping notes with the same pitch are merged
    // and the notes are split into monophonic voices preserving the pitch order (when two notes overlap, the lower note is
    // in a lower voice, 0 being the lowest) to return the notes of the first maxVoices voices (the notes of the higher
    // voices are ignored)
    std::vector<std::vector<Note>> getNotes(std::vector<std::array<float, modelNumNotes>> const& frames, std::vector<std::array<float, modelNumNotes>> const& onsets, bool inferOnsets, size_t maxVoices, float frameEnergyThreshold, float onsetEnergyThreshold, double minNoteDuration, long maxFramesBelowThreshold, std::optional<float> const minFreq = {}, std::optional<float> const maxFreq = {}, bool melodiaTrick = true);
} // namespace Bpvp
