#pragma once

#include <juce_graphics/juce_graphics.h>
#include "BinaryData.h"

/** The KNG Studio logo, built into the app (Assets/Images/KngLogo.png). */
namespace KngLogo
{
    namespace detail
    {
        struct Holder : public juce::DeletedAtShutdown
        {
            juce::Image image;
        };

        inline juce::Image build()
        {
            auto src = juce::ImageCache::getFromMemory (BinaryData::KngLogo_png, BinaryData::KngLogo_pngSize);

            if (! src.isValid())
                return {};

            juce::Image out (juce::Image::ARGB, src.getWidth(), src.getHeight(), true);
            juce::Image::BitmapData in (src, juce::Image::BitmapData::readOnly);
            juce::Image::BitmapData dst (out, juce::Image::BitmapData::writeOnly);

            for (int y = 0; y < src.getHeight(); ++y)
            {
                for (int x = 0; x < src.getWidth(); ++x)
                {
                    const auto c = in.getPixelColour (x, y);
                    const float level = juce::jmax (c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue());

                    if (level <= 0.02f)
                        dst.setPixelColour (x, y, juce::Colours::transparentBlack);
                    else
                        dst.setPixelColour (x, y, juce::Colour::fromFloatRGBA (c.getFloatRed() / level,
                                                                               c.getFloatGreen() / level,
                                                                               c.getFloatBlue() / level, level));
                }
            }

            return out;
        }
    }

    /** The logo with its black background turned see-through, so only the artwork is left.
        Built once on first use. An invalid image if the file wasn't built in. */
    inline const juce::Image& keyed()
    {
        static detail::Holder* holder = []
        {
            auto* h = new detail::Holder();
            h->image = detail::build();
            return h;
        }();

        return holder->image;
    }
}
