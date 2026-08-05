/**
 * @file llpanelrestartavoidance.cpp
 *
 * $LicenseInfo:firstyear=2024&license=mikostormlgpl$
 * MikoStorm Viewer Source Code
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

#include "llpanelrestartavoidance.h"

#include "llrestartavoidancemgr.h"

#include "llagent.h"
#include "llcheckboxctrl.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"

#include <boost/bind.hpp>
#include <set>

static LLPanelInjector<LLPanelRestartAvoidance> t_restart_avoidance("panel_restart_avoidance");

LLPanelRestartAvoidance::LLPanelRestartAvoidance()
    : LLPanel()
    , LLEventTimer(1.f)
    , mSafeListCtrl(NULL)
    , mInitialized(false)
{
    mCommitCallbackRegistrar.add("RestartAvoidance.Add", boost::bind(&LLPanelRestartAvoidance::onAddCurrentLocation, this));
    mCommitCallbackRegistrar.add("RestartAvoidance.Remove", boost::bind(&LLPanelRestartAvoidance::onRemoveSelected, this));
    mCommitCallbackRegistrar.add("RestartAvoidance.Save", boost::bind(&LLPanelRestartAvoidance::onSaveList, this));
    mCommitCallbackRegistrar.add("RestartAvoidance.Check", boost::bind(&LLPanelRestartAvoidance::onCheckNow, this));
    mCommitCallbackRegistrar.add("RestartAvoidance.Simulate", boost::bind(&LLPanelRestartAvoidance::onSimulateRestart, this));
}

LLPanelRestartAvoidance::~LLPanelRestartAvoidance()
{
    if (mStatusChangedConnection.connected())
    {
        mStatusChangedConnection.disconnect();
    }
}

bool LLPanelRestartAvoidance::postBuild()
{
    mSafeListCtrl = getChild<LLScrollListCtrl>("restart_avoidance_safe_list");

    bindControls();

    mStatusChangedConnection = RestartAvoidanceManager::instance().addStatusChangedCallback(
        boost::bind(&LLPanelRestartAvoidance::onStatusChanged, this));

    mInitialized = true;

    refreshSafeList();
    refreshStatus();

    return LLPanel::postBuild();
}

bool LLPanelRestartAvoidance::tick()
{
    refreshStatus();
    return false;
}

void LLPanelRestartAvoidance::bindControls()
{
    // Initialize all controls from the current setting values, then bind
    // commit callbacks so each change is written back to gSavedSettings.
    getChild<LLCheckBoxCtrl>("restart_avoidance_enable")->setValue(gSavedSettings.getBOOL("RestartAvoidanceEnable"));
    getChild<LLSpinCtrl>("restart_avoidance_wait_leave")->setValue((LLSD::Integer)gSavedSettings.getS32("RestartAvoidanceWaitLeave"));
    getChild<LLSpinCtrl>("restart_avoidance_wait_return")->setValue((LLSD::Integer)gSavedSettings.getS32("RestartAvoidanceWaitReturn"));
    getChild<LLSpinCtrl>("restart_avoidance_home_check")->setValue((LLSD::Integer)gSavedSettings.getS32("RestartAvoidanceHomeCheck"));
    getChild<LLSpinCtrl>("restart_avoidance_retry_fail")->setValue((LLSD::Integer)gSavedSettings.getS32("RestartAvoidanceRetryFail"));
    getChild<LLSpinCtrl>("restart_avoidance_region_timeout")->setValue((LLSD::Integer)gSavedSettings.getS32("RestartAvoidanceRegionTimeout"));
    getChild<LLSpinCtrl>("restart_avoidance_landing_x")->setValue((LLSD::Integer)gSavedSettings.getS32("RestartAvoidanceLandingX"));
    getChild<LLSpinCtrl>("restart_avoidance_landing_y")->setValue((LLSD::Integer)gSavedSettings.getS32("RestartAvoidanceLandingY"));
    getChild<LLSpinCtrl>("restart_avoidance_landing_z")->setValue((LLSD::Integer)gSavedSettings.getS32("RestartAvoidanceLandingZ"));

    getChild<LLUICtrl>("restart_avoidance_enable")->setCommitCallback(boost::bind(&LLPanelRestartAvoidance::onCommitEnable, this));
    getChild<LLUICtrl>("restart_avoidance_wait_leave")->setCommitCallback(boost::bind(&LLPanelRestartAvoidance::onCommitWaitLeave, this));
    getChild<LLUICtrl>("restart_avoidance_wait_return")->setCommitCallback(boost::bind(&LLPanelRestartAvoidance::onCommitWaitReturn, this));
    getChild<LLUICtrl>("restart_avoidance_home_check")->setCommitCallback(boost::bind(&LLPanelRestartAvoidance::onCommitHomeCheck, this));
    getChild<LLUICtrl>("restart_avoidance_retry_fail")->setCommitCallback(boost::bind(&LLPanelRestartAvoidance::onCommitRetryFail, this));
    getChild<LLUICtrl>("restart_avoidance_region_timeout")->setCommitCallback(boost::bind(&LLPanelRestartAvoidance::onCommitRegionTimeout, this));
    getChild<LLUICtrl>("restart_avoidance_landing_x")->setCommitCallback(boost::bind(&LLPanelRestartAvoidance::onCommitLandingX, this));
    getChild<LLUICtrl>("restart_avoidance_landing_y")->setCommitCallback(boost::bind(&LLPanelRestartAvoidance::onCommitLandingY, this));
    getChild<LLUICtrl>("restart_avoidance_landing_z")->setCommitCallback(boost::bind(&LLPanelRestartAvoidance::onCommitLandingZ, this));
}

void LLPanelRestartAvoidance::onCommitEnable()
{
    gSavedSettings.setBOOL("RestartAvoidanceEnable",
        getChild<LLCheckBoxCtrl>("restart_avoidance_enable")->getValue().asBoolean());
}

void LLPanelRestartAvoidance::onCommitWaitLeave()
{
    gSavedSettings.setS32("RestartAvoidanceWaitLeave",
        (S32)getChild<LLSpinCtrl>("restart_avoidance_wait_leave")->getValue().asInteger());
}

void LLPanelRestartAvoidance::onCommitWaitReturn()
{
    gSavedSettings.setS32("RestartAvoidanceWaitReturn",
        (S32)getChild<LLSpinCtrl>("restart_avoidance_wait_return")->getValue().asInteger());
}

void LLPanelRestartAvoidance::onCommitHomeCheck()
{
    gSavedSettings.setS32("RestartAvoidanceHomeCheck",
        (S32)getChild<LLSpinCtrl>("restart_avoidance_home_check")->getValue().asInteger());
}

void LLPanelRestartAvoidance::onCommitRetryFail()
{
    gSavedSettings.setS32("RestartAvoidanceRetryFail",
        (S32)getChild<LLSpinCtrl>("restart_avoidance_retry_fail")->getValue().asInteger());
}

void LLPanelRestartAvoidance::onCommitRegionTimeout()
{
    gSavedSettings.setS32("RestartAvoidanceRegionTimeout",
        (S32)getChild<LLSpinCtrl>("restart_avoidance_region_timeout")->getValue().asInteger());
}

void LLPanelRestartAvoidance::onCommitLandingX()
{
    gSavedSettings.setS32("RestartAvoidanceLandingX",
        (S32)getChild<LLSpinCtrl>("restart_avoidance_landing_x")->getValue().asInteger());
}

void LLPanelRestartAvoidance::onCommitLandingY()
{
    gSavedSettings.setS32("RestartAvoidanceLandingY",
        (S32)getChild<LLSpinCtrl>("restart_avoidance_landing_y")->getValue().asInteger());
}

void LLPanelRestartAvoidance::onCommitLandingZ()
{
    gSavedSettings.setS32("RestartAvoidanceLandingZ",
        (S32)getChild<LLSpinCtrl>("restart_avoidance_landing_z")->getValue().asInteger());
}

void LLPanelRestartAvoidance::onAddCurrentLocation()
{
    LLViewerRegion* region = gAgent.getRegion();
    if (!region)
    {
        return;
    }

    LLVector3 local_pos = gAgent.getPosAgentFromGlobal(gAgent.getPositionGlobal());

    LLSD entry;
    entry["region"] = region->getName();
    entry["x"] = (LLSD::Integer)(S32)local_pos.mV[VX];
    entry["y"] = (LLSD::Integer)(S32)local_pos.mV[VY];
    entry["z"] = (LLSD::Integer)(S32)local_pos.mV[VZ];
    entry["stamp"] = (LLSD::Integer)(S32)time(NULL);

    LLSD list = RestartAvoidanceManager::instance().getSafeList();
    if (!list.isArray())
    {
        list = LLSD::emptyArray();
    }
    list.append(entry);

    RestartAvoidanceManager::instance().setSafeList(list);
    LL_INFOS("RestartAvoidance") << "Added current location: " << entry["region"].asString()
        << " (" << entry["x"].asInteger() << ", " << entry["y"].asInteger() << ", "
        << entry["z"].asInteger() << "), list size now " << (S32)list.size() << LL_ENDL;
    refreshSafeList();
}

void LLPanelRestartAvoidance::onRemoveSelected()
{
    if (!mSafeListCtrl)
    {
        return;
    }

    std::vector<LLScrollListItem*> selected = mSafeListCtrl->getAllSelected();
    if (selected.empty())
    {
        return;
    }

    std::set<std::string> remove;
    for (std::vector<LLScrollListItem*>::const_iterator it = selected.begin(); it != selected.end(); ++it)
    {
        remove.insert((*it)->getValue().asString());
    }

    LLSD list = RestartAvoidanceManager::instance().getSafeList();
    LLSD new_list = LLSD::emptyArray();
    for (LLSD::array_const_iterator it = list.beginArray(); it != list.endArray(); ++it)
    {
        std::string region = (*it)["region"].asString();
        if (remove.count(region) > 0)
        {
            continue;
        }
        new_list.append(*it);
    }

    RestartAvoidanceManager::instance().setSafeList(new_list);
    refreshSafeList();
}

void LLPanelRestartAvoidance::onSaveList()
{
    RestartAvoidanceManager::instance().saveSafeList(RestartAvoidanceManager::instance().getSafeList());
    refreshSafeList();
}

void LLPanelRestartAvoidance::onCheckNow()
{
    LLSD list = RestartAvoidanceManager::instance().getSafeList();
    for (LLSD::array_const_iterator it = list.beginArray(); it != list.endArray(); ++it)
    {
        std::string region = (*it)["region"].asString();
        if (!region.empty())
        {
            RestartAvoidanceManager::instance().checkRegionStatus(region);
        }
    }
    refreshStatus();
}

void LLPanelRestartAvoidance::onSimulateRestart()
{
    RestartAvoidanceManager::instance().simulateRestart();
}

void LLPanelRestartAvoidance::refreshSafeList()
{
    if (!mSafeListCtrl)
    {
        return;
    }

    mSafeListCtrl->clearRows();

    LLSD list = RestartAvoidanceManager::instance().getSafeList();
    S32 added = 0;
    for (LLSD::array_const_iterator it = list.beginArray(); it != list.endArray(); ++it)
    {
        LLSD row;
        row["value"] = (*it)["region"].asString();

        row["columns"][0]["column"] = "region";
        row["columns"][0]["value"] = (*it)["region"].asString();

        row["columns"][1]["column"] = "x";
        row["columns"][1]["value"] = (*it)["x"].asInteger();

        row["columns"][2]["column"] = "y";
        row["columns"][2]["value"] = (*it)["y"].asInteger();

        row["columns"][3]["column"] = "z";
        row["columns"][3]["value"] = (*it)["z"].asInteger();

        row["columns"][4]["column"] = "stamp";
        row["columns"][4]["value"] = (*it)["stamp"].asInteger();

        mSafeListCtrl->addElement(row);
        added++;
    }
    LL_INFOS("RestartAvoidance") << "refreshSafeList: added " << added << " rows" << LL_ENDL;
}

void LLPanelRestartAvoidance::refreshStatus()
{
    if (!mInitialized)
    {
        return;
    }

    S32 online = RestartAvoidanceManager::instance().getOnlineCount();
    S32 down = RestartAvoidanceManager::instance().getDownCount();
    S32 awaiting = RestartAvoidanceManager::instance().getAwaitingCount();

    getChild<LLTextBox>("restart_avoidance_status")->setText(
        llformat("%d online, %d down, %d awaiting reply", online, down, awaiting));
}

void LLPanelRestartAvoidance::onStatusChanged()
{
    if (mInitialized)
    {
        refreshStatus();
    }
}
