/**
 * @file llfloatercameratimeline.h
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

#ifndef LL_FLOATER_CAMERA_TIMELINE_H
#define LL_FLOATER_CAMERA_TIMELINE_H

#include "llfloater.h"
#include "llframetimer.h"
#include "llfilepicker.h"
#include <vector>

class LLButton;
class LLSpinCtrl;
class LLSliderCtrl;
class LLScrollListCtrl;
class LLTextBox;

struct CameraKeyframe
{
    F32 time;               // Normalized position 0.0 - 1.0
    LLVector3 position;     // Camera position (agent coords)
    LLQuaternion rotation;  // Camera orientation
    F32 fov;                // Field of view
};

class LLFloaterCameraTimeline : public LLFloater
{
    friend class LLFloaterReg;

public:
    LOG_CLASS(LLFloaterCameraTimeline);

    LLFloaterCameraTimeline(const LLSD& key);
    ~LLFloaterCameraTimeline();

    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;
    void draw() override;

    static void onIdle(void* user_data);

private:
    // Keyframe management
    void captureCamera();
    void deleteSelectedKeyframe();
    void clearAllKeyframes();
    void insertKeyframe(const CameraKeyframe& kf);
    void redistributeTimes();

    // Playback
    void startPlayback();
    void stopPlayback();
    void updatePlayback(F32 delta_time);
    void applyCameraAtTime(F32 time);

    // Interpolation helpers
    static F32 smoothStep(F32 t);
    static LLVector3 lerpVec3(const LLVector3& a, const LLVector3& b, F32 t);
    static LLQuaternion slerpQuat(const LLQuaternion& a, const LLQuaternion& b, F32 t);

    // File I/O
    void saveToFile();
    void saveToFile(const std::vector<std::string>& filenames, LLFilePicker::ELoadFilter, LLFilePicker::ESaveFilter);
    void loadFromFile();
    void loadFromFile(const std::vector<std::string>& filenames, LLFilePicker::ELoadFilter, LLFilePicker::ESaveFilter);
    std::string getTimelineDir() const;

    // UI updates
    void updateKeyframeList();
    void updateUIState();
    void onTimelineScrub();
    void onSpeedChanged();
    void onDurationChanged();

    // UI controls
    LLScrollListCtrl* mKeyframeList;
    LLButton* mCaptureBtn;
    LLButton* mDeleteBtn;
    LLButton* mClearBtn;
    LLSliderCtrl* mTimelineSlider;
    LLTextBox* mTimeDisplay;
    LLButton* mPlayBtn;
    LLButton* mStopBtn;
    LLSliderCtrl* mSpeedSlider;
    LLSpinCtrl* mDurationSpinner;
    LLButton* mSaveBtn;
    LLButton* mLoadBtn;
    LLTextBox* mStatusText;

    // State
    std::vector<CameraKeyframe> mKeyframes;
    bool mPlaying;
    F32 mCurrentTime;        // 0.0 - 1.0
    F32 mPlaybackSpeed;      // Multiplier 0.25 - 4.0
    F32 mPlaybackDuration;   // Total duration in seconds
    LLFrameTimer mPlaybackTimer;
    bool mScrubbing;

    // Saved camera state for restore
    LLVector3 mSavedCameraOrigin;
    LLQuaternion mSavedCameraRotation;
    F32 mSavedFov;
};

#endif // LL_FLOATER_CAMERA_TIMELINE_H
