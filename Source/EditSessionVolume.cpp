#include "EditSession.h"

namespace te = tracktion;

// The hidden boost stage, tagged so the FX list can skip it
static te::VolumeAndPanPlugin* findBoost (te::PluginList& list)
{
    for (auto* p : list.getPlugins())
        if (auto* v = dynamic_cast<te::VolumeAndPanPlugin*> (p))
            if ((bool) v->state.getProperty ("kngBoost", false))
                return v;

    return nullptr;
}

static void applyVolume (te::Edit& edit, te::PluginList& list, te::VolumeAndPanPlugin& main, float db)
{
    db = juce::jlimit (-100.0f, 12.0f, db);

    // The bottom of the fader is silence
    if (db <= -59.9f)
        db = -100.0f;

    // The engine's volume stops at +6, so the rest goes to the boost stage
    const float mainDb  = juce::jmin (db, 6.0f);
    const float boostDb = juce::jmax (0.0f, db - 6.0f);

    main.setVolumeDb (mainDb);

    auto* boost = findBoost (list);

    if (boost == nullptr && boostDb > 0.0f)
    {
        auto p = edit.getPluginCache().createNewPlugin (te::VolumeAndPanPlugin::xmlTypeName, juce::PluginDescription());

        if (p != nullptr)
        {
            p->state.setProperty ("kngBoost", true, nullptr);

            // Just before the main volume, so the meters include it
            const int index = list.getPlugins().indexOf (&main);
            list.insertPlugin (p, index >= 0 ? index : -1, nullptr);
            boost = dynamic_cast<te::VolumeAndPanPlugin*> (p.get());
        }
    }

    if (boost != nullptr)
        boost->setVolumeDb (boostDb);
}

static float readVolume (te::PluginList& list, te::VolumeAndPanPlugin& main)
{
    float db = main.getVolumeDb();

    if (auto* boost = findBoost (list))
        db += boost->getVolumeDb();

    return db <= -59.9f ? -60.0f : db;
}

//==============================================================================
float EditSession::getTrackVolumeDb (int trackIndex)
{
    if (auto* at = getTrack (trackIndex))
        if (auto* vp = at->getVolumePlugin())
            return readVolume (at->pluginList, *vp);

    return 0.0f;
}

void EditSession::setTrackVolumeDb (int trackIndex, float db)
{
    if (auto* at = getTrack (trackIndex))
        if (auto* vp = at->getVolumePlugin())
            applyVolume (*edit, at->pluginList, *vp, db);
}

float EditSession::getMasterVolumeDb()
{
    if (auto mv = edit->getMasterVolumePlugin())
        return readVolume (edit->getMasterPluginList(), *mv);

    return 0.0f;
}

void EditSession::setMasterVolumeDb (float db)
{
    if (auto mv = edit->getMasterVolumePlugin())
        applyVolume (*edit, edit->getMasterPluginList(), *mv, db);
}
