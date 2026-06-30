/**
 * @file llfloaterphotogrammetry.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llfloaterphotogrammetry.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llcombobox.h"
#include "lldir.h"
#include "lldirpicker.h"
#include "llenvironment.h"
#include "llimagepng.h"
#include "llimagejpeg.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llprogressbar.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerpartsim.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "lluictrl.h"
#include "pipeline.h"

#include <iomanip>
#include <sstream>

const LLFloaterPhotogrammetry::ResolutionPreset LLFloaterPhotogrammetry::sResolutionPresets[] = {
    { "Current Window",      0, 0 },
    { "1920 x 1080",      1920, 1080 },
    { "2560 x 1440",      2560, 1440 },
    { "3840 x 2160 (4K)", 3840, 2160 },
    { "4096 x 2160",      4096, 2160 },
    { "7680 x 4320 (8K)", 7680, 4320 },
    { "Custom",              0, 0 },
};

const S32 LLFloaterPhotogrammetry::sNumPresets = sizeof(sResolutionPresets) / sizeof(sResolutionPresets[0]);

LLFloaterPhotogrammetry::LLFloaterPhotogrammetry(const LLSD& key)
    : LLFloater(key)
    , mTotalImagesSpinner(nullptr)
    , mNumVerticalSpinner(nullptr)
    , mDelaySpinner(nullptr)
    , mHeightMinSpinner(nullptr)
    , mHeightMaxSpinner(nullptr)
    , mResolutionCombo(nullptr)
    , mCustomWidthSpinner(nullptr)
    , mCustomHeightSpinner(nullptr)
    , mOutputDirEditor(nullptr)
    , mBrowseBtn(nullptr)
    , mStartBtn(nullptr)
    , mStopBtn(nullptr)
    , mProgressBar(nullptr)
    , mStatusText(nullptr)
    , mEstimatedSizeText(nullptr)
    , mCapturing(false)
    , mCountdownActive(false)
    , mCountdownRemaining(0)
    , mTotalImages(0)
    , mCurrentImageIndex(0)
    , mCurrentHorizIndex(0)
    , mCurrentVertIndex(0)
    , mNumHorizontal(36)
    , mNumVertical(3)
    , mImageWidth(1920)
    , mImageHeight(1080)
    , mHeightMin(0.0f)
    , mHeightMax(2.5f)
    , mDelay(3.0f)
    , mOrbitDistance(5.0f)
    , mSavedFov(0.0f)
{
}

LLFloaterPhotogrammetry::~LLFloaterPhotogrammetry()
{
    if (mCapturing)
    {
        stopCapture(false);
    }
}

bool LLFloaterPhotogrammetry::postBuild()
{
    mTotalImagesSpinner = getChild<LLSpinCtrl>("total_images");
    mNumVerticalSpinner = getChild<LLSpinCtrl>("num_vertical");
    mDelaySpinner = getChild<LLSpinCtrl>("capture_delay");
    mHeightMinSpinner = getChild<LLSpinCtrl>("height_min");
    mHeightMaxSpinner = getChild<LLSpinCtrl>("height_max");
    mResolutionCombo = getChild<LLComboBox>("resolution_combo");
    mCustomWidthSpinner = getChild<LLSpinCtrl>("custom_width");
    mCustomHeightSpinner = getChild<LLSpinCtrl>("custom_height");
    mOutputDirEditor = getChild<LLLineEditor>("output_dir");
    mBrowseBtn = getChild<LLButton>("browse_btn");
    mStartBtn = getChild<LLButton>("start_btn");
    mStopBtn = getChild<LLButton>("stop_btn");
    mProgressBar = getChild<LLProgressBar>("capture_progress");
    mStatusText = getChild<LLTextBox>("status_text");
    mEstimatedSizeText = getChild<LLTextBox>("estimated_size");
    mTotalImagesText = getChild<LLTextBox>("total_images");

    mStartBtn->setCommitCallback(boost::bind(&LLFloaterPhotogrammetry::onStartBtn, this));
    mStopBtn->setCommitCallback(boost::bind(&LLFloaterPhotogrammetry::onStopBtn, this));
    mBrowseBtn->setCommitCallback(boost::bind(&LLFloaterPhotogrammetry::onBrowseBtn, this));
    mResolutionCombo->setCommitCallback(boost::bind(&LLFloaterPhotogrammetry::onResolutionPreset, this));
    mTotalImagesSpinner->setCommitCallback(boost::bind(&LLFloaterPhotogrammetry::updateUIState, this));
    mNumVerticalSpinner->setCommitCallback(boost::bind(&LLFloaterPhotogrammetry::updateUIState, this));

    mStopBtn->setEnabled(false);

    updateUIState();
    onResolutionPreset();

    return true;
}

void LLFloaterPhotogrammetry::onOpen(const LLSD& key)
{
    S32 total_setting = gSavedSettings.getS32("PhotoGramTotalImages");
    if (total_setting < 100) total_setting = 108;
    S32 num_v = gSavedSettings.getS32("PhotoGramNumVertical");
    if (num_v < 1) num_v = 3;
    mDelay = gSavedSettings.getF32("PhotoGramDelay");
    if (mDelay <= 0.0f) mDelay = 3.0f;

    mTotalImagesSpinner->set((F32)total_setting);
    mNumVerticalSpinner->set((F32)num_v);
    mDelaySpinner->set(mDelay);

    updateUIState();
}

void LLFloaterPhotogrammetry::onClose(bool app_quitting)
{
    if (mCapturing)
    {
        stopCapture(false);
    }
}

S32 LLFloaterPhotogrammetry::getCurrentWidth() const
{
    S32 index = mResolutionCombo->getCurrentIndex();
    if (index >= 0 && index < sNumPresets)
    {
        if (index == 0)
        {
            return gViewerWindow->getWindowWidthRaw();
        }
        else if (index == sNumPresets - 1)
        {
            return (S32)mCustomWidthSpinner->get();
        }
        else
        {
            return sResolutionPresets[index].width;
        }
    }
    return gViewerWindow->getWindowWidthRaw();
}

S32 LLFloaterPhotogrammetry::getCurrentHeight() const
{
    S32 index = mResolutionCombo->getCurrentIndex();
    if (index >= 0 && index < sNumPresets)
    {
        if (index == 0)
        {
            return gViewerWindow->getWindowHeightRaw();
        }
        else if (index == sNumPresets - 1)
        {
            return (S32)mCustomHeightSpinner->get();
        }
        else
        {
            return sResolutionPresets[index].height;
        }
    }
    return gViewerWindow->getWindowHeightRaw();
}

void LLFloaterPhotogrammetry::onResolutionPreset()
{
    S32 index = mResolutionCombo->getCurrentIndex();
    bool is_custom = (index == sNumPresets - 1);
    mCustomWidthSpinner->setVisible(is_custom);
    mCustomHeightSpinner->setVisible(is_custom);

    updateUIState();
}

void LLFloaterPhotogrammetry::updateUIState()
{
    bool has_output_dir = !mOutputDirEditor->getValue().asString().empty();
    mStartBtn->setEnabled(!mCapturing && has_output_dir);

    S32 w = getCurrentWidth();
    S32 h = getCurrentHeight();
    S32 total_setting = (S32)mTotalImagesSpinner->get();
    S32 num_v = llmax((S32)1, (S32)mNumVerticalSpinner->get());
    S32 num_h = (S32)ceil((F32)total_setting / (F32)num_v);
    S32 total = num_h * num_v;

    S64 est_bytes = (S64)w * h * 3 * total * 3 / 2;
    std::string est_str;
    if (est_bytes > 1073741824)
    {
        est_str = llformat("~%.1f GB", est_bytes / 1073741824.0);
    }
    else if (est_bytes > 1048576)
    {
        est_str = llformat("~%.0f MB", est_bytes / 1048576.0);
    }
    else
    {
        est_str = llformat("~%.0f KB", est_bytes / 1024.0);
    }
    mEstimatedSizeText->setText(est_str);

    if (mTotalImagesText)
    {
        mTotalImagesText->setText(llformat("Result: %d images (%d steps x %d levels)", total, num_h, num_v));
    }
}

void LLFloaterPhotogrammetry::onBrowseBtn()
{
    std::string proposed_name = mOutputDirEditor->getValue().asString();
    if (proposed_name.empty())
    {
        proposed_name = gDirUtilp->getLindenUserDir() + gDirUtilp->getDirDelimiter() + "photogrammetry";
    }

    (new LLDirPickerThread(boost::bind(&LLFloaterPhotogrammetry::onBrowseDirSelected, this, _1, _2), proposed_name))->getFile();
}

void LLFloaterPhotogrammetry::onBrowseDirSelected(const std::vector<std::string>& filenames, std::string proposed_name)
{
    if (!filenames.empty())
    {
        mOutputDirEditor->setValue(filenames[0]);
        updateUIState();
    }
}

void LLFloaterPhotogrammetry::onStartBtn()
{
    S32 total_setting = (S32)mTotalImagesSpinner->get();
    mNumVertical = llmax((S32)1, (S32)mNumVerticalSpinner->get());
    mNumHorizontal = (S32)ceil((F32)total_setting / (F32)mNumVertical);
    mTotalImages = mNumHorizontal * mNumVertical;
    mDelay = (F32)mDelaySpinner->get();
    mHeightMin = (F32)mHeightMinSpinner->get();
    mHeightMax = (F32)mHeightMaxSpinner->get();
    mImageWidth = getCurrentWidth();
    mImageHeight = getCurrentHeight();
    mOutputDir = mOutputDirEditor->getValue().asString();

    startCapture();
}

void LLFloaterPhotogrammetry::startCapture()
{
    if (mCapturing) return;

    mCurrentImageIndex = 0;
    mCurrentHorizIndex = 0;
    mCurrentVertIndex = 0;
    mCapturing = true;
    mCountdownActive = true;
    mCountdownRemaining = llmax((S32)1, (S32)mDelay);

    // Save camera state
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    mSavedCameraOrigin = cam->getOrigin();
    mSavedCameraLookAt = mSavedCameraOrigin + cam->getAtAxis() * 10.f;
    mSavedCameraUp = cam->getUpAxis();
    mSavedFov = cam->getView();

    // Save focus target - orbits around whatever the camera is focused on
    // (avatar, another avatar, object, or the ground)
    mSavedFocusPosGlobal = gAgentCamera.getFocusGlobal();

    // Calculate orbit distance from current camera position to focus point
    LLVector3d cam_pos_global = gAgent.getPosGlobalFromAgent(mSavedCameraOrigin);
    LLVector3d cam_to_focus = cam_pos_global - mSavedFocusPosGlobal;
    mOrbitDistance = (F32)cam_to_focus.magVec();

    if (mOrbitDistance < 1.0f)
    {
        mOrbitDistance = 5.0f;
    }

    // Save settings for next time
    gSavedSettings.setS32("PhotoGramTotalImages", (S32)mTotalImagesSpinner->get());
    gSavedSettings.setS32("PhotoGramNumVertical", mNumVertical);
    gSavedSettings.setF32("PhotoGramDelay", mDelay);

    // Create output directory
    createOutputDir();

    // Allocate raw image buffer
    mRawImage = new LLImageRaw(mImageWidth, mImageHeight, 3);

    mStartBtn->setEnabled(false);
    mStopBtn->setEnabled(true);
    mStatusText->setText(llformat("Starting in %d...", mCountdownRemaining));
    mProgressBar->setValue(0.0);

    gIdleCallbacks.addFunction(onIdle, this);
}

void LLFloaterPhotogrammetry::stopCapture(bool restore_camera)
{
    if (!mCapturing) return;

    gIdleCallbacks.deleteFunction(onIdle, this);
    mCapturing = false;
    mCountdownActive = false;

    freezeWorld(false);

    if (restore_camera)
    {
        LLViewerCamera* cam = LLViewerCamera::getInstance();
        cam->setOriginAndLookAt(mSavedCameraOrigin, mSavedCameraUp, mSavedCameraLookAt);
        cam->setView(mSavedFov);

        // Also sync gAgentCamera so it doesn't override on the next frame
        LLVector3d cam_global = gAgent.getPosGlobalFromAgent(mSavedCameraOrigin);
        LLVector3d focus_global = gAgent.getPosGlobalFromAgent(mSavedCameraLookAt);
        gAgentCamera.setCameraPosAndFocusGlobal(cam_global, focus_global, LLUUID::null);
    }

    mRawImage = nullptr;

    mStartBtn->setEnabled(true);
    mStopBtn->setEnabled(false);
}

void LLFloaterPhotogrammetry::onStopBtn()
{
    mStatusText->setText(std::string("Capture aborted"));
    stopCapture(true);
}

void LLFloaterPhotogrammetry::onIdle(void* user_data)
{
    LLFloaterPhotogrammetry* self = (LLFloaterPhotogrammetry*)user_data;
    if (!self || !self->mCapturing) return;

    if (self->mCountdownActive)
    {
        if (self->mCountdownRemaining > 0)
        {
            self->mStatusText->setText(llformat("Starting in %d...", self->mCountdownRemaining));
            self->mCountdownRemaining--;
            return;
        }
        else
        {
            self->mCountdownActive = false;
            self->freezeWorld(true);
            self->mStatusText->setText(std::string("Capturing..."));
        }
    }

    self->captureNextImage();
}

void LLFloaterPhotogrammetry::captureNextImage()
{
    if (mCurrentImageIndex >= mTotalImages)
    {
        finishCapture();
        return;
    }

    mCurrentHorizIndex = mCurrentImageIndex % mNumHorizontal;
    mCurrentVertIndex = mCurrentImageIndex / mNumHorizontal;

    // Calculate angle and height
    F32 angle = (F32)mCurrentHorizIndex / (F32)mNumHorizontal * F_TWO_PI;
    F32 height_ratio = (F32)mCurrentVertIndex;
    if (mNumVertical > 1)
    {
        height_ratio /= (F32)(mNumVertical - 1);
    }
    F32 height_offset = mHeightMin + (mHeightMax - mHeightMin) * height_ratio;

    // Calculate camera position orbiting around saved focus point
    LLVector3d focus_pos = mSavedFocusPosGlobal;
    LLVector3 cam_pos_local(
        (F32)cos(angle) * mOrbitDistance,
        (F32)sin(angle) * mOrbitDistance,
        height_offset
    );
    LLVector3d cam_pos_global = focus_pos + LLVector3d(cam_pos_local);

    // Set camera directly on LLViewerCamera (bypass gAgentCamera animation)
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    LLVector3 cam_pos_agent = gAgent.getPosAgentFromGlobal(cam_pos_global);
    LLVector3 focus_agent = gAgent.getPosAgentFromGlobal(focus_pos);
    cam->setOriginAndLookAt(cam_pos_agent, LLVector3(0, 0, 1), focus_agent);

    // Set FOV to a reasonable value for photogrammetry (~54 degrees)
    cam->setViewNoBroadcast(F_PI_BY_TWO * 0.6f);

    // Take snapshot
    bool success = gViewerWindow->simpleSnapshot(mRawImage, mImageWidth, mImageHeight, 2);
    if (success)
    {
        saveCurrentImage();
    }
    else
    {
        LL_WARNS("PhotoGram") << "Failed to capture image " << mCurrentImageIndex << LL_ENDL;
    }

    // Update progress
    mCurrentImageIndex++;
    F32 progress = (F32)mCurrentImageIndex / (F32)mTotalImages;
    mProgressBar->setValue(progress);

    std::string status = llformat("Image %d/%d (H%d V%d)",
        mCurrentImageIndex, mTotalImages,
        mCurrentHorizIndex + 1, mCurrentVertIndex + 1);
    mStatusText->setText(status);
}

void LLFloaterPhotogrammetry::saveCurrentImage()
{
    LLPointer<LLImagePNG> png_image = new LLImagePNG;
    bool encode_ok = png_image->encode(mRawImage, 0);
    if (!encode_ok)
    {
        LL_WARNS("PhotoGram") << "Failed to encode PNG for image " << mCurrentImageIndex << LL_ENDL;
        return;
    }

    std::string filename = generateFilename(mCurrentHorizIndex, mCurrentVertIndex);
    std::string full_path = mOutputDir + gDirUtilp->getDirDelimiter() + filename;

    bool save_ok = png_image->save(full_path);
    if (save_ok)
    {
        LL_INFOS("PhotoGram") << "Saved: " << full_path << LL_ENDL;
    }
    else
    {
        LL_WARNS("PhotoGram") << "Failed to save: " << full_path << LL_ENDL;
    }
}

void LLFloaterPhotogrammetry::finishCapture()
{
    freezeWorld(false);

    LLViewerCamera* cam = LLViewerCamera::getInstance();
    cam->setOriginAndLookAt(mSavedCameraOrigin, mSavedCameraUp, mSavedCameraLookAt);
    cam->setView(mSavedFov);

    // Also sync gAgentCamera so it doesn't override on the next frame
    LLVector3d cam_global = gAgent.getPosGlobalFromAgent(mSavedCameraOrigin);
    LLVector3d focus_global = gAgent.getPosGlobalFromAgent(mSavedCameraLookAt);
    gAgentCamera.setCameraPosAndFocusGlobal(cam_global, focus_global, LLUUID::null);

    mStatusText->setText(llformat("Complete! %d images saved to %s", mTotalImages, mOutputDir.c_str()));
    mProgressBar->setValue(1.0);

    mCapturing = false;
    mRawImage = nullptr;

    gIdleCallbacks.deleteFunction(onIdle, this);

    mStartBtn->setEnabled(true);
    mStopBtn->setEnabled(false);
}

void LLFloaterPhotogrammetry::freezeWorld(bool enable)
{
    static bool clouds_scroll_paused = false;

    if (enable)
    {
        clouds_scroll_paused = LLEnvironment::instance().isCloudScrollPaused();
        LLEnvironment::instance().pauseCloudScroll();

        for (LLCharacter* character : LLCharacter::sInstances)
        {
            mAvatarPauseHandles.push_back(character->requestPause());
        }

        gSavedSettings.setBOOL("FreezeTime", true);
        LLViewerPartSim::getInstance()->enable(false);
    }
    else
    {
        if (!clouds_scroll_paused)
        {
            LLEnvironment::instance().resumeCloudScroll();
        }

        mAvatarPauseHandles.clear();

        gSavedSettings.setBOOL("FreezeTime", false);
        LLViewerPartSim::getInstance()->enable(true);
    }
}

void LLFloaterPhotogrammetry::createOutputDir()
{
    if (mOutputDir.empty())
    {
        mOutputDir = gDirUtilp->getLindenUserDir() + gDirUtilp->getDirDelimiter() + "photogrammetry";
    }

    if (!gDirUtilp->fileExists(mOutputDir))
    {
        LLFile::mkdir(mOutputDir);
    }
}

std::string LLFloaterPhotogrammetry::generateFilename(S32 horiz_idx, S32 vert_idx) const
{
    std::ostringstream filename;
    filename << "PhotoGram_V" << std::setw(2) << std::setfill('0') << vert_idx
             << "_H" << std::setw(3) << std::setfill('0') << horiz_idx
             << ".png";
    return filename.str();
}

void LLFloaterPhotogrammetry::draw()
{
    LLFloater::draw();

    if (!mCapturing) return;

    // Preview area drawing (top-down orbit view) would go here.
    // Reserved for future LLView subclass implementation.
}
