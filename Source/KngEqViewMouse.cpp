#include "KngEqView.h"
#include "KngEqPlugin.h"

namespace te = tracktion;

static bool hasGain (int type)
{
    return type == KngEq::lowShelf || type == KngEq::bell || type == KngEq::highShelf;
}

static bool isCut (int type)
{
    return type == KngEq::lowCut || type == KngEq::highCut;
}

//==============================================================================
void KngEqView::mouseDown (const juce::MouseEvent& e)
{
    dragging = -1;

    if (! graph().contains (e.position))
        return;

    const int b = bandAt (e.position);

    if (e.mods.isPopupMenu())
    {
        if (b >= 0)
            showBandMenu (b);

        return;
    }

    if (b >= 0)
    {
        selectBand (b);
        dragging = b;
        beginEdit();
    }
}

void KngEqView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    const int type = juce::roundToInt (value (dragging, KngEq::bandType));
    setValue (dragging, KngEq::bandFreq, (float) xToFreq (e.position.x));

    if (hasGain (type))
        setValue (dragging, KngEq::bandGain, (float) yToGain (e.position.y));

    repaint();
}

void KngEqView::mouseUp (const juce::MouseEvent&)
{
    dragging = -1;
}

void KngEqView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! graph().contains (e.position))
        return;

    // Double-click a dot to remove that band
    const int hit = bandAt (e.position);

    if (hit >= 0)
    {
        beginEdit();
        setValue (hit, KngEq::bandOn, 0.0f);
        selectBand (-1);
        return;
    }

    // Double-click empty space to add a band there
    const double f = xToFreq (e.position.x);
    const int type = f < 40.0 ? KngEq::lowCut : f > 15000.0 ? KngEq::highCut : KngEq::bell;

    std::vector<int> order;

    if (type == KngEq::lowCut)       order = { 0, 1, 2, 3, 4, 5, 6, 7 };
    else if (type == KngEq::highCut) order = { 7, 6, 5, 4, 3, 2, 1, 0 };
    else                             order = { 2, 3, 4, 5, 1, 6, 0, 7 };

    int band = -1;

    for (int b : order)
    {
        if (value (b, KngEq::bandOn) < 0.5f)
        {
            band = b;
            break;
        }
    }

    if (band < 0)
        return;   // all 8 bands in use

    beginEdit();
    setValue (band, KngEq::bandType, (float) type);
    setValue (band, KngEq::bandFreq, (float) f);
    setValue (band, KngEq::bandGain, type == KngEq::bell ? (float) yToGain (e.position.y) : 0.0f);
    setValue (band, KngEq::bandQ, type == KngEq::bell ? 1.0f : 0.707f);
    setValue (band, KngEq::bandSlope, 0.0f);
    setValue (band, KngEq::bandOn, 1.0f);
    selectBand (band);
}

void KngEqView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Only over the EQ screen; anywhere else the scroll goes to whatever is behind
    if (! graph().contains (e.position))
    {
        juce::Component::mouseWheelMove (e, wheel);
        return;
    }

    int b = bandAt (e.position);

    if (b < 0)
        b = selected;

    // Nothing to resize: let the scroll through
    if (b < 0 || wheel.deltaY == 0.0f)
    {
        juce::Component::mouseWheelMove (e, wheel);
        return;
    }

    // Scroll up for narrower, down for wider
    const float q = value (b, KngEq::bandQ) * (wheel.deltaY > 0.0f ? 1.15f : 1.0f / 1.15f);
    setValue (b, KngEq::bandQ, juce::jlimit (0.1f, 18.0f, q));
    repaint();
}

void KngEqView::showBandMenu (int band)
{
    juce::Component::SafePointer<KngEqView> safe (this);
    const int type = juce::roundToInt (value (band, KngEq::bandType));
    juce::PopupMenu menu;

    for (int t = 0; t < KngEq::typeNames().size(); ++t)
    {
        menu.addItem (KngEq::typeNames()[t], true, t == type, [safe, band, t]
        {
            if (safe == nullptr) return;
            safe->beginEdit();
            safe->setValue (band, KngEq::bandType, (float) t);
        });
    }

    if (isCut (type))
    {
        menu.addSeparator();
        const int slope = juce::roundToInt (value (band, KngEq::bandSlope));

        for (int s = 0; s < KngEq::slopeNames().size(); ++s)
        {
            menu.addItem (KngEq::slopeNames()[s], true, s == slope, [safe, band, s]
            {
                if (safe == nullptr) return;
                safe->beginEdit();
                safe->setValue (band, KngEq::bandSlope, (float) s);
            });
        }
    }

    menu.addSeparator();
    menu.addItem ("Remove Band", [safe, band]
    {
        if (safe == nullptr) return;
        safe->beginEdit();
        safe->setValue (band, KngEq::bandOn, 0.0f);
        safe->selectBand (-1);
    });

    menu.showMenuAsync ({});
}
