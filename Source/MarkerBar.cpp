#include "MarkerBar.h"

namespace te = tracktion;

MarkerBar::MarkerBar (EditSession& sessionToUse)
    : session (sessionToUse)
{
    startTimerHz (10);
}

MarkerBar::~MarkerBar()
{
    stopTimer();
}

int MarkerBar::getLabelWidth (te::Clip& marker) const
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (juce::Font (juce::FontOptions (11.0f)), marker.getName(), 0.0f, 0.0f);
    return juce::roundToInt (ga.getBoundingBox (0, -1, true).getWidth()) + 10;
}

te::Clip* MarkerBar::findMarkerAt (int x)
{
    for (auto& m : session.getMarkers())
    {
        const int mx = session.timeToX (m->getPosition().getStart().inSeconds(), getWidth());

        if (x >= mx - 3 && x <= mx + getLabelWidth (*m))
            return m.get();
    }

    return nullptr;
}

void MarkerBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d20));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    for (auto& m : session.getMarkers())
    {
        const int x = session.timeToX (m->getPosition().getStart().inSeconds(), getWidth());
        const int w = getLabelWidth (*m);

        if (x + w < 0 || x > getWidth())
            continue;

        g.setColour (juce::Colour (0xffe6a23a));
        g.fillRect (x, 0, 2, getHeight());
        g.fillRoundedRectangle ((float) x, 1.0f, (float) w, (float) getHeight() - 2.0f, 3.0f);

        g.setColour (juce::Colours::black);
        g.drawText (m->getName(), x + 5, 0, w - 6, getHeight(), juce::Justification::centredLeft);
    }

    if (session.getMarkers().empty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawText ("Double-click here to add a marker", getLocalBounds().reduced (6, 0),
                    juce::Justification::centredLeft);
    }
}

void MarkerBar::renameMarker (te::Clip::Ptr marker)
{
    auto* w = new juce::AlertWindow ("Rename Marker", "Name:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", marker->getName());
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    auto* s = &session;

    w->enterModalState (true, juce::ModalCallbackFunction::create ([s, marker, w] (int result)
    {
        const auto name = w->getTextEditorContents ("name").trim();

        if (result == 1 && name.isNotEmpty() && marker->getParent() != nullptr)
        {
            s->beginEdit();
            marker->setName (name);
            s->save();
        }
    }), true);
}

void MarkerBar::mouseDown (const juce::MouseEvent& e)
{
    dragMarker = nullptr;
    dragged = false;

    auto* marker = findMarkerAt (e.x);

    if (e.mods.isPopupMenu())
    {
        if (marker == nullptr)
            return;

        te::Clip::Ptr ptr (marker);
        auto* s = &session;
        juce::Component::SafePointer<MarkerBar> safe (this);

        juce::PopupMenu menu;
        menu.addItem ("Rename...", [safe, ptr] { if (safe != nullptr) safe->renameMarker (ptr); });
        menu.addItem ("Delete", [s, ptr]
        {
            s->beginEdit();
            ptr->removeFromParent();
            s->save();
        });
        menu.showMenuAsync ({});
        return;
    }

    dragMarker = marker;
}

void MarkerBar::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMarker == nullptr || e.getDistanceFromDragStart() < 3)
        return;

    if (! dragged)
    {
        session.beginEdit();
        dragged = true;
    }

    double t = juce::jmax (0.0, session.xToTime (e.x, getWidth()));

    if (session.isSnapEnabled() && ! e.mods.isAltDown())
        t = session.snapTime (t);

    dragMarker->setStart (te::TimePosition::fromSeconds (t), false, true);
}

void MarkerBar::mouseUp (const juce::MouseEvent&)
{
    if (dragMarker != nullptr)
    {
        // A click without a drag jumps to the marker
        if (dragged)
            session.save();
        else
            session.getTransport().setPosition (dragMarker->getPosition().getStart());
    }

    dragMarker = nullptr;
    dragged = false;
}

void MarkerBar::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (auto* marker = findMarkerAt (e.x))
    {
        renameMarker (marker);
        return;
    }

    double t = juce::jmax (0.0, session.xToTime (e.x, getWidth()));

    if (session.isSnapEnabled())
        t = session.snapTime (t);

    session.addMarkerAt (t);
}
