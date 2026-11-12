#pragma once

#include <juce_dsp/juce_dsp.h>
#include "KngFaceView.h"

class KngEqPlugin;

/** KNG EQ's face: mint on graphite, numbered band dots you drag over a live spectrum. */
class KngEqView : public KngFaceView
{
public:
    explicit KngEqView (KngEqPlugin& owner);
    ~KngEqView() override = default;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void paintTitle (juce::Graphics&, juce::Rectangle<float> area) override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<float> area) override;
    void tick() override;

    void updateSpectrum();
    void selectBand (int band);
    void rebuildKnobs();
    void showBandMenu (int band);
    void beginEdit();

    float value (int band, int p) const;
    void setValue (int band, int p, float v);
    int bandAt (juce::Point<float> p) const;
    juce::Point<float> handlePos (int band) const;

    juce::Rectangle<float> graph() const   { return displayArea.reduced (4.0f); }
    float freqToX (double f) const;
    double xToFreq (float x) const;
    float gainToY (double db) const;
    double yToGain (float y) const;

    KngEqPlugin& eq;
    int selected = -1, dragging = -1, builtFor = -2;
    KngFaceKnob* gainKnob = nullptr;

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ring = std::vector<float> ((size_t) fftSize, 0.0f);
    int ringPos = 0;
    std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f);
    std::vector<float> spectrumDb = std::vector<float> ((size_t) fftSize / 2, -100.0f);
    std::vector<float> pullBuffer = std::vector<float> (8192, 0.0f);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KngEqView)
};
