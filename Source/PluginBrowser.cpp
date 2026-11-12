#include "PluginBrowser.h"
#include "KngSynthPlugin.h"
#include "KngSamplerPlugin.h"
#include <map>

namespace te = tracktion;

//==============================================================================
std::vector<PluginBrowser::Entry> PluginBrowser::buildEntries (te::Engine& engine)
{
    std::vector<Entry> e;

    auto types = engine.getPluginManager().knownPluginList.getTypes();
    std::sort (types.begin(), types.end(), [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
    {
        return a.name.compareIgnoreCase (b.name) < 0;
    });

    // Built into KNG Studio
    e.push_back ({ "KNG Instruments", {}, {}, true, true });
    e.push_back ({ "KNG Synth",   "Built-in synth",              KngSynthPlugin::xmlTypeName,   true, false });
    e.push_back ({ "KNG Sampler", "Play any sample on the keys", KngSamplerPlugin::xmlTypeName, true, false });
    e.push_back ({ "KNG 808",     "Synth 808 with slides",       "kng808",                      true, false });

    // Your VST instruments
    bool anyInstruments = false;

    for (auto& d : types)
    {
        if (! d.isInstrument)
            continue;

        if (! anyInstruments)
        {
            e.push_back ({ "Your Instruments", {}, {}, true, true });
            anyInstruments = true;
        }

        e.push_back ({ d.name, d.manufacturerName, d.createIdentifierString(), true, false });
    }

    e.push_back ({ "KNG Effects", {}, {}, false, true });

    for (auto& [name, type] : EditSession::getBuiltInEffects())
        e.push_back ({ name, "KNG", type, false, false });

    // Your VST effects
    bool anyEffects = false;

    for (auto& d : types)
    {
        if (d.isInstrument)
            continue;

        if (! anyEffects)
        {
            e.push_back ({ "Your Effects", {}, {}, false, true });
            anyEffects = true;
        }

        e.push_back ({ d.name, d.manufacturerName, d.createIdentifierString(), false, false });
    }

    return e;
}

void PluginBrowser::showInstrumentMenu (te::Engine& engine, juce::Component& anchor,
                                        std::function<void (const Entry&)> onPick)
{
    juce::PopupMenu menu;
    std::map<juce::String, juce::PopupMenu> byMaker;
    auto pick = std::make_shared<std::function<void (const Entry&)>> (std::move (onPick));

    for (auto& entry : buildEntries (engine))
    {
        if (entry.header || ! entry.instrument)
            continue;

        if (entry.id.startsWith ("kng"))
        {
            menu.addItem (entry.name, [pick, entry] { (*pick) (entry); });
        }
        else
        {
            const auto maker = entry.detail.isNotEmpty() ? entry.detail : juce::String ("Other");
            byMaker[maker].addItem (entry.name, [pick, entry] { (*pick) (entry); });
        }
    }

    menu.addSeparator();

    if (byMaker.empty())
        menu.addItem ("No VST instruments yet. Use File > Scan Plugins.", false, false, nullptr);

    for (auto& [maker, sub] : byMaker)
        menu.addSubMenu (maker, sub);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&anchor));
}

bool PluginBrowser::parseDrag (const juce::var& description, Entry& out)
{
    const auto text = description.toString();

    if (! text.startsWith ("kngplugin|"))
        return false;

    auto parts = juce::StringArray::fromTokens (text, "|", "");

    if (parts.size() < 4)
        return false;

    out.instrument = parts[1] == "instrument";
    out.id = parts[2];
    parts.removeRange (0, 3);
    out.name = parts.joinIntoString ("|");
    out.header = false;
    return true;
}

//==============================================================================
PluginBrowser::PluginBrowser (te::Engine& engineToUse)
    : engine (engineToUse)
{
    addAndMakeVisible (search);
    search.setTextToShowWhenEmpty ("Search plugins...", juce::Colours::white.withAlpha (0.4f));
    search.onTextChange = [this] { refresh(); };

    addAndMakeVisible (list);
    list.setModel (this);
    list.setRowHeight (26);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1e2124));

    refresh();
    startTimer (2000);
}

PluginBrowser::~PluginBrowser()
{
    stopTimer();
    list.setModel (nullptr);
}

void PluginBrowser::timerCallback()
{
    // New VSTs after a scan show up on their own
    const int count = engine.getPluginManager().knownPluginList.getNumTypes();

    if (count != lastCount)
        refresh();
}

void PluginBrowser::refresh()
{
    lastCount = engine.getPluginManager().knownPluginList.getNumTypes();
    all = buildEntries (engine);
    shown.clear();

    const auto query = search.getText().trim();
    const Entry* pendingHeader = nullptr;

    for (auto& entry : all)
    {
        if (entry.header)
        {
            pendingHeader = &entry;
            continue;
        }

        if (query.isNotEmpty() && ! entry.name.containsIgnoreCase (query) && ! entry.detail.containsIgnoreCase (query))
            continue;

        if (pendingHeader != nullptr)
        {
            shown.push_back (*pendingHeader);
            pendingHeader = nullptr;
        }

        shown.push_back (entry);
    }

    list.updateContent();
    list.repaint();
}

//==============================================================================
int PluginBrowser::getNumRows()
{
    return (int) shown.size();
}

void PluginBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) shown.size()))
        return;

    const auto& e = shown[(size_t) row];

    if (e.header)
    {
        g.setColour (juce::Colour (0xff3a8ee6));
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (e.name.toUpperCase(), 8, 0, width - 16, height, juce::Justification::bottomLeft);
        return;
    }

    if (selected)
    {
        g.setColour (juce::Colour (0xff3a8ee6).withAlpha (0.25f));
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, 1.0f, width - 4.0f, height - 2.0f), 4.0f);
    }

    // Small badge: instrument or effect
    auto badge = juce::Rectangle<float> (8.0f, (height - 16) * 0.5f, 22.0f, 16.0f);
    g.setColour (e.instrument ? juce::Colour (0xff5ad45a).withAlpha (0.8f) : juce::Colour (0xffe6a23a).withAlpha (0.8f));
    g.fillRoundedRectangle (badge, 3.0f);
    g.setColour (juce::Colours::black);
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText (e.instrument ? "IN" : "FX", badge.toNearestInt(), juce::Justification::centred);

    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText (e.name, 36, 0, width - 40, height, juce::Justification::centredLeft, true);
}

juce::String PluginBrowser::getTooltipForRow (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) shown.size()) || shown[(size_t) row].header)
        return {};

    const auto& e = shown[(size_t) row];
    return e.detail + (e.instrument ? "  (drag onto the tracks or Rack, or double-click)"
                                    : "  (drag onto a track, or double-click to pick one)");
}

juce::var PluginBrowser::getDragSourceDescription (const juce::SparseSet<int>& rows)
{
    if (rows.isEmpty() || ! juce::isPositiveAndBelow (rows[0], (int) shown.size()))
        return {};

    const auto& e = shown[(size_t) rows[0]];

    if (e.header)
        return {};

    return "kngplugin|" + juce::String (e.instrument ? "instrument" : "effect") + "|" + e.id + "|" + e.name;
}

void PluginBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (session == nullptr || ! juce::isPositiveAndBelow (row, (int) shown.size()))
        return;

    const auto e = shown[(size_t) row];

    if (e.header)
        return;

    // Instrument: a new track with it, ready to play
    if (e.instrument)
    {
        session->addInstrumentTrack (e.id, e.name);
        return;
    }

    // Effect: pick which track it goes on
    auto* s = session;
    juce::PopupMenu menu;
    menu.addSectionHeader ("Add " + e.name + " to...");

    for (int i = 0; i < s->getNumTracks(); ++i)
        if (auto* t = s->getTrack (i))
            menu.addItem (t->getName(), [s, i, e] { s->addEffectById (i, e.id); });

    menu.showMenuAsync ({});
}

//==============================================================================
void PluginBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e2124));
}

void PluginBrowser::resized()
{
    auto r = getLocalBounds().reduced (4);
    search.setBounds (r.removeFromTop (28));
    r.removeFromTop (4);
    list.setBounds (r);
}
