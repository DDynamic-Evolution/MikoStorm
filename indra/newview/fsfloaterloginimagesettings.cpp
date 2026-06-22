/**
 * @file fsfloaterloginimagesettings.cpp
 * @brief Login image settings floater
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
 * Copyright (c) 2025 The Phoenix Firestorm Project, Inc.
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsfloaterloginimagesettings.h"
#include "llviewercontrol.h"
#include "llviewermenufile.h"

FSFloaterLoginImageSettings::FSFloaterLoginImageSettings(const LLSD& key) :
    LLFloater(key)
{
    mCommitCallbackRegistrar.add("LoginImage.SetPath", boost::bind(&FSFloaterLoginImageSettings::onClickSetPath, this));
    mCommitCallbackRegistrar.add("LoginImage.ClearPath", boost::bind(&FSFloaterLoginImageSettings::onClickClearPath, this));
}

bool FSFloaterLoginImageSettings::postBuild()
{
    return true;
}

void FSFloaterLoginImageSettings::onClickSetPath()
{
    LLFilePickerReplyThread::startPicker(boost::bind(&FSFloaterLoginImageSettings::onPathSelected, this, _1), LLFilePicker::FFLOAD_IMAGE, false);
}

void FSFloaterLoginImageSettings::onClickClearPath()
{
    gSavedSettings.setString("LoginImagePath", "");
}

void FSFloaterLoginImageSettings::onPathSelected(const std::vector<std::string>& filenames)
{
    if (!filenames.empty())
    {
        gSavedSettings.setString("LoginImagePath", filenames[0]);
    }
}
