/**
 * @file animationspeed.cpp
 * @brief Animation Speed floater for the bottom toolbar dock
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2024, Miko @ Phoenix Firestorm
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
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "animationspeed.h"

#include "fscommon.h"
#include "llbutton.h"
#include "llcharacter.h"
#include "llmotioncontroller.h"
#include "llsliderctrl.h"
#include "lltoolbarview.h"
#include "lltransientfloatermgr.h"

FloaterAnimationSpeed::FloaterAnimationSpeed(const LLSD& key)
:   LLTransientDockableFloater(nullptr, false, key),
    mSpeedSlider(nullptr),
    mResetButton(nullptr)
{
    if (!FSCommon::isLegacySkin())
    {
        LLTransientFloaterMgr::getInstance()->addControlView(this);
    }
}

FloaterAnimationSpeed::~FloaterAnimationSpeed()
{
    if (!FSCommon::isLegacySkin())
    {
        LLTransientFloaterMgr::getInstance()->removeControlView(this);
    }
}

bool FloaterAnimationSpeed::postBuild()
{
    mSpeedSlider = getChild<LLSliderCtrl>("speed_slider");
    mResetButton = getChild<LLButton>("reset_btn");

    mSpeedSlider->setCommitCallback(boost::bind(&FloaterAnimationSpeed::onChangeSpeed, this));
    mResetButton->setCommitCallback(boost::bind(&FloaterAnimationSpeed::onClickReset, this));

    return LLTransientDockableFloater::postBuild();
}

void FloaterAnimationSpeed::onOpen(const LLSD& key)
{
    F32 current_factor = LLMotionController::getCurrentTimeFactor();
    mSpeedSlider->setValue(current_factor);

    dockToToolbarButton();
}

void FloaterAnimationSpeed::onClose(bool app_quitting)
{
    LLTransientDockableFloater::onClose(app_quitting);
}

void FloaterAnimationSpeed::onChangeSpeed()
{
    F32 time_factor = mSpeedSlider->getValueF32();
    LLMotionController::setCurrentTimeFactor(time_factor);
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        character->setAnimTimeFactor(time_factor);
    }
}

void FloaterAnimationSpeed::onClickReset()
{
    mSpeedSlider->setValue(1.0f);
    LLMotionController::setCurrentTimeFactor(1.0f);
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        character->setAnimTimeFactor(1.0f);
    }
}

void FloaterAnimationSpeed::dockToToolbarButton()
{
    LLCommandId command_id("animationspeed");
    S32 toolbar_loc = gToolBarView->hasCommand(command_id);

    if (toolbar_loc != LLToolBarEnums::TOOLBAR_NONE && !FSCommon::isLegacySkin())
    {
        LLDockControl::DocAt doc_at = LLDockControl::TOP;
        switch (toolbar_loc)
        {
            case LLToolBarEnums::TOOLBAR_LEFT:
                doc_at = LLDockControl::RIGHT;
                break;

            case LLToolBarEnums::TOOLBAR_RIGHT:
                doc_at = LLDockControl::LEFT;
                break;
        }
        setCanDock(true);
        LLView* anchor_panel = gToolBarView->findChildView("animationspeed");
        setUseTongue(anchor_panel);
        mDockControl.reset(new LLDockControl(anchor_panel, this, getDockTongue(doc_at), doc_at));
        setDocked(isDocked(), false);
    }
    else
    {
        setUseTongue(false);
        setDocked(false, false);
        setCanDock(false);
        setDockControl(NULL);
    }
}
