#pragma once

#include <tracktion_engine/tracktion_engine.h>

/** KNG plugin panels in their own windows: drag them to a second screen, keep them on top, dock them back. */
namespace KngPopOut
{
    /** Opens the plugin's panel in its own window, or brings its window forward. */
    void open (tracktion::Plugin& plugin);

    /** True when this component is inside a pop-out window. */
    bool isPoppedOut (const juce::Component& component);

    /** Closes every pop-out window (the song is closing). */
    void closeAll();

    /** What Dock does: show the plugin in the bottom panel. Set by the main window. */
    void setDockHandler (std::function<void (tracktion::Plugin&)> handler);
}
