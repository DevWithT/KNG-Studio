#include "EditSession.h"

namespace te = tracktion;

//==============================================================================
std::vector<EditSession::AutomationTarget> EditSession::getAutomationTargets (int trackIndex)
{
    std::vector<AutomationTarget> result;
    auto* t = getTrack (trackIndex);

    if (t == nullptr)
        return result;

    if (auto* vp = t->getVolumePlugin())
    {
        result.push_back ({ "vol", "Volume", vp->volParam.get() });
        result.push_back ({ "pan", "Pan", vp->panParam.get() });
    }

    if (auto* send = findSend (*t))
        result.push_back ({ "send", "Reverb Send", send->gain.get() });

    // Every parameter on the track's instrument and FX (capped so huge plugins stay usable)
    for (auto* p : getFxPlugins (t->pluginList))
    {
        auto params = p->getAutomatableParameters();
        const int count = juce::jmin (params.size(), 200);

        for (int k = 0; k < count; ++k)
            result.push_back ({ "p:" + p->itemID.toString() + ":" + juce::String (k),
                                p->getName() + ": " + params[k]->getParameterName(),
                                params[k] });
    }

    return result;
}

te::AutomatableParameter* EditSession::getAutomationParam (int trackIndex)
{
    auto targets = getAutomationTargets (trackIndex);

    if (targets.empty())
        return nullptr;

    const auto id = getAutomationParamId (trackIndex);

    for (auto& target : targets)
        if (target.id == id)
            return target.param;

    return targets.front().param;
}

juce::String EditSession::getAutomationParamId (int trackIndex)
{
    if (auto* t = getTrack (trackIndex))
        return t->state.getProperty ("kngAutoParam", "vol").toString();

    return "vol";
}

void EditSession::setAutomationParamId (int trackIndex, const juce::String& id)
{
    if (auto* t = getTrack (trackIndex))
        t->state.setProperty ("kngAutoParam", id, nullptr);
}

bool EditSession::isAutomationShown (int trackIndex)
{
    if (auto* t = getTrack (trackIndex))
        return (bool) t->state.getProperty ("kngAutoShown", false);

    return false;
}

void EditSession::setAutomationShown (int trackIndex, bool shouldShow)
{
    if (auto* t = getTrack (trackIndex))
        t->state.setProperty ("kngAutoShown", shouldShow, nullptr);
}

void EditSession::setAutomationWrite (bool shouldWrite)
{
    edit->getAutomationRecordManager().setWritingAutomation (shouldWrite);
}

bool EditSession::isAutomationWrite()
{
    return edit->getAutomationRecordManager().isWritingAutomation();
}
