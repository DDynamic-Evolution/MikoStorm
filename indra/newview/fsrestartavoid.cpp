#include "llviewerprecompiledheaders.h"

#include "fsrestartavoid.h"

#include "llagent.h"
#include "llnotificationsutil.h"
#include "llregionhandle.h"
#include "llstring.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llworld.h"
#include "llworldmapmessage.h"

static FSRestartAvoid* sInstance = NULL;

FSRestartAvoid& FSRestartAvoid::instance()
{
    if (!sInstance)
    {
        sInstance = new FSRestartAvoid();
    }
    return *sInstance;
}

FSRestartAvoid::FSRestartAvoid()
    : LLEventTimer(1.f)
    , mState(STATE_IDLE)
    , mOriginalX(0)
    , mOriginalY(0)
    , mOriginalZ(0)
    , mTimer(0.f)
    , mEvacuateDelay(30)
    , mRetryCount(0)
{
}

FSRestartAvoid::~FSRestartAvoid()
{
    if (mRegionChangedConnection.connected())
    {
        mRegionChangedConnection.disconnect();
    }
}

bool FSRestartAvoid::isEnabled()
{
    static LLCachedControl<bool> sEnabled(gSavedSettings, "FSRestartAvoidEnabled");
    return sEnabled;
}

bool FSRestartAvoid::isActive()
{
    return sInstance && sInstance->mState != STATE_IDLE;
}

void FSRestartAvoid::storeOriginalLocation()
{
    LLViewerRegion* region = gAgent.getRegion();
    if (!region) return;

    LLVector3d global_pos = gAgent.getPositionGlobal();
    instance().mOriginalRegion = region->getName();
    instance().mOriginalX = (S32)global_pos.mdV[VX];
    instance().mOriginalY = (S32)global_pos.mdV[VY];
    instance().mOriginalZ = (S32)global_pos.mdV[VZ];
}

void FSRestartAvoid::clearState()
{
    if (!sInstance) return;
    sInstance->mState = STATE_IDLE;
    sInstance->mTimer = 0.f;
    sInstance->mRetryCount = 0;
    if (sInstance->mRegionChangedConnection.connected())
    {
        sInstance->mRegionChangedConnection.disconnect();
    }
}

void FSRestartAvoid::onRegionRestart(const std::string& region_name, S32 seconds)
{
    if (!isEnabled() || mState != STATE_IDLE) return;

    storeOriginalLocation();

    static LLCachedControl<S32> sDelay(gSavedSettings, "FSRestartAvoidDelay");
    mEvacuateDelay = (S32)sDelay;
    if (mEvacuateDelay < 1) mEvacuateDelay = 1;
    if (mEvacuateDelay > 120) mEvacuateDelay = 120;

    mTimer = (F32)mEvacuateDelay;
    mState = STATE_EVACUATING;
    mRetryCount = 0;

    mRegionChangedConnection = gAgent.addRegionChangedCallback(
        boost::bind(&FSRestartAvoid::onRegionChanged, this));

    LLSD args;
    args["REGION"] = region_name;
    args["DELAY"] = (LLSD::Integer)mEvacuateDelay;
    LLNotificationsUtil::add("FSRestartAvoidActivated", args);

}

bool FSRestartAvoid::tick()
{
    if (mState == STATE_IDLE)
    {
        return true;
    }

    if (mState == STATE_EVACUATING)
    {
        mTimer -= 1.f;
        if (mTimer <= 0.f)
        {
            evacuate();
        }
        return false;
    }

    if (mState == STATE_EVACUATED)
    {
        mTimer -= 1.f;
        if (mTimer <= 0.f)
        {
            mRetryCount++;
            attemptReturn();
            mTimer = 60.f;
        }
        return false;
    }

    return false;
}

std::vector<FSRestartAvoid::Destination> FSRestartAvoid::parseDestinations()
{
    std::vector<Destination> result;
    std::string raw = gSavedSettings.getString("FSRestartAvoidDestinations");
    if (raw.empty()) return result;

    std::istringstream stream(raw);
    std::string line;
    while (std::getline(stream, line))
    {
        LLStringUtil::trim(line);
        if (line.empty() || line[0] == '#') continue;

        size_t s1 = line.find('/');
        if (s1 == std::string::npos) continue;
        size_t s2 = line.find('/', s1 + 1);
        if (s2 == std::string::npos) continue;
        size_t s3 = line.find('/', s2 + 1);
        if (s3 == std::string::npos) continue;

        Destination dest;
        dest.mRegion = line.substr(0, s1);
        dest.mX = (S32)strtol(line.substr(s1 + 1, s2 - s1 - 1).c_str(), NULL, 10);
        dest.mY = (S32)strtol(line.substr(s2 + 1, s3 - s2 - 1).c_str(), NULL, 10);
        dest.mZ = (S32)strtol(line.substr(s3 + 1).c_str(), NULL, 10);
        result.push_back(dest);
    }

    return result;
}

void FSRestartAvoid::evacuate()
{
    std::vector<Destination> destinations = parseDestinations();
    if (destinations.empty())
    {
        LLNotificationsUtil::add("FSRestartAvoidNoDestinations");
        clearState();
        return;
    }

    mState = STATE_EVACUATED;
    mTimer = 60.f;

    Destination& dest = destinations[0];

    LLWorldMapMessage::url_callback_t callback =
        [dest](U64 region_handle, const std::string& url, const LLUUID& snapshot_id, bool teleport)
    {
        LLVector3d global_pos = from_region_handle(region_handle);
        global_pos += LLVector3d((F64)dest.mX, (F64)dest.mY, (F64)dest.mZ);
        gAgent.teleportViaLocation(global_pos);

        LLSD args;
        args["REGION"] = dest.mRegion;
        LLNotificationsUtil::add("FSRestartAvoidTeleported", args);
    };

    LLWorldMapMessage::getInstance()->sendNamedRegionRequest(
        dest.mRegion,
        callback,
        std::string(),
        true);
}

void FSRestartAvoid::attemptReturn()
{
    mState = STATE_RETURNING;
    LLVector3d global_pos((F64)mOriginalX, (F64)mOriginalY, (F64)mOriginalZ);
    gAgent.teleportViaLocation(global_pos);
}

void FSRestartAvoid::onRegionChanged()
{
    if (mState != STATE_RETURNING && mState != STATE_EVACUATED) return;

    LLViewerRegion* region = gAgent.getRegion();
    if (!region) return;

    std::string current_region = region->getName();
    if (current_region == mOriginalRegion)
    {
        LLSD args;
        args["REGION"] = mOriginalRegion;
        LLNotificationsUtil::add("FSRestartAvoidReturned", args);
        clearState();
    }
    else if (mState == STATE_RETURNING)
    {
        mState = STATE_EVACUATED;
        mTimer = 60.f;
    }
}
