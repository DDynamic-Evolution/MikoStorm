#ifndef LL_CAMERA_JOINT_SELECTOR_H
#define LL_CAMERA_JOINT_SELECTOR_H
#include "llagentcamera.h"
#include "lltimer.h"
class LLComboBox;
// Shared numeric-ID binding for Camera Controls, Phototools and Preferences.
class LLCameraJointSelector
{
public:
    void bind(LLComboBox* combo);
    void refresh(bool force = false);
private:
    LLComboBox* mCombo{nullptr};
    camera_joint_list_t mJoints;
    LLTimer mTimer;
    S32 mSelected{-2};
};
#endif
