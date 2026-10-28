// <MikoStorm>

#include "llviewerprecompiledheaders.h"

#include "fsmikoavatarlog.h"

#include "llagentdata.h"
#include "llavatarname.h"
#include "llavatarnamecache.h"
#include "llcharacter.h"
#include "lldir.h"
#include "llfile.h"
#include "llstring.h"
#include "llviewercontrol.h"
#include "llvoavatar.h"

#include <ctime>
#include <fstream>

FSMikoAvatarLog::FSMikoAvatarLog()
{
}

FSMikoAvatarLog::~FSMikoAvatarLog()
{
    cancelPending();
}

void FSMikoAvatarLog::cancelPending()
{
    for (auto& entry : mPending) entry.second.connection.disconnect();
    mPending.clear();
}

//static
std::string FSMikoAvatarLog::getDefaultFilePath()
{
    return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "mikostorm_avatars.txt");
}

std::string FSMikoAvatarLog::getFilePath() const
{
    std::string path = gSavedSettings.getString("MikoAvatarLogPath");
    LLStringUtil::trim(path);
    if (path.empty())
    {
        return getDefaultFilePath();
    }
    return path;
}

void FSMikoAvatarLog::ensureLoaded()
{
    const std::string path = getFilePath();
    if (mLoaded && path == mLoadedPath)
    {
        return;
    }
    cancelPending();
    ++mGeneration;
    mSeenIds.clear();
    mRetryAfter.clear();
    mLoadedPath = path;
    mLoaded = true;

    llifstream file(path);
    if (!file.is_open())
    {
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        // Format: "<timestamp> | <display name> | <username> | <uuid>"
        size_t pos = line.rfind(" | ");
        std::string uuid_str = (pos != std::string::npos) ? line.substr(pos + 3) : line;
        LLStringUtil::trim(uuid_str);

        LLUUID id(uuid_str);
        if (!id.isNull())
        {
            mSeenIds.insert(id);
        }
    }
}

void FSMikoAvatarLog::logAvatar(const LLUUID& id)
{
    static LLCachedControl<bool> log_enabled(gSavedSettings, "MikoAvatarLogEnabled", false);
    if (!log_enabled())
    {
        return;
    }
    if (id.isNull() || id == gAgentID)
    {
        return;
    }

    ensureLoaded();

    if (mSeenIds.find(id) != mSeenIds.end())
    {
        return;
    }
    const auto now = Clock::now();
    auto retry = mRetryAfter.find(id);
    if (retry != mRetryAfter.end() && now < retry->second) return;
    auto pending = mPending.find(id);
    if (pending != mPending.end())
    {
        if (now - pending->second.started < std::chrono::seconds(30)) return;
        pending->second.connection.disconnect();
        mPending.erase(pending);
    }
    mPending.emplace(id, Pending{});
    const auto generation = mGeneration;
    auto connection = LLAvatarNameCache::get(id,
        [this, generation](const LLUUID& avatar, const LLAvatarName& name)
        { onNameReceived(avatar, name, generation); });
    // Cached names may invoke the callback synchronously, removing the pending entry.
    pending = mPending.find(id);
    if (pending != mPending.end()) pending->second.connection = connection;
    else connection.disconnect();
}

void FSMikoAvatarLog::onNameReceived(const LLUUID& id, const LLAvatarName& name, std::size_t generation)
{
    if (generation != mGeneration) return;
    mPending.erase(id);
    if (!gSavedSettings.getBOOL("MikoAvatarLogEnabled") || getFilePath() != mLoadedPath) return;
    if (writeEntry(name.getDisplayName(false), name.getUserName(), id))
    {
        mSeenIds.insert(id);
        mRetryAfter.erase(id);
    }
    else
    {
        // A bad/locked path must not mark this avatar saved or flood the disk each frame.
        mRetryAfter[id] = Clock::now() + std::chrono::seconds(10);
    }
}

void FSMikoAvatarLog::logAllPresentAvatars()
{
    static LLCachedControl<bool> log_enabled(gSavedSettings, "MikoAvatarLogEnabled", false);
    if (!log_enabled())
    {
        return;
    }

    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(character);
        if (avatar)
        {
            logAvatar(avatar->getID());
        }
    }
}

bool FSMikoAvatarLog::writeEntry(const std::string& display_name, const std::string& username, const LLUUID& id)
{
    const std::string path = getFilePath();

    time_t now;
    time(&now);
    struct tm lt;
#ifdef LL_WINDOWS
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    char time_buffer[32];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", &lt);

    llofstream file(path, std::ios_base::app);
    if (!file.is_open())
    {
        LL_WARNS("MikoAvatarLog") << "Could not open avatar log file for writing: " << path << LL_ENDL;
        return false;
    }

    file << time_buffer << " | " << display_name << " | " << username << " | " << id.asString() << "\n";
    file.flush();
    const bool written = file.good();
    file.close();
    if (!written || file.fail()) return false;

    LL_INFOS("MikoAvatarLog") << "Logged encountered avatar: " << display_name << " (" << id << ")" << LL_ENDL;
    return true;
}

// </MikoStorm>
