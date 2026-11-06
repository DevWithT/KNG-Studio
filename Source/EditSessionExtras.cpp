#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
// Punch in/out

void EditSession::setPunchEnabled (bool shouldPunch)
{
    // No region yet, so make a 4-bar one at the playhead
    if (shouldPunch && getLoopEnd() <= getLoopStart())
    {
        const double bar = 4.0 * 60.0 / juce::jmax (1.0, getBpm());
        const double start = std::floor (getTransport().getPosition().inSeconds() / bar) * bar;
        setLoopRange (start, start + bar * 4.0);
    }

    edit->recordingPunchInOut = shouldPunch;
}

bool EditSession::isPunchEnabled()
{
    return edit->recordingPunchInOut.get();
}

//==============================================================================
// Where FX go: after any instrument or aux return, before everything else

int EditSession::getFirstFxIndex (te::AudioTrack& track)
{
    int index = 0;

    for (int i = 0; i < track.pluginList.size(); ++i)
    {
        auto* p = track.pluginList[i];

        if (p->isSynth() || dynamic_cast<te::AuxReturnPlugin*> (p) != nullptr)
            index = i + 1;
    }

    return index;
}

//==============================================================================
// Track chain templates

juce::File EditSession::getTemplatesFolder()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("KNG Studio")
                      .getChildFile ("Templates");
    folder.createDirectory();
    return folder;
}

juce::StringArray EditSession::getTemplateNames()
{
    juce::StringArray names;

    for (auto& f : getTemplatesFolder().findChildFiles (juce::File::findFiles, false, "*.kngchain"))
        names.add (f.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

bool EditSession::saveChainTemplate (int trackIndex, const juce::String& name)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return false;

    juce::ValueTree chain ("KNGCHAIN");

    // FX only, instruments stay with their track
    for (auto* p : getFxPlugins (track->pluginList))
    {
        if (p->isSynth())
            continue;

        edit->flushPluginStateIfNeeded (*p);
        chain.appendChild (p->state.createCopy(), nullptr);
    }

    const auto safeName = juce::File::createLegalFileName (name.trim());
    auto file = getTemplatesFolder().getChildFile ((safeName.isEmpty() ? juce::String ("Template") : safeName) + ".kngchain");

    if (auto xml = chain.createXml())
        return xml->writeTo (file);

    return false;
}

bool EditSession::loadChainTemplate (int trackIndex, const juce::String& name)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return false;

    auto xml = juce::parseXML (getTemplatesFolder().getChildFile (name + ".kngchain"));

    if (xml == nullptr)
        return false;

    auto chain = juce::ValueTree::fromXml (*xml);

    if (! chain.isValid())
        return false;

    beginEdit();

    // Replace the current FX, but leave an instrument alone
    for (auto* p : getFxPlugins (track->pluginList))
        if (! p->isSynth())
            p->deleteFromParent();

    int index = getFirstFxIndex (*track);

    for (auto child : chain)
    {
        // Fresh ID so it doesn't clash with the original plugin
        auto copy = child.createCopy();
        copy.setProperty (te::IDs::id, edit->createNewItemID().toString(), nullptr);

        if (track->pluginList.insertPlugin (copy, index) != nullptr)
            ++index;
    }

    save();
    return true;
}

//==============================================================================
// KngTune

static bool isKngTuneName (const juce::String& name)
{
    return name.removeCharacters (" _-").containsIgnoreCase ("kngtune");
}

bool EditSession::addKngTune (int trackIndex)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return false;

    // Already on the track, so just open it
    for (auto* p : track->pluginList)
    {
        if (isKngTuneName (p->getName()))
        {
            p->showWindowExplicitly();
            return true;
        }
    }

    for (auto& d : engine.getPluginManager().knownPluginList.getTypes())
    {
        if (d.isInstrument || ! isKngTuneName (d.name))
            continue;

        auto plugin = edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, d);

        if (plugin == nullptr)
            return false;

        beginEdit();
        track->pluginList.insertPlugin (plugin, getFirstFxIndex (*track), nullptr);
        plugin->showWindowExplicitly();
        save();
        return true;
    }

    return false;
}
