#ifndef FS_RESTART_AVOID_H
#define FS_RESTART_AVOID_H

#include "lleventtimer.h"
#include "llstring.h"

class LLVector3d;

class FSRestartAvoid : public LLEventTimer
{
public:
    static FSRestartAvoid& instance();

    void onRegionRestart(const std::string& region_name, S32 seconds);

    static void storeOriginalLocation();
    static void clearState();

    static bool isEnabled();
    static bool isActive();

private:
    FSRestartAvoid();
    ~FSRestartAvoid();

    enum EState
    {
        STATE_IDLE,
        STATE_EVACUATING,
        STATE_EVACUATED,
        STATE_RETURNING
    };

    bool tick() override;
    void evacuate();
    void attemptReturn();
    void onRegionChanged();

    struct Destination
    {
        std::string mRegion;
        S32 mX;
        S32 mY;
        S32 mZ;
    };

    std::vector<Destination> parseDestinations();

    EState mState;
    std::string mOriginalRegion;
    S32 mOriginalX;
    S32 mOriginalY;
    S32 mOriginalZ;
    F32 mTimer;
    S32 mEvacuateDelay;
    S32 mRetryCount;

    boost::signals2::connection mRegionChangedConnection;
};

#endif
