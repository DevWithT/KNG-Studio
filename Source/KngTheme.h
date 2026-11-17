#pragma once

#include <juce_graphics/juce_graphics.h>

/** Flat Dark colors. Every restyled screen reads its colors from here. */
namespace KngTheme
{
    // Shape
    inline constexpr float radius = 5.0f;     // corner radius for buttons, boxes and cards

    // Surfaces
    inline const juce::Colour bg        { 0xff1f2023 };   // window and arrangement background
    inline const juce::Colour menu      { 0xff232427 };   // menu bar and status bar
    inline const juce::Colour chrome    { 0xff2d2e32 };   // transport bar
    inline const juce::Colour panel     { 0xff26272a };   // browser, inspector, toolbars
    inline const juce::Colour panel2    { 0xff232427 };   // dock body
    inline const juce::Colour inset     { 0xff17181a };   // readouts, text fields, wells
    inline const juce::Colour line      { 0xff151618 };   // 1px separators
    inline const juce::Colour line2     { 0xff45464c };   // dividers on chrome, slider tracks

    // Text
    inline const juce::Colour text      { 0xffd6d7da };
    inline const juce::Colour muted     { 0xffa4a6ad };
    inline const juce::Colour ink       { 0xff14171c };   // dark text that sits on a lane color

    // Accent (use sparingly: selection, active tab, on-state)
    inline const juce::Colour accent    { 0xff4aa3ff };
    inline const juce::Colour accentInk { 0xff0c1824 };   // text on the accent
    inline const juce::Colour accentSoft{ 0xff2c3f55 };
    inline const juce::Colour selection { 0xff34506f };   // selected list row

    // Buttons
    inline const juce::Colour btn       { 0xff34353a };
    inline const juce::Colour btnLine   { 0xff44454b };
    inline const juce::Colour rec       { 0xffe5604f };

    // Lane colors (track header fill, clip blocks)
    inline const juce::Colour laneOrange { 0xffc97d56 };
    inline const juce::Colour laneYellow { 0xffc0a054 };
    inline const juce::Colour laneBlue   { 0xff7c86c9 };
    inline const juce::Colour laneTeal   { 0xff5fa8a0 };
    inline const juce::Colour laneGreen  { 0xff809670 };
    inline const juce::Colour lanePink   { 0xffcd6e82 };
    inline const juce::Colour lanePurple { 0xffa075b0 };
    inline const juce::Colour laneBrown  { 0xffac8a6e };
    inline const juce::Colour laneGrey   { 0xff7d848c };
}
