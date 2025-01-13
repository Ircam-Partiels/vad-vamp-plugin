#include "vad.h"
#include <algorithm>
#include <cmath>
#include <vamp-sdk/PluginAdapter.h>

#if defined(_MSC_VER)
#define forcedinline __forceinline
#else
#define forcedinline inline __attribute__((always_inline))
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

void Vad::Plugin::Resampler::prepare(double sampleRate)
{
    mSourceSampleRate = sampleRate;
    reset();
}

std::tuple<size_t, size_t> Vad::Plugin::Resampler::process(size_t numInputSamples, float const* inputBuffer, size_t numOutputSamples, float* outputBuffer)
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

void Vad::Plugin::Resampler::reset()
{
    mIndexBuffer = 0;
    mSubSamplePos = 1.0;
    std::fill(mLastInputSamples.begin(), mLastInputSamples.end(), 0.0f);
}

void Vad::Plugin::Resampler::setTargetSampleRate(double sampleRate) noexcept
{
    mTargetSampleRate = sampleRate;
}

double Vad::Plugin::Resampler::getRatio() const noexcept
{
    return mSourceSampleRate / mTargetSampleRate;
}

Vad::Plugin::Plugin(float inputSampleRate)
: Vamp::Plugin(inputSampleRate)
{
    mIntermediate.resize(80, 0);
    mResampler.prepare(static_cast<double>(inputSampleRate));
    mHandle = handle_uptr(fvad_new(), [](Fvad* ctx)
                          {
                              if(ctx != nullptr)
                              {
                                  fvad_free(ctx);
                              }
                          });
}

bool Vad::Plugin::initialise(size_t channels, size_t stepSize, size_t blockSize)
{
    if(channels != static_cast<size_t>(1) || stepSize != blockSize)
    {
        return false;
    }
    reset();
    mBuffer.reserve(gModelSampleRate * 2);
    mBlockSize = blockSize;
    return mHandle != nullptr;
}

std::string Vad::Plugin::getIdentifier() const
{
    return "vad";
}

std::string Vad::Plugin::getName() const
{
    return "VAD";
}

std::string Vad::Plugin::getDescription() const
{
    return "Speech detection using OpenAI's Whisper model.";
}

std::string Vad::Plugin::getMaker() const
{
    return "Ircam";
}

int Vad::Plugin::getPluginVersion() const
{
    return VAD_VAMP_PLUGIN_PLUGIN_VERSION;
}

std::string Vad::Plugin::getCopyright() const
{
    return "libfvad copyright (c) 2011 by the WebRTC project authors (all rights reserved) and copyright (c) 2016 by Daniel Pirch. VAD Vamp Plugin by Pierre Guillot. Copyright 2024 Ircam. All rights reserved.";
}

Vad::Plugin::InputDomain Vad::Plugin::getInputDomain() const
{
    return TimeDomain;
}

size_t Vad::Plugin::getPreferredBlockSize() const
{
    return static_cast<size_t>(1024);
}

size_t Vad::Plugin::getPreferredStepSize() const
{
    return static_cast<size_t>(0);
}

Vad::Plugin::OutputList Vad::Plugin::getOutputDescriptors() const
{
    OutputDescriptor d;
    d.identifier = "token";
    d.name = "Token";
    d.description = "Tokens generated by speech detection";
    d.unit = "";
    d.hasFixedBinCount = true;
    d.binCount = static_cast<size_t>(0);
    d.hasKnownExtents = false;
    d.minValue = 0.0f;
    d.maxValue = 0.0f;
    d.isQuantized = false;
    d.sampleType = OutputDescriptor::SampleType::VariableSampleRate;
    d.hasDuration = true;
    return {d};
}

void Vad::Plugin::reset()
{
    if(mHandle != nullptr)
    {
        fvad_reset(mHandle.get());
        fvad_set_sample_rate(mHandle.get(), 8000);
        fvad_set_mode(mHandle.get(), static_cast<int>(mMode));
    }
    mBuffer.clear();
    mBufferPosition = 0;
    mAdvancement = 0;
    mResampler.reset();
    mLastActive.reset();
}

Vad::Plugin::ParameterList Vad::Plugin::getParameterDescriptors() const
{
    ParameterList list;
    {
        ParameterDescriptor param;
        param.identifier = "mode";
        param.name = "Mode";
        param.description = "The ";
        param.unit = "";
        param.valueNames = {"Quality", "Low Bit Rate", "Aggressive", "Very Aggressive"};
        param.minValue = 0.0f;
        param.maxValue = 3.0f;
        param.defaultValue = 0.0f;
        param.isQuantized = true;
        param.quantizeStep = 1.0f;
        list.push_back(std::move(param));
    }
    {
        ParameterDescriptor param;
        param.identifier = "blocksize";
        param.name = "Block Size";
        param.description = "The ";
        param.unit = "";
        param.valueNames = {"Small (80)", "Medium (160)", "Large (240)"};
        param.minValue = 0.0f;
        param.maxValue = 2.0f;
        param.defaultValue = 0.0f;
        param.isQuantized = true;
        param.quantizeStep = 1.0f;
        list.push_back(std::move(param));
    }
    return list;
}

void Vad::Plugin::setParameter(std::string paramid, float newval)
{
    if(paramid == "mode")
    {
        mMode = static_cast<size_t>(std::floor(std::clamp(newval, 0.0f, 3.0f)));
    }
    else if(paramid == "blocksize")
    {
        switch(static_cast<size_t>(std::floor(std::clamp(newval, 0.0f, 2.0f))))
        {
            case 0:
                mIntermediate.resize(80);
                break;
            case 1:
                mIntermediate.resize(160);
                break;
            case 2:
                mIntermediate.resize(240);
                break;
            default:
                mIntermediate.resize(80);
                break;
        }
    }
    else
    {
        std::cerr << "Invalid parameter : " << paramid << "\n";
    }
}

float Vad::Plugin::getParameter(std::string paramid) const
{
    if(paramid == "mode")
    {
        return static_cast<float>(mMode);
    }
    if(paramid == "blocksize")
    {
        return static_cast<float>(mIntermediate.size() / 80) - 1.0f;
    }
    std::cerr << "Invalid parameter : " << paramid << "\n";
    return 0.0f;
}

void Vad::Plugin::getCurrentFeatures(Vad::Plugin::FeatureList& fl)
{
    auto const running = Vamp::RealTime::frame2RealTime(mAdvancement - mBufferPosition, gModelSampleRate);
    size_t index = 0;
    while(index < mBufferPosition)
    {
        if(mBufferPosition - index >= mIntermediate.size())
        {
            auto const frame = static_cast<long>(mAdvancement - mBufferPosition + index);
            auto const time = Vamp::RealTime::frame2RealTime(frame, gModelSampleRate);
            for(size_t i = 0; i < mIntermediate.size(); ++i)
            {
                static auto constexpr max = static_cast<double>(std::numeric_limits<int16_t>::max());
                auto const val = static_cast<double>(mBuffer.at(index + i));
                mIntermediate[i] = static_cast<int16_t>(std::clamp(std::round(val * (max + 1.0)), -max, max));
            }
            auto const result = fvad_process(mHandle.get(), mIntermediate.data(), static_cast<int>(mIntermediate.size()));
            if(result == 0 && mLastActive.has_value())
            {
                Feature feature;
                feature.hasTimestamp = true;
                feature.timestamp = Vamp::RealTime::frame2RealTime(mLastActive.value(), gModelSampleRate);
                feature.hasDuration = true;
                feature.duration = time - feature.timestamp;
                fl.push_back(feature);
                mLastActive.reset();
            }
            else if(result == 1 && !mLastActive.has_value())
            {
                mLastActive = frame;
            }
            index += mIntermediate.size();
        }
        else if(index > 0)
        {
            auto const difference = mBufferPosition - index;
            for(size_t i = 0; i < difference; ++i)
            {
                mBuffer[i] = mBuffer.at(index++);
            }
            mBufferPosition = difference;
        }
        else
        {
            index += mBufferPosition;
        }
    }
}

Vad::Plugin::FeatureSet Vad::Plugin::process(float const* const* inputBuffers, [[maybe_unused]] Vamp::RealTime timestamp)
{
    auto blockSize = mBlockSize;
    auto const* inputBuffer = inputBuffers[0];
    size_t inputPosition = 0;
    auto const scaleRatio = static_cast<double>(gModelSampleRate) / static_cast<double>(getInputSampleRate());

    FeatureList fl;
    while(blockSize > 0)
    {
        auto const remaining = mBuffer.size() - mBufferPosition;
        auto const scaleSize = static_cast<size_t>(std::ceil(static_cast<double>(blockSize) * scaleRatio));
        if(remaining < scaleSize)
        {
            mBuffer.resize(mBuffer.size() + (scaleSize - remaining), 0.0f);
        }
        auto const result = mResampler.process(blockSize, inputBuffer + inputPosition, scaleSize, mBuffer.data() + mBufferPosition);
        if(std::get<0>(result) != blockSize)
        {
            std::cerr << "Missing input samples\n";
        }
        mBufferPosition += std::get<1>(result);
        mAdvancement += std::get<1>(result);
        inputPosition += blockSize;
        blockSize -= blockSize;
        getCurrentFeatures(fl);
    }
    return {{0, fl}};
}

Vad::Plugin::FeatureSet Vad::Plugin::getRemainingFeatures()
{
    FeatureList fl;
    getCurrentFeatures(fl);
    if(mBufferPosition > 0)
    {
        auto const missing = mIntermediate.size() - mBufferPosition;
        std::fill_n(std::next(mBuffer.begin(), static_cast<long>(mBufferPosition)), missing, 0.0f);
        mBufferPosition += missing;
        mAdvancement += missing;
        getCurrentFeatures(fl);
    }
    if(mLastActive.has_value())
    {
        auto const frame = static_cast<long>(mAdvancement);
        auto const time = Vamp::RealTime::frame2RealTime(frame, gModelSampleRate);
        Feature feature;
        feature.hasTimestamp = true;
        feature.timestamp = Vamp::RealTime::frame2RealTime(mLastActive.value(), gModelSampleRate);
        feature.hasDuration = true;
        feature.duration = time - feature.timestamp;
        fl.push_back(feature);
        mLastActive.reset();
    }
    return {{0, fl}};
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
                static Vamp::PluginAdapter<Vad::Plugin> adaptater;
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
                return Ive::PluginAdapter::getDescriptor<Vad::Plugin>();
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
