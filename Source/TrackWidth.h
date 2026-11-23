#pragma once

#include "EditSession.h"
#include "KngWidthPlugin.h"

// A track's Width control. It drives a hidden KNG Width stage that sits right after the track's volume.
// The stage is only made the first time the width leaves 100%.
namespace KngTrackWidth
{
    inline tracktion::Plugin* find (tracktion::AudioTrack& track)
    {
        for (auto* p : track.pluginList)
            if ((bool) p->state.getProperty ("kngWidthStage", false))
                return p;

        return nullptr;
    }

    // 1.0 is untouched, 0.0 is mono, 2.0 is double width
    inline float get (tracktion::AudioTrack& track)
    {
        if (auto* fx = dynamic_cast<KngWidthPlugin*> (find (track)))
        {
            auto prm = fx->getParam (KngWidthPlugin::width);

            if (prm != nullptr)
                return prm->getCurrentValue();
        }

        return 1.0f;
    }

    inline void set (EditSession& session, tracktion::AudioTrack& track, float width)
    {
        auto* existing = find (track);

        if (existing == nullptr)
        {
            if (std::abs (width - 1.0f) < 0.005f)
                return;

            auto p = session.getEdit().getPluginCache().createNewPlugin (KngWidthPlugin::xmlTypeName, juce::PluginDescription());

            if (p == nullptr)
                return;

            p->state.setProperty ("kngWidthStage", true, nullptr);

            // Right after the volume stage
            int index = track.pluginList.size();

            if (auto* vp = track.getVolumePlugin())
            {
                for (int i = 0; i < track.pluginList.size(); ++i)
                {
                    if (track.pluginList[i] == vp)
                    {
                        index = i + 1;
                        break;
                    }
                }
            }

            track.pluginList.insertPlugin (p, index, nullptr);
            existing = p.get();

            // Mono Bass off, so only the width changes
            if (auto* fx = dynamic_cast<KngWidthPlugin*> (existing))
            {
                auto mb = fx->getParam (KngWidthPlugin::monoBass);

                if (mb != nullptr)
                    mb->setParameter (20.0f, juce::sendNotification);
            }
        }

        if (auto* fx = dynamic_cast<KngWidthPlugin*> (existing))
        {
            auto prm = fx->getParam (KngWidthPlugin::width);

            if (prm != nullptr)
                prm->setParameter (width, juce::sendNotification);
        }
    }
}
