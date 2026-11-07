#include "PianoRoll.h"
#include "TheoryHelper.h"

namespace te = tracktion;

//==============================================================================
void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d20));

    // Toolbar
    g.setColour (juce::Colour (0xff26292d));
    g.fillRect (getLocalBounds().removeFromTop (toolbarHeight));

    auto grid = getGridArea();

    {
        juce::Graphics::ScopedSaveState saveState (g);
        g.reduceClipRegion (grid);

        paintRows (g, grid);
        paintGridLines (g, grid);
        paintNotes (g, grid);

        // Playhead
        const double playBeat = clip->getContentBeatAtTime (session.getTransport().getPosition()).inBeats();
        g.setColour (juce::Colours::yellow);
        g.fillRect (beatToX (playBeat), grid.getY(), 2, grid.getHeight());
    }

    paintKeyboard (g);
    paintRuler (g);
    paintVelocity (g);
}

//==============================================================================
void PianoRoll::paintRows (juce::Graphics& g, juce::Rectangle<int> grid)
{
    const int s = getScaleIndex();
    const int c = getChordType();
    const Theory::Scale* scale = s >= 0 ? &Theory::scales()[(size_t) s] : nullptr;
    const Theory::ChordType* chord = c >= 0 ? &Theory::chordTypes()[(size_t) c] : nullptr;

    for (int note = topNote; note >= 0 && noteToY (note) < grid.getBottom(); --note)
    {
        const int y = noteToY (note);
        juce::Rectangle<int> row (grid.getX(), y, grid.getWidth(), noteHeight);

        auto base = Theory::isBlackKey (note) ? juce::Colour (0xff23272b) : juce::Colour (0xff2b3035);

        // Out-of-key rows go darker
        if (scale != nullptr && ! Theory::inScale (note, getKeyRoot(), *scale))
            base = base.darker (0.5f);

        g.setColour (base);
        g.fillRect (row);

        // Chord tones light up in every octave, root in orange
        if (chord != nullptr)
        {
            const int tone = Theory::chordToneIndex (note, getChordRoot(), *chord);

            if (tone >= 0)
            {
                g.setColour (tone == 0 ? juce::Colour (0xffe6a23a).withAlpha (0.22f)
                                       : juce::Colour (0xff3a8ee6).withAlpha (0.17f));
                g.fillRect (row);
            }
        }

        if (note == hoverNote)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRect (row);
        }

        // Brighter line under every C
        g.setColour (note % 12 == 0 ? juce::Colours::white.withAlpha (0.16f) : juce::Colours::black.withAlpha (0.25f));
        g.drawHorizontalLine (y + noteHeight - 1, (float) grid.getX(), (float) grid.getRight());
    }

    // Shade outside the clip
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    const int cx1 = beatToX (getClipStartBeat());
    const int cx2 = beatToX (getClipEndBeat());

    if (cx1 > grid.getX())
        g.fillRect (grid.getX(), grid.getY(), cx1 - grid.getX(), grid.getHeight());

    if (cx2 < grid.getRight())
        g.fillRect (cx2, grid.getY(), grid.getRight() - cx2, grid.getHeight());
}

void PianoRoll::paintGridLines (juce::Graphics& g, juce::Rectangle<int> grid)
{
    const double step = getGridBeats() > 0.0 ? getGridBeats() : 0.25;
    const double visibleEnd = xToBeat (grid.getRight());

    for (int i = (int) std::floor (viewBeat / step); i * step <= visibleEnd; ++i)
    {
        const double b = i * step;
        const bool isBeat = std::abs (b - std::round (b)) < 1e-6;
        const bool isBar  = isBeat && ((int) std::round (b) % 4) == 0;

        if (step * pxPerBeat < 4.0 && ! isBeat)
            continue;

        g.setColour (juce::Colours::white.withAlpha (isBar ? 0.25f : isBeat ? 0.11f : 0.04f));
        g.drawVerticalLine (beatToX (b), (float) grid.getY(), (float) grid.getBottom());
    }
}

void PianoRoll::paintNotes (juce::Graphics& g, juce::Rectangle<int> grid)
{
    const int s = getScaleIndex();
    const Theory::Scale* scale = s >= 0 ? &Theory::scales()[(size_t) s] : nullptr;

    g.setFont (juce::Font (juce::FontOptions (10.0f)));

    for (auto* n : clip->getSequence().getNotes())
    {
        auto r = getNoteRect (*n);

        if (! r.intersects (grid))
            continue;

        const float vel = n->getVelocity() / 127.0f;
        const bool sel = selected.contains (n);
        const bool outOfKey = scale != nullptr && ! Theory::inScale (n->getNoteNumber(), getKeyRoot(), *scale);

        // Notes outside the key turn red so you can see them
        auto colour = outOfKey ? juce::Colour (0xffd05a5a)
                               : juce::Colour (0xff3a8ee6).interpolatedWith (juce::Colour (0xffe6893a), vel);

        g.setColour (colour);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);

        g.setColour (sel ? juce::Colours::white : juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (r.toFloat(), 3.0f, sel ? 2.0f : 1.0f);

        if (r.getWidth() > 28)
        {
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.drawText (Theory::noteName (n->getNoteNumber()), r.reduced (4, 0), juce::Justification::centredLeft);
        }
    }
}

//==============================================================================
void PianoRoll::paintKeyboard (juce::Graphics& g)
{
    auto kb = getKeyboardArea();

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (kb);

    const int s = getScaleIndex();
    const int c = getChordType();
    const Theory::Scale* scale = s >= 0 ? &Theory::scales()[(size_t) s] : nullptr;
    const Theory::ChordType* chord = c >= 0 ? &Theory::chordTypes()[(size_t) c] : nullptr;

    for (int note = topNote; note >= 0 && noteToY (note) < kb.getBottom(); --note)
    {
        const int y = noteToY (note);
        const bool black = Theory::isBlackKey (note);
        juce::Rectangle<int> key (kb.getX(), y, kb.getWidth(), noteHeight);

        g.setColour (black ? juce::Colour (0xff1b1b1b) : juce::Colour (0xffe8e8e8));
        g.fillRect (key);

        if (note == hoverNote)
        {
            g.setColour (juce::Colour (0xff3a8ee6).withAlpha (0.4f));
            g.fillRect (key);
        }

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawHorizontalLine (y + noteHeight - 1, (float) kb.getX(), (float) kb.getRight());

        // Badge for chord tones, dot for other notes in the key
        auto badge = key.removeFromRight (24).reduced (2, 2);
        const int tone = chord != nullptr ? Theory::chordToneIndex (note, getChordRoot(), *chord) : -1;

        if (tone >= 0)
        {
            g.setColour (tone == 0 ? juce::Colour (0xffe6a23a) : juce::Colour (0xff3a8ee6));
            g.fillRoundedRectangle (badge.toFloat(), 3.0f);
            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (juce::FontOptions (9.5f)));
            g.drawText (Theory::toneShort (chord->intervals[(size_t) tone]), badge, juce::Justification::centred);
        }
        else if (scale != nullptr && Theory::inScale (note, getKeyRoot(), *scale))
        {
            g.setColour (juce::Colour (0xff3a8ee6).withAlpha (0.8f));
            g.fillEllipse (badge.withSizeKeepingCentre (5, 5).toFloat());
        }

        // Every key gets its name and octave
        g.setColour (black ? juce::Colours::white.withAlpha (0.8f) : juce::Colours::black.withAlpha (0.8f));
        g.setFont (juce::Font (juce::FontOptions (note % 12 == 0 ? 11.0f : 9.5f)));
        g.drawText (Theory::noteName (note), key.withTrimmedLeft (5), juce::Justification::centredLeft);
    }
}

void PianoRoll::paintVelocity (juce::Graphics& g)
{
    auto va = getVelocityArea();

    g.setColour (juce::Colour (0xff17191b));
    g.fillRect (va);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine (va.getY(), (float) va.getX(), (float) va.getRight());

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (va);

    for (auto* n : clip->getSequence().getNotes())
    {
        const int x = beatToX (n->getStartBeat().inBeats());
        const int h = juce::roundToInt (n->getVelocity() / 127.0 * (va.getHeight() - 6));

        g.setColour (selected.contains (n) ? juce::Colours::white : juce::Colour (0xff3a8ee6));
        g.fillRect (x, va.getBottom() - h, 4, h);
    }
}

void PianoRoll::paintRuler (juce::Graphics& g)
{
    auto r = getRulerArea();

    g.setColour (juce::Colour (0xff202326));
    g.fillRect (r);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawHorizontalLine (r.getBottom() - 1, (float) r.getX(), (float) r.getRight());

    juce::Graphics::ScopedSaveState saveState (g);
    g.reduceClipRegion (r);

    auto& tempo = session.getEdit().tempoSequence;
    const double endBeat = xToBeat (r.getRight());
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    // Beat ticks
    for (int b = (int) std::floor (viewBeat); b <= endBeat; ++b)
    {
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawVerticalLine (beatToX (b), (float) r.getBottom() - 5.0f, (float) r.getBottom());
    }

    // Bar numbers, counted the same as the main timeline
    for (int b = (int) std::floor (viewBeat / 4.0) * 4; b <= endBeat; b += 4)
    {
        const int x = beatToX (b);
        const double songBeat = tempo.toBeats (clip->getTimeOfContentBeat (te::BeatPosition::fromBeats (b))).inBeats();
        const int bar = (int) std::round (songBeat / 4.0) + 1;

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.drawVerticalLine (x, (float) r.getY() + 6.0f, (float) r.getBottom());
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.drawText (juce::String (bar), x + 4, r.getY(), 40, r.getHeight(), juce::Justification::centredLeft);
    }

    // Playhead marker
    const double playBeat = clip->getContentBeatAtTime (session.getTransport().getPosition()).inBeats();
    const float px = (float) beatToX (playBeat);
    juce::Path tri;
    tri.addTriangle (px - 5.0f, (float) r.getY(), px + 5.0f, (float) r.getY(), px, (float) r.getY() + 8.0f);
    g.setColour (juce::Colours::yellow);
    g.fillPath (tri);
}
