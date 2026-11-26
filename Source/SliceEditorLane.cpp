#include "SliceEditorView.h"

//==============================================================================
// Chop to Lane button: each slice becomes its own clip on a new lane.

void SliceEditorView::setupLane()
{
    addAndMakeVisible (laneButton);
    laneButton.setTooltip ("Put each slice on a new lane as its own clip, so you can drag, copy and delete them on the timeline. Reverse, pitch, volume and tempo match apply.");
    laneButton.setWantsKeyboardFocus (false);
    laneButton.onClick = [this] { chopToLane(); };
}

void SliceEditorView::chopToLane()
{
    session.setSliceFx (sliceFx);
    session.sliceKeepLengthForNextWrite = ! recordPitchToggle.getToggleState();
    session.setSliceSourceBpm (matchToggle.getToggleState() ? sourceBpm.getValue() : 0.0);

    const int lane = session.chopClipToLane (*clip, getFullCuts());

    session.setSliceFx ({});
    session.sliceKeepLengthForNextWrite = true;
    session.setSliceSourceBpm (0.0);

    if (lane >= 0)
        close();
    else
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Slice Editor",
                                                "Couldn't put the slices on a lane.");
}
