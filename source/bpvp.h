#pragma once

#include "bpvp_model.h"
#include <IvePluginAdapter.hpp>
#include <array>
#include <memory>
#include <set>
#include <tensorflow/lite/c/c_api.h>

namespace Bpvp
{
    class Plugin
    : public Vamp::Plugin
    , public Ive::PluginExtension
    {
    public:
        Plugin(float inputSampleRate);
        ~Plugin() override = default;

        // Vamp::Plugin
        bool initialise(size_t channels, size_t stepSize, size_t blockSize) override;

        InputDomain getInputDomain() const override;

        std::string getIdentifier() const override;
        std::string getName() const override;
        std::string getDescription() const override;
        std::string getMaker() const override;
        int getPluginVersion() const override;
        std::string getCopyright() const override;

        size_t getPreferredBlockSize() const override;
        size_t getPreferredStepSize() const override;
        OutputList getOutputDescriptors() const override;

        void reset() override;
        FeatureSet process(float const* const* inputBuffers, Vamp::RealTime timestamp) override;
        FeatureSet getRemainingFeatures() override;

        ParameterList getParameterDescriptors() const override;
        float getParameter(std::string paramid) const override;
        void setParameter(std::string paramid, float newval) override;

        // Ive::PluginExtension
        OutputExtraList getOutputExtraDescriptors(size_t outputDescriptorIndex) const override;

    private:
        void processModel();

        class Resampler
        {
        public:
            Resampler() = default;
            ~Resampler() = default;

            void prepare(double sampleRate);
            std::tuple<size_t, size_t> process(size_t numInputSamples, float const* inputBuffer, size_t numOutputSamples, float* outputBuffer);
            void reset();

            void setTargetSampleRate(double sampleRate) noexcept;
            double getRatio() const noexcept;

        private:
            double mSourceSampleRate{48000.0};
            double mTargetSampleRate{static_cast<double>(modelSampleRate)};
            std::array<float, 5> mLastInputSamples;
            double mSubSamplePos{1.0};
            size_t mIndexBuffer{0};
        };

        using model_uptr = std::unique_ptr<TfLiteModel, void (*)(TfLiteModel*)>;
        using interpreter_options_uptr = std::unique_ptr<TfLiteInterpreterOptions, void (*)(TfLiteInterpreterOptions*)>;
        using interpreter_uptr = std::unique_ptr<TfLiteInterpreter, void (*)(TfLiteInterpreter*)>;
        model_uptr mModel{nullptr, nullptr};
        interpreter_uptr mInterpreter{nullptr, nullptr};
        Resampler mResampler;
        std::array<float, modelBlockSize> mInputBuffer;
        std::array<float, modelTensorSize> mOuputBuffer;
        std::vector<float> mAccumulatedFrames;
        std::vector<float> mAccumulatedOnsets;
        size_t mInputBufferPosition{0};
        size_t mBlockSize{0};
        float mFrameThreshold{0.7f};
        float mOnsetThreshold{0.5f};
        int mMinNoteDuration{120};
    };
} // namespace Bpvp
