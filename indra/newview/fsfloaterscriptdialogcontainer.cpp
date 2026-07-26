/**
 * @file fsfloaterscriptdialogcontainer.cpp
 * @brief Multifloater containing active Script Dialog floaters in separate tab container.
 * @author minerjr@firestorm
 *
 * $LicenseInfo:firstyear=2026&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2026 The Phoenix Firestorm Project, Inc.
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
#include "llviewercontrol.h"
#include "llchiclet.h"
#include "llchicletbar.h"
#include "llfloaterreg.h"
#include "llscriptfloater.h"
#include "fsfloaterscriptdialogcontainer.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "lltoolbarview.h"
#include "llcontrol.h"
#include "lltrans.h"

//
// FSFloaterScriptDialogContainer
//
FSFloaterScriptDialogContainer::FSFloaterScriptDialogContainer(const LLSD& seed) :
    LLMultiFloater(seed),
    mIsAddingNewSession(false),
    mInitialWidth(0),
    mInitialHeight(0),
    mCloseAllBtn(nullptr)
{
}

FSFloaterScriptDialogContainer::~FSFloaterScriptDialogContainer()
{
    mSessions.clear();
    mNewMessageConnection.disconnect();
}

bool FSFloaterScriptDialogContainer::postBuild()
{
    mInitialWidth  = getRect().getWidth();
    mInitialHeight = getRect().getHeight();

    mTabContainer = getChild<LLTabContainer>("script_dialog_box_tab_container");
    mTabContainer->setAllowRearrange(true);
    mTabContainer->setRearrangeCallback(boost::bind(&FSFloaterScriptDialogContainer::onScriptTabRearrange, this, _1, _2));

    mCloseAllBtn = getChild<LLButton>("CloseAll");
    mCloseAllBtn->setCommitCallback(boost::bind(&FSFloaterScriptDialogContainer::onClickCloseAll, this));

    return true;
}

void FSFloaterScriptDialogContainer::onScriptTabRearrange(S32 tab_index, LLPanel* tab_panel)
{
    LLFloater* pFloater = dynamic_cast<LLFloater*>(tab_panel);
    if (!pFloater)
        return;

    const LLUUID& idSession = pFloater->getKey().asUUID();
    if (idSession.isNull())
        return;

    LLChicletPanel* pChicletPanel = LLChicletBar::instance().getChicletPanel();
    LLChiclet*      pChiclet      = pChicletPanel->findChiclet<LLChiclet>(idSession);
    pChicletPanel->setChicletIndex(pChiclet, tab_index - mTabContainer->getNumLockedTabs());
}

void FSFloaterScriptDialogContainer::onOpen(const LLSD& key)
{
    LLMultiFloater::onOpen(key);
    LLFloater* active_floater = getActiveFloater();
    if (active_floater && !active_floater->hasFocus())
    {
        mTabContainer->setFocus(true);
    }
}

void FSFloaterScriptDialogContainer::onClose(bool app_quitting)
{
    if (app_quitting)
    {
        for (S32 i = 0; i < mTabContainer->getTabCount(); ++i)
        {
            LLScriptFloater* floater = dynamic_cast<LLScriptFloater*>(mTabContainer->getPanelByIndex(i));
            if (floater)
            {
                floater->onClose(app_quitting);
            }
        }
    }
}

void FSFloaterScriptDialogContainer::onClickMinimize()
{
    LLFloater::onClickMinimize(this);
}

void FSFloaterScriptDialogContainer::onClickCloseAll()
{
    LLNotificationsUtil::add("SDConfirmCloseAll", LLSD(), LLSD(),
                             boost::bind(&FSFloaterScriptDialogContainer::confirmCloseAllCallback, this, _1, _2));
}

void FSFloaterScriptDialogContainer::closeAllImpl()
{
    S32 lastTabCount = mTabContainer->getTabCount();
    for (S32 index = lastTabCount - 1; index >= 0; index--)
    {
        LLScriptFloater* last_floater = (LLScriptFloater*)mTabContainer->getPanelByIndex(index);
        removeFloater(last_floater);
        last_floater->closeFloater();
    }
}

bool FSFloaterScriptDialogContainer::confirmCloseAllCallback(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option == 0)
    {
        closeAllImpl();
        return true;
    }
    return false;
}

void FSFloaterScriptDialogContainer::addFloater(LLFloater* floaterp, bool select_added_floater, LLTabContainer::eInsertionPoint insertion_point)
{
    if (!floaterp)
        return;

    floaterp->setCanMinimize(false);
    floaterp->setTitle(floaterp->getShortTitle());

    if (floaterp->getHost() == this)
    {
        openFloater(floaterp->getKey());
        setFocus(true);
        return;
    }

    LLFloater* active_floater = getActiveFloater();

    if (!getVisible())
    {
        S32 current_height = getRect().getHeight();
        reshape(mInitialWidth, mInitialHeight);
        translate(0, current_height - mInitialHeight);
    }

    LLMultiFloater::addFloater(floaterp, false, insertion_point);

    if (active_floater && !active_floater->hasFocus())
    {
        mTabContainer->selectTabPanel(active_floater);
        mTabContainer->setFocus(true);
    }

    LLUUID session_id     = floaterp->getKey();
    mSessions[session_id] = floaterp;
    floaterp->mCloseSignal.connect(boost::bind(&FSFloaterScriptDialogContainer::onCloseFloater, this, session_id));
}

void FSFloaterScriptDialogContainer::addNewSession(LLFloater* floaterp)
{
    mIsAddingNewSession = true;
    addFloater(floaterp, false, LLTabContainer::END);
    mIsAddingNewSession = false;
}

void FSFloaterScriptDialogContainer::removeFloater(LLFloater* floaterp)
{
    LLScriptFloater* script_floaterp = dynamic_cast<LLScriptFloater*>(floaterp);
    if (script_floaterp)
    {
        mFlashingTabStates.erase(floaterp);

        script_floaterp->setTitle("");
        script_floaterp->setDocked(false, true);
        script_floaterp->setCanDock(false);
        if (!script_floaterp->isDocked())
        {
            LLMultiFloater::removeFloater(floaterp);
        }

        if (mTabContainer->getTabCount() == 0)
        {
            closeFloater();
        }
    }
}

void FSFloaterScriptDialogContainer::removeFloater(const LLUUID& id)
{
    LLScriptFloater* script_floaterp = getFloater(id);

    if (script_floaterp)
    {
        mFlashingTabStates.erase(script_floaterp);

        script_floaterp->setTitle("");
        script_floaterp->setDocked(false, true);
        script_floaterp->setCanDock(false);

        LLMultiFloater::removeFloater(script_floaterp);

        if (mTabContainer->getTabCount() == 0)
        {
            closeFloater();
        }
    }
}

bool FSFloaterScriptDialogContainer::hasFloater(LLFloater* floaterp) const
{
    for (S32 i = 0; i < mTabContainer->getTabCount(); ++i)
    {
        if (dynamic_cast<LLFloater*>(mTabContainer->getPanelByIndex(i)) == floaterp)
        {
            return true;
        }
    }
    return false;
}

bool FSFloaterScriptDialogContainer::hasFloater(const LLUUID& id) const
{
    for (S32 i = 0; i < mTabContainer->getTabCount(); ++i)
    {
        LLScriptFloater* current_floater = dynamic_cast<LLScriptFloater*>(mTabContainer->getPanelByIndex(i));
        if (current_floater && current_floater->getNotificationId() == id)
        {
            return true;
        }
    }
    return false;
}

LLScriptFloater* FSFloaterScriptDialogContainer::getFloater(const LLUUID& id) const
{
    for (S32 i = 0; i < mTabContainer->getTabCount(); ++i)
    {
        LLScriptFloater* current_floater = dynamic_cast<LLScriptFloater*>(mTabContainer->getPanelByIndex(i));
        if (current_floater && current_floater->getNotificationId() == id)
        {
            return current_floater;
        }
    }

    return nullptr;
}

void FSFloaterScriptDialogContainer::onCloseFloater(const LLUUID& id)
{
    mSessions.erase(id);
    if (isShown())
    {
        setFocus(true);
    }
    else if (isMinimized())
    {
        setMinimized(true);
    }
}

void FSFloaterScriptDialogContainer::onNewMessageReceived(const LLSD& msg)
{
    LLUUID     session_id      = msg["session_id"].asUUID();
    LLFloater* floaterp        = get_ptr_in_map(mSessions, session_id);
    LLFloater* current_floater = LLMultiFloater::getActiveFloater();

    if (floaterp && current_floater && floaterp != current_floater)
    {
        startFlashingTab(floaterp, msg["message"].asString());
    }
}

FSFloaterScriptDialogContainer* FSFloaterScriptDialogContainer::findInstance()
{
    return LLFloaterReg::findTypedInstance<FSFloaterScriptDialogContainer>("fs_script_dialog_container");
}

FSFloaterScriptDialogContainer* FSFloaterScriptDialogContainer::getInstance()
{
    return LLFloaterReg::getTypedInstance<FSFloaterScriptDialogContainer>("fs_script_dialog_container");
}

void FSFloaterScriptDialogContainer::setMinimized(bool b)
{
    LLMultiFloater::setMinimized(b);

    if (!b)
    {
        LLFloater *floaterp = getCurrentScriptFloater();
        updateFloaterTitle(floaterp);
    }
}

void FSFloaterScriptDialogContainer::draw()
{
    LLFloater::draw();
}

LLFloater* FSFloaterScriptDialogContainer::getCurrentScriptFloater()
{
    LLScriptFloater* script_floater = dynamic_cast<LLScriptFloater*>(mTabContainer->getPanelByIndex(mTabContainer->getCurrentPanelIndex()));
    if (script_floater)
    {
        return script_floater;
    }

    return nullptr;
}

void FSFloaterScriptDialogContainer::addFlashingSession(const LLUUID& session_id)
{
    uuid_vec_t::iterator found = std::find(mFlashingSessions.begin(), mFlashingSessions.end(), session_id);
    if (found == mFlashingSessions.end())
    {
        mFlashingSessions.emplace_back(session_id);
    }

    checkFlashing();
}

void FSFloaterScriptDialogContainer::checkFlashing()
{
    gToolBarView->flashCommand(LLCommandId("script_container"), !mFlashingSessions.empty(), isMinimized());
}

void FSFloaterScriptDialogContainer::computeResizeLimits(S32& new_min_width, S32& new_min_height)
{
    static LLUICachedControl<S32> tabcntr_close_btn_size ("UITabCntrCloseBtnSize", 0);
    const LLFloater::Params& default_params = LLFloater::getDefaultParams();
    S32 floater_header_size = default_params.header_height;
    S32 tabcntr_header_height = LLPANEL_BORDER_WIDTH + tabcntr_close_btn_size;
    S32 close_all_button_height = mCloseAllBtn->getRect().getHeight() + 7;
    S32 tabcntr_min_width = 0;
    if (mTabContainer->getTabPosition() == LLTabContainer::TabPosition::LEFT)
    {
        tabcntr_min_width = mTabContainer->getMinTabWidth();
    }
    else
    {
        tabcntr_min_width = mTabContainer->getTotalTabWidth();
    }
    for (S32 tab_idx = 0; tab_idx < mTabContainer->getTabCount(); ++tab_idx)
    {
        LLFloater* floaterp = (LLFloater*)mTabContainer->getPanelByIndex(tab_idx);
        if (floaterp)
        {
            new_min_width = llmax(new_min_width, floaterp->getMinWidth() + LLPANEL_BORDER_WIDTH * 2 + tabcntr_min_width);
            new_min_height = llmax(new_min_height, floaterp->getMinHeight() + floater_header_size + tabcntr_header_height + close_all_button_height);
        }
    }
}

void FSFloaterScriptDialogContainer::growToFit(S32 content_width, S32 content_height)
{
    static LLUICachedControl<S32> tabcntr_close_btn_size ("UITabCntrCloseBtnSize", 0);
    const LLFloater::Params& default_params = LLFloater::getDefaultParams();
    S32 floater_header_size = default_params.header_height;
    S32 tabcntr_header_height = LLPANEL_BORDER_WIDTH + tabcntr_close_btn_size;
    S32 close_all_button_height = mCloseAllBtn->getRect().getHeight() + 7;
    S32 tabcntr_min_width = 0;
    if (mTabContainer->getTabPosition() == LLTabContainer::TabPosition::LEFT)
    {
        tabcntr_min_width = mTabContainer->getMinTabWidth();
    }
    else
    {
        tabcntr_min_width = mTabContainer->getTotalTabWidth();
    }
    S32 new_width = llmax(getRect().getWidth(), content_width + LLPANEL_BORDER_WIDTH * 2 + tabcntr_min_width);
    S32 new_height = llmax(getRect().getHeight(), content_height + floater_header_size + tabcntr_header_height + close_all_button_height);

    if (isMinimized())
    {
        LLRect newrect;
        newrect.setLeftTopAndSize(getExpandedRect().mLeft, getExpandedRect().mTop, new_width, new_height);
        setExpandedRect(newrect);
    }
    else
    {
        S32 old_height = getRect().getHeight();
        reshape(new_width, new_height);
        translate(0, old_height - new_height);
    }
}

void FSFloaterScriptDialogContainer::tabOpen(LLFloater* opened_floater, bool from_click)
{
    mFlashingTabStates.erase(opened_floater);
}

void FSFloaterScriptDialogContainer::startFlashingTab(LLFloater* floater, const std::string& message)
{
    if (LLMultiFloater::isFloaterFlashing(floater))
    {
        LLMultiFloater::setFloaterFlashing(floater, false);
    }
    LLMultiFloater::setFloaterFlashing(floater, true, false);
}

// EOF
