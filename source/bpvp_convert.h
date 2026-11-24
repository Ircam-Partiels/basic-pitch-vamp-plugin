// This file contains code ported from the Basic Pitch Python library
// (https://github.com/spotify/basic-pitch, v0.4.0, basic_pitch/note_creation.py
// and basic_pitch/inference.py):
//
// Copyright 2022 Spotify AB
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Modified by Pierre Guillot (IRCAM): translated from Python to C++ and adapted
// to the Basic Pitch Vamp Plugin.

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
