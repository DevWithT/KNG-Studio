#include "AutomationLane.h"

namespace te = tracktion;

//==============================================================================
AutomationHeader::AutomationHeader (EditSession& sessionToUse, int trackIndexToUse)
    : session (sessionToUse), trackIndex (trackIndexToUse)
{
    addAndMakeVisible (label);
    label.setFont (juce::Font (juce::FontOptions (12.0f)));

    addAndMakeVisible (paramBox);
    paramBox.onChange = [this]
    {
        const int sel = paramBox.getSelectedId() - 1;

        if (juce::isPositiveAndBelow (sel, lastIds.size()))
            session.setAutomationParamId (trackIndex, lastIds[sel]);
    };

    addAndMakeVisible (clearButton);
    clearButton.onClick = [this]
    {
        if (auto* param = session.getAutomationParam (trackIndex))
        {
            session.beginEdit();
            param->getCurve().clear (&session.getEdit().getUndoManager());
            session.save();
        }
    };

    refreshList();
    startTimerHz (2);
}

AutomationHeader::~AutomationHeader()
{
    stopTimer();
}

void AutomationHeader::refreshList()
{
    auto targets = session.getAutomationTargets (trackIndex);

    juce::StringArray ids;

    for (auto& t : targets)
        ids.add (t.id);

    const auto currentId = session.getAutomationParamId (trackIndex);

    if (ids != lastIds)
    {
        lastIds = ids;
        paramBox.clear (juce::dontSendNotification);

        for (int i = 0; i < (int) targets.size(); ++i)
            paramBox.addItem (targets[(size_t) i].name, i + 1);
    }

    if (! paramBox.isPopupActive())
    {
        const int index = lastIds.indexOf (currentId);
        paramBox.setSelectedId (index >= 0 ? index + 1 : 1, juce::dontSendNotification);
    }
}

void AutomationHeader::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff2e3236));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
}

void AutomationHeader::resized()
{
    auto r = getLocalBounds().reduced (6);

    auto top = r.removeFromTop (18);
    clearButton.setBounds (top.removeFromRight (50));
    label.setBounds (top);

    r.removeFromTop (4);
    paramBox.setBounds (r.removeFromTop (24));
}

//==============================================================================
AutomationLane::AutomationLane (EditSession& sessionToUse, int trackIndexToUse)
    : session (sessionToUse), trackIndex (trackIndexToUse)
{
    startTimerHz (15);
}

AutomationLane::~AutomationLane()
{
    stopTimer();
}

int AutomationLane::valueToY (float value, juce::Range<float> range) const
{
    const float norm = (value - range.getStart()) / juce::jmax (1.0e-6f, range.getLength());
    return 4 + juce::roundToInt ((1.0f - juce::jlimit (0.0f, 1.0f, norm)) * (getHeight() - 8));
}

float AutomationLane::yToValue (int y, juce::Range<float> range) const
{
    const float norm = 1.0f - juce::jlimit (0.0f, 1.0f, (y - 4) / (float) juce::jmax (1, getHeight() - 8));
    return range.getStart() + norm * range.getLength();
}

int AutomationLane::findPointAt (te::AutomationCurve& curve, juce::Point<int> p, juce::Range<float> range)
{
    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        const int x = session.timeToX (curve.getPointTime (i).inSeconds(), getWidth());
        const int y = valueToY (curve.getPointValue (i), range);

        if (std::abs (x - p.x) <= 7 && std::abs (y - p.y) <= 7)
            return i;
    }

    return -1;
}

void AutomationLane::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff232629));

    auto* param = session.getAutomationParam (trackIndex);

    if (param == nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.drawText ("Nothing to automate on this track", getLocalBounds(), juce::Justification::centred);
        return;
    }

    auto& curve = param->getCurve();
    const auto range = param->getValueRange();
    const float fallback = param->getDefaultValue().value_or (param->getCurrentValue());

    // The line
    juce::Path path;

    for (int x = 0; x <= getWidth(); x += 3)
    {
        const double t = juce::jmax (0.0, session.xToTime (x, getWidth()));
        const float v = curve.getNumPoints() > 0
                          ? curve.getValueAt (te::TimePosition::fromSeconds (t), fallback)
                          : param->getCurrentValue();

        const float y = (float) valueToY (v, range);

        if (x == 0)
            path.startNewSubPath ((float) x, y);
        else
            path.lineTo ((float) x, y);
    }

    g.setColour (juce::Colour (0xffe6a23a));
    g.strokePath (path, juce::PathStrokeType (2.0f));

    // The points
    g.setColour (juce::Colours::white);

    for (int i = 0; i < curve.getNumPoints(); ++i)
    {
        const int x = session.timeToX (curve.getPointTime (i).inSeconds(), getWidth());
        const int y = valueToY (curve.getPointValue (i), range);
        g.fillEllipse ((float) x - 4.0f, (float) y - 4.0f, 8.0f, 8.0f);
    }

    // Current value
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (param->getParameterName() + "  " + param->getCurrentValueAsString(),
                getLocalBounds().reduced (6, 2).removeFromTop (14), juce::Justification::left);

    // Playhead
    const int px = session.timeToX (session.getTransport().getPosition().inSeconds(), getWidth());
    g.setColour (juce::Colours::yellow);
    g.fillRect (px, 0, 2, getHeight());
}

void AutomationLane::mouseDown (const juce::MouseEvent& e)
{
    if (session.getTransport().isRecording())
        return;

    auto* param = session.getAutomationParam (trackIndex);

    if (param == nullptr)
        return;

    auto& curve = param->getCurve();
    const auto range = param->getValueRange();
    auto* um = &session.getEdit().getUndoManager();

    const int index = findPointAt (curve, e.getPosition(), range);

    // Right-click a point to delete it
    if (e.mods.isPopupMenu())
    {
        if (index >= 0)
        {
            session.beginEdit();
            curve.removePoint (index, um);
            session.save();
        }

        return;
    }

    session.beginEdit();

    if (index >= 0)
    {
        dragIndex = index;
    }
    else
    {
        double t = juce::jmax (0.0, session.xToTime (e.x, getWidth()));

        if (session.isSnapEnabled() && ! e.mods.isAltDown())
            t = session.snapTime (t);

        dragIndex = curve.addPoint (te::TimePosition::fromSeconds (t), yToValue (e.y, range), 0.0f, um);
    }

    repaint();
}

void AutomationLane::mouseDrag (const juce::MouseEvent& e)
{
    if (dragIndex < 0)
        return;

    auto* param = session.getAutomationParam (trackIndex);

    if (param == nullptr)
        return;

    auto& curve = param->getCurve();
    const auto range = param->getValueRange();

    double t = juce::jmax (0.0, session.xToTime (e.x, getWidth()));

    if (session.isSnapEnabled() && ! e.mods.isAltDown())
        t = session.snapTime (t);

    dragIndex = curve.movePoint (*param, dragIndex, te::TimePosition::fromSeconds (t),
                                 yToValue (e.y, range), false, &session.getEdit().getUndoManager());
    repaint();
}

void AutomationLane::mouseUp (const juce::MouseEvent&)
{
    if (dragIndex >= 0)
        session.save();

    dragIndex = -1;
}

void AutomationLane::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown())
    {
        if (wheel.deltaY != 0.0f)
            session.zoomAround (session.xToTime (e.x, getWidth()), wheel.deltaY > 0.0f ? 0.8 : 1.25);

        return;
    }

    if (e.mods.isShiftDown() || std::abs (wheel.deltaX) > std::abs (wheel.deltaY))
    {
        const float d = wheel.deltaX != 0.0f ? wheel.deltaX : wheel.deltaY;
        session.scrollBy (-d * session.getViewLength() * 0.5);
        return;
    }

    juce::Component::mouseWheelMove (e, wheel);
}
