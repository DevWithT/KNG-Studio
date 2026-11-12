#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** The fonts built into the app for the KNG plugin faces. */
namespace KngFonts
{
    enum Face { barlowSemi, barlowHeavy, anton, spaceGrotesk, sora, mono, numFaces };

    /** Null if the font file wasn't built in. */
    juce::Typeface::Ptr get (Face face);

    /** That font at a height, with optional letter spacing. Falls back to the default font. */
    juce::Font font (Face face, float height, float kerning = 0.0f);
}
