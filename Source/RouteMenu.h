#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

// Where a track's output goes: Master, or another track (a bus).
namespace KngRoute
{
    inline juce::String currentName (tracktion::AudioTrack& track)
    {
        if (auto* dest = track.getOutput().getDestinationTrack())
            return dest->getName();

        return "Master";
    }

    // Adds "Master" plus every track this one can safely feed (no loops) as ticked items.
    inline void addItems (juce::PopupMenu& menu, EditSession& session, tracktion::AudioTrack& track)
    {
        auto* s = &session;
        tracktion::AudioTrack::Ptr from (&track);
        auto* current = track.getOutput().getDestinationTrack();

        menu.addItem ("Master", true, current == nullptr, [s, from]
        {
            s->beginEdit();
            from->getOutput().setOutputToDefaultDevice (false);
            s->save();
        });

        menu.addSeparator();

        for (int i = 0; i < s->getNumTracks(); ++i)
        {
            tracktion::AudioTrack::Ptr dest (s->getTrack (i));

            // Skip itself and anything that already feeds into this track
            if (dest == nullptr || dest == from || dest->getOutput().feedsInto (from.get()))
                continue;

            menu.addItem (dest->getName(), true, dest.get() == current, [s, from, dest]
            {
                s->beginEdit();
                from->getOutput().setOutputToTrack (dest.get());
                s->save();
            });
        }
    }

    // Same list tucked into a submenu called "Output: <where it goes>".
    inline void addMenu (juce::PopupMenu& menu, EditSession& session, tracktion::AudioTrack& track)
    {
        juce::PopupMenu sub;
        addItems (sub, session, track);
        menu.addSubMenu ("Output: " + currentName (track), sub);
    }
}
