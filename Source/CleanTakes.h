#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "EditSession.h"

// Clean Takes: deletes the takes a clip is not using. Asks first, and asks whether to delete the audio files too.
namespace KngCleanTakes
{
    /** The selected clip, if exactly one clip is selected and it is audio with more than one take. */
    inline tracktion::WaveAudioClip* selectedTakeClip (EditSession& session)
    {
        const auto clips = session.getSelectedClips();

        if (clips.size() != 1)
            return nullptr;

        auto* wave = dynamic_cast<tracktion::WaveAudioClip*> (clips[0].get());

        if (wave == nullptr || wave->getCompManager().getNumTakes() < 2)
            return nullptr;

        return wave;
    }

    inline void run (EditSession& session, tracktion::WaveAudioClip& clipToClean, std::function<void()> done = {})
    {
        if (session.getTransport().isRecording())
            return;

        tracktion::WaveAudioClip::Ptr clip (&clipToClean);

        if (clip->getCompManager().getNumTakes() < 2)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Clean Takes",
                                                    "This clip has only one take, so there is nothing to clean.");
            return;
        }

        EditSession* s = &session;

        juce::AlertWindow::showYesNoCancelBox (
            juce::MessageBoxIconType::QuestionIcon, "Clean Takes",
            "Delete the takes this clip is not using?\n\n"
            "Delete takes only: removes them from the song and keeps the audio files on disk.\n"
            "Delete takes and files: also deletes the audio files. Undo cannot bring those files back.",
            "Delete takes only", "Delete takes and files", "Cancel", nullptr,
            juce::ModalCallbackFunction::create ([s, clip, done] (int result)
            {
                if (result != 1 && result != 2)
                    return;

                s->beginEdit ("Clean takes");
                clip->deleteAllUnusedTakes (result == 2);
                s->save();

                if (done)
                    done();
            }));
    }
}
