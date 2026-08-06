/**
 * @file llrestartavoidancemgr.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llrestartavoidancemgr.h"

#include "curl/curl.h"

#include "fscorehttputil.h"
#include "llagent.h"
#include "llcorehttputil.h"
#include "llhttpconstants.h"
#include "llnotificationsutil.h"
#include "llregionhandle.h"
#include "llsdjson.h"
#include "llstring.h"
#include "lltrans.h"
#include "lluri.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llworldmapmessage.h"

#include <boost/bind.hpp>
#include <boost/json.hpp>

const S32 RestartAvoidanceManager::MAX_RETRY_COUNT = 10;
RestartAvoidanceManager* RestartAvoidanceManager::sInstance = NULL;

RestartAvoidanceManager& RestartAvoidanceManager::instance()
{
    if (!sInstance)
    {
        sInstance = new RestartAvoidanceManager();
    }
    return *sInstance;
}

RestartAvoidanceManager::RestartAvoidanceManager()
    : LLEventTimer(1.f)
    , mState(STATE_IDLE)
    , mOriginalX(0)
    , mOriginalY(0)
    , mOriginalZ(0)
    , mTimer(0.f)
    , mRetryCount(0)
    , mEvacuateDelay(30)
    , mPendingReturn(false)
    , mOnlineCount(0)
    , mDownCount(0)
    , mAwaitingCount(0)
{
    mSafeList = loadSafeList();
    updateStatusText();
}

RestartAvoidanceManager::~RestartAvoidanceManager()
{
    if (mRegionChangedConnection.connected())
    {
        mRegionChangedConnection.disconnect();
    }
}

bool RestartAvoidanceManager::isEnabled() const
{
    return gSavedSettings.getBOOL("RestartAvoidanceEnable");
}

bool RestartAvoidanceManager::isActive() const
{
    return mState != STATE_IDLE;
}

void RestartAvoidanceManager::storeOriginalLocation()
{
    LLViewerRegion* region = gAgent.getRegion();
    if (!region)
    {
        LL_WARNS("RestartAvoidance") << "Cannot store home location, no current region" << LL_ENDL;
        return;
    }

    LLVector3d global_pos = gAgent.getPositionGlobal();
    mOriginalRegion = region->getName();
    mOriginalX = (S32)global_pos.mdV[VX];
    mOriginalY = (S32)global_pos.mdV[VY];
    mOriginalZ = (S32)global_pos.mdV[VZ];

    LL_INFOS("RestartAvoidance") << "Stored home location: " << mOriginalRegion << " "
        << mOriginalX << ", " << mOriginalY << ", " << mOriginalZ << LL_ENDL;
}

void RestartAvoidanceManager::onRegionRestart(const std::string& region_name, S32 seconds)
{
    if (!isEnabled())
    {
        LL_DEBUGS("RestartAvoidance") << "Ignoring restart of " << region_name
            << " (restart avoidance disabled)" << LL_ENDL;
        return;
    }

    if (mState != STATE_IDLE)
    {
        LL_DEBUGS("RestartAvoidance") << "Ignoring restart of " << region_name
            << " (already in state " << mState << ")" << LL_ENDL;
        return;
    }

    if (!region_name.empty() && gAgent.getRegion() && region_name != gAgent.getRegion()->getName())
    {
        LL_DEBUGS("RestartAvoidance") << "Restart notice for " << region_name
            << " does not match current region, ignoring" << LL_ENDL;
        return;
    }

    LL_INFOS("RestartAvoidance") << "Region restart detected for " << region_name
        << " in " << seconds << " seconds" << LL_ENDL;

    storeOriginalLocation();
    autoLeaveAndReturn(seconds);
}

void RestartAvoidanceManager::autoLeaveAndReturn(S32 seconds_until_restart)
{
    if (mOriginalRegion.empty())
    {
        storeOriginalLocation();
    }
    if (mOriginalRegion.empty())
    {
        LL_WARNS("RestartAvoidance") << "Cannot start restart avoidance without a home region" << LL_ENDL;
        return;
    }

    mEvacuateDelay = llclamp(gSavedSettings.getS32("RestartAvoidanceWaitLeave"), 1, 600);
    if (seconds_until_restart > 0 && seconds_until_restart < mEvacuateDelay)
    {
        // The restart is sooner than the configured leave delay, leave immediately.
        LL_INFOS("RestartAvoidance") << "Restart in " << seconds_until_restart
            << "s is sooner than configured leave delay of " << mEvacuateDelay
            << "s, leaving now" << LL_ENDL;
        mEvacuateDelay = 1;
    }
    mTimer = (F32)mEvacuateDelay;
    mState = STATE_LEAVING;
    mRetryCount = 0;
    mPendingReturn = false;

    if (!mRegionChangedConnection.connected())
    {
        mRegionChangedConnection = gAgent.addRegionChangedCallback(
            boost::bind(&RestartAvoidanceManager::onRegionChanged, this));
    }

    LLSD args;
    args["REGION"] = mOriginalRegion;
    args["DELAY"] = (LLSD::Integer)mEvacuateDelay;
    LLNotificationsUtil::add("RestartAvoidanceActivated", args);

    updateStatusText();
    mStatusChangedSignal();
}

void RestartAvoidanceManager::simulateRestart()
{
    LL_INFOS("RestartAvoidance") << "Simulated region restart requested" << LL_ENDL;

    storeOriginalLocation();
    if (mOriginalRegion.empty())
    {
        return;
    }

    mEvacuateDelay = 5;
    mTimer = (F32)mEvacuateDelay;
    mState = STATE_LEAVING;
    mRetryCount = 0;
    mPendingReturn = false;

    if (!mRegionChangedConnection.connected())
    {
        mRegionChangedConnection = gAgent.addRegionChangedCallback(
            boost::bind(&RestartAvoidanceManager::onRegionChanged, this));
    }

    LLSD args;
    args["REGION"] = mOriginalRegion;
    LLNotificationsUtil::add("RestartAvoidanceSimulated", args);

    updateStatusText();
    mStatusChangedSignal();
}

bool RestartAvoidanceManager::tick()
{
    if (mState == STATE_IDLE)
    {
        return false;
    }

    if (mState == STATE_LEAVING)
    {
        mTimer -= 1.f;
        if (mTimer <= 0.f)
        {
            LL_INFOS("RestartAvoidance") << "Leave countdown finished, evacuating" << LL_ENDL;
            evacuate();
        }
        return false;
    }

    if (mState == STATE_LEFT)
    {
        mTimer -= 1.f;
        if (mTimer <= 0.f)
        {
            if (mPendingReturn)
            {
                attemptReturn();
            }
            else
            {
                checkHomeRegion();
            }
        }
        return false;
    }

    if (mState == STATE_RETURNING)
    {
        // Safety net: if the return teleport never completes (no region change
        // callback), fall back to the retry logic.
        mTimer -= 1.f;
        if (mTimer <= 0.f)
        {
            mRetryCount++;
            if (mRetryCount >= MAX_RETRY_COUNT)
            {
                LL_WARNS("RestartAvoidance") << "Giving up after " << mRetryCount
                    << " failed return attempts" << LL_ENDL;
                LLSD args;
                args["COUNT"] = (LLSD::Integer)mRetryCount;
                LLNotificationsUtil::add("RestartAvoidanceMaxRetries", args);
                mState = STATE_IDLE;
                mPendingReturn = false;
                updateStatusText();
                mStatusChangedSignal();
                return false;
            }

            mState = STATE_LEFT;
            mPendingReturn = false;
            mTimer = (F32)llclamp(gSavedSettings.getS32("RestartAvoidanceRetryFail"), 5, 3600);
            LL_WARNS("RestartAvoidance") << "Return teleport did not complete, retrying in "
                << (S32)mTimer << "s" << LL_ENDL;
            updateStatusText();
            mStatusChangedSignal();
        }
        return false;
    }

    return false;
}

void RestartAvoidanceManager::evacuate()
{
    if (mSafeList.size() == 0)
    {
        mSafeList = loadSafeList();
    }

    if (mSafeList.size() == 0 || !mSafeList[0].has("region"))
    {
        LL_WARNS("RestartAvoidance") << "Cannot evacuate, safe list is empty" << LL_ENDL;
        LLNotificationsUtil::add("RestartAvoidanceNoSafeList");
        mState = STATE_IDLE;
        mPendingReturn = false;
        updateStatusText();
        mStatusChangedSignal();
        return;
    }

    std::string dest_region = mSafeList[0]["region"].asString();
    if (dest_region.empty())
    {
        LL_WARNS("RestartAvoidance") << "Cannot evacuate, first safe list entry has no region" << LL_ENDL;
        LLNotificationsUtil::add("RestartAvoidanceNoSafeList");
        mState = STATE_IDLE;
        mPendingReturn = false;
        updateStatusText();
        mStatusChangedSignal();
        return;
    }

    LLVector3d landing_offset(
        (F64)gSavedSettings.getS32("RestartAvoidanceLandingX"),
        (F64)gSavedSettings.getS32("RestartAvoidanceLandingY"),
        (F64)gSavedSettings.getS32("RestartAvoidanceLandingZ"));

    mState = STATE_LEFT;
    mPendingReturn = false;
    mTimer = (F32)llclamp(gSavedSettings.getS32("RestartAvoidanceHomeCheck"), 5, 3600);

    LLWorldMapMessage::url_callback_t callback =
        [dest_region, landing_offset](U64 region_handle, const std::string& url, const LLUUID& snapshot_id, bool teleport)
    {
        if (region_handle == 0)
        {
            LL_WARNS("RestartAvoidance") << "Could not resolve destination region "
                << dest_region << " for evacuation" << LL_ENDL;
            LLSD args;
            args["REGION"] = dest_region;
            LLNotificationsUtil::add("RestartAvoidanceEvacuateFailed", args);
            return;
        }

        LLVector3d global_pos = from_region_handle(region_handle);
        global_pos += landing_offset;
        gAgent.teleportViaLocation(global_pos);

        LLSD args;
        args["REGION"] = dest_region;
        LLNotificationsUtil::add("RestartAvoidanceEvacuated", args);
    };

    LL_INFOS("RestartAvoidance") << "Evacuating to " << dest_region << LL_ENDL;
    LLWorldMapMessage::getInstance()->sendNamedRegionRequest(
        dest_region,
        callback,
        std::string(),
        true);

    updateStatusText();
    mStatusChangedSignal();
}

void RestartAvoidanceManager::checkHomeRegion()
{
    if (mOriginalRegion.empty())
    {
        mState = STATE_IDLE;
        return;
    }

    mTimer = (F32)llclamp(gSavedSettings.getS32("RestartAvoidanceHomeCheck"), 5, 3600);

    if (mPendingChecks.count(mOriginalRegion))
    {
        LL_DEBUGS("RestartAvoidance") << "Home region status check already in flight" << LL_ENDL;
        return;
    }

    LL_DEBUGS("RestartAvoidance") << "Checking home region " << mOriginalRegion << LL_ENDL;
    checkRegionStatus(mOriginalRegion);
}

void RestartAvoidanceManager::attemptReturn()
{
    mState = STATE_RETURNING;
    mPendingReturn = false;
    mTimer = (F32)llclamp(gSavedSettings.getS32("RestartAvoidanceRetryFail"), 5, 3600);

    LLVector3d global_pos((F64)mOriginalX, (F64)mOriginalY, (F64)mOriginalZ);
    LL_INFOS("RestartAvoidance") << "Attempting to return to " << mOriginalRegion
        << " (" << mOriginalX << ", " << mOriginalY << ", " << mOriginalZ << ")" << LL_ENDL;
    gAgent.teleportViaLocation(global_pos);

    updateStatusText();
    mStatusChangedSignal();
}

void RestartAvoidanceManager::onRegionChanged()
{
    if (mState == STATE_IDLE)
    {
        return;
    }

    LLViewerRegion* region = gAgent.getRegion();
    if (!region)
    {
        return;
    }

    std::string current = region->getName();
    LL_DEBUGS("RestartAvoidance") << "Region changed to " << current << " (state=" << mState << ")" << LL_ENDL;

    if (current == mOriginalRegion)
    {
        if (mState == STATE_RETURNING)
        {
            LL_INFOS("RestartAvoidance") << "Returned to " << mOriginalRegion << LL_ENDL;
        }
        else
        {
            LL_INFOS("RestartAvoidance") << "Already at " << mOriginalRegion << ", cancelling" << LL_ENDL;
        }

        LLSD args;
        args["REGION"] = mOriginalRegion;
        LLNotificationsUtil::add("RestartAvoidanceReturned", args);

        mState = STATE_IDLE;
        mRetryCount = 0;
        mPendingReturn = false;
        updateStatusText();
        mStatusChangedSignal();
        return;
    }

    if (mState == STATE_RETURNING)
    {
        // The return teleport landed somewhere else - wait and retry.
        mRetryCount++;
        if (mRetryCount >= MAX_RETRY_COUNT)
        {
            LL_WARNS("RestartAvoidance") << "Giving up after " << mRetryCount
                << " failed return attempts" << LL_ENDL;
            LLSD args;
            args["COUNT"] = (LLSD::Integer)mRetryCount;
            LLNotificationsUtil::add("RestartAvoidanceMaxRetries", args);
            mState = STATE_IDLE;
            mPendingReturn = false;
            updateStatusText();
            mStatusChangedSignal();
            return;
        }

        mState = STATE_LEFT;
        mPendingReturn = false;
        mTimer = (F32)llclamp(gSavedSettings.getS32("RestartAvoidanceRetryFail"), 5, 3600);
        LL_WARNS("RestartAvoidance") << "Return teleport landed in " << current
            << ", retrying in " << (S32)mTimer << "s" << LL_ENDL;
        updateStatusText();
        mStatusChangedSignal();
    }
}

std::string RestartAvoidanceManager::makeRegionStatusURL(const std::string& region_name) const
{
    std::string map_server = gSavedSettings.getString("CurrentMapServerURL");
    if (map_server.empty())
    {
        map_server = "http://map.secondlife.com";
    }
    if (!map_server.empty() && map_server[map_server.size() - 1] == '/')
    {
        map_server = map_server.substr(0, map_server.size() - 1);
    }
    return map_server + "/region/" + LLURI::escape(region_name);
}

void RestartAvoidanceManager::checkRegionStatus(const std::string& region_name)
{
    if (region_name.empty())
    {
        return;
    }

    mRegionStatus[region_name] = STATUS_AWAITING;
    mPendingChecks.insert(region_name);

    std::string url = makeRegionStatusURL(region_name);
    S32 timeout = llclamp(gSavedSettings.getS32("RestartAvoidanceRegionTimeout"), 1, 120);

    LLCore::HttpOptions::ptr_t httpOpts = std::make_shared<LLCore::HttpOptions>();
    httpOpts->setTimeout((unsigned int)timeout);

    std::string check_name = region_name;
    LL_DEBUGS("RestartAvoidance") << "Requesting status of " << region_name
        << " from " << url << " (timeout " << timeout << "s)" << LL_ENDL;

    FSCoreHttpUtil::callbackHttpGetRaw(
        url,
        boost::bind(&RestartAvoidanceManager::handleRegionStatusResponse, this, check_name, _1, true),
        boost::bind(&RestartAvoidanceManager::handleRegionStatusResponse, this, check_name, _1, false),
        LLCore::HttpHeaders::ptr_t(),
        httpOpts);

    updateStatusText();
    mStatusChangedSignal();
}

void RestartAvoidanceManager::handleRegionStatusResponse(const std::string& region_name, const LLSD& http_data, bool success)
{
    mPendingChecks.erase(region_name);

    ERegionStatus status = STATUS_DOWN;
    bool is_timeout = false;

    if (!success)
    {
        LLCore::HttpStatus http_status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(http_data);
        is_timeout = (http_status.getType() == LLCore::HttpStatus::EXT_CURL_EASY
            && http_status.getStatus() == CURLE_OPERATION_TIMEDOUT);
        LL_DEBUGS("RestartAvoidance") << "Status check for " << region_name << " failed: "
            << http_status.toTerseString() << " (timeout=" << is_timeout << ")" << LL_ENDL;

        if (is_timeout)
        {
            status = STATUS_AWAITING;
        }
    }
    else
    {
        const LLSD::Binary& raw = http_data[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
        std::string body;
        body.assign(raw.begin(), raw.end());
        LL_DEBUGS("RestartAvoidance") << "Status reply for " << region_name << ": " << body << LL_ENDL;

        if (!body.empty())
        {
            try
            {
                LLSD parsed = LlsdFromJson(boost::json::parse(body));
                if (parsed.has("success") && parsed["success"].asBoolean())
                {
                    LLSD result = parsed.has("result") ? parsed["result"] : LLSD();
                    if (result.isArray() && result.size() > 0)
                    {
                        status = STATUS_ONLINE;
                    }
                }
            }
            catch (...)
            {
                LL_WARNS("RestartAvoidance") << "Failed to parse status reply for " << region_name << LL_ENDL;
                status = STATUS_DOWN;
            }
        }
    }

    mRegionStatus[region_name] = status;
    LL_DEBUGS("RestartAvoidance") << "Region " << region_name << " status is " << (S32)status << LL_ENDL;

    // If the home region just came back online, start the return countdown.
    if (mState == STATE_LEFT && region_name == mOriginalRegion && !mPendingReturn && status == STATUS_ONLINE)
    {
        mPendingReturn = true;
        mTimer = (F32)llclamp(gSavedSettings.getS32("RestartAvoidanceWaitReturn"), 5, 3600);
        LL_INFOS("RestartAvoidance") << "Home region " << mOriginalRegion
            << " is back online, returning in " << (S32)mTimer << "s" << LL_ENDL;
    }

    updateStatusText();
    mStatusChangedSignal();
}

LLSD RestartAvoidanceManager::loadSafeList() const
{
    LLSD list = gSavedSettings.getLLSD("RestartAvoidanceSafeList");
    if (!list.isArray())
    {
        list = LLSD::emptyArray();
    }
    return list;
}

void RestartAvoidanceManager::saveSafeList(const LLSD& list)
{
    mSafeList = list;
    gSavedSettings.setLLSD("RestartAvoidanceSafeList", list);
    LL_DEBUGS("RestartAvoidance") << "Saved " << list.size() << " safe region(s)" << LL_ENDL;
}

void RestartAvoidanceManager::setSafeList(const LLSD& list)
{
    mSafeList = list;
}

RestartAvoidanceManager::ERegionStatus RestartAvoidanceManager::getRegionStatus(const std::string& region_name) const
{
    std::map<std::string, ERegionStatus>::const_iterator it = mRegionStatus.find(region_name);
    if (it == mRegionStatus.end())
    {
        return STATUS_UNKNOWN;
    }
    return it->second;
}

boost::signals2::connection RestartAvoidanceManager::addStatusChangedCallback(const status_changed_signal_t::slot_type& cb)
{
    return mStatusChangedSignal.connect(cb);
}

void RestartAvoidanceManager::updateStatusText()
{
    mOnlineCount = 0;
    mDownCount = 0;
    mAwaitingCount = 0;

    for (std::map<std::string, ERegionStatus>::const_iterator it = mRegionStatus.begin(); it != mRegionStatus.end(); ++it)
    {
        switch (it->second)
        {
        case STATUS_ONLINE:
            mOnlineCount++;
            break;
        case STATUS_DOWN:
            mDownCount++;
            break;
        case STATUS_AWAITING:
        case STATUS_UNKNOWN:
        default:
            mAwaitingCount++;
            break;
        }
    }

    mStatusText = llformat("%d online, %d down, %d awaiting reply", mOnlineCount, mDownCount, mAwaitingCount);
}
