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

static te::AudioTrack* findBusTrack (EditSession& s, int bus)
{
    for (int i = 0; i < s.getNumTracks(); ++i)
        if (auto* t = s.getTrack (i))
            if (auto* r = findReturn (*t))
                if (r->busNumber.get() == bus)
                    return t;

    return nullptr;
}

// True if sound from `from` ends up in `to`, by its output or by any send
static bool reaches (EditSession& s, te::AudioTrack& from, te::AudioTrack& to, int depth)
{
    if (&from == &to)
        return true;

    if (depth > 16)
        return true;   // a runaway chain counts as a loop

    if (auto* dest = from.getOutput().getDestinationTrack())
        if (reaches (s, *dest, to, depth + 1))
            return true;

    for (auto* p : from.pluginList)
        if (auto* send = dynamic_cast<te::AuxSendPlugin*> (p))
            if (auto* busTrack = findBusTrack (s, send->busNumber.get()))
                if (reaches (s, *busTrack, to, depth + 1))
                    return true;

    return false;
}

static int indexInList (te::PluginList& list, te::Plugin* plugin)
{
    for (int i = 0; i < list.size(); ++i)
        if (list[i] == plugin)
            return i;

    return -1;
}

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

    // Never make a loop (A to B and B back to A)
    if (auto* dest = findBusTrack (*this, bus))
        if (dest == track || reaches (*this, *dest, *track, 0))
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
    comp->thresholdGain.setParameter (0.1f, juce::sendNotification);

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

//==============================================================================
// Output routing and send controls

juce::Array<int> EditSession::getSendBuses (int trackIndex)
{
    juce::Array<int> result;

    if (auto* t = getTrack (trackIndex))
        for (auto* p : t->pluginList)
            if (auto* send = dynamic_cast<te::AuxSendPlugin*> (p))
                result.addIfNotAlreadyThere (send->busNumber.get());

    return result;
}

bool EditSession::isSendPreFader (int trackIndex, int bus)
{
    if (auto* t = getTrack (trackIndex))
        if (auto* send = findSendTo (*t, bus))
            if (auto* vp = t->getVolumePlugin())
                return indexInList (t->pluginList, send) < indexInList (t->pluginList, vp);

    return false;
}

void EditSession::setSendPreFader (int trackIndex, int bus, bool preFader)
{
    auto* t = getTrack (trackIndex);

    if (t == nullptr)
        return;

    auto* send = findSendTo (*t, bus);
    auto* vp = t->getVolumePlugin();

    if (send == nullptr || vp == nullptr || isSendPreFader (trackIndex, bus) == preFader)
        return;

    auto& list = t->pluginList;
    const int from = list.state.indexOf (send->state);
    const int volumeIndex = list.state.indexOf (vp->state);

    if (from < 0 || volumeIndex < 0)
        return;

    beginEdit();

    // A send taps the sound where it sits in the chain: before the volume = pre-fader, at the end = post-fader
    if (preFader)
        list.state.moveChild (from, volumeIndex, &edit->getUndoManager());
    else
        list.state.moveChild (from, -1, &edit->getUndoManager());

    save();
}

bool EditSession::isSendMuted (int trackIndex, int bus)
{
    if (auto* t = getTrack (trackIndex))
        if (auto* send = findSendTo (*t, bus))
            return ! send->isEnabled();

    return false;
}

void EditSession::setSendMuted (int trackIndex, int bus, bool shouldMute)
{
    if (auto* t = getTrack (trackIndex))
    {
        if (auto* send = findSendTo (*t, bus))
        {
            beginEdit();
            send->setEnabled (! shouldMute);
            save();
        }
    }
}

void EditSession::removeSend (int trackIndex, int bus)
{
    if (auto* t = getTrack (trackIndex))
    {
        if (auto* send = findSendTo (*t, bus))
        {
            beginEdit();
            send->deleteFromParent();
            save();
        }
    }
}

juce::Array<int> EditSession::getBusTargets (int trackIndex)
{
    juce::Array<int> result;
    auto* track = getTrack (trackIndex);

    if (track == nullptr)
        return result;

    for (int bus : getBusNumbers())
        if (auto* dest = findBusTrack (*this, bus))
            if (dest != track && ! reaches (*this, *dest, *track, 0))
                result.add (bus);

    return result;
}

juce::StringArray EditSession::getBusSources (int bus)
{
    juce::StringArray names;
    auto* busTrack = findBusTrack (*this, bus);

    if (busTrack == nullptr)
        return names;

    for (int i = 0; i < getNumTracks(); ++i)
    {
        auto* t = getTrack (i);

        if (t == nullptr || t == busTrack)
            continue;

        bool feeds = t->getOutput().getDestinationTrack() == busTrack;

        for (auto* p : t->pluginList)
            if (auto* send = dynamic_cast<te::AuxSendPlugin*> (p))
                if (send->busNumber.get() == bus)
                    feeds = true;

        if (feeds)
            names.addIfNotAlreadyThere (t->getName());
    }

    return names;
}

bool EditSession::canFeed (te::AudioTrack& from, te::AudioTrack& dest)
{
    return &from != &dest && ! reaches (*this, dest, from, 0);
}
