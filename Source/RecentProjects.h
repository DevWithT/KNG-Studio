#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>

// The songs in Documents\KNG Studio, newest first. Used by File > Recent Projects.
namespace KngRecent
{
    inline juce::Array<juce::File> list (int maxCount = 8)
    {
        const auto root = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KNG Studio");
        juce::Array<juce::File> edits;

        for (auto& folder : root.findChildFiles (juce::File::findDirectories, false))
            for (auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.tracktionedit"))
                edits.add (f);

        std::sort (edits.begin(), edits.end(), [] (const juce::File& a, const juce::File& b)
        {
            return a.getLastModificationTime() > b.getLastModificationTime();
        });

        if (edits.size() > maxCount)
            edits.removeRange (maxCount, edits.size() - maxCount);

        return edits;
    }
}
