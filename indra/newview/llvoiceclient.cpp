 /**
 * @file llvoiceclient.cpp
 * @brief Voice client delegation class implementation.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llvoiceclient.h"
#include "llvoicewebrtc.h"
#include "llviewernetwork.h"
#include "llviewercontrol.h"
#include "llcommandhandler.h"
#include "lldir.h"
#include "llhttpnode.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "llui.h"
#include "llkeyboard.h"
#include "llagent.h"
#include "lltrans.h"
#include "lluiusage.h"

const F32 LLVoiceClient::OVERDRIVEN_POWER_LEVEL = 0.7f;

// <FS:Ansariel> Centralized voice power level
const F32 LLVoiceClient::POWER_LEVEL_0 = LLVoiceClient::OVERDRIVEN_POWER_LEVEL / 3.f;
const F32 LLVoiceClient::POWER_LEVEL_1 = LLVoiceClient::OVERDRIVEN_POWER_LEVEL * 2.f / 3.f;
const F32 LLVoiceClient::POWER_LEVEL_2 = LLVoiceClient::OVERDRIVEN_POWER_LEVEL;
// </FS:Ansariel> Centralized voice power level

// <FS:Ansariel> Add callback for user volume change
user_voice_volume_change_callback_t LLVoiceClient::sUserVolumeUpdateSignal;

const F32 LLVoiceClient::VOLUME_MIN = 0.f;
const F32 LLVoiceClient::VOLUME_DEFAULT = 0.5f;
const F32 LLVoiceClient::VOLUME_MAX = 1.0f;


std::string LLVoiceClientStatusObserver::status2string(LLVoiceClientStatusObserver::EStatusType inStatus)
{
    std::string result = "UNTRANSLATED";

    // Prevent copy-paste errors when updating this list...
#define CASE(x)  case x:  result = #x;  break

    switch(inStatus)
    {
            CASE(STATUS_LOGIN_RETRY);
            CASE(STATUS_LOGGED_IN);
            CASE(STATUS_JOINING);
            CASE(STATUS_JOINED);
            CASE(STATUS_LEFT_CHANNEL);
            CASE(STATUS_VOICE_DISABLED);
            CASE(STATUS_VOICE_ENABLED);
            CASE(BEGIN_ERROR_STATUS);
            CASE(ERROR_CHANNEL_FULL);
            CASE(ERROR_CHANNEL_LOCKED);
            CASE(ERROR_NOT_AVAILABLE);
            CASE(ERROR_UNKNOWN);
        default:
            {
                std::ostringstream stream;
                stream << "UNKNOWN(" << (int)inStatus << ")";
                result = stream.str();
            }
            break;
    }

#undef CASE

    return result;
}

LLVoiceModuleInterface *getVoiceModule(const std::string &voice_server_type)
{
    if (voice_server_type == WEBRTC_VOICE_SERVER_TYPE || voice_server_type.empty())
    {
        return (LLVoiceModuleInterface *) LLWebRTCVoiceClient::getInstance();
    }
    else
    {
        return nullptr;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

LLVoiceClient::LLVoiceClient(LLPumpIO *pump)
    :
    mSpatialVoiceModule(NULL),
    mNonSpatialVoiceModule(NULL),
    m_servicePump(NULL),
    mPTTDirty(true),
    mPTT(true),
    mUsePTT(true),
    mPTTMouseButton(0),
    mPTTKey(0),
    mPTTIsToggle(false),
    mUserPTTState(false),
    mMuteMic(false),
    mDisableMic(false)
{
    init(pump);
}

//---------------------------------------------------
// Basic setup/shutdown

LLVoiceClient::~LLVoiceClient()
{
}

void LLVoiceClient::init(LLPumpIO *pump)
{
    m_servicePump = pump;
    LLWebRTCVoiceClient::getInstance()->init(pump);
}

void LLVoiceClient::userAuthorized(const std::string& user_id, const LLUUID &agentID)
{
    if (mRegionChangedCallbackSlot.connected())
    {
        mRegionChangedCallbackSlot.disconnect();
    }
    mRegionChangedCallbackSlot = gAgent.addRegionChangedCallback(boost::bind(&LLVoiceClient::onRegionChanged, this));
    LLWebRTCVoiceClient::getInstance()->userAuthorized(user_id, agentID);
}

void LLVoiceClient::handleSimulatorFeaturesReceived(const LLSD &simulatorFeatures)
{
    std::string voiceServerType = simulatorFeatures["VoiceServerType"].asString();

    if (mSpatialVoiceModule && !mNonSpatialVoiceModule)
    {
        LLVoiceVersionInfo version = mSpatialVoiceModule->getVersion();
        if (version.internalVoiceServerType != voiceServerType)
        {
            mSpatialVoiceModule->processChannels(false);
        }
    }
    setSpatialVoiceModule(simulatorFeatures["VoiceServerType"].asString());

    if (mSpatialVoiceModule && !mNonSpatialVoiceModule)
    {
        if (!mSpatialCredentials.isUndefined())
        {
            mSpatialVoiceModule->setSpatialChannel(mSpatialCredentials);
        }
        mSpatialVoiceModule->processChannels(true);
    }
}

static void simulator_features_received_callback(const LLUUID& region_id)
{
    LLViewerRegion *region = gAgent.getRegion();
    if (region && (region->getRegionID() == region_id))
    {
        LLSD simulatorFeatures;
        region->getSimulatorFeatures(simulatorFeatures);
        if (LLVoiceClient::getInstance())
        {
            LLVoiceClient::getInstance()->handleSimulatorFeaturesReceived(simulatorFeatures);
        }
    }
}

void LLVoiceClient::onRegionChanged()
{
    LLViewerRegion *region = gAgent.getRegion();
    if (region && region->simulatorFeaturesReceived())
    {
        LLSD simulatorFeatures;
        region->getSimulatorFeatures(simulatorFeatures);
        if (LLVoiceClient::getInstance())
        {
            LLVoiceClient::getInstance()->handleSimulatorFeaturesReceived(simulatorFeatures);
        }
    }
    else if (region)
    {
        if (mSimulatorFeaturesReceivedSlot.connected())
        {
            mSimulatorFeaturesReceivedSlot.disconnect();
        }
        mSimulatorFeaturesReceivedSlot =
                region->setSimulatorFeaturesReceivedCallback(boost::bind(&simulator_features_received_callback, _1));
    }
}

void LLVoiceClient::setSpatialVoiceModule(const std::string &voice_server_type)
{
    LLVoiceModuleInterface *module = getVoiceModule(voice_server_type);
    if (!module)
    {
        return;
    }
    if (module != mSpatialVoiceModule)
    {
        if (inProximalChannel())
        {
            mSpatialVoiceModule->processChannels(false);
            module->processChannels(true);
        }
        mSpatialVoiceModule = module;
        mSpatialVoiceModule->updateSettings();
    }
}

void LLVoiceClient::setNonSpatialVoiceModule(const std::string &voice_server_type)
{
    mNonSpatialVoiceModule = getVoiceModule(voice_server_type);
    if (!mNonSpatialVoiceModule)
    {
        if (mSpatialVoiceModule)
        {
            mSpatialVoiceModule->processChannels(true);
        }
        return;
    }
    mNonSpatialVoiceModule->updateSettings();
}

void LLVoiceClient::setHidden(bool hidden)
{
    LL_INFOS("Voice") << "( " << (hidden ? "true" : "false") << " )" << LL_ENDL;
#ifdef OPENSIM
    LLWebRTCVoiceClient::getInstance()->setHidden(hidden && LLGridManager::getInstance()->isInSecondLife());
#else
    LLWebRTCVoiceClient::getInstance()->setHidden(hidden);
#endif
}

void LLVoiceClient::terminate()
{
    if (LLWebRTCVoiceClient::instanceExists())
    {
        LLWebRTCVoiceClient::getInstance()->terminate();
    }
    mSpatialVoiceModule = NULL;
    m_servicePump = NULL;

    if (LLSpeakerVolumeStorage::instanceExists())
    {
        LLSpeakerVolumeStorage::deleteSingleton();
    }
}

const LLVoiceVersionInfo LLVoiceClient::getVersion()
{
    if (mSpatialVoiceModule)
    {
        return mSpatialVoiceModule->getVersion();
    }
    else
    {
        LLVoiceVersionInfo result;
        result.serverVersion = std::string();
        result.voiceServerType = std::string();
        result.mBuildVersion = std::string();
        return result;
    }
}

void LLVoiceClient::updateSettings()
{
    setUsePTT(gSavedSettings.getBOOL("PTTCurrentlyEnabled"));
    setPTTIsToggle(gSavedSettings.getBOOL("PushToTalkToggle"));
    mDisableMic = gSavedSettings.getBOOL("VoiceDisableMic");

    updateMicMuteLogic();

    LLWebRTCVoiceClient::getInstance()->updateSettings();
}

//--------------------------------------------------
// tuning

void LLVoiceClient::tuningStart()
{
    LLWebRTCVoiceClient::getInstance()->tuningStart();
}

void LLVoiceClient::tuningStop()
{
    LLWebRTCVoiceClient::getInstance()->tuningStop();
}

bool LLVoiceClient::inTuningMode()
{
    return LLWebRTCVoiceClient::getInstance()->inTuningMode();
}

void LLVoiceClient::tuningSetMicVolume(float volume)
{
    LLWebRTCVoiceClient::getInstance()->tuningSetMicVolume(volume);
}

void LLVoiceClient::tuningSetSpeakerVolume(float volume)
{
    LLWebRTCVoiceClient::getInstance()->tuningSetSpeakerVolume(volume);
}

float LLVoiceClient::tuningGetEnergy(void)
{
    return LLWebRTCVoiceClient::getInstance()->tuningGetEnergy();
}

//------------------------------------------------
// devices

bool LLVoiceClient::deviceSettingsAvailable()
{
    return LLWebRTCVoiceClient::getInstance()->deviceSettingsAvailable();
}

bool LLVoiceClient::deviceSettingsUpdated()
{
    return LLWebRTCVoiceClient::getInstance()->deviceSettingsUpdated();
}

void LLVoiceClient::refreshDeviceLists(bool clearCurrentList)
{
    LLWebRTCVoiceClient::getInstance()->refreshDeviceLists(clearCurrentList);
}

void LLVoiceClient::setCaptureDevice(const std::string& name)
{
    LLWebRTCVoiceClient::getInstance()->setCaptureDevice(name);
}

void LLVoiceClient::setRenderDevice(const std::string& name)
{
    LLWebRTCVoiceClient::getInstance()->setRenderDevice(name);
}

const LLVoiceDeviceList& LLVoiceClient::getCaptureDevices()
{
    return LLWebRTCVoiceClient::getInstance()->getCaptureDevices();
}


const LLVoiceDeviceList& LLVoiceClient::getRenderDevices()
{
    return LLWebRTCVoiceClient::getInstance()->getRenderDevices();
}


//--------------------------------------------------
// participants

void LLVoiceClient::getParticipantList(std::set<LLUUID> &participants) const
{
    LLWebRTCVoiceClient::getInstance()->getParticipantList(participants);
}

bool LLVoiceClient::isParticipant(const LLUUID &speaker_id) const
{
    return LLWebRTCVoiceClient::getInstance()->isParticipant(speaker_id);
}


//--------------------------------------------------
// text chat

bool LLVoiceClient::isSessionTextIMPossible(const LLUUID& id)
{
    return true;
}

bool LLVoiceClient::isSessionCallBackPossible(const LLUUID& id)
{
    return true;
}

//----------------------------------------------
// channels

bool LLVoiceClient::inProximalChannel()
{
    if (mSpatialVoiceModule)
    {
        return mSpatialVoiceModule->inProximalChannel();
    }
    else
    {
        return false;
    }
}

void LLVoiceClient::setNonSpatialChannel(
    const LLSD& channelInfo,
    bool notify_on_first_join,
    bool hangup_on_last_leave)
{
    setNonSpatialVoiceModule(channelInfo["voice_server_type"].asString());
    if (mSpatialVoiceModule && mSpatialVoiceModule != mNonSpatialVoiceModule)
    {
        mSpatialVoiceModule->processChannels(false);
    }
    if (mNonSpatialVoiceModule)
    {
        mNonSpatialVoiceModule->processChannels(true);
        mNonSpatialVoiceModule->setNonSpatialChannel(channelInfo, notify_on_first_join, hangup_on_last_leave);
    }
}

void LLVoiceClient::setSpatialChannel(const LLSD &channelInfo)
{
    mSpatialCredentials    = channelInfo;
    LLViewerRegion *region = gAgent.getRegion();
    if (region && region->simulatorFeaturesReceived())
    {
        LLSD simulatorFeatures;
        region->getSimulatorFeatures(simulatorFeatures);
        setSpatialVoiceModule(simulatorFeatures["VoiceServerType"].asString());
    }
    else
    {
        return;
    }

    if (mSpatialVoiceModule)
    {
        mSpatialVoiceModule->setSpatialChannel(channelInfo);
    }
}

void LLVoiceClient::leaveNonSpatialChannel()
{
    if (mNonSpatialVoiceModule)
    {
        mNonSpatialVoiceModule->leaveNonSpatialChannel();
        mNonSpatialVoiceModule->processChannels(false);
        mNonSpatialVoiceModule = nullptr;
    }
}

void LLVoiceClient::activateSpatialChannel(bool activate)
{
    if (mSpatialVoiceModule)
    {
        mSpatialVoiceModule->processChannels(activate);
    }
}

bool LLVoiceClient::isCurrentChannel(const LLSD& channelInfo)
{
    return LLWebRTCVoiceClient::getInstance()->isCurrentChannel(channelInfo);
}

bool LLVoiceClient::compareChannels(const LLSD &channelInfo1, const LLSD &channelInfo2)
{
    return LLWebRTCVoiceClient::getInstance()->compareChannels(channelInfo1, channelInfo2);
}

LLVoiceP2PIncomingCallInterfacePtr LLVoiceClient::getIncomingCallInterface(const LLSD& voice_call_info)
{
    LLVoiceModuleInterface *module = getVoiceModule(voice_call_info["voice_server_type"]);
    if (module)
    {
        return module->getIncomingCallInterface(voice_call_info);
    }
    return nullptr;

}

//---------------------------------------
// outgoing calls
LLVoiceP2POutgoingCallInterface *LLVoiceClient::getOutgoingCallInterface(const LLSD& voiceChannelInfo)
{
    LLVoiceModuleInterface *module = getVoiceModule(WEBRTC_VOICE_SERVER_TYPE);
    return dynamic_cast<LLVoiceP2POutgoingCallInterface *>(module);
}

//------------------------------------------
// Volume/gain


void LLVoiceClient::setVoiceVolume(F32 volume)
{
    LLWebRTCVoiceClient::getInstance()->setVoiceVolume(volume);
}

void LLVoiceClient::setMicGain(F32 gain)
{
    LLWebRTCVoiceClient::getInstance()->setMicGain(gain);
}


//------------------------------------------
// enable/disable voice features

// <FS:Ansariel> Bypass LLCachedControls for voice status update
//bool LLVoiceClient::voiceEnabled()
bool LLVoiceClient::voiceEnabled(bool no_cache)
// </FS:Ansariel>
{
    if (no_cache)
    {
        return gSavedSettings.getBOOL("EnableVoiceChat") && !gSavedSettings.getBOOL("CmdLineDisableVoice") && !gNonInteractive;
    }

    static LLCachedControl<bool> enable_voice_chat(gSavedSettings, "EnableVoiceChat");
    static LLCachedControl<bool> cmd_line_disable_voice(gSavedSettings, "CmdLineDisableVoice");
    return enable_voice_chat && !cmd_line_disable_voice && !gNonInteractive;
}

void LLVoiceClient::setVoiceEnabled(bool enabled)
{
    if (LLWebRTCVoiceClient::instanceExists())
    {
        LLWebRTCVoiceClient::getInstance()->setVoiceEnabled(enabled);
    }
}

// <FS:TJ> Fix Nearby Voice when changing voice device settings
void LLVoiceClient::notifyVoiceConnected()
{
#ifndef DISABLE_WEBRTC
    LLWebRTCVoiceClient::getInstance()->notifyVoiceConnected();
#endif
}
// </FS:TJ>

void LLVoiceClient::updateMicMuteLogic()
{
    bool new_mic_mute = false;

    if(mUsePTT)
    {
        new_mic_mute = !mUserPTTState;
    }

    if(mMuteMic || mDisableMic)
    {
        new_mic_mute = true;
    }
    LLWebRTCVoiceClient::getInstance()->setMuteMic(new_mic_mute);
}

void LLVoiceClient::setMuteMic(bool muted)
{
    if (mMuteMic != muted)
    {
        mMuteMic = muted;
        updateMicMuteLogic();
        mMicroChangedSignal();
    }
}


// ----------------------------------------------
// PTT

void LLVoiceClient::setUserPTTState(bool ptt)
{
    if (ptt)
    {
        LLUIUsage::instance().logCommand("Agent.EnableMicrophone");
    }
    mUserPTTState = ptt;
    updateMicMuteLogic();
    mMicroChangedSignal();
}

bool LLVoiceClient::getUserPTTState()
{
    return mUserPTTState;
}

void LLVoiceClient::setUsePTT(bool usePTT)
{
    if(usePTT && !mUsePTT)
    {
        mUserPTTState = false;
    }
    mUsePTT = usePTT;

    updateMicMuteLogic();
}

void LLVoiceClient::setPTTIsToggle(bool PTTIsToggle)
{
    if(!PTTIsToggle && mPTTIsToggle)
    {
        mUserPTTState = false;
    }

    mPTTIsToggle = PTTIsToggle;

    updateMicMuteLogic();
}

bool LLVoiceClient::getPTTIsToggle()
{
    return mPTTIsToggle;
}

void LLVoiceClient::inputUserControlState(bool down)
{
    if(mPTTIsToggle)
    {
        if(down)
        {
            toggleUserPTTState();
        }
    }
    else
    {
        setUserPTTState(down);
        make_ui_sound("UISndMicToggle"); // <FS:PP> FIRE-33249 Mic toggle
    }
}

void LLVoiceClient::toggleUserPTTState(void)
{
    setUserPTTState(!getUserPTTState());
    make_ui_sound("UISndMicToggle"); // <FS:PP> FIRE-33249 Mic toggle
}


//-------------------------------------------
// nearby speaker accessors

bool LLVoiceClient::getVoiceEnabled(const LLUUID& id) const
{
    return isParticipant(id);
}

std::string LLVoiceClient::getDisplayName(const LLUUID& id) const
{
    return LLWebRTCVoiceClient::getInstance()->getDisplayName(id);
}

bool LLVoiceClient::isVoiceWorking() const
{
    return LLWebRTCVoiceClient::getInstance()->isVoiceWorking();
}

bool LLVoiceClient::isParticipantAvatar(const LLUUID& id)
{
    return true;
}

bool LLVoiceClient::isOnlineSIP(const LLUUID& id)
{
    return false;
}

bool LLVoiceClient::getIsSpeaking(const LLUUID& id)
{
    return LLWebRTCVoiceClient::getInstance()->getIsSpeaking(id);
}

bool LLVoiceClient::getIsModeratorMuted(const LLUUID& id)
{
    return LLWebRTCVoiceClient::getInstance()->getIsModeratorMuted(id);
}

F32 LLVoiceClient::getCurrentPower(const LLUUID& id)
{
    return LLWebRTCVoiceClient::getInstance()->getCurrentPower(id);
}

bool LLVoiceClient::getOnMuteList(const LLUUID& id)
{
    return LLMuteList::getInstance()->isMuted(id, LLMute::flagVoiceChat);
}

F32 LLVoiceClient::getUserVolume(const LLUUID& id)
{
    return LLWebRTCVoiceClient::getInstance()->getUserVolume(id);
}

void LLVoiceClient::setUserVolume(const LLUUID& id, F32 volume)
{
    LLWebRTCVoiceClient::getInstance()->setUserVolume(id, volume);
    // <FS:Ansariel> Add callback for user volume change
    sUserVolumeUpdateSignal(id);
}

// <FS:Ansariel> Add callback for user volume change
boost::signals2::connection LLVoiceClient::setUserVolumeUpdateCallback(const user_voice_volume_change_callback_t::slot_type& cb)
{
    return sUserVolumeUpdateSignal.connect(cb);
}
// </FS:Ansariel>

// <FS:Ansariel> Centralized voice power level
EVoicePowerLevel LLVoiceClient::getPowerLevel(const LLUUID& id)
{
    F32 power = getCurrentPower(id);
    if (getOnMuteList(id))
    {
        return VPL_MUTED;
    }
    else if (power == 0.f && !getIsSpeaking(id))
    {
        return VPL_PTT_Off;
    }
    else if (power < POWER_LEVEL_0)
    {
        return VPL_PTT_On;
    }
    else if (power < POWER_LEVEL_1)
    {
        return VPL_Level1;
    }
    else if (power < POWER_LEVEL_2)
    {
        return VPL_Level2;
    }
    else
    {
        return VPL_Level3;
    }
}
// </FS:Ansariel> Centralized voice power level

//--------------------------------------------------
// status observers

void LLVoiceClient::addObserver(LLVoiceClientStatusObserver* observer)
{
    LLWebRTCVoiceClient::getInstance()->addObserver(observer);
}

void LLVoiceClient::removeObserver(LLVoiceClientStatusObserver* observer)
{
    if (LLWebRTCVoiceClient::instanceExists())
    {
        LLWebRTCVoiceClient::getInstance()->removeObserver(observer);
    }
}

void LLVoiceClient::addObserver(LLFriendObserver* observer)
{
    LLWebRTCVoiceClient::getInstance()->addObserver(observer);
}

void LLVoiceClient::removeObserver(LLFriendObserver* observer)
{
    if (LLWebRTCVoiceClient::instanceExists())
    {
        LLWebRTCVoiceClient::getInstance()->removeObserver(observer);
    }
}

void LLVoiceClient::addObserver(LLVoiceClientParticipantObserver* observer)
{
    LLWebRTCVoiceClient::getInstance()->addObserver(observer);
}

void LLVoiceClient::removeObserver(LLVoiceClientParticipantObserver* observer)
{
    if (LLWebRTCVoiceClient::instanceExists())
    {
        LLWebRTCVoiceClient::getInstance()->removeObserver(observer);
    }
}

std::string LLVoiceClient::sipURIFromID(const LLUUID &id) const
{
    if (mNonSpatialVoiceModule)
    {
        return mNonSpatialVoiceModule->sipURIFromID(id);
    }
    else if (mSpatialVoiceModule)
    {
        return mSpatialVoiceModule->sipURIFromID(id);
    }
    else
    {
        return std::string();
    }
}

LLSD LLVoiceClient::getP2PChannelInfoTemplate(const LLUUID& id) const
{
    if (mNonSpatialVoiceModule)
    {
        return mNonSpatialVoiceModule->getP2PChannelInfoTemplate(id);
    }
    else if (mSpatialVoiceModule)
    {
        return mSpatialVoiceModule->getP2PChannelInfoTemplate(id);
    }
    else
    {
        return LLSD();
    }
}

///////////////////
// version checking

class LLViewerRequiredVoiceVersion : public LLHTTPNode
{
    static bool sAlertedUser;
    virtual void post(
                      LLHTTPNode::ResponsePtr response,
                      const LLSD& context,
                      const LLSD& input) const
    {
        std::string voice_server_type = WEBRTC_VOICE_SERVER_TYPE;
        if (input.has("body") && input["body"].has("voice_server_type"))
        {
            voice_server_type = input["body"]["voice_server_type"].asString();
        }

        LLVoiceModuleInterface *voiceModule = NULL;

        if (voice_server_type == WEBRTC_VOICE_SERVER_TYPE || voice_server_type.empty())
        {
            voiceModule = (LLVoiceModuleInterface *) LLWebRTCVoiceClient::getInstance();
        }
        else
        {
            LL_WARNS("Voice") << "Unknown voice server type " << voice_server_type << LL_ENDL;
            return;
        }

        LLVoiceVersionInfo versionInfo = voiceModule->getVersion();
        if (input.has("body") && input["body"].has("major_version") &&
            input["body"]["major_version"].asInteger() > versionInfo.majorVersion)
        {
            if (!sAlertedUser)
            {
                sAlertedUser = true;
                LL_WARNS("Voice") << "Voice server version mismatch " << input["body"]["major_version"].asInteger() << "/"
                                  << versionInfo.majorVersion
                                  << LL_ENDL;
            }
            return;
        }
    }
};

class LLViewerParcelVoiceInfo : public LLHTTPNode
{
    virtual void post(
                      LLHTTPNode::ResponsePtr response,
                      const LLSD& context,
                      const LLSD& input) const
    {
        if ( input.has("body") )
        {
            LLSD body = input["body"];

            if ( body.has("voice_credentials") )
            {
                LLVoiceClient::getInstance()->setSpatialChannel(body["voice_credentials"]);
            }
        }
    }
};

const std::string LLSpeakerVolumeStorage::SETTINGS_FILE_NAME = "volume_settings.xml";

LLSpeakerVolumeStorage::LLSpeakerVolumeStorage()
{
    load();
}

LLSpeakerVolumeStorage::~LLSpeakerVolumeStorage()
{
}

//virtual
void LLSpeakerVolumeStorage::cleanupSingleton()
{
    save();
}

void LLSpeakerVolumeStorage::storeSpeakerVolume(const LLUUID& speaker_id, F32 volume)
{
    if ((volume >= LLVoiceClient::VOLUME_MIN) && (volume <= LLVoiceClient::VOLUME_MAX))
    {
        mSpeakersData[speaker_id] = volume;
    }
    else
    {
        LL_WARNS("Voice") << "Attempted to store out of range volume " << volume << " for " << speaker_id << LL_ENDL;
        llassert(0);
    }
}

bool LLSpeakerVolumeStorage::getSpeakerVolume(const LLUUID& speaker_id, F32& volume)
{
    speaker_data_map_t::const_iterator it = mSpeakersData.find(speaker_id);

    if (it != mSpeakersData.end())
    {
        volume = it->second;
        return true;
    }

    return false;
}

void LLSpeakerVolumeStorage::removeSpeakerVolume(const LLUUID& speaker_id)
{
    mSpeakersData.erase(speaker_id);
}

/* static */ F32 LLSpeakerVolumeStorage::transformFromLegacyVolume(F32 volume_in)
{
    F32 volume_out = 0.f;
    volume_in = llclamp(volume_in, 0.f, 1.0f);

    if (volume_in <= 0.5f)
    {
        volume_out = volume_in * volume_in * 4.f * 0.56f;
    }
    else
    {
        volume_out = (1.f - 0.56f) * (4.f * volume_in * volume_in - 1.f) / 3.f + 0.56f;
    }

    return volume_out;
}

/* static */ F32 LLSpeakerVolumeStorage::transformToLegacyVolume(F32 volume_in)
{
    F32 volume_out = 0.f;
    volume_in = llclamp(volume_in, 0.f, 1.0f);

    if (volume_in <= 0.56f)
    {
        volume_out = sqrt(volume_in / (4.f * 0.56f));
    }
    else
    {
        volume_out = sqrt((3.f * (volume_in - 0.56f) / (1.f - 0.56f) + 1.f) / 4.f);
    }

    return volume_out;
}

void LLSpeakerVolumeStorage::load()
{
    std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, SETTINGS_FILE_NAME);

    LL_INFOS("Voice") << "Loading stored speaker volumes from: " << filename << LL_ENDL;

    LLSD settings_llsd;
    llifstream file;
    file.open(filename.c_str());
    if (file.is_open())
    {
        if (LLSDParser::PARSE_FAILURE == LLSDSerialize::fromXML(settings_llsd, file))
        {
            LL_WARNS("Voice") << "failed to parse " << filename << LL_ENDL;

        }

    }

    for (LLSD::map_const_iterator iter = settings_llsd.beginMap();
        iter != settings_llsd.endMap(); ++iter)
    {
        F32 volume = transformFromLegacyVolume((F32)iter->second.asReal());

        storeSpeakerVolume(LLUUID(iter->first), volume);
    }
}

void LLSpeakerVolumeStorage::save()
{
    std::string user_dir = gDirUtilp->getLindenUserDir();
    if (!user_dir.empty())
    {
        std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, SETTINGS_FILE_NAME);
        LLSD settings_llsd;

        LL_INFOS("Voice") << "Saving stored speaker volumes to: " << filename << LL_ENDL;

        for(speaker_data_map_t::const_iterator iter = mSpeakersData.begin(); iter != mSpeakersData.end(); ++iter)
        {
            F32 volume = transformToLegacyVolume(iter->second);

            settings_llsd[iter->first.asString()] = volume;
        }

        llofstream file;
        file.open(filename.c_str());
        LLSDSerialize::toPrettyXML(settings_llsd, file);
    }
}

bool LLViewerRequiredVoiceVersion::sAlertedUser = false;

LLHTTPRegistration<LLViewerParcelVoiceInfo>
gHTTPRegistrationMessageParcelVoiceInfo(
                                        "/message/ParcelVoiceInfo");

LLHTTPRegistration<LLViewerRequiredVoiceVersion>
gHTTPRegistrationMessageRequiredVoiceVersion(
                                             "/message/RequiredVoiceVersion");
