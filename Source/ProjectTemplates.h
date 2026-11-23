#pragma once

#include <juce_core/juce_core.h>

// Song templates: a saved song with its audio clips taken out.
// Tracks, lanes, patterns, Rack sounds, effects, sends and tempo stay.
// They live in Documents\KNG Studio\ProjectTemplates as .kngtemplate files.
namespace KngProjectTemplates
{
    inline juce::File getFolder()
    {
        auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                          .getChildFile ("KNG Studio")
                          .getChildFile ("ProjectTemplates");
        folder.createDirectory();
        return folder;
    }

    inline juce::StringArray getNames()
    {
        juce::StringArray names;

        for (auto& f : getFolder().findChildFiles (juce::File::findFiles, false, "*.kngtemplate"))
            names.add (f.getFileNameWithoutExtension());

        names.sortNatural();
        return names;
    }

    inline juce::File getFile (const juce::String& name)
    {
        return getFolder().getChildFile (name + ".kngtemplate");
    }

    // Audio clips point at files inside the original song's folder, so they don't belong in a template
    inline void stripAudioClips (juce::XmlElement& xml)
    {
        for (auto* child = xml.getFirstChildElement(); child != nullptr;)
        {
            auto* next = child->getNextElement();

            if (child->hasTagName ("AUDIOCLIP"))
                xml.removeChildElement (child, true);
            else
                stripAudioClips (*child);

            child = next;
        }
    }

    // projectFile is a saved .tracktionedit
    inline bool save (const juce::File& projectFile, const juce::String& name)
    {
        const auto safeName = juce::File::createLegalFileName (name.trim());

        if (safeName.isEmpty())
            return false;

        auto xml = juce::parseXML (projectFile);

        if (xml == nullptr)
            return false;

        stripAudioClips (*xml);
        return xml->writeTo (getFile (safeName));
    }
}
