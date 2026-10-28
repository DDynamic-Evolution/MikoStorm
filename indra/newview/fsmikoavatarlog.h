#ifndef FSMIKOAVATARLOG_H
#define FSMIKOAVATARLOG_H
#include "llsingleton.h"
#include "lluuid.h"
#include <boost/signals2/connection.hpp>
#include <chrono>
#include <map>
#include <set>
#include <string>
class LLAvatarName;
class FSMikoAvatarLog : public LLSingleton<FSMikoAvatarLog>
{
    LLSINGLETON(FSMikoAvatarLog);
    ~FSMikoAvatarLog();
public:
    void logAvatar(const LLUUID& id);
    void logAllPresentAvatars();
    std::string getFilePath() const;
    static std::string getDefaultFilePath();
private:
    using Clock = std::chrono::steady_clock;
    struct Pending
    {
        boost::signals2::connection connection;
        Clock::time_point started{Clock::now()};
    };
    void ensureLoaded();
    void cancelPending();
    void onNameReceived(const LLUUID& id, const LLAvatarName& name, std::size_t generation);
    bool writeEntry(const std::string& display_name, const std::string& username, const LLUUID& id);
    bool mLoaded{false};
    std::string mLoadedPath;
    std::size_t mGeneration{0};
    std::set<LLUUID> mSeenIds;
    std::map<LLUUID, Pending> mPending;
    std::map<LLUUID, Clock::time_point> mRetryAfter;
};
#endif
