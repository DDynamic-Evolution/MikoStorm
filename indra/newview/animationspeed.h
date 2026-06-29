/**
 * @file animationspeed.h
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

#ifndef ANIMATIONSPEED_H
#define ANIMATIONSPEED_H

#include "lltransientdockablefloater.h"

class LLSliderCtrl;
class LLButton;

class FloaterAnimationSpeed : public LLTransientDockableFloater
{
    friend class LLFloaterReg;

private:
    FloaterAnimationSpeed(const LLSD& key);
    ~FloaterAnimationSpeed();

    void onChangeSpeed();
    void onClickReset();

public:
    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;

    void dockToToolbarButton();

private:
    LLSliderCtrl*   mSpeedSlider;
    LLButton*       mResetButton;
};

#endif // ANIMATIONSPEED_H
