#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

class CompView : public juce::Component,
                 private juce::Timer
{
public:
    CompView (EditSession& sessionToUse, tracktion::WaveAudioClip& clipToComp);
    ~CompView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    struct Section
    {
        double start, end;
        int take;
    };

    void timerCallback() override;
    tracktion::WaveCompManager& cm();
    std::vector<Section> getSections();

    void ensureComp();
    void useTakeForRange (int takeIndex, double t1, double t2);
    void useTakeAt (int takeIndex, double t);
    void flatten();

    juce::Rectangle<int> getRowArea (int row) const;   // row 0 is the comp, 1+ are takes
    int rowAt (int y) const;
    int timeToX (double t) const;
    double xToTime (int x) const;
    static juce::Colour colourForTake (int take);

    EditSession& session;
    tracktion::WaveAudioClip::Ptr clip;
    juce::OwnedArray<tracktion::SmartThumbnail> thumbs;
    juce::Range<double> range;
    int numTakes = 0;

    juce::TextButton flattenButton { "Flatten" };
    juce::Label hintLabel;

    int dragTake = -1;
    double dragStart = 0.0, dragEnd = 0.0;

    static constexpr int toolbarHeight = 34;
    static constexpr int labelWidth    = 70;
    static constexpr int rowHeight     = 60;
    static constexpr int rowGap        = 4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompView)
};

//==============================================================================
class CompWindow : public juce::DocumentWindow
{
public:
    CompWindow (EditSession& session, tracktion::WaveAudioClip& clip);

    void closeButtonPressed() override              { setVisible (false); }
    tracktion::WaveAudioClip* getClip() const       { return clip.get(); }

private:
    tracktion::WaveAudioClip::Ptr clip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompWindow)
};
