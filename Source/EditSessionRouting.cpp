#include "EditSession.h"

namespace te = tracktion;

static te::AuxReturnPlugin* findReturn (te::AudioTrack& track)
{
    return track.pluginList.findFirstPluginOfType<te::AuxReturnPlugin>();
}

static te::AuxSendPlugin* findSendTo (te::AudioTrack& track, int bus)
{
    for (auto* p : track.pluginList)
        if (auto* send = dynamic_cast<te::AuxSendPlugin*> (p))
            if (send->busNumber.get() == bus)
                return send;

    return nullptr;
}

static te::CompressorPlugin* findDuck (te::AudioTrack& track)
{
    for (auto* p : track.pluginList)
        if (auto* c = dynamic_cast<te::CompressorPlugin*> (p))
            if ((bool) c->state.getProperty ("kngDuck", false))
                return c;

    return nullptr;
}

// Where effects go: before the volume, meter and sends
static int firstUtilityIndex (te::PluginList& list)
{
    for (int i = 0; i < list.size(); ++i)
    {
        auto* p = list[i];

        if (dynamic_cast<te::VolumeAndPanPlugin*> (p) != nullptr
             || dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr
             || dynamic_cast<te::AuxSendPlugin*> (p) != nullptr)
            return i;
    }

    return list.size();
}

//==============================================================================
// Buses and sends

juce::Array<int> EditSession::getBusNumbers()
{
    juce::Array<int> buses;

    for (int i = 0; i < getNumTracks(); ++i)
        if (auto* t = getTrack (i))
            if (auto* r = findReturn (*t))
                buses.addIfNotAlreadyThere (r->busNumber.get());

    buses.sort();
    return buses;
}

juce::String EditSession::getBusName (int bus)
{
    const auto name = edit->getAuxBusName (bus);

    if (name.isNotEmpty())
        return name;

    return bus == 0 ? juce::String ("Reverb Bus") : "Bus " + juce::String (bus + 1);
}

int EditSession::addBus (const juce::String& name)
{
    int next = 0;

    for (int b : getBusNumbers())
        next = juce::jmax (next, b + 1);

    beginEdit();
    addTrack();

    auto* track = getTrack (getNumTracks() - 1);

    if (track == nullptr)
        return -1;

    track->setName (name);

    auto p = edit->getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, juce::PluginDescription());

    if (auto* r = dynamic_cast<te::AuxReturnPlugin*> (p.get()))
    {
        r->busNumber = next;
        track->pluginList.insertPlugin (p, 0, nullptr);
    }

    edit->setAuxBusName (next, name);
    save();
    return next;
}

float EditSession::getSendToBusDb (int trackIndex, int bus)
{
    if (auto* t = getTrack (trackIndex))
        if (auto* send = findSendTo (*t, bus))
            return send->getGainDb();

    return -60.0f;
}

void EditSession::setSendToBusDb (int trackIndex, int bus, float db)
{
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return;

    // A bus can't send to itself
    if (auto* r = findReturn (*track))
        if (r->busNumber.get() == bus)
            return;

    auto* send = findSendTo (*track, bus);

    if (send == nullptr)
    {
        if (db <= -60.0f)
            return;

        auto p = edit->getPluginCache().createNewPlugin (te::AuxSendPlugin::create());

        if (auto* s = dynamic_cast<te::AuxSendPlugin*> (p.get()))
        {
            s->busNumber = bus;

            // Post-fader: goes after the volume and meter
            track->pluginList.insertPlugin (p, track->pluginList.size(), nullptr);
            send = s;
        }
    }

    if (send != nullptr)
        send->setGainDb (db);
}

//==============================================================================
// Sidechain ducking

void EditSession::duckFromTrack (int targetIndex, int sourceIndex)
{
    auto* target = getTrack (targetIndex);
    auto* source = getTrack (sourceIndex);

    if (target == nullptr || source == nullptr || target == source)
        return;

    beginEdit();

    auto* comp = findDuck (*target);

    if (comp == nullptr)
    {
        auto p = edit->getPluginCache().createNewPlugin (te::CompressorPlugin::xmlTypeName, juce::PluginDescription());
        comp = dynamic_cast<te::CompressorPlugin*> (p.get());

        if (comp == nullptr)
            return;

        comp->state.setProperty ("kngDuck", true, nullptr);
        target->pluginList.insertPlugin (p, firstUtilityIndex (target->pluginList), nullptr);
    }

    // Listen to the source track instead of this track's own audio
    comp->setSidechainSourceID (source->itemID);
    comp->guessSidechainRouting();
    comp->useSidechainTrigger = true;

    // A strong duck to start from; tweak it in the FX tab
    if (comp->thresholdGain != nullptr)
        comp->thresholdGain->setParameter (0.1f, juce::sendNotification);

    save();
}

void EditSession::removeDucking (int targetIndex)
{
    if (auto* t = getTrack (targetIndex))
    {
        if (auto* comp = findDuck (*t))
        {
            beginEdit();
            comp->deleteFromParent();
            save();
        }
    }
}

int EditSession::getDuckSource (int targetIndex)
{
    auto* t = getTrack (targetIndex);

    if (t == nullptr)
        return -1;

    auto* comp = findDuck (*t);

    if (comp == nullptr)
        return -1;

    const auto sourceID = comp->getSidechainSourceID();

    for (int i = 0; i < getNumTracks(); ++i)
        if (auto* other = getTrack (i))
            if (other->itemID == sourceID)
                return i;

    return -1;
}
