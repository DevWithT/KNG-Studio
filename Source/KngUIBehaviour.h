#pragma once

#include <tracktion_engine/tracktion_engine.h>

/** Tells the engine which song is open, so MIDI controller moves and MIDI learn reach it. */
class KngUIBehaviour : public tracktion::UIBehaviour
{
public:
    static void setCurrentEdit (tracktion::Edit* edit)        { current() = edit; }

    tracktion::Edit* getCurrentlyFocusedEdit() override       { return current(); }
    tracktion::Edit* getLastFocusedEdit() override            { return current(); }

private:
    static tracktion::Edit*& current()
    {
        static tracktion::Edit* edit = nullptr;
        return edit;
    }
};
