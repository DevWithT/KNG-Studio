#include "KngPresets.h"

namespace te = tracktion;

static constexpr int userBase = 1000, saveId = 9000, folderId = 9001;

juce::File KngPresets::folderFor (te::Plugin& plugin)
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("KNG Studio").getChildFile ("Presets").getChildFile (plugin.getPluginType());
}

juce::Array<juce::File> KngPresets::list (te::Plugin& plugin)
{
    auto files = folderFor (plugin).findChildFiles (juce::File::findFiles, false, "*.kngpreset");

    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileName().compareIgnoreCase (b.getFileName()) < 0;
    });

    return files;
}

bool KngPresets::save (te::Plugin& plugin, const juce::String& name)
{
    juce::XmlElement xml ("KNGPRESET");
    xml.setAttribute ("plugin", plugin.getPluginType());

    for (auto* param : plugin.getAutomatableParameters())
    {
        auto* e = xml.createNewChildElement ("PARAM");
        e->setAttribute ("id", param->paramID);
        e->setAttribute ("value", (double) param->getCurrentValue());
    }

    auto folder = folderFor (plugin);
    folder.createDirectory();
    return xml.writeTo (folder.getChildFile (juce::File::createLegalFileName (name) + ".kngpreset"));
}

bool KngPresets::load (te::Plugin& plugin, const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr)
        return false;

    for (auto* e : xml->getChildIterator())
    {
        const auto id = e->getStringAttribute ("id");

        for (auto* param : plugin.getAutomatableParameters())
            if (param->paramID == id)
                param->setParameter ((float) e->getDoubleAttribute ("value"), juce::sendNotification);
    }

    return true;
}

//==============================================================================
KngPresetMenu::KngPresetMenu()
{
    setTextWhenNothingSelected ("Presets");
    onChange = [this] { picked(); };
}

void KngPresetMenu::attach (te::Plugin& pluginToUse, const juce::StringArray& factoryNames,
                            std::function<void (int)> applyFactory)
{
    plugin = &pluginToUse;
    factory = factoryNames;
    onFactory = std::move (applyFactory);
    rebuild();
}

void KngPresetMenu::rebuild()
{
    clear (juce::dontSendNotification);

    if (plugin == nullptr)
        return;

    if (! factory.isEmpty())
    {
        addSectionHeading ("Factory");

        for (int i = 0; i < factory.size(); ++i)
            addItem (factory[i], i + 1);
    }

    userFiles = KngPresets::list (*plugin);

    if (! userFiles.isEmpty())
    {
        addSectionHeading ("Yours");

        for (int i = 0; i < userFiles.size(); ++i)
            addItem (userFiles[i].getFileNameWithoutExtension(), userBase + i);
    }

    addSeparator();
    addItem ("Save Preset...", saveId);
    addItem ("Open Presets Folder", folderId);
}

void KngPresetMenu::beginEdit()
{
    if (plugin != nullptr)
        if (auto* um = plugin->getUndoManager())
            um->beginNewTransaction();
}

void KngPresetMenu::picked()
{
    const int id = getSelectedId();

    if (id <= 0 || plugin == nullptr)
        return;

    if (id < userBase)
    {
        beginEdit();

        if (onFactory)
            onFactory (id - 1);

        return;
    }

    if (id < saveId)
    {
        beginEdit();
        KngPresets::load (*plugin, userFiles[id - userBase]);
        return;
    }

    setSelectedId (0, juce::dontSendNotification);

    if (id == folderId)
    {
        auto folder = KngPresets::folderFor (*plugin);
        folder.createDirectory();
        folder.startAsProcess();
        return;
    }

    // Save Preset...
    auto* w = new juce::AlertWindow ("Save Preset", "Name this preset:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "My " + plugin->getName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<KngPresetMenu> safe (this);

    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        const auto name = w->getTextEditorContents ("name").trim();

        if (result != 1 || name.isEmpty() || safe == nullptr || safe->plugin == nullptr)
            return;

        if (KngPresets::save (*safe->plugin, name))
        {
            safe->rebuild();
            safe->setText (name, juce::dontSendNotification);
        }
    }), true);
}
