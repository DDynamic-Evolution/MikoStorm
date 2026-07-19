/*
 * @file fsfloaterimsetsound.h
 * @brief Set a custom IM sound for a contact
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
 *
 * $/LicenseInfo$
 */

#ifndef FS_FLOATERIMSETSOUND_H
#define FS_FLOATERIMSETSOUND_H

#include "llfloater.h"

class LLAvatarName;
class LLLineEditor;
class LLButton;

class FSFloaterIMSetSound : public LLFloater
{
public:
    FSFloaterIMSetSound(const LLSD& target);
    bool postBuild() override;
    void onOpen(const LLSD& target) override;

    static void showInstance(const LLUUID& agent_id);

private:
    ~FSFloaterIMSetSound() = default;

    void onClickPreview();
    void onClickClear();
    void onClickOk();
    void onClickCancel();

    void loadCurrentSound();

    static void onAvatarNameCache(const LLUUID& id, const LLAvatarName& name);

    LLUUID          mAgentID;
    LLLineEditor*   mSoundUuidEditor;
    LLButton*       mClearButton;
};

#endif // FS_FLOATERIMSETSOUND_H
