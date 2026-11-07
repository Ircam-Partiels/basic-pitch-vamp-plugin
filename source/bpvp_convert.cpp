#include "bpvp_convert.h"
#include "bpvp.h"
#include <algorithm>
#include <cassert>
#include <limits>
#include <map>
#include <tuple>

namespace Bpvp
{
    using Matrix = std::vector<std::array<float, modelNumNotes>>;

    static float hertzToMidi(float freq)
    {
        return 69.0f + 12.0f * std::log2(freq / 440.0f);
    }

    static float midiToHertz(float midi)
    {
        return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
    }

    // basic_pitch.inference.predict: round(minimum_note_length * AUDIO_SAMPLE_RATE / FFT_HOP)
    static long secondsToFrames(double seconds)
    {
        return std::lround(seconds * static_cast<double>(modelSampleRate) / static_cast<double>(modelFFTHope));
    }

    // basic_pitch.note_creation.constrain_frequency: index of the note bin for a frequency
    static size_t frequencyToNoteIndex(float freq)
    {
        auto const index = std::lround(hertzToMidi(freq)) - static_cast<long>(modelNoteOffset);
        return static_cast<size_t>(std::clamp(index, 0L, static_cast<long>(modelNumNotes)));
    }

    // basic_pitch.note_creation.get_infered_onsets
    static Matrix getInferredOnsets(Matrix const& onsets, Matrix const& frames, size_t numDiff = 2)
    {
        Matrix framesDiff(frames.size()); // value-initialized to zero (the first numDiff frames stay at zero)
        for(size_t frame = numDiff; frame < frames.size(); ++frame)
        {
            for(size_t note = 0; note < modelNumNotes; ++note)
            {
                auto minDiff = std::numeric_limits<float>::max();
                for(size_t diff = 1; diff <= numDiff; ++diff)
                {
                    minDiff = std::min(minDiff, frames[frame][note] - frames[frame - diff][note]);
                }
                framesDiff[frame][note] = std::max(minDiff, 0.0f);
            }
        }

        auto maxDiff = 0.0f;
        auto maxOnset = 0.0f;
        for(size_t frame = 0; frame < frames.size(); ++frame)
        {
            maxDiff = std::max(maxDiff, *std::max_element(framesDiff[frame].cbegin(), framesDiff[frame].cend()));
            maxOnset = std::max(maxOnset, *std::max_element(onsets[frame].cbegin(), onsets[frame].cend()));
        }

        // rescale the differences to have the same max as the onsets and use the max of both
        auto const ratio = maxDiff > 0.0f ? maxOnset / maxDiff : 0.0f;
        for(size_t frame = 0; frame < frames.size(); ++frame)
        {
            std::transform(onsets[frame].cbegin(), onsets[frame].cend(), framesDiff[frame].cbegin(), framesDiff[frame].begin(), [&](auto const onset, auto const diff)
                           {
                               return std::max(onset, diff * ratio);
                           });
        }
        return framesDiff;
    }

    double frameToSeconds(size_t frame)
    {
        auto const block = frame / static_cast<size_t>(modelNumValidFrames);
        auto const index = frame % static_cast<size_t>(modelNumValidFrames);
        auto const sample = block * static_cast<size_t>(modelBlockHop) + index * static_cast<size_t>(modelFFTHope);
        return static_cast<double>(sample) / static_cast<double>(modelSampleRate);
    }

    std::vector<std::vector<Note>> getNotes(Matrix const& inputFrames, Matrix const& inputOnsets, bool inferOnsets, size_t maxVoices, float frameEnergyThreshold, float onsetEnergyThreshold, double minNoteDuration, long maxFramesBelowThreshold, std::optional<float> const minFreq, std::optional<float> const maxFreq, bool melodiaTrick)
    {
        auto const numFrames = std::min(inputFrames.size(), inputOnsets.size());
        if(numFrames == 0)
        {
            return {};
        }
        auto const lastFrameIndex = numFrames - 1;
        auto const minNoteLength = secondsToFrames(minNoteDuration);
        auto const minNoteIndex = minFreq.has_value() ? frequencyToNoteIndex(minFreq.value()) : size_t(0);
        auto const maxNoteIndex = std::max(minNoteIndex, maxFreq.has_value() ? frequencyToNoteIndex(maxFreq.value()) : size_t(modelNumNotes));

        // constrain_frequency: zero out the bins outside of the [minNoteIndex, maxNoteIndex) range
        auto const constrain = [&](Matrix& matrix)
        {
            for(auto& frame : matrix)
            {
                std::fill(frame.begin(), std::next(frame.begin(), static_cast<long>(minNoteIndex)), 0.0f);
                std::fill(std::next(frame.begin(), static_cast<long>(maxNoteIndex)), frame.end(), 0.0f);
            }
        };
        Matrix frames(inputFrames.cbegin(), std::next(inputFrames.cbegin(), static_cast<long>(numFrames)));
        Matrix onsets(inputOnsets.cbegin(), std::next(inputOnsets.cbegin(), static_cast<long>(numFrames)));
        constrain(frames);
        constrain(onsets);
        if(inferOnsets)
        {
            onsets = getInferredOnsets(onsets, frames);
        }

        // «frames» keeps the (constrained) amplitudes used to compute the note amplitudes,
        // «energy» is the remaining energy consumed by the note extraction
        auto energy = frames;
        auto const clearEnergy = [&](size_t frame, size_t note)
        {
            auto& cframe = energy[frame];
            cframe[note] = 0.0f;
            if(note < modelNumNotes - 1)
            {
                cframe[note + 1] = 0.0f;
            }
            if(note > 0)
            {
                cframe[note - 1] = 0.0f;
            }
        };

        std::vector<Note> notes;
        auto const addNote = [&](size_t start, size_t end, size_t note)
        {
            assert(end > start);
            auto amplitude = 0.0;
            for(auto frame = start; frame < end; ++frame)
            {
                amplitude += static_cast<double>(frames[frame][note]);
            }
            amplitude /= static_cast<double>(end - start);
            notes.push_back({frameToSeconds(start), frameToSeconds(end), midiToHertz(static_cast<float>(note + modelNoteOffset)), static_cast<float>(amplitude)});
        };

        // Loop over the onsets that are strict local maxima along the time axis (scipy.signal.argrelmax,
        // so the first and the last frames are never peaks), backwards in time then from high to low notes
        for(auto frameIndex = static_cast<long>(numFrames) - 2; frameIndex >= 1; --frameIndex)
        {
            auto const start = static_cast<size_t>(frameIndex);
            for(auto note = maxNoteIndex; note-- > minNoteIndex;)
            {
                auto const onset = onsets[start][note];
                if(onset < onsetEnergyThreshold || onset <= onsets[start - 1][note] || onset <= onsets[start + 1][note])
                {
                    continue;
                }

                // find the frame where the energy drops below the threshold for too long
                auto end = start + 1;
                long numFramesBelow = 0;
                while(end < lastFrameIndex && numFramesBelow < maxFramesBelowThreshold)
                {
                    numFramesBelow = energy[end][note] < frameEnergyThreshold ? numFramesBelow + 1 : 0;
                    ++end;
                }
                end -= static_cast<size_t>(numFramesBelow);

                if(static_cast<long>(end - start) <= minNoteLength)
                {
                    continue;
                }
                for(auto frame = start; frame < end; ++frame)
                {
                    clearEnergy(frame, note);
                }
                addNote(start, end, note);
            }
        }

        // Melodia trick: repeatedly take the cell with the maximum remaining energy and extend it forward
        // and backward while the energy doesn't stay below the threshold for too long.
        // The energy only decreases (cells are set to zero) so a max-heap with lazy invalidation gives the
        // same picking order as numpy.argmax (max energy, then first frame, then first note).
        if(melodiaTrick)
        {
            struct Cell
            {
                float energy;
                size_t frame;
                size_t note;
            };
            auto const lowerPriority = [](Cell const& lhs, Cell const& rhs)
            {
                if(lhs.energy != rhs.energy)
                {
                    return lhs.energy < rhs.energy;
                }
                if(lhs.frame != rhs.frame)
                {
                    return lhs.frame > rhs.frame;
                }
                return lhs.note > rhs.note;
            };

            std::vector<Cell> cells;
            for(size_t frame = 0; frame < numFrames; ++frame)
            {
                for(auto note = minNoteIndex; note < maxNoteIndex; ++note)
                {
                    if(energy[frame][note] > frameEnergyThreshold)
                    {
                        cells.push_back({energy[frame][note], frame, note});
                    }
                }
            }
            std::make_heap(cells.begin(), cells.end(), lowerPriority);

            while(!cells.empty())
            {
                std::pop_heap(cells.begin(), cells.end(), lowerPriority);
                auto const cell = cells.back();
                cells.pop_back();
                if(energy[cell.frame][cell.note] != cell.energy)
                {
                    continue; // already consumed by another note
                }

                auto const middle = cell.frame;
                auto const note = cell.note;
                energy[middle][note] = 0.0f;

                // forward pass
                auto forward = middle + 1;
                long numFramesBelow = 0;
                while(forward < lastFrameIndex && numFramesBelow < maxFramesBelowThreshold)
                {
                    numFramesBelow = energy[forward][note] < frameEnergyThreshold ? numFramesBelow + 1 : 0;
                    clearEnergy(forward, note);
                    ++forward;
                }
                auto const end = forward - 1 - static_cast<size_t>(numFramesBelow);

                // backward pass
                auto backward = static_cast<long>(middle) - 1;
                numFramesBelow = 0;
                while(backward > 0 && numFramesBelow < maxFramesBelowThreshold)
                {
                    numFramesBelow = energy[static_cast<size_t>(backward)][note] < frameEnergyThreshold ? numFramesBelow + 1 : 0;
                    clearEnergy(static_cast<size_t>(backward), note);
                    --backward;
                }
                auto const start = static_cast<size_t>(backward + 1 + numFramesBelow);

                assert(start <= middle && middle <= end && end < numFrames);
                if(static_cast<long>(end - start) <= minNoteLength)
                {
                    continue;
                }
                addNote(start, end, note);
            }
        }

        // Merge the overlapping notes with the same pitch
        std::sort(notes.begin(), notes.end(), [](auto const& lhs, auto const& rhs)
                  {
                      return std::tie(lhs.start, lhs.pitch) < std::tie(rhs.start, rhs.pitch);
                  });
        for(auto it = notes.begin(); it != notes.end(); ++it)
        {
            auto next = std::next(it);
            while(next != notes.end() && next->start < it->end)
            {
                if(next->pitch == it->pitch)
                {
                    it->end = std::max(it->end, next->end);
                    next = notes.erase(next);
                }
                else
                {
                    ++next;
                }
            }
        }

        // Split the notes into monophonic voices that preserve the pitch order: the voice of a note is one above the
        // highest voice of the lower notes that overlap it (or 0 if there is none), so when two notes overlap, the
        // lower note is always in a lower voice. The notes are processed from the lowest to the highest pitch, and
        // since the overlapping notes with the same pitch have been merged, the notes of a pitch are disjoint and
        // sorted by start (so are their ends). Only the notes of the first maxVoices voices are returned.
        std::sort(notes.begin(), notes.end(), [](auto const& lhs, auto const& rhs)
                  {
                      return std::tie(lhs.pitch, lhs.start) < std::tie(rhs.pitch, rhs.start);
                  });
        struct VoiceNote
        {
            double start;
            double end;
            size_t voice;
        };
        std::map<float, std::vector<VoiceNote>> pitchNotes;
        std::vector<std::vector<Note>> voiceNotes(maxVoices);
        for(auto const& note : notes)
        {
            size_t voice = 0;
            for(auto const& [pitch, lowerNotes] : pitchNotes)
            {
                auto it = std::partition_point(lowerNotes.cbegin(), lowerNotes.cend(), [&](auto const& lowerNote)
                                               {
                                                   return lowerNote.end <= note.start;
                                               });
                for(; it != lowerNotes.cend() && it->start < note.end; ++it)
                {
                    voice = std::max(voice, it->voice + 1);
                }
            }
            pitchNotes[note.pitch].push_back({note.start, note.end, voice});
            if(voice < maxVoices)
            {
                voiceNotes[voice].push_back(note);
            }
        }
        for(auto& notesOfVoice : voiceNotes)
        {
            std::sort(notesOfVoice.begin(), notesOfVoice.end(), [](auto const& lhs, auto const& rhs)
                      {
                          return lhs.start < rhs.start;
                      });
        }
        return voiceNotes;
    }
} // namespace Bpvp
