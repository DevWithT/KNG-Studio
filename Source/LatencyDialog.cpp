#include "LatencyDialog.h"
#include <algorithm>
#include <vector>

LatencyDialog::LatencyDialog (EditSession& sessionToUse, juce::AudioDeviceManager& deviceManagerToUse)
    : session (sessionToUse), deviceManager (deviceManagerToUse)
{
    for (auto* c : std::initializer_list<juce::Component*> { &infoLabel, &runButton, &resultLabel,
                                                             &offsetLabel, &offsetSlider, &applyButton })
        addAndMakeVisible (c);

    infoLabel.setJustificationType (juce::Justification::topLeft);
    infoLabel.setText ("Use speakers, not headphones. Put your mic near them and turn the volume up. "
                       "The test listens on your first input and fires 5 clicks.",
                       juce::dontSendNotification);

    resultLabel.setJustificationType (juce::Justification::topLeft);
    resultLabel.setText ("Current offset: " + juce::String (EditSession::getLatencyOffsetMs(), 1) + " ms",
                         juce::dontSendNotification);

    offsetSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    offsetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 22);
    offsetSlider.setRange (-200.0, 200.0, 0.1);
    offsetSlider.setValue (EditSession::getLatencyOffsetMs(), juce::dontSendNotification);

    runButton.onClick   = [this] { runTest(); };
    applyButton.onClick = [this]
    {
        session.setLatencyOffsetMs (offsetSlider.getValue());
        resultLabel.setText ("Offset applied: " + juce::String (offsetSlider.getValue(), 1) + " ms",
                             juce::dontSendNotification);
    };

    setSize (480, 300);
}

LatencyDialog::~LatencyDialog()
{
    stopTimer();
    stopTest();
}

void LatencyDialog::runTest()
{
    if (running || session.getTransport().isRecording())
        return;

    session.stop();

    // Monitoring would feed the mic back into the speakers and mess up the reading
    monitoringWasOn = session.isMonitoring();
    session.setMonitoring (false);

    tester.reset();
    deviceManager.addAudioCallback (&tester);
    running = true;

    runButton.setEnabled (false);
    resultLabel.setText ("Listening...", juce::dontSendNotification);
    startTimerHz (10);
}

void LatencyDialog::stopTest()
{
    if (! running)
        return;

    deviceManager.removeAudioCallback (&tester);
    running = false;
    session.setMonitoring (monitoringWasOn);
}

void LatencyDialog::timerCallback()
{
    if (running && tester.isFinished())
        finishTest();
}

void LatencyDialog::finishTest()
{
    stopTimer();
    stopTest();
    runButton.setEnabled (true);

    std::vector<int> hits;

    for (int i = 0; i < LatencyTester::numRuns; ++i)
        if (tester.getResult (i) > 0)
            hits.push_back (tester.getResult (i));

    if (hits.size() < 3)
    {
        resultLabel.setText ("Couldn't hear the click. Turn the speakers up, move the mic closer, "
                             "and make sure the mic is on your first input.",
                             juce::dontSendNotification);
        return;
    }

    // Middle result, so one stray hit doesn't throw it off
    std::sort (hits.begin(), hits.end());
    const int median = hits[hits.size() / 2];

    const double sr = juce::jmax (1.0, tester.getSampleRate());
    const double measuredMs = median / sr * 1000.0;

    // Tracktion already compensates for what the driver reports, so only apply the rest
    double reportedMs = 0.0;

    if (auto* device = deviceManager.getCurrentAudioDevice())
        reportedMs = (device->getInputLatencyInSamples() + device->getOutputLatencyInSamples()) / sr * 1000.0;

    const double offsetMs = std::round ((measuredMs - reportedMs) * 10.0) / 10.0;

    offsetSlider.setValue (offsetMs, juce::dontSendNotification);
    session.setLatencyOffsetMs (offsetMs);

    resultLabel.setText ("Round trip: " + juce::String (measuredMs, 1) + " ms\n"
                         + "Driver reports: " + juce::String (reportedMs, 1) + " ms\n"
                         + "Offset applied: " + juce::String (offsetMs, 1) + " ms",
                         juce::dontSendNotification);
}

void LatencyDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2b2f33));
}

void LatencyDialog::resized()
{
    auto r = getLocalBounds().reduced (12);

    infoLabel.setBounds (r.removeFromTop (54));
    r.removeFromTop (6);
    runButton.setBounds (r.removeFromTop (28).removeFromLeft (120));
    r.removeFromTop (10);
    resultLabel.setBounds (r.removeFromTop (70));
    r.removeFromTop (10);

    auto row = r.removeFromTop (26);
    offsetLabel.setBounds (row.removeFromLeft (90));
    offsetSlider.setBounds (row);

    r.removeFromTop (8);
    applyButton.setBounds (r.removeFromTop (28).removeFromLeft (120));
}
