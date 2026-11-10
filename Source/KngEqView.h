#pragma once

#include <tracktion_engine/tracktion_engine.h>
#include <juce_dsp/juce_dsp.h>

class KngEqPlugin;

/** KNG EQ panel: draggable band dots over a live spectrum, with controls for the selected band. */
class KngEqView : public tracktion::Plugin::EditorComponent,
                  private juce::Timer
{
public:
    explicit KngEqView (KngEqPlugin& pluginToEdit);
    ~KngEqView() override;

    bool allowWindowResizing() override                            { return true; }
    juce::ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;
    void updateSpectrum();
    void syncControls();
    void selectBand (int band);
    void showBandMenu (int band);
    void beginEdit();

    float value (int band, int p) const;
    void setValue (int band, int p, float v);
    int bandAt (juce::Point<float> p) const;
    juce::Point<float> handlePos (int band) const;

    float freqToX (double f) const;
    double xToFreq (float x) const;
    float gainToY (double db) const;
    double yToGain (float y) const;

    KngEqPlugin& plugin;
    tracktion::Plugin::Ptr pluginRef;
    juce::Rectangle<float> graph;
    int selected = -1, dragging = -1;

    juce::Label bandLabel;
    juce::ToggleButton onButton { "On" };
    juce::ComboBox typeBox, slopeBox;
    juce::Slider freqKnob, gainKnob, qKnob, outKnob;
    juce::Label freqLabel { {}, "Freq" }, gainLabel { {}, "Gain" }, qLabel { {}, "Q" }, outLabel { {}, "Output" };

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
