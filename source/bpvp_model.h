#pragma once
#include <cstddef>
#include <cstring>

namespace Bpvp
{
    extern const void* model;
    extern const size_t model_size;

    static auto constexpr modelSampleRate = 22050;
    static auto constexpr modelBlockSize = 43844;
    static auto constexpr modelNumFrames = 172;
    static auto constexpr modelNumNotes = 88;
    static auto constexpr modelNoteOffset = 21;
    static auto constexpr modelTensorSize = modelNumFrames * modelNumNotes;

} // namespace Bpvp
