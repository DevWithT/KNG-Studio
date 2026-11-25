#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/** What to do to one slice when it is written out: play it backwards, shift its pitch, change its volume. */
struct SliceFx
{
    bool reverse = false;
    float semitones = 0.0f;     // pitch shift like a record speeding up, so the slice gets shorter or longer
    float gainDb = 0.0f;

    bool isDefault() const      { return ! reverse && semitones == 0.0f && gainDb == 0.0f; }
};

/** Returns a processed copy of the slice. The result can be a different length when the pitch changes. */
inline juce::AudioBuffer<float> applySliceFx (const juce::AudioBuffer<float>& in, const SliceFx& fx)
{
    juce::AudioBuffer<float> buffer;
    buffer.makeCopyOf (in);
    const int channels = buffer.getNumChannels();

    if (fx.reverse)
        for (int ch = 0; ch < channels; ++ch)
            buffer.reverse (ch, 0, buffer.getNumSamples());

    if (fx.semitones != 0.0f && buffer.getNumSamples() > 1)
    {
        const double ratio = std::pow (2.0, (double) fx.semitones / 12.0);
        const int inLen = buffer.getNumSamples();
        const int outLen = juce::jmax (1, (int) ((double) inLen / ratio));
        juce::AudioBuffer<float> out (channels, outLen);

        for (int ch = 0; ch < channels; ++ch)
        {
            for (int i = 0; i < outLen; ++i)
            {
                const double pos = (double) i * ratio;
                const int i0 = juce::jmin (inLen - 1, (int) pos);
                const int i1 = juce::jmin (inLen - 1, i0 + 1);
                const float frac = (float) (pos - (double) i0);
                const float a = buffer.getSample (ch, i0);
                const float b = buffer.getSample (ch, i1);
                out.setSample (ch, i, a + (b - a) * frac);
            }
        }

        buffer = std::move (out);
    }

    if (fx.gainDb != 0.0f)
        buffer.applyGain (juce::Decibels::decibelsToGain (fx.gainDb));

    for (int ch = 0; ch < channels; ++ch)
        juce::FloatVectorOperations::clip (buffer.getWritePointer (ch), buffer.getReadPointer (ch),
                                           -1.0f, 1.0f, buffer.getNumSamples());

    return buffer;
}
