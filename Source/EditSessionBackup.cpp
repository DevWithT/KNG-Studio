#include "EditSession.h"
#include <algorithm>

juce::File EditSession::getLockFile() const
{
    return getProjectFolder().getChildFile (".kng_session.lock");
}

juce::File EditSession::getBackupsFolder() const
{
    auto folder = getProjectFolder().getChildFile ("Backups");
    folder.createDirectory();
    return folder;
}

void EditSession::timerCallback()
{
    // Never touch the file mid-take
    if (getTransport().isRecording())
        return;

    // Autosave anything that didn't save on its own (faders, plugin knobs, Auto Write)
    if (edit->hasChangedSinceSaved())
        save();

    if (juce::Time::getCurrentTime() - lastBackupTime >= juce::RelativeTime::minutes (5))
        makeBackup();
}

void EditSession::makeBackup()
{
    save();

    auto dest = getBackupsFolder().getChildFile (getProjectName() + " "
                                                 + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S")
                                                 + ".tracktionedit");
    editFile.copyFileTo (dest);

    lastBackupTime = juce::Time::getCurrentTime();
    pruneBackups();
}

void EditSession::pruneBackups()
{
    auto files = getBackupsFolder().findChildFiles (juce::File::findFiles, false, "*.tracktionedit");

    // Newest first, keep 20
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getLastModificationTime() > b.getLastModificationTime();
    });

    for (int i = 20; i < files.size(); ++i)
        files.getReference (i).deleteFile();
}
