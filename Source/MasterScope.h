#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "EditSession.h"
#include "KngLoudnessPlugin.h"

/** The master output, drawn live: the waveform and the spectrum. The small one sits in the master strip.
    View > Master Output... opens a big one. It reads the newest samples from the loudness meter on the master. */
class MasterScope : public juce::Component,
                    public juce::SettableTooltipClient,
                    private juce::Timer
{
public:
    explicit MasterScope (EditSession& sessionToUse)
        : session (sessionToUse),
          fft (fftOrder),
          window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false)
    {
        setTooltip ("Master output: waveform and spectrum. View > Master Output opens a bigger one.");
        levels.fill (floorDb);
        startTimerHz (30);
    }

    ~MasterScope() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff1b1d20));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

        const bool big = getHeight() > 120;
        auto area = getLocalBounds().reduced (big ? 10 : 3).toFloat();
        const bool stacked = (float) getHeight() > (float) getWidth() * 0.35f;

        juce::Rectangle<float> scopeArea, specArea;

        if (stacked)
        {
            scopeArea = area.removeFromTop (area.getHeight() * 0.34f);
            area.removeFromTop (8.0f);
            specArea = area;
        }
        else
        {
            scopeArea = area.removeFromLeft (area.getWidth() * 0.4f);
            area.removeFromLeft (4.0f);
            specArea = area;
        }

        drawScope (g, scopeArea, big);
        drawSpectrum (g, specArea, big);
    }

    /** Opens the big window, or brings the open one to the front. Returns the new window, or nullptr if one was already open. */
    static juce::DialogWindow* open (EditSession& session)
    {
        static juce::Component::SafePointer<juce::DialogWindow> existing;

        if (existing != nullptr)
        {
            existing->setVisible (true);
            existing->toFront (true);
            return nullptr;
        }

        auto* content = new MasterScope (session);
        content->setSize (640, 340);

        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (content);
        o.dialogTitle = "Master Output";
        o.dialogBackgroundColour = juce::Colour (0xff1e2124);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = true;

        auto* w = o.create();
        w->setVisible (true);
        existing = w;
        return w;
    }

private:
    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;   // 4096
    static constexpr int scopeLen = 1024;
    static constexpr int bars = 40;
    static constexpr float floorDb = -90.0f;
    static constexpr double minHz = 30.0, maxHz = 18000.0;

    void timerCallback() override
    {
        auto* meter = session.getLoudnessMeter();

        if (meter == nullptr)
            return;

        const auto stamp = meter->copyRecent (left.data(), right.data(), fftSize);
        const bool fresh = stamp != lastStamp;
        lastStamp = stamp;

        sampleRate = meter->getScopeSampleRate();

        if (sampleRate < 8000.0)
            sampleRate = 44100.0;

        if (fresh)
        {
            analyse();
        }
        else
        {
            // Nothing is playing: let the bars fall
            for (auto& v : levels)
                v = juce::jmax (floorDb, v - 3.0f);
        }

        repaint();
    }

    void analyse()
    {
        // The waveform starts at an upward zero crossing, so it holds still
        scopeStart = 0;

        for (int i = fftSize - scopeLen; i > 0; --i)
        {
            const auto a = left[(size_t) i - 1] + right[(size_t) i - 1];
            const auto b = left[(size_t) i] + right[(size_t) i];

            if (a < 0.0f && b >= 0.0f)
            {
                scopeStart = i;
                break;
            }
        }

        // How alike left and right are: +1 is mono, 0 is wide, below 0 can cancel in mono
        double ll = 0.0, rr = 0.0, lr = 0.0;

        for (int i = 0; i < fftSize; ++i)
        {
            const double l = left[(size_t) i], r = right[(size_t) i];
            ll += l * l;
            rr += r * r;
            lr += l * r;
        }

        const float corr = (ll > 1.0e-9 && rr > 1.0e-9) ? (float) (lr / std::sqrt (ll * rr)) : 0.0f;
        correlation += (corr - correlation) * 0.2f;

        // The spectrum
        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = 0.5f * (left[(size_t) i] + right[(size_t) i]);

        for (int i = fftSize; i < fftSize * 2; ++i)
            fftData[(size_t) i] = 0.0f;

        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const double binHz = sampleRate / (double) fftSize;
        const double ratio = std::pow (maxHz / minHz, 1.0 / (double) bars);

        for (int b = 0; b < bars; ++b)
        {
            const double f0 = minHz * std::pow (ratio, (double) b);
            const double f1 = f0 * ratio;
            const int b0 = juce::jlimit (1, fftSize / 2 - 1, (int) std::floor (f0 / binHz));
            const int b1 = juce::jlimit (b0 + 1, fftSize / 2, (int) std::ceil (f1 / binHz));

            float mag = 0.0f;

            for (int k = b0; k < b1; ++k)
                mag = juce::jmax (mag, fftData[(size_t) k]);

            // A full-scale sine reads about 0 dB. A gentle tilt (3 dB per octave) makes music look level.
            float db = juce::Decibels::gainToDecibels (mag / ((float) fftSize * 0.25f), floorDb);
            db += (float) (3.0 * std::log2 (std::sqrt (f0 * f1) / 1000.0));
            db = juce::jlimit (floorDb, 0.0f, db);

            auto& lv = levels[(size_t) b];
            lv += (db - lv) * (db > lv ? 0.6f : 0.15f);
        }
    }

    void drawScope (juce::Graphics& g, juce::Rectangle<float> a, bool big)
    {
        const float mid = a.getCentreY();
        const float half = a.getHeight() * 0.5f;

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRect (a.getX(), mid, a.getWidth(), 1.0f);

        auto trace = [&] (const std::array<float, (size_t) fftSize>& s, juce::Colour colour)
        {
            juce::Path p;
            const int w = juce::jmax (2, (int) a.getWidth());

            for (int x = 0; x < w; ++x)
            {
                const int idx = scopeStart + (int) ((double) x * (scopeLen - 1) / (double) (w - 1));
                const float y = mid - juce::jlimit (-1.0f, 1.0f, s[(size_t) idx]) * half * 0.9f;

                if (x == 0)
                    p.startNewSubPath (a.getX(), y);
                else
                    p.lineTo (a.getX() + (float) x, y);
            }

            g.setColour (colour);
            g.strokePath (p, juce::PathStrokeType (1.2f));
        };

        trace (left, juce::Colour (0xff5aa8e6));
        trace (right, juce::Colour (0xffe6a23a).withAlpha (0.8f));

        if (big)
        {
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.setColour (correlation < 0.0f ? juce::Colour (0xffe6503a) : juce::Colours::white.withAlpha (0.6f));
            g.drawText ("L/R match " + juce::String (correlation, 2) + (correlation < 0.0f ? "  (out of phase)" : ""),
                        a.reduced (4.0f, 2.0f).toNearestInt(), juce::Justification::topRight);
        }
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> a, bool big)
    {
        juce::Rectangle<float> labelArea;

        if (big)
            labelArea = a.removeFromBottom (16.0f);

        auto yFor = [&] (float db) { return a.getBottom() - juce::jmap (db, floorDb, 0.0f, 0.0f, a.getHeight()); };

        if (big)
        {
            g.setFont (juce::Font (juce::FontOptions (10.0f)));

            for (float db : { -20.0f, -40.0f, -60.0f })
            {
                g.setColour (juce::Colours::white.withAlpha (0.07f));
                g.fillRect (a.getX(), yFor (db), a.getWidth(), 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.3f));
                g.drawText (juce::String ((int) db), (int) a.getX() + 2, (int) yFor (db) - 12, 30, 12, juce::Justification::centredLeft);
            }

            for (double f : { 100.0, 1000.0, 10000.0 })
            {
                const float x = a.getX() + a.getWidth() * (float) (std::log (f / minHz) / std::log (maxHz / minHz));
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + "k" : juce::String ((int) f),
                            (int) x - 16, (int) labelArea.getY() + 2, 32, 12, juce::Justification::centred);
            }
        }

        const float w = a.getWidth() / (float) bars;

        for (int b = 0; b < bars; ++b)
        {
            const float db = levels[(size_t) b];
            const float h = juce::jmap (db, floorDb, 0.0f, 0.0f, a.getHeight());

            if (h < 0.5f)
                continue;

            g.setColour (juce::Colour (0xff5ad45a).interpolatedWith (juce::Colour (0xffe6503a), juce::jlimit (0.0f, 1.0f, (db + 30.0f) / 30.0f)));
            g.fillRect (a.getX() + (float) b * w + 0.5f, a.getBottom() - h, juce::jmax (1.0f, w - 1.0f), h);
        }
    }

    EditSession& session;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, (size_t) fftSize> left {}, right {};
    std::array<float, (size_t) fftSize * 2> fftData {};
    std::array<float, (size_t) bars> levels {};

    juce::uint32 lastStamp = 0;
    double sampleRate = 44100.0;
    int scopeStart = 0;
    float correlation = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterScope)
};
