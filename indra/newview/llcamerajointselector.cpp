#include "llviewerprecompiledheaders.h"
#include "llcamerajointselector.h"
#include "llcombobox.h"
#include "llviewercontrol.h"
void LLCameraJointSelector::bind(LLComboBox* combo)
{
    mCombo = combo;
    if (!mCombo) return;
    mCombo->setCommitCallback([](LLUICtrl* control, const LLSD&)
    {
        gSavedSettings.setS32("CameraFollowJoint", control->getValue().asInteger());
    });
    refresh(true);
}
void LLCameraJointSelector::refresh(bool force)
{
    if (!mCombo) return;
    bool rebuilt = false;
    // Poll for late/changed skeletons, without allocating a joint list every frame.
    if (force || mTimer.getElapsedTimeF32() >= 1.f)
    {
        mTimer.reset();
        auto joints = getCameraJointList();
        if (force || joints != mJoints)
        {
            mJoints = std::move(joints);
            mCombo->clearRows();
            for (const auto& joint : mJoints) mCombo->add(joint.first, joint.second);
            rebuilt = true;
        }
    }
    const S32 selected = gSavedSettings.getS32("CameraFollowJoint");
    if (rebuilt || selected != mSelected)
    {
        mCombo->setValue(selected);
        mSelected = selected;
    }
}
