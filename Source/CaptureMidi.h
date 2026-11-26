#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

// Edit > Capture MIDI and its shortcut: runs the capture and shows a message if there was nothing to capture.
namespace KngCapture
{
    inline void run (EditSession& session)
    {
        const auto problem = session.captureMidi();

        if (problem.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Capture MIDI", problem);
    }
}
