/**
 * @file llfloaterphotogrammetry.h
 * @brief Floater for automated photogrammetry capture
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2025
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

#ifndef LL_FLOATER_PHOTOGRAMMETRY_H
#define LL_FLOATER_PHOTOGRAMMETRY_H

#include "llfloater.h"
#include "llimagepng.h"
#include "llagentcamera.h"

class LLButton;
class LLSpinCtrl;
class LLComboBox;
class LLLineEditor;
class LLTextBox;
class LLProgressBar;
class LLUICtrl;

class LLFloaterPhotogrammetry : public LLFloater
{
    friend class LLFloaterReg;

public:
    LOG_CLASS(LLFloaterPhotogrammetry);

    LLFloaterPhotogrammetry(const LLSD& key);
    ~LLFloaterPhotogrammetry();

    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;
    void draw() override;

    static void onIdle(void* user_data);

private:
    void onStartBtn();
    void onStopBtn();
    void onBrowseBtn();
    void onBrowseDirSelected(const std::vector<std::string>& filenames, std::string proposed_name);
    void onResolutionPreset();

    void startCapture();
    void stopCapture(bool restore_camera = true);
    void captureNextImage();
    void saveCurrentImage();
    void finishCapture();

    void freezeWorld(bool enable);
    void createOutputDir();
    std::string generateFilename(S32 horiz_idx, S32 vert_idx) const;
    std::string generateOutputDir() const;

    void updateUIState();
    void updatePreview();

    // Resolution helpers
    struct ResolutionPreset
    {
        std::string label;
        S32 width;
        S32 height;
    };
    static const ResolutionPreset sResolutionPresets[];
    static const S32 sNumPresets;

    S32 getCurrentWidth() const;
    S32 getCurrentHeight() const;

    // UI controls
    LLSpinCtrl* mTotalImagesSpinner;
    LLSpinCtrl* mNumVerticalSpinner;
    LLSpinCtrl* mDelaySpinner;
    LLSpinCtrl* mHeightMinSpinner;
    LLSpinCtrl* mHeightMaxSpinner;
    LLComboBox* mResolutionCombo;
    LLSpinCtrl* mCustomWidthSpinner;
    LLSpinCtrl* mCustomHeightSpinner;
    LLLineEditor* mOutputDirEditor;
    LLButton* mBrowseBtn;
    LLButton* mStartBtn;
    LLButton* mStopBtn;
    LLProgressBar* mProgressBar;
    LLTextBox* mStatusText;
    LLTextBox* mEstimatedSizeText;
    LLTextBox* mTotalImagesText;

    // Capture state
    bool mCapturing;
    bool mCountdownActive;
    S32 mCountdownRemaining;
    S32 mTotalImages;
    S32 mCurrentImageIndex;
    S32 mCurrentHorizIndex;
    S32 mCurrentVertIndex;
    S32 mNumHorizontal;
    S32 mNumVertical;
    S32 mImageWidth;
    S32 mImageHeight;
    F32 mHeightMin;
    F32 mHeightMax;
    F32 mDelay;
    std::string mOutputDir;
    F32 mOrbitDistance;

    // Saved camera state (agent coords for direct LLViewerCamera restore)
    LLVector3 mSavedCameraOrigin;
    LLVector3 mSavedCameraLookAt;
    LLVector3 mSavedCameraUp;
    F32 mSavedFov;

    // Orbit target position (global coords)
    LLVector3d mSavedFocusPosGlobal;

    // Avatar pause handles
    std::vector<LLAnimPauseRequest> mAvatarPauseHandles;

    // Raw image buffer for capture
    LLPointer<LLImageRaw> mRawImage;
};

#endif // LL_FLOATER_PHOTOGRAMMETRY_H
