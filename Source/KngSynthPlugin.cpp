#include "KngSynthPlugin.h"
#include "KngSynthPresets.h"
#include "KngSynthView.h"

namespace te = tracktion;

const char* KngSynthPlugin::xmlTypeName = "kngsynth";

KngSynthPlugin::KngSynthPlugin (te::PluginCreationInfo creationInfo)
    : te::Plugin (creationInfo)
{
    auto* um = getUndoManager();

    // Every knob is an automatable parameter, saved in the plugin's state
    for (int i = 0; i < KngSynth::numParams; ++i)
    {
        const auto& p = KngSynth::info (i);

        juce::NormalisableRange<float> range (p.min, p.max, p.step);

        if (p.mid > p.min && p.mid < p.max)
            range.setSkewForCentre (p.mid);

        auto* value = values.add (new juce::CachedValue<float>());
        value->referTo (state, p.id, um, p.def);

        auto* param = addParam (p.id, p.name, range);
        param->attachToCurrentValue (*value);
        params[(size_t) i] = param;
    }
}

KngSynthPlugin::~KngSynthPlugin()
{
    notifyListenersOfDeletion();

    for (auto& p : params)
        if (p != nullptr)
            p->detachFromCurrentValue();
}

void KngSynthPlugin::initialise (const te::PluginInitialisationInfo& initInfo)
{
    sampleRate = initInfo.sampleRate;
    reset();
}

void KngSynthPlugin::reset()
{
    for (auto& v : voices)
    {
        v.active = false;
        v.amp = {};
        v.filt = {};
        v.filterL.reset();
        v.filterR.reset();
    }

    heldCount = 0;
}

void KngSynthPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    for (int i = 0; i < KngSynth::numParams; ++i)
    {
        const juce::Identifier id (KngSynth::info (i).id);

        if (v.hasProperty (id))
            values[i]->setValue ((float) v.getProperty (id), nullptr);
    }

    for (auto* p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

std::unique_ptr<te::Plugin::EditorComponent> KngSynthPlugin::createEditor()
{
    return std::make_unique<KngSynthView> (*this);
}

void KngSynthPlugin::applyPreset (int presetIndex)
{
    const auto& list = KngSynth::presets();

    if (! juce::isPositiveAndBelow (presetIndex, (int) list.size()))
        return;

    for (int i = 0; i < KngSynth::numParams; ++i)
        params[(size_t) i]->setParameter (KngSynth::info (i).def, juce::sendNotification);

    for (auto& [p, value] : list[(size_t) presetIndex].values)
        params[(size_t) p]->setParameter (value, juce::sendNotification);
}

//==============================================================================
// Notes

void KngSynthPlugin::removeHeld (int note)
{
    int w = 0;

    for (int r = 0; r < heldCount; ++r)
        if (held[(size_t) r] != note)
            held[(size_t) w++] = held[(size_t) r];

    heldCount = w;
}

void KngSynthPlugin::noteOn (int note, float velocity, const Block& b)
{
    using namespace KngSynth;
    const int mode = juce::roundToInt (b.v[voiceMode]);

    if (mode == 0)
    {
        // The same note again: let the old one ring out
        for (auto& v : voices)
        {
            if (v.active && v.note == note && v.amp.stage != Envelope::release)
            {
                v.amp.noteOff();
                v.filt.noteOff();
            }
        }

        Voice* target = nullptr;

        for (auto& v : voices)
        {
            if (! v.active)
            {
                target = &v;
                break;
            }
        }

        // All busy: steal the quietest fading voice, otherwise the oldest
        if (target == nullptr)
        {
            for (auto& v : voices)
                if (v.amp.stage == Envelope::release && (target == nullptr || v.amp.level < target->amp.level))
                    target = &v;

            if (target == nullptr)
            {
                target = &voices[0];

                for (auto& v : voices)
                    if (v.age < target->age)
                        target = &v;
            }

            target->active = false;
        }

        target->start (note, velocity, true, false, ++voiceCounter);
        return;
    }

    // Mono and legato: one voice, the newest note wins
    removeHeld (note);

    if (heldCount < (int) held.size())
        held[(size_t) heldCount++] = note;

    auto& v = voices[0];
    const bool overlapping = v.active && v.amp.stage != Envelope::release;
    const bool slide = overlapping && b.v[glide] > 0.0005f;

    v.start (note, velocity, ! (mode == 2 && overlapping), slide, ++voiceCounter);
}

void KngSynthPlugin::noteOff (int note, const Block& b)
{
    using namespace KngSynth;
    const int mode = juce::roundToInt (b.v[voiceMode]);

    if (mode == 0)
    {
        for (auto& v : voices)
        {
            if (v.active && v.note == note && v.amp.stage != Envelope::release)
            {
                v.amp.noteOff();
                v.filt.noteOff();
            }
        }

        return;
    }

    removeHeld (note);
    auto& v = voices[0];

    if (! v.active || v.note != note)
        return;

    // Another note is still held: go back to it (sliding if glide is on)
    if (heldCount > 0)
    {
        v.start (held[(size_t) heldCount - 1], v.velocity, mode == 1, b.v[glide] > 0.0005f, ++voiceCounter);
    }
    else
    {
        v.amp.noteOff();
        v.filt.noteOff();
    }
}
