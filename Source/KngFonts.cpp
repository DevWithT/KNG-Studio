#include "KngFonts.h"
#include "BinaryData.h"
#include <array>

namespace KngFonts
{
    static const char* fileFor (Face face)
    {
        switch (face)
        {
            case barlowSemi:   return "BarlowCondensed-SemiBold.ttf";
            case barlowHeavy:  return "BarlowCondensed-ExtraBold.ttf";
            case anton:        return "Anton-Regular.ttf";
            case spaceGrotesk: return "SpaceGrotesk-Bold.ttf";
            case sora:         return "Sora-ExtraBold.ttf";
            case mono:         return "JetBrainsMono-Bold.ttf";
            default:           break;
        }

        return "";
    }

    // Loaded once, released when the app shuts down
    class Cache : public juce::DeletedAtShutdown
    {
    public:
        ~Cache() override   { clearSingletonInstance(); }

        std::array<juce::Typeface::Ptr, numFaces> faces;
        std::array<bool, numFaces> tried {};

        JUCE_DECLARE_SINGLETON (Cache, true)
    };

    JUCE_IMPLEMENT_SINGLETON (Cache)

    juce::Typeface::Ptr get (Face face)
    {
        auto* cache = Cache::getInstance();

        if (cache == nullptr || face < 0 || face >= numFaces)
            return {};

        const auto i = (size_t) face;

        if (! cache->tried[i])
        {
            cache->tried[i] = true;
            const juce::String wanted (fileFor (face));

            for (int r = 0; r < BinaryData::namedResourceListSize; ++r)
            {
                const char* name = BinaryData::namedResourceList[r];

                if (wanted == BinaryData::getNamedResourceOriginalFilename (name))
                {
                    int size = 0;

                    if (auto* data = BinaryData::getNamedResource (name, size))
                        cache->faces[i] = juce::Typeface::createSystemTypefaceFor (data, (size_t) size);

                    break;
                }
            }
        }

        return cache->faces[i];
    }

    juce::Font font (Face face, float height, float kerning)
    {
        auto options = juce::FontOptions (height);

        if (auto tf = get (face))
            options = juce::FontOptions().withTypeface (tf).withHeight (height);

        return juce::Font (options.withKerningFactor (kerning));
    }
}
