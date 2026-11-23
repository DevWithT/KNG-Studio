#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditSession.h"

// Sidechain source picker shared by every place a plugin slot shows a menu.
namespace KngSidechain
{
    // Adds a "Sidechain" submenu (None plus every other track) to the menu.
    // Does nothing when the plugin has no sidechain input.
    inline void addMenu (juce::PopupMenu& menu, EditSession& session, tracktion::Plugin& plugin)
    {
        if (! plugin.canSidechain())
            return;

        tracktion::Plugin::Ptr ptr (&plugin);
        auto* s = &session;
        const auto current = plugin.getSidechainSourceName();

        juce::PopupMenu sub;

        sub.addItem ("None", true, current.isEmpty(), [s, ptr]
        {
            s->beginEdit();
            ptr->setSidechainSourceByName ("<none>");
            s->save();
        });

        sub.addSeparator();

        for (auto& n : plugin.getSidechainSourceNames (false))
        {
            // Names look like "3. Vocal"; the part after ". " is the track name
            const bool ticked = n.fromFirstOccurrenceOf (". ", false, false) == current;

            sub.addItem (n, true, ticked, [s, ptr, n]
            {
                s->beginEdit();
                ptr->setSidechainSourceByName (n);
                s->save();
            });
        }

        menu.addSubMenu (current.isEmpty() ? juce::String ("Sidechain: None") : "Sidechain: " + current, sub);
    }
}
