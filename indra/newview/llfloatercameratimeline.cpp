/**
 * @file llfloatercameratimeline.cpp
 * @brief Floater for camera position/rotation timeline recording and playback
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2026
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

#include "llfloatercameratimeline.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "lldir.h"
#include "lldirpicker.h"
#include "llfile.h"
#include "lllineeditor.h"
#include "llsliderctrl.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llviewercamera.h"
#include "llviewermenufile.h"
#include "llcoordframe.h"

static const std::string TIMELINE_SAVE_SUBDIRECTORY = "camera_timelines";
static const std::string TIMELINE_FILE_EXT = ".camera_timeline.xml";

LLFloaterCameraTimeline::LLFloaterCameraTimeline(const LLSD& key)
    : LLFloater(key),
      mPlaying(false),
      mCurrentTime(0.f),
      mPlaybackSpeed(1.f),
      mPlaybackDuration(30.f),
      mScrubbing(false)
{
}

LLFloaterCameraTimeline::~LLFloaterCameraTimeline()
{
    if (mPlaying)
    {
        stopPlayback();
    }
}

bool LLFloaterCameraTimeline::postBuild()
{
    mKeyframeList = getChild<LLScrollListCtrl>("keyframe_list");
    mCaptureBtn = getChild<LLButton>("capture_btn");
    mDeleteBtn = getChild<LLButton>("delete_btn");
    mClearBtn = getChild<LLButton>("clear_btn");
    mTimelineSlider = getChild<LLSliderCtrl>("timeline_slider");
    mTimeDisplay = getChild<LLTextBox>("time_display");
    mPlayBtn = getChild<LLButton>("play_btn");
    mStopBtn = getChild<LLButton>("stop_btn");
    mSpeedSlider = getChild<LLSliderCtrl>("speed_slider");
    mDurationSpinner = getChild<LLSpinCtrl>("duration_spinner");
    mSaveBtn = getChild<LLButton>("save_btn");
    mLoadBtn = getChild<LLButton>("load_btn");
    mStatusText = getChild<LLTextBox>("status_text");

    mKeyframeList->setCommitCallback([this](LLUICtrl*, const LLSD&) { updateUIState(); });
    mKeyframeList->setCommitOnSelectionChange(true);

    mCaptureBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { captureCamera(); });
    mDeleteBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { deleteSelectedKeyframe(); });
    mClearBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { clearAllKeyframes(); });
    mPlayBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { startPlayback(); });
    mStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { stopPlayback(); });
    mSaveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { saveToFile(); });
    mLoadBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { loadFromFile(); });

    mSpeedSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSpeedChanged(); });
    mDurationSpinner->setCommitCallback([this](LLUICtrl*, const LLSD&) { onDurationChanged(); });

    mTimelineSlider->setCommitCallback([this](LLUICtrl*, const LLSD&) { onTimelineScrub(); });

    mSpeedSlider->setValue(mPlaybackSpeed);
    mDurationSpinner->setValue(mPlaybackDuration);
    mStopBtn->setEnabled(false);

    updateUIState();
    return true;
}

void LLFloaterCameraTimeline::onOpen(const LLSD& key)
{
    updateUIState();
}

void LLFloaterCameraTimeline::onClose(bool app_quitting)
{
    if (mPlaying)
    {
        stopPlayback();
    }
}

void LLFloaterCameraTimeline::draw()
{
    LLFloater::draw();
}

// static
void LLFloaterCameraTimeline::onIdle(void* user_data)
{
    LLFloaterCameraTimeline* self = static_cast<LLFloaterCameraTimeline*>(user_data);
    if (!self || !self->mPlaying)
        return;

    F32 dt = self->mPlaybackTimer.getElapsedTimeAndResetF32();
    self->updatePlayback(dt);
}

// --- Keyframe Management ---

void LLFloaterCameraTimeline::captureCamera()
{
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    if (!cam) return;

    CameraKeyframe kf;
    kf.position = cam->getOrigin();
    kf.rotation = cam->getQuaternion();
    kf.fov = cam->getView();

    // If playing, insert at current playback position
    if (mPlaying)
    {
        kf.time = mCurrentTime;
    }
    // If scrubbing (timeline moved from 0), insert at scrub position
    else if (mCurrentTime > 0.f)
    {
        kf.time = mCurrentTime;
    }
    // Otherwise, append after the last keyframe
    else if (!mKeyframes.empty())
    {
        kf.time = mKeyframes.back().time + (1.f / (F32)mKeyframes.size());
        if (kf.time > 1.f) kf.time = 1.f;
    }
    else
    {
        kf.time = 0.f;
    }

    insertKeyframe(kf);
    updateKeyframeList();
    updateUIState();

    mStatusText->setText(llformat("Captured keyframe #%d at t=%.2f", (int)mKeyframes.size(), kf.time));
}

void LLFloaterCameraTimeline::insertKeyframe(const CameraKeyframe& kf)
{
    mKeyframes.push_back(kf);

    // Sort by time
    std::sort(mKeyframes.begin(), mKeyframes.end(),
        [](const CameraKeyframe& a, const CameraKeyframe& b) { return a.time < b.time; });
}

void LLFloaterCameraTimeline::redistributeTimes()
{
    if (mKeyframes.size() <= 1)
    {
        if (!mKeyframes.empty())
            mKeyframes[0].time = 0.f;
        return;
    }

    // Redistribute evenly across the timeline
    for (size_t i = 0; i < mKeyframes.size(); ++i)
    {
        mKeyframes[i].time = (F32)i / (F32)(mKeyframes.size() - 1);
    }
}

void LLFloaterCameraTimeline::deleteSelectedKeyframe()
{
    S32 sel = mKeyframeList->getFirstSelectedIndex();
    if (sel < 0 || sel >= (S32)mKeyframes.size())
        return;

    mKeyframes.erase(mKeyframes.begin() + sel);
    updateKeyframeList();
    updateUIState();

    mStatusText->setText(llformat("Deleted keyframe. %d remaining", (int)mKeyframes.size()));
}

void LLFloaterCameraTimeline::clearAllKeyframes()
{
    mKeyframes.clear();
    mCurrentTime = 0.f;
    if (mPlaying) stopPlayback();
    updateKeyframeList();
    updateUIState();
    mStatusText->setText(std::string("All keyframes cleared"));
}

// --- Playback ---

void LLFloaterCameraTimeline::startPlayback()
{
    if (mKeyframes.size() < 2)
    {
        mStatusText->setText(std::string("Need at least 2 keyframes to play"));
        return;
    }

    mPlaying = true;
    if (mCurrentTime <= 0.f || mCurrentTime >= 1.f)
    {
        mCurrentTime = 0.f;
    }
    mPlaybackTimer.reset();

    LL_WARNS("CameraTimeline") << "Playback started from time=" << mCurrentTime
        << " keyframes=" << mKeyframes.size()
        << " kf0.pos=(" << mKeyframes[0].position.mV[VX]
        << "," << mKeyframes[0].position.mV[VY]
        << "," << mKeyframes[0].position.mV[VZ] << ")" << LL_ENDL;

    // Save camera state for restore
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    mSavedCameraOrigin = cam->getOrigin();
    mSavedCameraRotation = cam->getQuaternion();
    mSavedFov = cam->getView();

    gIdleCallbacks.addFunction(onIdle, this);

    mPlayBtn->setEnabled(false);
    mStopBtn->setEnabled(true);
    mCaptureBtn->setEnabled(false);
    mStatusText->setText(std::string("Playing..."));
}

void LLFloaterCameraTimeline::stopPlayback()
{
    mPlaying = false;
    gIdleCallbacks.deleteFunction(onIdle, this);

    gAgentCamera.setRollAngle(0.f);

    mPlayBtn->setEnabled(mKeyframes.size() >= 2);
    mStopBtn->setEnabled(false);
    mCaptureBtn->setEnabled(true);
    mStatusText->setText(std::string("Stopped"));
}

void LLFloaterCameraTimeline::updatePlayback(F32 delta_time)
{
    if (!mPlaying || mKeyframes.size() < 2)
        return;

    F32 total_seconds = mPlaybackDuration;
    if (total_seconds <= 0.f) total_seconds = 1.f;

    mCurrentTime += (delta_time * mPlaybackSpeed) / total_seconds;

    if (mCurrentTime >= 1.f)
    {
        mCurrentTime = 1.f;
        applyCameraAtTime(1.f);
        stopPlayback();
        mStatusText->setText(std::string("Playback complete"));
        return;
    }

    applyCameraAtTime(mCurrentTime);

    // Update timeline slider
    mTimelineSlider->setValue(mCurrentTime * 100.f);

    // Update time display
    F32 seconds = mCurrentTime * total_seconds;
    mTimeDisplay->setText(llformat("%.1fs / %.1fs", seconds, total_seconds));
}

void LLFloaterCameraTimeline::applyCameraAtTime(F32 time)
{
    if (mKeyframes.size() < 2)
        return;

    // Find surrounding keyframes
    size_t idx_before = 0;
    for (size_t i = 0; i < mKeyframes.size() - 1; ++i)
    {
        if (mKeyframes[i + 1].time >= time)
        {
            idx_before = i;
            break;
        }
        if (i == mKeyframes.size() - 2)
        {
            idx_before = i;
        }
    }

    size_t idx_after = llmin(idx_before + 1, mKeyframes.size() - 1);

    LLVector3 pos;
    LLQuaternion rot;
    F32 fov;

    if (idx_before == idx_after)
    {
        const CameraKeyframe& kf = mKeyframes[idx_before];
        pos = kf.position;
        rot = kf.rotation;
        fov = kf.fov;
    }
    else
    {
        F32 range = mKeyframes[idx_after].time - mKeyframes[idx_before].time;
        F32 t = (range > 0.f) ? (time - mKeyframes[idx_before].time) / range : 0.f;
        t = llclamp(t, 0.f, 1.f);
        t = smoothStep(t);

        const CameraKeyframe& kf_before = mKeyframes[idx_before];
        const CameraKeyframe& kf_after = mKeyframes[idx_after];

        pos = lerpVec3(kf_before.position, kf_after.position, t);
        rot = slerpQuat(kf_before.rotation, kf_after.rotation, t);
        fov = kf_before.fov + (kf_after.fov - kf_before.fov) * t;
    }

    static bool logged = false;
    if (!logged)
    {
        LL_WARNS("CameraTimeline") << "applyCameraAtTime FIRST CALL: time=" << time
            << " idx_before=" << idx_before << " idx_after=" << idx_after
            << " pos=(" << pos.mV[VX] << "," << pos.mV[VY] << "," << pos.mV[VZ] << ")"
            << LL_ENDL;
        logged = true;
    }

    LLVector3 forward = LLVector3::x_axis * rot;
    LLVector3d pos_global = gAgent.getPosGlobalFromAgent(pos);
    LLVector3d focus_global = gAgent.getPosGlobalFromAgent(pos + forward);
    gAgentCamera.setCameraPosAndFocusGlobal(pos_global, focus_global, LLUUID::null);
    gAgentCamera.setCameraAnimating(false);

    // Extract roll from quaternion:
    // The viewer applies roll as a final rotation around the forward axis.
    // Compute the angle between the quaternion's up vector and the "natural" up
    // (world up projected perpendicular to forward).
    LLVector3 captured_up = LLVector3::z_axis * rot;
    LLVector3 world_up = LLVector3::z_axis;
    LLVector3 natural_up = world_up - forward * (forward * world_up);
    F32 len = natural_up.length();
    if (len > 0.001f)
    {
        natural_up /= len;
        LLVector3 right = forward % natural_up;
        F32 roll = atan2(right * captured_up, natural_up * captured_up);
        gAgentCamera.setRollAngle(roll);
    }
    else
    {
        gAgentCamera.setRollAngle(0.f);
    }

    LLViewerCamera::getInstance()->setViewNoBroadcast(fov);
}

// --- Interpolation Helpers ---

// static
F32 LLFloaterCameraTimeline::smoothStep(F32 t)
{
    t = llclamp(t, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// static
LLVector3 LLFloaterCameraTimeline::lerpVec3(const LLVector3& a, const LLVector3& b, F32 t)
{
    return LLVector3(
        a.mV[VX] + (b.mV[VX] - a.mV[VX]) * t,
        a.mV[VY] + (b.mV[VY] - a.mV[VY]) * t,
        a.mV[VZ] + (b.mV[VZ] - a.mV[VZ]) * t
    );
}

// static
LLQuaternion LLFloaterCameraTimeline::slerpQuat(const LLQuaternion& a, const LLQuaternion& b, F32 t)
{
    return slerp(t, a, b);
}

// --- File I/O ---

std::string LLFloaterCameraTimeline::getTimelineDir() const
{
    std::string dir = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, TIMELINE_SAVE_SUBDIRECTORY);
    LLFile::mkdir(dir);
    return dir;
}

void LLFloaterCameraTimeline::saveToFile()
{
    if (mKeyframes.empty())
    {
        mStatusText->setText(std::string("No keyframes to save"));
        return;
    }

    std::string proposed_name = "camera_timeline";
    LLFilePickerReplyThread::startPicker(
        boost::bind(&LLFloaterCameraTimeline::saveToFile, this, _1, _2, _3),
        LLFilePicker::FFSAVE_XML, proposed_name);
}

void LLFloaterCameraTimeline::saveToFile(const std::vector<std::string>& filenames, LLFilePicker::ELoadFilter, LLFilePicker::ESaveFilter)
{
    LL_WARNS("CameraTimeline") << "saveToFile callback fired, files=" << filenames.size() << LL_ENDL;

    if (filenames.empty()) return;

    LLSD data;
    data["version"] = 1;
    data["duration"] = mPlaybackDuration;
    data["speed"] = mPlaybackSpeed;

    LLSD keyframes_llsd;
    for (const CameraKeyframe& kf : mKeyframes)
    {
        LLSD kf_data;
        kf_data["time"] = kf.time;
        kf_data["px"] = kf.position.mV[VX];
        kf_data["py"] = kf.position.mV[VY];
        kf_data["pz"] = kf.position.mV[VZ];
        kf_data["rx"] = kf.rotation.mQ[VX];
        kf_data["ry"] = kf.rotation.mQ[VY];
        kf_data["rz"] = kf.rotation.mQ[VZ];
        kf_data["rw"] = kf.rotation.mQ[VW];
        kf_data["fov"] = kf.fov;
        keyframes_llsd.append(kf_data);
    }
    data["keyframes"] = keyframes_llsd;

    std::string filename = filenames[0];
    llofstream file(filename);
    if (!file.is_open())
    {
        mStatusText->setText(std::string("Failed to open file for writing"));
        return;
    }
    LLSDSerialize::toPrettyXML(data, file);
    file.close();

    mStatusText->setText(llformat("Saved %d keyframes", (int)mKeyframes.size()));
}

void LLFloaterCameraTimeline::loadFromFile()
{
    LLFilePickerReplyThread::startPicker(
        boost::bind(&LLFloaterCameraTimeline::loadFromFile, this, _1, _2, _3),
        LLFilePicker::FFLOAD_XML, false);
}

void LLFloaterCameraTimeline::loadFromFile(const std::vector<std::string>& filenames, LLFilePicker::ELoadFilter, LLFilePicker::ESaveFilter)
{
    LL_WARNS("CameraTimeline") << "loadFromFile callback fired, files=" << filenames.size() << LL_ENDL;

    if (filenames.empty())
    {
        LL_WARNS("CameraTimeline") << "loadFromFile: no filenames, returning" << LL_ENDL;
        return;
    }

    LL_WARNS("CameraTimeline") << "loadFromFile: opening " << filenames[0] << LL_ENDL;

    llifstream infile(filenames[0]);
    if (!infile.is_open())
    {
        LL_WARNS("CameraTimeline") << "loadFromFile: failed to open file" << LL_ENDL;
        mStatusText->setText(std::string("Failed to open file"));
        return;
    }

    LLSD data;
    if (!LLSDSerialize::fromXML(data, infile))
    {
        LL_WARNS("CameraTimeline") << "loadFromFile: failed to parse XML" << LL_ENDL;
        mStatusText->setText(std::string("Failed to parse file"));
        return;
    }
    infile.close();

    mKeyframes.clear();

    if (data.has("keyframes"))
    {
        const LLSD& keyframes_llsd = data["keyframes"];
        LL_WARNS("CameraTimeline") << "loadFromFile: found " << keyframes_llsd.size() << " keyframes in file" << LL_ENDL;

        for (S32 i = 0; i < keyframes_llsd.size(); ++i)
        {
            const LLSD& kf_data = keyframes_llsd[i];
            CameraKeyframe kf;
            kf.time = kf_data["time"].asReal();
            kf.position.mV[VX] = kf_data["px"].asReal();
            kf.position.mV[VY] = kf_data["py"].asReal();
            kf.position.mV[VZ] = kf_data["pz"].asReal();
            kf.rotation.mQ[VX] = kf_data["rx"].asReal();
            kf.rotation.mQ[VY] = kf_data["ry"].asReal();
            kf.rotation.mQ[VZ] = kf_data["rz"].asReal();
            kf.rotation.mQ[VW] = kf_data["rw"].asReal();
            kf.fov = kf_data["fov"].asReal();
            mKeyframes.push_back(kf);

            LL_WARNS("CameraTimeline") << "  kf[" << i << "] t=" << kf.time
                << " pos=(" << kf.position.mV[VX] << "," << kf.position.mV[VY] << "," << kf.position.mV[VZ] << ")"
                << " rot=(" << kf.rotation.mQ[VX] << "," << kf.rotation.mQ[VY] << "," << kf.rotation.mQ[VZ] << "," << kf.rotation.mQ[VW] << ")"
                << " fov=" << kf.fov << LL_ENDL;
        }
    }

    if (data.has("duration"))
        mPlaybackDuration = data["duration"].asReal();
    if (data.has("speed"))
        mPlaybackSpeed = data["speed"].asReal();

    LL_WARNS("CameraTimeline") << "loadFromFile: loaded " << mKeyframes.size() << " keyframes, duration=" << mPlaybackDuration << " speed=" << mPlaybackSpeed << LL_ENDL;

    mDurationSpinner->setValue(mPlaybackDuration);
    mSpeedSlider->setValue(mPlaybackSpeed);

    updateKeyframeList();
    updateUIState();

    mStatusText->setText(llformat("Loaded %d keyframes", (int)mKeyframes.size()));
}

// --- UI Updates ---

void LLFloaterCameraTimeline::updateKeyframeList()
{
    mKeyframeList->deleteAllItems();

    for (size_t i = 0; i < mKeyframes.size(); ++i)
    {
        const CameraKeyframe& kf = mKeyframes[i];
        F32 seconds = kf.time * mPlaybackDuration;

        LLSD row;
        row["columns"][0]["value"] = (S32)i + 1;
        row["columns"][1]["value"] = llformat("%.2fs", seconds);
        row["columns"][2]["value"] = llformat("(%.1f, %.1f, %.1f)",
            kf.position.mV[VX], kf.position.mV[VY], kf.position.mV[VZ]);
        row["columns"][3]["value"] = llformat("%.0f°", kf.fov * RAD_TO_DEG);

        mKeyframeList->addElement(row);
    }
}

void LLFloaterCameraTimeline::updateUIState()
{
    bool has_keyframes = !mKeyframes.empty();
    bool has_multiple = mKeyframes.size() >= 2;

    mDeleteBtn->setEnabled(has_keyframes && mKeyframeList->getFirstSelectedIndex() >= 0);
    mClearBtn->setEnabled(has_keyframes);
    mPlayBtn->setEnabled(has_multiple && !mPlaying);
    mStopBtn->setEnabled(mPlaying);
    mSaveBtn->setEnabled(has_keyframes);

    // Update timeline slider
    mTimelineSlider->setValue(mCurrentTime * 100.f);
}

void LLFloaterCameraTimeline::onTimelineScrub()
{
    if (mPlaying) return;

    mScrubbing = true;
    F32 val = (F32)mTimelineSlider->getValue().asReal();
    mCurrentTime = val / 100.f;

    if (mKeyframes.size() >= 2)
    {
        applyCameraAtTime(mCurrentTime);
    }

    F32 seconds = mCurrentTime * mPlaybackDuration;
    mTimeDisplay->setText(llformat("%.1fs / %.1fs", seconds, mPlaybackDuration));
    mScrubbing = false;
}

void LLFloaterCameraTimeline::onSpeedChanged()
{
    mPlaybackSpeed = (F32)mSpeedSlider->getValue().asReal();
}

void LLFloaterCameraTimeline::onDurationChanged()
{
    mPlaybackDuration = (F32)mDurationSpinner->get();
    if (mPlaybackDuration < 0.5f) mPlaybackDuration = 0.5f;
    updateKeyframeList();
}
