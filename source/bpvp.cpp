#include "bpvp.h"
#include "bpvp_convert.h"
#include <cmath>
#include <filesystem>
#include <vamp-sdk/PluginAdapter.h>

#if !defined(_WIN32)
#include <dlfcn.h>
#else
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#if defined(_MSC_VER)
#define forcedinline __forceinline
#else
#define forcedinline inline __attribute__((always_inline))
#endif

#ifndef NDEBUG
#define BpvpDbg(message) std::cout << "Bpvp: " << message << "\n"
#define BpvpErr(message) std::cerr << "Bpvp: " << message << "\n";

[[maybe_unused]] static void print(TfLiteInterpreter* interpreter)
{
    BpvpDbg("Prepare");
    auto const printTensor = [&](TfLiteTensor const* tensor, int index)
    {
        auto const numDims = TfLiteTensorNumDims(tensor);
        BpvpDbg("  Index " << index << " " << TfLiteTensorName(tensor) << " Num Dims " << numDims << " Type " << TfLiteTensorType(tensor));
        for(int32_t dimIndex = 0; dimIndex < numDims; ++dimIndex)
        {
            auto const dimLength = TfLiteTensorDim(tensor, dimIndex);
            BpvpDbg("      dim" << dimIndex << " Size " << dimLength);
        }
    };

    auto const numInputs = TfLiteInterpreterGetInputTensorCount(interpreter);
    BpvpDbg("Num Inputs " << numInputs);
    for(auto inputIndex = 0; inputIndex < numInputs; ++inputIndex)
    {
        auto const* input = TfLiteInterpreterGetInputTensor(interpreter, inputIndex);
        printTensor(input, inputIndex);
    }

    auto const numOutputs = TfLiteInterpreterGetOutputTensorCount(interpreter);
    BpvpDbg("Num Outputs " << numOutputs);
    for(auto outputIndex = 0; outputIndex < numOutputs; ++outputIndex)
    {
        auto const* output = TfLiteInterpreterGetOutputTensor(interpreter, outputIndex);
        printTensor(output, outputIndex);
    }
}

#else
#define BpvpDbg(message)
#define BpvpErr(message)

[[maybe_unused]] static void print([[maybe_unused]] TfLiteInterpreter* interpreter)
{
}

#endif

namespace ResamplerUtils
{
    template <int k>
    struct LagrangeResampleHelper
    {
        static forcedinline void calc(float& a, float b) noexcept { a *= b * (1.0f / k); }
    };

    template <>
    struct LagrangeResampleHelper<0>
    {
        static forcedinline void calc(float&, float) noexcept {}
    };

    template <int k>
    static float calcCoefficient(float input, float offset) noexcept
    {
        LagrangeResampleHelper<0 - k>::calc(input, -2.0f - offset);
        LagrangeResampleHelper<1 - k>::calc(input, -1.0f - offset);
        LagrangeResampleHelper<2 - k>::calc(input, 0.0f - offset);
        LagrangeResampleHelper<3 - k>::calc(input, 1.0f - offset);
        LagrangeResampleHelper<4 - k>::calc(input, 2.0f - offset);
        return input;
    }

    static float valueAtOffset(const float* inputs, float offset, int index) noexcept
    {
        auto result = 0.0f;
        result += calcCoefficient<0>(inputs[index], offset);
        index = (++index % 5);
        result += calcCoefficient<1>(inputs[index], offset);
        index = (++index % 5);
        result += calcCoefficient<2>(inputs[index], offset);
        index = (++index % 5);
        result += calcCoefficient<3>(inputs[index], offset);
        index = (++index % 5);
        result += calcCoefficient<4>(inputs[index], offset);
        return result;
    }
} // namespace ResamplerUtils

namespace Bpvp
{
    static std::filesystem::path getContentsDir()
    {
        static auto const dir = []() -> std::filesystem::path
        {
#if !defined(_WIN32)
            Dl_info info;
            if(dladdr(reinterpret_cast<void const*>(&getContentsDir), &info) != 0 && info.dli_fname != nullptr)
            {
                return std::filesystem::absolute(std::filesystem::path(info.dli_fname)).parent_path().parent_path();
            }
#else
            HMODULE module = nullptr;
            if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPWSTR>(&getContentsDir), &module) != 0)
            {
                std::vector<wchar_t> filename(MAX_PATH);
                auto capacity = static_cast<DWORD>(filename.size());
                auto length = GetModuleFileNameW(module, filename.data(), capacity);
                while(length == capacity)
                {
                    filename.resize(filename.size() * 2);
                    capacity = static_cast<DWORD>(filename.size());
                    length = GetModuleFileNameW(module, filename.data(), capacity);
                }
                if(length != 0)
                {
                    return std::filesystem::path(filename.data()).parent_path().parent_path();
                }
            }
#endif
            return std::filesystem::current_path() / "Contents";
        }();
        return dir;
    }

    static std::filesystem::path getModelPath()
    {
        static auto const path = getContentsDir() / "Resources" / "models" / "nmp.tflite";
        return path;
    }
} // namespace Bpvp

void Bpvp::Plugin::Resampler::prepare(double sampleRate)
{
    mSourceSampleRate = sampleRate;
    reset();
}

std::tuple<size_t, size_t> Bpvp::Plugin::Resampler::process(size_t numInputSamples, float const* inputBuffer, size_t numOutputSamples, float* outputBuffer)
{
    double const speedRatio = getRatio();
    size_t numGeneratedSamples = 0;
    size_t numUsedSamples = 0;
    auto subSamplePos = mSubSamplePos;
    while(numUsedSamples < numInputSamples && numGeneratedSamples < numOutputSamples)
    {
        while(subSamplePos >= 1.0 && numUsedSamples < numInputSamples)
        {
            mLastInputSamples[mIndexBuffer] = inputBuffer[numUsedSamples++];
            if(++mIndexBuffer == mLastInputSamples.size())
            {
                mIndexBuffer = 0;
            }
            subSamplePos -= 1.0;
        }
        if(subSamplePos < 1.0)
        {
            outputBuffer[numGeneratedSamples++] = ResamplerUtils::valueAtOffset(mLastInputSamples.data(), static_cast<float>(subSamplePos), static_cast<int>(mIndexBuffer));
            subSamplePos += speedRatio;
        }
    }
    while(subSamplePos >= 1.0 && numUsedSamples < numInputSamples)
    {
        mLastInputSamples[mIndexBuffer] = inputBuffer[numUsedSamples++];
        if(++mIndexBuffer == mLastInputSamples.size())
        {
            mIndexBuffer = 0;
        }
        subSamplePos -= 1.0;
    }
    mSubSamplePos = subSamplePos;
    return std::make_tuple(numUsedSamples, numGeneratedSamples);
}

void Bpvp::Plugin::Resampler::reset()
{
    mIndexBuffer = 0;
    mSubSamplePos = 1.0;
    std::fill(mLastInputSamples.begin(), mLastInputSamples.end(), 0.0f);
}

void Bpvp::Plugin::Resampler::setTargetSampleRate(double sampleRate) noexcept
{
    mTargetSampleRate = sampleRate;
}

double Bpvp::Plugin::Resampler::getRatio() const noexcept
{
    return mSourceSampleRate / mTargetSampleRate;
}

Bpvp::Plugin::Plugin(float inputSampleRate)
: Vamp::Plugin(inputSampleRate)
{
    mResampler.prepare(static_cast<double>(inputSampleRate));
}

bool Bpvp::Plugin::initialise(size_t channels, size_t stepSize, size_t blockSize)
{
    if(channels != static_cast<size_t>(1) || stepSize != blockSize)
    {
        return false;
    }
    reset();
    mBlockSize = blockSize;
    return mInterpreter != nullptr;
}

std::string Bpvp::Plugin::getIdentifier() const
{
    return "basicpitch";
}

std::string Bpvp::Plugin::getName() const
{
    return "Basic Pitch";
}

std::string Bpvp::Plugin::getDescription() const
{
    return "Automatic music transcription using lightweight neural network by Spotify's Audio Intelligence Lab.";
}

std::string Bpvp::Plugin::getMaker() const
{
    return "Ircam";
}

int Bpvp::Plugin::getPluginVersion() const
{
    return PLUGIN_VERSION;
}

std::string Bpvp::Plugin::getCopyright() const
{
    return "Basic Pitch model by Spotify's Audio Intelligence Lab. Basic Pitch Vamp Plugin by Pierre Guillot. Copyright 2024 Ircam. All rights reserved.";
}

Bpvp::Plugin::InputDomain Bpvp::Plugin::getInputDomain() const
{
    return TimeDomain;
}

size_t Bpvp::Plugin::getPreferredBlockSize() const
{
    return static_cast<size_t>(1024);
}

size_t Bpvp::Plugin::getPreferredStepSize() const
{
    return static_cast<size_t>(0);
}

Bpvp::Plugin::OutputList Bpvp::Plugin::getOutputDescriptors() const
{
    // Each output is a monophonic voice (0 is the lowest voice), the identifier of the first voice
    // is the identifier of the output of the previous versions of the plugin
    OutputList list;
    for(size_t voice = 0; voice < mNumVoices; ++voice)
    {
        OutputDescriptor d;
        d.identifier = voice == 0 ? std::string("pitch") : "pitch" + std::to_string(voice + 1);
        d.name = "Pitch " + std::to_string(voice + 1);
        d.description = "Pitch of the voice " + std::to_string(voice + 1) + " estimated from the input signal (the voice 1 is the lowest voice)";
        d.unit = "Hertz";
        d.hasFixedBinCount = true;
        d.binCount = static_cast<size_t>(1);
        d.hasKnownExtents = true;
        d.minValue = 0.0f;
        d.maxValue = getInputSampleRate() / 2.0f;
        d.isQuantized = false;
        d.sampleType = OutputDescriptor::SampleType::VariableSampleRate;
        d.hasDuration = true;
        list.push_back(std::move(d));
    }
    return list;
}

void Bpvp::Plugin::reset()
{
    mInterpreter.reset();
    mModel.reset();

    auto const modelPath = getModelPath();
    if(!std::filesystem::exists(modelPath))
    {
        BpvpErr("Failed to retrieve the model from the plugin bundle!");
        return;
    }
    mModel = model_uptr(TfLiteModelCreateFromFile(modelPath.string().c_str()), [](TfLiteModel* m)
                        {
                            if(m != nullptr)
                            {
                                TfLiteModelDelete(m);
                            }
                        });
    if(mModel == nullptr)
    {
        BpvpErr("TfLite failed to allocate model!");
        return;
    }

    auto options = interpreter_options_uptr(TfLiteInterpreterOptionsCreate(), [](TfLiteInterpreterOptions* o)
                                            {
                                                if(o != nullptr)
                                                {
                                                    TfLiteInterpreterOptionsDelete(o);
                                                }
                                            });
    if(options == nullptr)
    {
        BpvpErr("TfLite failed to allocate option!");
    }
    else
    {
        mInterpreter = interpreter_uptr(TfLiteInterpreterCreate(mModel.get(), options.get()), [](TfLiteInterpreter* i)
                                        {
                                            if(i != nullptr)
                                            {
                                                TfLiteInterpreterDelete(i);
                                            }
                                        });
        if(mInterpreter == nullptr)
        {
            BpvpErr("TfLite failed to allocate interpreter!");
        }
        else
        {
            auto const result = TfLiteInterpreterAllocateTensors(mInterpreter.get());
            if(result != TfLiteStatus::kTfLiteOk)
            {
                BpvpErr("TfLite failed to allocate tensors!");
                mInterpreter.reset();
            }
            else
            {
                print(mInterpreter.get());
            }
        }
    }
    // basic_pitch.inference.get_audio_input: the audio is prepended with half of the overlap
    std::fill(mInputBuffer.begin(), mInputBuffer.end(), 0.0f);
    mInputBufferPosition = static_cast<size_t>(modelBlockPadding / 2);
    mNumSamples = 0;
    mStartTime = Vamp::RealTime::zeroTime;
    mResampler.reset();
    mAccumulatedFrames.clear();
    mAccumulatedOnsets.clear();
}

Bpvp::Plugin::ParameterList Bpvp::Plugin::getParameterDescriptors() const
{
    ParameterList list;
    {
        ParameterDescriptor param;
        param.identifier = "framethreshold";
        param.name = "Frame Threshold";
        param.description = "The frame's amplitude threshold";
        param.unit = "";
        param.minValue = 0.05f;
        param.maxValue = 0.95f;
        param.defaultValue = 0.3f;
        param.isQuantized = true;
        param.quantizeStep = 0.01f;
        list.push_back(std::move(param));
    }
    {
        ParameterDescriptor param;
        param.identifier = "onsetthreshold";
        param.name = "Onset Threshold";
        param.description = "The onset's amplitude threshold";
        param.unit = "";
        param.minValue = 0.05f;
        param.maxValue = 0.95f;
        param.defaultValue = 0.5f;
        param.isQuantized = true;
        param.quantizeStep = 0.01f;
        list.push_back(std::move(param));
    }
    {
        ParameterDescriptor param;
        param.identifier = "minnoteduration";
        param.name = "Minimum Note Duration";
        param.description = "The minimum note duration";
        param.unit = "ms";
        param.minValue = 0.0f;
        param.maxValue = 1000.0f;
        param.defaultValue = 128.0f;
        param.isQuantized = true;
        param.quantizeStep = 1.0f;
        list.push_back(std::move(param));
    }
    return list;
}

void Bpvp::Plugin::setParameter(std::string paramid, float newval)
{
    if(paramid == "framethreshold")
    {
        mFrameThreshold = std::clamp(newval, 0.05f, 0.95f);
    }
    else if(paramid == "onsetthreshold")
    {
        mOnsetThreshold = std::clamp(newval, 0.05f, 0.95f);
    }
    else if(paramid == "minnoteduration")
    {
        mMinNoteDuration = static_cast<int>(std::round(std::clamp(newval, 0.0f, 1000.0f)));
    }
    else
    {
        std::cerr << "Invalid parameter : " << paramid << "\n";
    }
}

float Bpvp::Plugin::getParameter(std::string paramid) const
{
    if(paramid == "framethreshold")
    {
        return mFrameThreshold;
    }
    if(paramid == "onsetthreshold")
    {
        return mOnsetThreshold;
    }
    if(paramid == "minnoteduration")
    {
        return static_cast<float>(mMinNoteDuration);
    }
    std::cerr << "Invalid parameter : " << paramid << "\n";
    return 0.0f;
}

Bpvp::Plugin::OutputExtraList Bpvp::Plugin::getOutputExtraDescriptors(size_t outputDescriptorIndex) const
{
    OutputExtraList list;
    if(outputDescriptorIndex < mNumVoices)
    {
        OutputExtraDescriptor d;
        d.identifier = "amplitude";
        d.name = "Amplitude";
        d.description = "The amplitude of the note";
        d.unit = "";
        d.hasKnownExtents = true;
        d.minValue = 0.0f;
        d.maxValue = 1.0f;
        d.isQuantized = false;
        d.quantizeStep = 0.0f;
        list.push_back(std::move(d));
    }
    return list;
}

std::optional<Bpvp::Plugin::VoiceDescriptor> Bpvp::Plugin::getOutputVoiceDescriptor(size_t outputDescriptorIndex) const
{
    if(outputDescriptorIndex != 0)
    {
        return {};
    }
    VoiceDescriptor d;
    d.identifier = "voices";
    d.name = "Number of Voices";
    d.description = "The number of monophonic voices to output (the voices are ordered from the lowest to the highest)";
    d.unit = "";
    d.minValue = 1.0f;
    d.maxValue = static_cast<float>(maxNumVoices);
    d.defaultValue = static_cast<float>(defaultNumVoices);
    d.isQuantized = true;
    d.quantizeStep = 1.0f;
    return d;
}

size_t Bpvp::Plugin::getOutputVoiceCount(size_t outputDescriptorIndex) const
{
    return outputDescriptorIndex == 0 ? mNumVoices : 1;
}

void Bpvp::Plugin::setOutputVoiceCount(std::string const& identifier, size_t count)
{
    if(identifier == "pitch")
    {
        mNumVoices = std::clamp(count, static_cast<size_t>(1), maxNumVoices);
    }
}

void Bpvp::Plugin::processModel(bool flush)
{
    // basic_pitch.inference.run_inference: the windows of modelBlockSize samples overlap by modelBlockPadding
    // samples and half of the overlapping frames are removed at the beginning and the end of each window
    // (unwrap_output). When flushing, the last windows are padded with zeros (window_audio_file).
    static auto constexpr hopSize = static_cast<size_t>(modelBlockHop);
    static auto constexpr firstValidFrame = static_cast<size_t>(modelNumOverlappingFrames / 2);
    while(mInputBufferPosition >= modelBlockSize || (flush && mInputBufferPosition > 0))
    {
        if(mInputBufferPosition < modelBlockSize)
        {
            std::fill(std::next(mInputBuffer.begin(), static_cast<long>(mInputBufferPosition)), std::next(mInputBuffer.begin(), modelBlockSize), 0.0f);
        }
        std::copy(mInputBuffer.cbegin(), std::next(mInputBuffer.cbegin(), modelBlockSize), mAudioBuffer.begin());
        if(mInputBufferPosition > hopSize)
        {
            std::copy(std::next(mInputBuffer.cbegin(), static_cast<long>(hopSize)), std::next(mInputBuffer.cbegin(), static_cast<long>(mInputBufferPosition)), mInputBuffer.begin());
            mInputBufferPosition -= hopSize;
        }
        else
        {
            mInputBufferPosition = 0;
        }

        TfLiteTensorCopyFromBuffer(TfLiteInterpreterGetInputTensor(mInterpreter.get(), 0), mAudioBuffer.data(), mAudioBuffer.size() * sizeof(float));
        TfLiteInterpreterInvoke(mInterpreter.get());

        auto const addFrames = [&](std::vector<std::array<float, modelNumNotes>>& buffer)
        {
            buffer.reserve(buffer.size() + modelNumValidFrames);
            for(size_t frame = firstValidFrame; frame < firstValidFrame + modelNumValidFrames; ++frame)
            {
                auto const it = std::next(mOuputBuffer.cbegin(), static_cast<long>(frame * modelNumNotes));
                buffer.push_back({});
                std::copy(it, std::next(it, modelNumNotes), buffer.back().begin());
            }
        };

        TfLiteTensorCopyToBuffer(TfLiteInterpreterGetOutputTensor(mInterpreter.get(), 0), mOuputBuffer.data(), mOuputBuffer.size() * sizeof(float));
        addFrames(mAccumulatedOnsets);

        TfLiteTensorCopyToBuffer(TfLiteInterpreterGetOutputTensor(mInterpreter.get(), 1), mOuputBuffer.data(), mOuputBuffer.size() * sizeof(float));
        addFrames(mAccumulatedFrames);
    }
}

Bpvp::Plugin::FeatureSet Bpvp::Plugin::process(float const* const* inputBuffers, Vamp::RealTime timestamp)
{
    if(mNumSamples == 0)
    {
        mStartTime = timestamp;
    }
    auto const* inputBuffer = inputBuffers[0];
    size_t inputPosition = 0;
    auto remainingSamples = mBlockSize;
    while(remainingSamples > 0)
    {
        auto const remainingOutput = mInputBuffer.size() - mInputBufferPosition;
        auto const result = mResampler.process(remainingSamples, inputBuffer + inputPosition, remainingOutput, mInputBuffer.data() + mInputBufferPosition);
        mInputBufferPosition += std::get<1>(result);
        mNumSamples += std::get<1>(result);
        if(mInputBufferPosition >= mInputBuffer.size())
        {
            processModel(false);
        }
        inputPosition += std::get<0>(result);
        remainingSamples -= std::get<0>(result);
    }
    return {};
}

Bpvp::Plugin::FeatureSet Bpvp::Plugin::getRemainingFeatures()
{
    if(mNumSamples == 0)
    {
        return {};
    }
    processModel(true);

    // basic_pitch.inference.unwrap_output: trim to the original audio length
    auto const duration = static_cast<double>(mNumSamples) / static_cast<double>(modelSampleRate);
    while(!mAccumulatedFrames.empty() && frameToSeconds(mAccumulatedFrames.size() - 1) >= duration)
    {
        mAccumulatedFrames.pop_back();
    }
    mAccumulatedOnsets.resize(std::min(mAccumulatedOnsets.size(), mAccumulatedFrames.size()));
    if(mAccumulatedFrames.empty() || mAccumulatedOnsets.empty())
    {
        return {};
    }
    // The notes of each voice are returned in the feature list of the corresponding output
    auto const voices = getNotes(mAccumulatedFrames, mAccumulatedOnsets, true, mNumVoices, mFrameThreshold, mOnsetThreshold, static_cast<double>(mMinNoteDuration) / 1000.0, 11, 80.0f, 8000.0f);
    FeatureSet fs;
    for(size_t voice = 0; voice < voices.size(); ++voice)
    {
        auto const& notes = voices.at(voice);
        auto& fl = fs[static_cast<int>(voice)];
        fl.reserve(notes.size() * 2);
        for(auto const& note : notes)
        {
            Feature feature;
            feature.hasTimestamp = true;
            feature.timestamp = mStartTime + Vamp::RealTime::fromSeconds(note.start);
            feature.hasDuration = true;
            feature.duration = Vamp::RealTime::fromSeconds(note.end - note.start);
            feature.values = {static_cast<float>(note.pitch), static_cast<float>(note.amplitude)};
            fl.push_back(std::move(feature));
            feature.hasTimestamp = true;
            feature.timestamp = mStartTime + Vamp::RealTime::fromSeconds(note.end);
            feature.hasDuration = true;
            feature.duration = Vamp::RealTime();
            feature.values = {};
            fl.push_back(std::move(feature));
        }
    }
    return fs;
}

#ifdef __cplusplus
extern "C"
{
#endif
    VampPluginDescriptor const* vampGetPluginDescriptor(unsigned int version, unsigned int index)
    {
        if(version < 1)
        {
            return nullptr;
        }
        switch(index)
        {
            case 0:
            {
                static Vamp::PluginAdapter<Bpvp::Plugin> adaptater;
                return adaptater.getDescriptor();
            }
            default:
            {
                return nullptr;
            }
        }
    }

    IVE_EXTERN IvePluginDescriptor const* iveGetPluginDescriptor(unsigned int version, unsigned int index)
    {
        if(version < 1)
        {
            return nullptr;
        }
        switch(index)
        {
            case 0:
            {
                return Ive::PluginAdapter::getDescriptor<Bpvp::Plugin>();
            }
            default:
            {
                return nullptr;
            }
        }
    }
#ifdef __cplusplus
}
#endif
