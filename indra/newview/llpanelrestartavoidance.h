/**
 * @file llpanelrestartavoidance.h
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

#ifndef LL_PANEL_RESTART_AVOIDANCE_H
#define LL_PANEL_RESTART_AVOIDANCE_H

#include "lleventtimer.h"
#include "llpanel.h"

class LLScrollListCtrl;

class LLPanelRestartAvoidance : public LLPanel, public LLEventTimer
{
    LOG_CLASS(LLPanelRestartAvoidance);

public:
    LLPanelRestartAvoidance();
    ~LLPanelRestartAvoidance();

    bool postBuild() override;

protected:
    bool tick() override;

private:
    void bindControls();
    void refreshStatus();
    void refreshSafeList();

    void onCommitEnable();
    void onCommitWaitLeave();
    void onCommitWaitReturn();
    void onCommitHomeCheck();
    void onCommitRetryFail();
    void onCommitRegionTimeout();
    void onCommitLandingX();
    void onCommitLandingY();
    void onCommitLandingZ();

    void onAddCurrentLocation();
    void onRemoveSelected();
    void onSaveList();
    void onCheckNow();
    void onSimulateRestart();

    void onStatusChanged();

    LLScrollListCtrl* mSafeListCtrl;
    bool mInitialized;
    boost::signals2::connection mStatusChangedConnection;
};

#endif // LL_PANEL_RESTART_AVOIDANCE_H
