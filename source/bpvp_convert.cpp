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
        return static_cast<double>(frame) / static_cast<double>(modelNumFrames) * modelBlockDuration + 0.1751664994f;
    }

    static constexpr long secondsToFrame(auto seconds)
    {
        return static_cast<long>(std::ceil((seconds) / modelBlockDuration * static_cast<double>(modelNumFrames)));
    }

    static std::vector<std::array<float, modelNumNotes>> getInferredOnsets(std::vector<std::array<float, modelNumNotes>> const& onsets, std::vector<std::array<float, modelNumNotes>> const& frames, size_t numDiff = 2)
    {
        std::vector<std::array<float, modelNumNotes>> notesDiff;
        notesDiff.resize(frames.size());
        for(auto& notes : notesDiff)
        {
            std::fill(notes.begin(), notes.end(), 1.0f);
        }

        auto maxDiff = 0.0f;
        auto maxOnset = 0.0f;
        for(size_t diff = 1; diff <= numDiff; ++diff)
        {
            for(size_t frame = 0; frame < frames.size(); ++frame)
            {
                for(size_t note = 0; note < modelNumNotes; ++note)
                {
                    auto const currentEnergy = frames.at(frame).at(note);
                    auto const previousEnergy = (frame >= diff) ? frames.at(frame - diff).at(note) : 0.0f;
                    auto const diffEnergy = std::max(currentEnergy - previousEnergy, 0.0f);
                    notesDiff[frame][note] = std::min((frame >= numDiff) ? diffEnergy : 0.0f, notesDiff.at(frame).at(note));

                    maxOnset = std::max(onsets.at(frame).at(note), maxOnset);
                    maxDiff = std::max(notesDiff.at(frame).at(note), maxDiff);
                }
            }
        }

        auto const ratio = maxDiff >= 0.0f ? maxOnset / maxDiff : 0.0f;
        for(size_t frame = 0; frame < frames.size(); ++frame)
        {
            std::transform(onsets.at(frame).cbegin(), onsets.at(frame).cend(), notesDiff.at(frame).cbegin(), notesDiff[frame].begin(), [&](auto const& lhs, auto const& rhs)
                           {
                               return std::max(lhs, rhs * ratio);
                           });
        }
        return notesDiff;
    }

    std::vector<Note> getNotes(std::vector<std::array<float, modelNumNotes>> const& currentFrames, std::vector<std::array<float, modelNumNotes>> const& currentOnsets, bool inferOnsets, size_t voiceIndex, float frameEnergyThreshold, float onsetEnergyThreshold, double minNoteDuration, long maxFramesBelowThreshold, std::optional<float> const minFreq, std::optional<float> const maxFreq, bool melodiaTrick)
    {
        std::vector<Note> notes;
        auto onsets = inferOnsets ? getInferredOnsets(currentOnsets, currentFrames) : currentOnsets;
        auto frames = currentFrames;

        auto const numFrames = frames.size();
        auto const minNoteLength = secondsToFrame(minNoteDuration);
        auto const lastFrameIndex = numFrames - 1;
        auto const maxNoteIndex = std::clamp(maxFreq.has_value() ? static_cast<size_t>(std::round(hertzToMidi(maxFreq.value())) - modelNoteOffset) : modelNumNotes, size_t(0), size_t(modelNumNotes));
        auto const minNoteIndex = std::clamp(minFreq.has_value() ? static_cast<size_t>(std::round(hertzToMidi(minFreq.value())) - modelNoteOffset) : size_t(0), size_t(0), size_t(modelNumNotes));

        for(long frameStartIndex = static_cast<long>(lastFrameIndex) - 1; frameStartIndex >= 0; --frameStartIndex)
        {
            auto const fsi = static_cast<size_t>(frameStartIndex);
            for(long noteIndex = static_cast<long>(maxNoteIndex) - 1; noteIndex >= static_cast<long>(minNoteIndex); noteIndex--)
            {
                auto const ni = static_cast<size_t>(noteIndex);
                auto const currentOnset = onsets.at(fsi).at(ni);
                auto const previousOnset = (fsi <= 0) ? currentOnset : onsets.at(fsi - 1).at(ni);
                auto const nextOnset = (fsi >= lastFrameIndex) ? currentOnset : onsets.at(fsi + 1).at(ni);

                if(currentOnset >= onsetEnergyThreshold && currentOnset >= previousOnset && currentOnset >= nextOnset)
                {
                    auto fei = fsi + 1;
                    auto accumulatedFrames = 0;
                    while(fei < lastFrameIndex && accumulatedFrames < maxFramesBelowThreshold)
                    {
                        auto const energy = frames.at(fei).at(ni);
                        accumulatedFrames = energy < frameEnergyThreshold ? accumulatedFrames + 1 : 0;
                        ++fei;
                    }
                    fei -= accumulatedFrames;
                    auto const frameDuration = fei - fsi;

                    if(frameDuration > minNoteLength)
                    {
                        auto amplitude = 0.0;
                        for(auto cf = fsi; cf < fei; cf++)
                        {
                            auto& cframe = frames[cf];
                            cframe[ni] = 0.0f;
                            if(ni < modelNumNotes - 1)
                            {
                                cframe[ni + 1] = 0.0f;
                            }
                            if(ni > 0)
                            {
                                cframe[ni - 1] = 0.0f;
                            }
                            amplitude += static_cast<double>(currentFrames.at(cf).at(ni));
                        }
                        amplitude /= static_cast<double>(frameDuration);
                        notes.push_back({frameToSeconds(fsi), frameToSeconds(fei), midiToHertz(ni + modelNoteOffset), static_cast<float>(amplitude)});
                    }
                }
            }
        }

        if(melodiaTrick)
        {
            for(long frameIndex = static_cast<long>(lastFrameIndex) - 1; frameIndex >= 0; --frameIndex)
            {
                auto const fi = static_cast<size_t>(frameIndex);
                for(long noteIndex = static_cast<long>(maxNoteIndex) - 1; noteIndex >= static_cast<long>(minNoteIndex); noteIndex--)
                {
                    auto const ni = static_cast<size_t>(noteIndex);
                    auto const energy = frames.at(fi).at(ni);
                    if(energy > frameEnergyThreshold)
                    {
                        frames[fi][ni] = 0.0f;
                        auto fei = frameIndex + 1;
                        {
                            auto accumulatedFrames = 0;
                            while(fei < lastFrameIndex && accumulatedFrames < maxFramesBelowThreshold)
                            {
                                auto& cframe = frames[fei];
                                accumulatedFrames = cframe.at(ni) < frameEnergyThreshold ? accumulatedFrames + 1 : 0;
                                cframe[ni] = 0.0f;
                                if(noteIndex < modelNumNotes - 1)
                                {
                                    cframe[ni + 1] = 0.0f;
                                }
                                if(noteIndex > 0)
                                {
                                    cframe[ni - 1] = 0.0f;
                                }
                                ++fei;
                            }
                            fei -= (accumulatedFrames + 1);
                        }

                        auto fsi = frameIndex - 1;
                        {
                            auto accumulatedFrames = 0;
                            while(fsi > 0 && accumulatedFrames < maxFramesBelowThreshold)
                            {
                                auto& cframe = frames[fsi];
                                accumulatedFrames = cframe.at(ni) < frameEnergyThreshold ? accumulatedFrames + 1 : 0;
                                cframe[ni] = 0.0f;
                                if(noteIndex < modelNumNotes - 1)
                                {
                                    cframe[ni + 1] = 0.0f;
                                }
                                if(noteIndex > 0)
                                {
                                    cframe[ni - 1] = 0.0f;
                                }
                                --fsi;
                            }

                            fsi += (accumulatedFrames + 1);
                        }

                        assert(fsi >= 0);
                        assert(fei < numFrames);

                        auto const frameDuration = fei - fsi;
                        if(frameDuration > minNoteLength)
                        {
                            auto amplitude = 0.0;
                            for(auto cf = fsi; cf < fei; cf++)
                            {
                                amplitude += static_cast<double>(currentFrames.at(cf).at(ni));
                            }
                            amplitude /= static_cast<double>(frameDuration);
                            notes.push_back({frameToSeconds(fsi), frameToSeconds(fei), midiToHertz(ni + modelNoteOffset), static_cast<float>(amplitude)});
                        }
                    }
                }
            }
        }

        auto const noteCmp = [](auto const& lhs, auto const& rhs)
        {
            return lhs.start < rhs.start || (lhs.start <= rhs.start && lhs.pitch < rhs.pitch);
        };

        if(!notes.empty())
        {
            std::sort(notes.begin(), notes.end(), noteCmp);
            for(auto it = notes.begin(); it < notes.end(); ++it)
            {
                auto next = std::next(it);
                while(next != notes.end() && next->start < it->end)
                {
                    if(std::abs(next->pitch - it->pitch) < std::numeric_limits<float>::epsilon())
                    {
                        it->end = std::max(it->end, next->end);
                        next = notes.erase(next);
                    }
                    else
                    {
                        next = std::next(next);
                    }
                }
            }

            //            for(size_t voice = 0; voice <= voiceIndex; ++voice)
            //            {
            //                std::vector<Note> remainings;
            //                for(auto it = notes.begin(); it < notes.end(); ++it)
            //                {
            //                    auto next = std::next(it);
            //                    while(next != notes.end() && next->start < it->end)
            //                    {
            //                        if(next->pitch > it->pitch)
            //                        {
            //                            remainings.push_back(*next);
            //                            next = notes.erase(next);
            //                        }
            //                        else
            //                        {
            //                            next = std::next(next);
            //                        }
            //                    }
            //                }
            //                if(voice < voiceIndex)
            //                {
            //                    notes = remainings;
            //                }
            //            }
        }

        return notes;
    }
} // namespace Bpvp
