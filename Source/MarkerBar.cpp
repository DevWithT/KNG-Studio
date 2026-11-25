#include "MarkerBar.h"

namespace te = tracktion;

static bool isSection (te::Clip& m)
{
    return (bool) m.state.getProperty ("kngSection", false);
}

// The same name always gets the same colour, so every Chorus matches
static juce::Colour sectionColour (const juce::String& name)
{
    static const juce::uint32 palette[] = { 0xffe6a23a, 0xff5aa8e6, 0xff7bd47b, 0xffd46bbb, 0xffb08be6, 0xffe6e06b };
    return juce::Colour (palette[(size_t) ((juce::uint32) name.trim().toLowerCase().hashCode() % 6u)]);
}

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

te::Clip* MarkerBar::findMarkerAt (int x, bool* onRightEdge)
{
    if (onRightEdge != nullptr)
        *onRightEdge = false;

    const auto markers = session.getMarkers();

    // Plain markers first, so a marker that sits inside a section can still be picked
    for (auto& m : markers)
    {
        if (isSection (*m))
            continue;

        const int mx = session.timeToX (m->getPosition().getStart().inSeconds(), getWidth());

        if (x >= mx - 3 && x <= mx + getLabelWidth (*m))
            return m.get();
    }

    for (auto& m : markers)
    {
        if (! isSection (*m))
            continue;

        const int sx = session.timeToX (m->getPosition().getStart().inSeconds(), getWidth());
        const int ex = session.timeToX (m->getPosition().getEnd().inSeconds(), getWidth());

        if (x >= sx && x <= ex + 3)
        {
            if (onRightEdge != nullptr)
                *onRightEdge = x >= ex - 5;

            return m.get();
        }
    }

    return nullptr;
}

//==============================================================================
void MarkerBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d20));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    const auto markers = session.getMarkers();
    const int h = getHeight();

    // Sections first, as blocks
    for (auto& m : markers)
    {
        if (! isSection (*m))
            continue;

        const int x1 = session.timeToX (m->getPosition().getStart().inSeconds(), getWidth());
        const int x2 = session.timeToX (m->getPosition().getEnd().inSeconds(), getWidth());

        if (x2 < 0 || x1 > getWidth())
            continue;

        const int w = juce::jmax (6, x2 - x1);

        g.setColour (sectionColour (m->getName()).withAlpha (0.9f));
        g.fillRoundedRectangle ((float) x1, 1.0f, (float) w, (float) h - 2.0f, 3.0f);

        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawText (m->getName(), x1 + 6, 0, juce::jmax (0, w - 14), h, juce::Justification::centredLeft, true);

        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRect (x1 + w - 4, 4, 2, h - 8);
    }

    // Plain markers on top
    for (auto& m : markers)
    {
        if (isSection (*m))
            continue;

        const int x = session.timeToX (m->getPosition().getStart().inSeconds(), getWidth());
        const int w = getLabelWidth (*m);

        if (x + w < 0 || x > getWidth())
            continue;

        g.setColour (juce::Colour (0xffe6a23a));
        g.fillRect (x, 0, 2, h);
        g.fillRoundedRectangle ((float) x, 1.0f, (float) w, (float) h - 2.0f, 3.0f);

        g.setColour (juce::Colours::black);
        g.drawText (m->getName(), x + 5, 0, w - 6, h, juce::Justification::centredLeft);
    }

    if (markers.empty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawText ("Double-click: add a marker. Right-click: add a section.", getLocalBounds().reduced (6, 0),
                    juce::Justification::centredLeft);
    }
}

//==============================================================================
void MarkerBar::renameMarker (te::Clip::Ptr marker)
{
    auto* w = new juce::AlertWindow ("Rename", "Name:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", marker->getName());
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    auto* s = &session;

    w->enterModalState (true, juce::ModalCallbackFunction::create ([s, marker, w] (int result)
    {
        const auto name = w->getTextEditorContents ("name").trim();

        if (result == 1 && name.isNotEmpty() && marker->getParent() != nullptr)
        {
            s->beginEdit ("Rename marker");
            marker->setName (name);
            s->save();
        }
    }), true);
}

void MarkerBar::showMarkerMenu (te::Clip* marker)
{
    te::Clip::Ptr ptr (marker);
    auto* s = &session;
    const bool section = isSection (*marker);
    juce::Component::SafePointer<MarkerBar> safe (this);

    juce::PopupMenu menu;
    menu.addItem ("Rename...", [safe, ptr] { if (safe != nullptr) safe->renameMarker (ptr); });

    if (section)
    {
        menu.addItem ("Duplicate Section (makes room after it)", [s, ptr]
        {
            if (ptr->getParent() != nullptr)
                s->duplicateSection (*ptr);
        });

        menu.addItem ("Select Clips in Section", [s, ptr]
        {
            if (ptr->getParent() == nullptr)
                return;

            juce::Array<te::Clip*> clips;

            for (auto& c : s->getSectionClips (ptr->getPosition().getStart().inSeconds(),
                                               ptr->getPosition().getEnd().inSeconds()))
                clips.add (c.get());

            s->clearSelection();
            s->addClipsToSelection (clips);
        });
    }

    menu.addSeparator();
    menu.addItem (section ? "Delete Section (keeps its clips)" : "Delete", [s, ptr, section]
    {
        s->beginEdit (section ? "Delete section" : "Delete marker");
        ptr->removeFromParent();
        s->save();
    });
    menu.showMenuAsync ({});
}

void MarkerBar::showAddSectionMenu (int x)
{
    double t = juce::jmax (0.0, session.xToTime (x, getWidth()));

    if (session.isSnapEnabled())
        t = session.snapTime (t);

    auto* s = &session;
    juce::Component::SafePointer<MarkerBar> safe (this);
    const double length = 16.0 * 60.0 / juce::jmax (1.0, session.getBpm());   // four bars of 4/4

    static const char* names[] = { "Intro", "Verse", "Pre-Chorus", "Chorus", "Hook", "Bridge", "Drop", "Outro" };

    juce::PopupMenu menu;

    for (auto* n : names)
    {
        const juce::String name (n);
        menu.addItem ("Add " + name + " here", [s, t, length, name] { s->addSection (t, name, length); });
    }

    menu.addSeparator();
    menu.addItem ("Add Section with your own name...", [safe, t, length] { if (safe != nullptr) safe->askSectionName (t, length); });
    menu.showMenuAsync ({});
}

void MarkerBar::askSectionName (double start, double length)
{
    auto* w = new juce::AlertWindow ("New Section", "Name this section:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "Verse");
    w->addButton ("Add", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    auto* s = &session;

    w->enterModalState (true, juce::ModalCallbackFunction::create ([s, start, length, w] (int result)
    {
        const auto name = w->getTextEditorContents ("name").trim();

        if (result == 1 && name.isNotEmpty())
            s->addSection (start, name, length);
    }), true);
}

//==============================================================================
void MarkerBar::mouseMove (const juce::MouseEvent& e)
{
    bool edge = false;
    auto* m = findMarkerAt (e.x, &edge);

    setMouseCursor (m != nullptr && edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void MarkerBar::mouseDown (const juce::MouseEvent& e)
{
    dragMarker = nullptr;
    dragged = false;
    resizing = false;

    bool edge = false;
    auto* marker = findMarkerAt (e.x, &edge);

    if (e.mods.isPopupMenu())
    {
        if (marker == nullptr)
            showAddSectionMenu (e.x);
        else
            showMarkerMenu (marker);

        return;
    }

    dragMarker = marker;

    if (marker != nullptr)
    {
        resizing = isSection (*marker) && edge;
        dragOrigStart = marker->getPosition().getStart().inSeconds();
        dragOrigLength = marker->getPosition().getLength().inSeconds();
        dragGrabOffset = juce::jmax (0.0, session.xToTime (e.x, getWidth())) - dragOrigStart;
    }
}

void MarkerBar::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMarker == nullptr || e.getDistanceFromDragStart() < 3)
        return;

    const bool section = isSection (*dragMarker);

    if (! dragged)
    {
        session.beginEdit (resizing ? "Resize section" : (section ? "Move section" : "Move marker"));
        dragged = true;
    }

    const bool snap = session.isSnapEnabled() && ! e.mods.isAltDown();
    const double mouseTime = juce::jmax (0.0, session.xToTime (e.x, getWidth()));

    if (resizing)
    {
        double end = mouseTime;

        if (snap)
            end = session.snapTime (end);

        dragMarker->setLength (te::TimeDuration::fromSeconds (juce::jmax (0.1, end - dragOrigStart)), true);
        return;
    }

    // The block keeps the spot you grabbed it by
    double t = juce::jmax (0.0, mouseTime - (section ? dragGrabOffset : 0.0));

    if (snap)
        t = session.snapTime (t);

    dragMarker->setStart (te::TimePosition::fromSeconds (t), false, true);
}

void MarkerBar::mouseUp (const juce::MouseEvent&)
{
    if (dragMarker != nullptr)
    {
        if (dragged)
        {
            // A section takes the clips that started inside it along
            if (! resizing && isSection (*dragMarker))
            {
                const double delta = dragMarker->getPosition().getStart().inSeconds() - dragOrigStart;

                if (std::abs (delta) > 0.0005)
                    session.moveClipsInRange (dragOrigStart, dragOrigStart + dragOrigLength, delta);
            }

            session.save();
        }
        else
        {
            // A click without a drag jumps to the marker
            session.getTransport().setPosition (dragMarker->getPosition().getStart());
        }
    }

    dragMarker = nullptr;
    dragged = false;
    resizing = false;
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
