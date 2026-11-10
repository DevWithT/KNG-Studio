#include "FxChainComponent.h"
#include <map>
#include "GenericPluginPanel.h"

namespace te = tracktion;

//==============================================================================
FxChainComponent::Row::Row (FxChainComponent& owner, te::Plugin& p)
    : plugin (&p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &name, &openButton, &enabledButton,
                                                             &upButton, &downButton, &removeButton })
        addAndMakeVisible (c);

    name.setText (p.getName(), juce::dontSendNotification);
    enabledButton.setToggleState (p.isEnabled(), juce::dontSendNotification);

    auto* s = &owner.session;
    auto* l = &owner.list;
    auto ptr = plugin;

    openButton.onClick    = [ptr, this, s]
    {
        // VST plugins have their own window; KNG effects open in the dock; other built-ins get the knob panel
        if (dynamic_cast<te::ExternalPlugin*> (ptr.get()) != nullptr)
            ptr->showWindowExplicitly();
        else if (ptr->getPluginType().startsWith ("kng") && s->onShowPluginEditor)
            s->onShowPluginEditor (*ptr);
        else
            GenericPluginPanel::showFor (*ptr, openButton);
    };
    enabledButton.onClick = [this] { plugin->setEnabled (enabledButton.getToggleState()); };
    upButton.onClick      = [s, l, ptr] { s->movePlugin (*l, ptr.get(), -1); };
    downButton.onClick    = [s, l, ptr] { s->movePlugin (*l, ptr.get(), 1); };
    removeButton.onClick  = [s, ptr] { s->removePlugin (ptr.get()); };
}

void FxChainComponent::Row::resized()
{
    auto r = getLocalBounds().reduced (2);

    removeButton.setBounds (r.removeFromRight (30));
    r.removeFromRight (3);
    downButton.setBounds (r.removeFromRight (50));
    r.removeFromRight (3);
    upButton.setBounds (r.removeFromRight (40));
    r.removeFromRight (3);
    enabledButton.setBounds (r.removeFromRight (48));
    r.removeFromRight (3);
    openButton.setBounds (r.removeFromRight (50));
    r.removeFromRight (6);
    name.setBounds (r);
}

//==============================================================================
FxChainComponent::FxChainComponent (EditSession& sessionToUse, te::PluginList& listToUse)
    : session (sessionToUse), list (listToUse), ownerTrack (listToUse.getOwnerTrack())
{
    addAndMakeVisible (addButton);
    addButton.onClick = [this] { showAddMenu(); };

    rebuild();
    startTimerHz (5);

    setSize (460, 320);
}

FxChainComponent::~FxChainComponent()
{
    stopTimer();
    rows.clear();
}

void FxChainComponent::timerCallback()
{
    // Track was deleted; the dock clears this panel
    if (ownerTrack != nullptr && ! te::getAllTracks (session.getEdit()).contains (ownerTrack.get()))
        return;

    if (session.getFxPlugins (list) != shown)
    {
        rebuild();
        return;
    }

    for (auto* row : rows)
        row->enabledButton.setToggleState (row->plugin->isEnabled(), juce::dontSendNotification);
}

void FxChainComponent::rebuild()
{
    rows.clear();
    shown = session.getFxPlugins (list);

    for (auto* p : shown)
    {
        if ((bool) p->state.getProperty ("kngBoost", false)) continue;

        addAndMakeVisible (rows.add (new Row (*this, *p)));
    }

    resized();
}

void FxChainComponent::showAddMenu()
{
    auto& pm = session.getEdit().engine.getPluginManager();
    auto types = pm.knownPluginList.getTypes();

    std::map<juce::String, juce::PopupMenu> byMaker;
    auto* s = &session;
    auto* l = &list;

    for (auto& d : types)
    {
        if (d.isInstrument)
            continue;

        const auto maker = d.manufacturerName.isNotEmpty() ? d.manufacturerName : juce::String ("Other");
        byMaker[maker].addItem (d.name, [s, l, d] { s->addPlugin (*l, d); });
    }

    juce::PopupMenu menu;

    juce::PopupMenu builtIn;

    for (auto& [name, type] : EditSession::getBuiltInEffects())
        builtIn.addItem (name, [s, l, type] { s->addBuiltInPlugin (*l, type); });

    menu.addSubMenu ("KNG Built-in", builtIn);
    menu.addSeparator();

    if (byMaker.empty())
        menu.addItem ("No plugins yet. Click Scan Plugins up top.", false, false, nullptr);

    for (auto& [maker, sub] : byMaker)
        menu.addSubMenu (maker, sub);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addButton));
}

void FxChainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2b2f33));

    if (rows.isEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawText ("No plugins on this chain yet.", getLocalBounds().withTrimmedTop (40),
                    juce::Justification::centredTop);
    }
}

void FxChainComponent::resized()
{
    auto r = getLocalBounds().reduced (8);

    addButton.setBounds (r.removeFromTop (28).removeFromLeft (140));
    r.removeFromTop (8);

    for (auto* row : rows)
    {
        row->setBounds (r.removeFromTop (30));
        r.removeFromTop (2);
    }
}

//==============================================================================
FxChainWindow::FxChainWindow (const juce::String& title, EditSession& session, te::PluginList& l)
    : DocumentWindow (title, juce::Colours::darkgrey, DocumentWindow::closeButton),
      list (&l)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new FxChainComponent (session, l), true);
    setResizable (true, false);
    centreWithSize (460, 320);
    setVisible (true);
}
