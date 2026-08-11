/**
 * @file llrestartavoidancemgr.h
 *
 * $LicenseInfo:firstyear=2024&license=mikostormlgpl$
 * MikoStorm Viewer Source Code
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * $/LicenseInfo$
 */

#ifndef LL_RESTART_AVOIDANCE_MGR_H
#define LL_RESTART_AVOIDANCE_MGR_H

#include "lleventtimer.h"
#include "llsd.h"

#include <map>
#include <set>

class LLViewerRegion;

class RestartAvoidanceManager : public LLEventTimer
{
    LOG_CLASS(RestartAvoidanceManager);

public:
    enum ERegionStatus
    {
        STATUS_UNKNOWN = 0,
        STATUS_ONLINE,
        STATUS_DOWN,
        STATUS_AWAITING
    };

    enum EState
    {
        STATE_IDLE = 0,
        STATE_LEAVING,
        STATE_LEFT,
        STATE_RETURNING
    };

    static RestartAvoidanceManager& instance();

    bool isEnabled() const;
    bool isActive() const;
    EState getState() const { return mState; }

    // Entry point called when the viewer receives a region restart notice.
    void onRegionRestart(const std::string& region_name, S32 seconds);

    // Starts the full evacuate-and-return cycle for the current region.
    // seconds_until_restart is the time to the scheduled restart (0/-1 if unknown).
    void autoLeaveAndReturn(S32 seconds_until_restart = 0);

    // Runs a simulated restart using the current region as the "home" region.
    void simulateRestart();

    // Triggers an asynchronous status check for the given region.
    void checkRegionStatus(const std::string& region_name);

    // Safe region list management (LLSD array under RestartAvoidanceSafeList).
    LLSD loadSafeList() const;
    void saveSafeList(const LLSD& list);
    const LLSD& getSafeList() const { return mSafeList; }
    void setSafeList(const LLSD& list);

    // Latest status of a region (STATUS_UNKNOWN if never checked).
    ERegionStatus getRegionStatus(const std::string& region_name) const;

    // Counters for the status line: "X online, Y down, Z awaiting reply".
    S32 getOnlineCount() const { return mOnlineCount; }
    S32 getDownCount() const { return mDownCount; }
    S32 getAwaitingCount() const { return mAwaitingCount; }

    // Signal emitted whenever region statuses or the state machine changes.
    typedef boost::signals2::signal<void()> status_changed_signal_t;
    boost::signals2::connection addStatusChangedCallback(const status_changed_signal_t::slot_type& cb);

protected:
    bool tick() override;

private:
    RestartAvoidanceManager();
    ~RestartAvoidanceManager();

    void evacuate();
    void attemptReturn();
    void onRegionChanged();

    void checkHomeRegion();
    void updateStatusText();
    void storeOriginalLocation();

    // HTTP result handling for region status requests.
    void handleRegionStatusResponse(const std::string& region_name, const LLSD& http_data, bool success);

    std::string makeRegionStatusURL(const std::string& region_name) const;

    EState mState;
    std::string mOriginalRegion;
    S32 mOriginalX;
    S32 mOriginalY;
    S32 mOriginalZ;
    F32 mTimer;
    S32 mRetryCount;
    S32 mEvacuateDelay;
    bool mPendingReturn;
    std::string mEvacuateTargetRegion;

    LLSD mSafeList;
    std::map<std::string, ERegionStatus> mRegionStatus;
    std::set<std::string> mPendingChecks;
    S32 mOnlineCount;
    S32 mDownCount;
    S32 mAwaitingCount;
    std::string mStatusText;

    boost::signals2::connection mRegionChangedConnection;
    status_changed_signal_t mStatusChangedSignal;

    static RestartAvoidanceManager* sInstance;
    static const S32 MAX_RETRY_COUNT;
};

#endif // LL_RESTART_AVOIDANCE_MGR_H
