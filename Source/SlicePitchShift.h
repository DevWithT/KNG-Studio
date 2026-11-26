#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include "SliceFx.h"

#include <algorithm>
#include <cmath>
#include <vector>

// Offline pitch shift that keeps the length of the audio.
// Uses the engine's own time stretcher (SoundTouch, better quality), the same
// one clip Pitch uses, so a shifted slice sounds like a shifted clip.
namespace KngSlicePitchShift
{
    // Returns the buffer shifted by `semitones`, with exactly the same length.
    // If the stretcher can't start, the input comes back unchanged.
    inline juce::AudioBuffer<float> shiftKeepLength (const juce::AudioBuffer<float>& input,
                                                      double sampleRate, float semitones)
    {
        const int numChannels = input.getNumChannels();
        const int numIn = input.getNumSamples();

        if (numChannels <= 0 || numIn <= 0 || sampleRate <= 0.0 || std::abs (semitones) < 0.001f)
            return input;

        constexpr int blockSize = 512;

        tracktion::TimeStretcher stretcher;
        stretcher.initialise (sampleRate, blockSize, numChannels,
                              tracktion::TimeStretcher::soundtouchBetter, {}, false);

        if (! stretcher.isInitialised() || ! stretcher.setSpeedAndPitch (1.0f, semitones))
            return input;

        // Silence on the end gives the stretcher room to finish the last sound.
        // It is trimmed off again below, so the result is the original length.
        const int padding = stretcher.getMaxFramesNeeded();
        const int numTotalIn = numIn + padding;

        juce::AudioBuffer<float> source (numChannels, numTotalIn);
        source.clear();

        for (int ch = 0; ch < numChannels; ++ch)
            source.copyFrom (ch, 0, input, ch, 0, numIn);

        juce::AudioBuffer<float> result (numChannels, numIn);
        result.clear();

        juce::AudioBuffer<float> block (numChannels, blockSize);
        std::vector<const float*> inPtrs ((size_t) numChannels);

        int readPos = 0;
        int written = 0;

        auto collect = [&] (int numOut)
        {
            const int n = juce::jmin (numOut, numIn - written);

            if (n <= 0)
                return;

            for (int ch = 0; ch < numChannels; ++ch)
                result.copyFrom (ch, written, block, ch, 0, n);

            written += n;
        };

        // Feed exactly what the stretcher asks for, take what it gives back.
        int guard = 0;
        const int maxIterations = numTotalIn * 2 + 4096;

        while (readPos < numTotalIn && written < numIn && guard++ < maxIterations)
        {
            const int need = stretcher.getFramesNeeded();
            const int give = juce::jmin (need, numTotalIn - readPos);

            for (int ch = 0; ch < numChannels; ++ch)
                inPtrs[(size_t) ch] = source.getReadPointer (ch, readPos);

            const int numOut = stretcher.processData (inPtrs.data(), give, block.getArrayOfWritePointers());
            readPos += give;
            collect (numOut);
        }

        // Out of input: flush the tail.
        guard = 0;

        while (written < numIn && guard++ < 4096)
        {
            const int numOut = stretcher.flush (block.getArrayOfWritePointers());

            if (numOut <= 0)
                break;

            collect (numOut);
        }

        return result;
    }

    // The one place a slice gets its effects.
    // keepLength true:  pitch is a real shift, the slice keeps its length.
    // keepLength false: record-style pitch, speed changes with it (old behaviour).
    inline juce::AudioBuffer<float> applyFx (const juce::AudioBuffer<float>& input, double sampleRate,
                                             const SliceFx& fx, bool keepLength)
    {
        if (! keepLength || std::abs (fx.semitones) < 0.001f)
            return applySliceFx (input, fx);

        // Pitch first, so reverse and gain work on the shifted sound.
        auto shifted = shiftKeepLength (input, sampleRate, (float) fx.semitones);

        SliceFx rest = fx;
        rest.semitones = 0;

        return rest.isDefault() ? shifted : applySliceFx (shifted, rest);
    }
}
