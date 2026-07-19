/*
 * @file fsfloaterimsetsound.cpp
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

#include "llviewerprecompiledheaders.h"

#include "fsfloaterimsetsound.h"

#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llfloaterreg.h"
#include "lllineeditor.h"
#include "llui.h"
#include "llviewercontrol.h"

FSFloaterIMSetSound::FSFloaterIMSetSound(const LLSD& target)
:   LLFloater(target),
    mAgentID(target.asUUID()),
    mSoundUuidEditor(nullptr),
    mClearButton(nullptr)
{
}

// static
void FSFloaterIMSetSound::showInstance(const LLUUID& agent_id)
{
    LLFloaterReg::showInstance("fs_im_sound", agent_id, true);
}

bool FSFloaterIMSetSound::postBuild()
{
    mSoundUuidEditor = getChild<LLLineEditor>("sound_uuid_editor");
    mClearButton = getChild<LLButton>("clear_btn");

    childSetAction("preview_btn", boost::bind(&FSFloaterIMSetSound::onClickPreview, this));
    childSetAction("clear_btn", boost::bind(&FSFloaterIMSetSound::onClickClear, this));
    childSetAction("ok_btn", boost::bind(&FSFloaterIMSetSound::onClickOk, this));
    childSetAction("cancel_btn", boost::bind(&FSFloaterIMSetSound::onClickCancel, this));

    return true;
}

void FSFloaterIMSetSound::onOpen(const LLSD& target)
{
    if (target.isUUID())
    {
        mAgentID = target.asUUID();
    }
    else if (target.isString())
    {
        mAgentID = LLUUID(target.asString());
    }

    loadCurrentSound();

    LLAvatarName av_name;
    if (LLAvatarNameCache::get(mAgentID, &av_name))
    {
        getChild<LLUICtrl>("label_avatar_name")->setValue(av_name.getDisplayName());
    }
    else
    {
        getChild<LLUICtrl>("label_avatar_name")->setValue(mAgentID.asString());
        LLAvatarNameCache::get(mAgentID, boost::bind(&FSFloaterIMSetSound::onAvatarNameCache, _1, _2));
    }
}

// static
void FSFloaterIMSetSound::onAvatarNameCache(const LLUUID& id, const LLAvatarName& name)
{
    if (auto floater = LLFloaterReg::findTypedInstance<FSFloaterIMSetSound>("fs_im_sound"))
    {
        if (floater->mAgentID == id)
        {
            floater->getChild<LLUICtrl>("label_avatar_name")->setValue(name.getDisplayName());
        }
    }
}

void FSFloaterIMSetSound::loadCurrentSound()
{
    LLSD customSounds = gSavedPerAccountSettings.getLLSD("FSPerAccountIMSounds");
    std::string key = mAgentID.asString();
    if (customSounds.has(key))
    {
        LLUUID sound_id(customSounds[key].asString());
        if (sound_id.notNull())
        {
            mSoundUuidEditor->setText(sound_id.asString());
        }
    }
    else
    {
        mSoundUuidEditor->clear();
    }

    mClearButton->setEnabled(customSounds.has(key));
}

void FSFloaterIMSetSound::onClickPreview()
{
    std::string uuid_str = mSoundUuidEditor->getText();
    LLStringUtil::trim(uuid_str);
    if (!uuid_str.empty())
    {
        LLUUID sound_id(uuid_str);
        if (sound_id.notNull())
        {
            make_ui_sound(sound_id);
        }
    }
}

void FSFloaterIMSetSound::onClickClear()
{
    LLSD customSounds = gSavedPerAccountSettings.getLLSD("FSPerAccountIMSounds");
    std::string key = mAgentID.asString();
    if (customSounds.has(key))
    {
        customSounds.erase(key);
        gSavedPerAccountSettings.setLLSD("FSPerAccountIMSounds", customSounds);
    }

    mSoundUuidEditor->clear();
    mClearButton->setEnabled(false);
}

void FSFloaterIMSetSound::onClickOk()
{
    std::string uuid_str = mSoundUuidEditor->getText();
    LLStringUtil::trim(uuid_str);

    LLSD customSounds = gSavedPerAccountSettings.getLLSD("FSPerAccountIMSounds");
    std::string key = mAgentID.asString();

    if (!uuid_str.empty())
    {
        LLUUID sound_id(uuid_str);
        if (sound_id.notNull())
        {
            customSounds[key] = sound_id.asString();
            gSavedPerAccountSettings.setLLSD("FSPerAccountIMSounds", customSounds);
        }
    }

    closeFloater();
}

void FSFloaterIMSetSound::onClickCancel()
{
    closeFloater();
}
