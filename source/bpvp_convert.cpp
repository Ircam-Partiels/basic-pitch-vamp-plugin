#include "bpvp_convert.h"
#include "bpvp_model.h"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace Bpvp
{
    static float hertzToMidi(float freq)
    {
        return 69.0f + 12.0f * std::log2(freq / 440.0f);
    }

    static float midiToHertz(float midi)
    {
        return 440.0f * std::pow(2.0, (midi - 69.0f) / 12.0f);
    }

    static constexpr double frameToSeconds(auto frame)
    {
        return static_cast<double>(frame) / static_cast<double>(modelNumFrames) * modelBlockDuration;
    }

    static constexpr long secondsToFrame(auto seconds)
    {
        return static_cast<long>(std::round(seconds / modelBlockDuration * static_cast<double>(modelNumFrames)));
    }

    static std::vector<float> getInferredOnsets(std::vector<float> const& onsets, std::vector<float> const& frames, size_t numFrames, size_t numNotes, size_t numDiff = 2)
    {
        std::vector<float> notesDiff(numFrames * numNotes, 1.0f);
        auto maxDiff = 0.0f;
        auto maxOnset = 0.0f;
        for(size_t diff = 1; diff <= numDiff; ++diff)
        {
            for(size_t frame = 0; frame < numFrames; ++frame)
            {
                for(size_t note = 0; note < numNotes; ++note)
                {
                    auto const currentEnergy = frames.at(frame * numNotes + note);
                    auto const previousEnergy = (frame >= diff) ? frames.at((frame - diff) * numNotes + note) : 0.0f;
                    auto const diffEnergy = std::max(currentEnergy - previousEnergy, 0.0f);
                    notesDiff[frame * numNotes + note] = std::min((frame >= numDiff) ? diffEnergy : 0.0f, notesDiff.at(frame * numNotes + note));

                    maxOnset = std::max(onsets.at(frame * numNotes + note), maxOnset);
                    maxDiff = std::max(notesDiff.at(frame * numNotes + note), maxDiff);
                }
            }
        }

        auto const ratio = maxDiff >= 0.0f ? maxOnset / maxDiff : 0.0f;
        std::transform(onsets.cbegin(), onsets.cend(), notesDiff.cbegin(), notesDiff.begin(), [&](auto const& lhs, auto const& rhs)
                       {
                           return std::max(lhs, rhs * ratio);
                       });
        return notesDiff;
    }

    std::vector<Note> getNotes(std::vector<float> const& currentFrames, std::vector<float> const& currentOnsets, size_t numFrames, size_t numNotes, bool inferOnsets, float frameEnergyThreshold, float onsetEnergyThreshold, double minNoteDuration, long maxFramesBelowThreshold, std::optional<float> const maxFreq, std::optional<float> const minFreq, bool melodiaTrick)
    {
        std::vector<Note> notes;
        auto onsets = inferOnsets ? getInferredOnsets(currentOnsets, currentFrames, numFrames, numNotes) : currentOnsets;
        auto frames = currentFrames;

        auto const minNoteLength = secondsToFrame(minNoteDuration);
        auto const lastFrameIndex = numFrames - 1;
        auto const maxNoteIndex = maxFreq.has_value() ? static_cast<size_t>(std::round(hertzToMidi(maxFreq.value())) - modelNoteOffset) : numNotes;
        auto const minNoteIndex = minFreq.has_value() ? static_cast<size_t>(std::round(hertzToMidi(minFreq.value())) - modelNoteOffset) : size_t(0);

        for(long frameStartIndex = static_cast<long>(lastFrameIndex) - 1; frameStartIndex >= 0; --frameStartIndex)
        {
            for(long noteIndex = static_cast<long>(maxNoteIndex) - 1; noteIndex >= static_cast<long>(minNoteIndex); noteIndex--)
            {
                auto const currentOnset = onsets.at(static_cast<size_t>(frameStartIndex) * numNotes + static_cast<size_t>(noteIndex));
                auto const previousOnset = (frameStartIndex <= 0) ? currentOnset : onsets.at(static_cast<size_t>(frameStartIndex - 1) * numNotes + static_cast<size_t>(noteIndex));
                auto const nextOnset = (frameStartIndex >= static_cast<long>(lastFrameIndex)) ? currentOnset : onsets.at(static_cast<size_t>(frameStartIndex + 1) * numNotes + static_cast<size_t>(noteIndex));
                if(currentOnset >= onsetEnergyThreshold && currentOnset >= previousOnset && currentOnset >= nextOnset)
                {
                    auto frameEndIndex = frameStartIndex + 1;
                    auto accumulatedFrames = 0;
                    while(frameEndIndex < lastFrameIndex && accumulatedFrames < maxFramesBelowThreshold)
                    {
                        auto const energy = frames.at(static_cast<size_t>(frameEndIndex) * numNotes + static_cast<size_t>(noteIndex));
                        accumulatedFrames = energy < frameEnergyThreshold ? accumulatedFrames + 1 : 0;
                        ++frameEndIndex;
                    }
                    frameEndIndex -= accumulatedFrames;
                    auto const frameDuration = frameEndIndex - frameStartIndex;
                    if(frameDuration > minNoteLength)
                    {
                        auto amplitude = 0.0;
                        for(auto f = frameStartIndex; f < frameEndIndex; f++)
                        {
                            auto const index = static_cast<size_t>(f) * numNotes + static_cast<size_t>(noteIndex);
                            frames[index] = 0.0f;
                            if(noteIndex < numNotes - 1)
                            {
                                frames[index + 1] = 0.0f;
                            }
                            if(noteIndex > 0)
                            {
                                frames[index - 1] = 0.0f;
                            }
                            amplitude += static_cast<double>(currentFrames.at(index));
                        }
                        amplitude /= static_cast<double>(frameDuration);
                        notes.push_back({frameToSeconds(frameStartIndex), frameToSeconds(frameEndIndex), midiToHertz(noteIndex + modelNoteOffset), static_cast<float>(amplitude)});
                    }
                }
            }
        }

        if(melodiaTrick)
        {
            for(long frameIndex = static_cast<long>(lastFrameIndex) - 1; frameIndex >= 0; --frameIndex)
            {
                for(long noteIndex = static_cast<long>(maxNoteIndex) - 1; noteIndex >= static_cast<long>(minNoteIndex); noteIndex--)
                {
                    auto const index = static_cast<size_t>(frameIndex) * numNotes + noteIndex;
                    auto const energy = frames.at(index);
                    if(energy > frameEnergyThreshold)
                    {
                        frames[index] = 0.0f;
                        auto frameEndIndex = frameIndex + 1;
                        {
                            auto accumulatedFrames = 0;
                            while(frameEndIndex < lastFrameIndex && accumulatedFrames < maxFramesBelowThreshold)
                            {
                                auto const cenergy = frames.at(static_cast<size_t>(frameEndIndex) * numNotes + static_cast<size_t>(noteIndex));
                                accumulatedFrames = cenergy < frameEnergyThreshold ? accumulatedFrames + 1 : 0;

                                frames[static_cast<size_t>(frameEndIndex) * numNotes + static_cast<size_t>(noteIndex)] = 0.0f;
                                if(noteIndex < numNotes - 1)
                                {
                                    frames[static_cast<size_t>(frameEndIndex) * numNotes + static_cast<size_t>(noteIndex) + 1] = 0.0f;
                                }
                                if(noteIndex > 0)
                                {
                                    frames[static_cast<size_t>(frameEndIndex) * numNotes + static_cast<size_t>(noteIndex) - 1] = 0.0f;
                                }
                                ++frameEndIndex;
                            }
                            frameEndIndex -= (accumulatedFrames + 1);
                        }

                        auto frameStartIndex = frameIndex - 1;
                        {
                            auto accumulatedFrames = 0;
                            while(frameStartIndex > 0 && accumulatedFrames < maxFramesBelowThreshold)
                            {
                                auto const cenergy = frames.at(static_cast<size_t>(frameStartIndex) * numNotes + static_cast<size_t>(noteIndex));
                                accumulatedFrames = cenergy < frameEnergyThreshold ? accumulatedFrames + 1 : 0;

                                frames[static_cast<size_t>(frameStartIndex) * numNotes + static_cast<size_t>(noteIndex)] = 0.0f;
                                if(noteIndex < numNotes - 1)
                                {
                                    frames[static_cast<size_t>(frameStartIndex) * numNotes + static_cast<size_t>(noteIndex) + 1] = 0.0f;
                                }
                                if(noteIndex > 0)
                                {
                                    frames[static_cast<size_t>(frameStartIndex) * numNotes + static_cast<size_t>(noteIndex) - 1] = 0.0f;
                                }
                                --frameStartIndex;
                            }

                            frameStartIndex += (accumulatedFrames + 1);
                        }

                        assert(frameStartIndex >= 0);
                        assert(frameEndIndex < numFrames);

                        auto const frameDuration = frameEndIndex - frameStartIndex;
                        if(frameDuration > minNoteLength)
                        {
                            auto amplitude = 0.0;
                            for(auto f = frameStartIndex; f < frameEndIndex; f++)
                            {
                                auto const cindex = static_cast<size_t>(f) * numNotes + static_cast<size_t>(noteIndex);
                                amplitude += static_cast<double>(currentFrames.at(cindex));
                            }
                            amplitude /= static_cast<double>(frameDuration);
                            notes.push_back({frameToSeconds(frameStartIndex), frameToSeconds(frameEndIndex), midiToHertz(noteIndex + modelNoteOffset), static_cast<float>(amplitude)});
                        }
                    }
                }
            }
        }
        std::sort(notes.begin(), notes.end(), [](auto const& lhs, auto const& rhs)
                  {
                      return lhs.start < rhs.start || (std::abs(lhs.start - rhs.start) < std::numeric_limits<double>::epsilon() && lhs.end < rhs.end);
                  });

        if(!notes.empty())
        {
            auto next = std::next(notes.begin());
            while(next != notes.end())
            {
                auto it = std::prev(next);
                if(next->start < it->end)
                {
                    if(next->pitch < it->pitch)
                    {
                        next = notes.erase(it);
                    }
                    else
                    {
                        next = notes.erase(next);
                    }
                }
                else
                {
                    next = std::next(next);
                }
            }
        }
        return notes;
    }
} // namespace Bpvp
