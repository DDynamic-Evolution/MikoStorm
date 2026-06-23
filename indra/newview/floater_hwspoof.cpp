#include "llviewerprecompiledheaders.h"

#include "floater_hwspoof.h"

#include "hwspoof_engine.h"
#include "llviewercontrol.h"

FloaterHWSpoof::FloaterHWSpoof(const LLSD& key)
    : LLFloater(key)
{
    mCommitCallbackRegistrar.add("HWSpoof.Reroll", boost::bind(&FloaterHWSpoof::onClickReroll, this));
}

bool FloaterHWSpoof::postBuild()
{
    updateLabels();
    return true;
}

void FloaterHWSpoof::updateLabels()
{
    getChild<LLUICtrl>("real_nodeid")->setValue(LLSD(hwspoof_get_real_nodeid_str()));
    getChild<LLUICtrl>("real_machineid")->setValue(LLSD(hwspoof_get_real_machineid_str()));
    getChild<LLUICtrl>("real_id0")->setValue(LLSD(hwspoof_get_real_serial()));
    getChild<LLUICtrl>("real_macid")->setValue(LLSD(hwspoof_get_real_macid_str()));

    getChild<LLUICtrl>("spoof_nodeid")->setValue(LLSD(hwspoof_get_faux_nodeid_str()));
    getChild<LLUICtrl>("spoof_machineid")->setValue(LLSD(hwspoof_get_faux_machineid_str()));
    getChild<LLUICtrl>("spoof_id0")->setValue(LLSD(hwspoof_get_id0()));
    getChild<LLUICtrl>("spoof_macid")->setValue(LLSD(hwspoof_get_macid()));

    getChild<LLUICtrl>("hwspoof_username")->setValue(LLSD(hwspoof_get_username()));
    getChild<LLUICtrl>("hwspoof_seed")->setValue(LLSD(hwspoof_get_seed()));
}

void FloaterHWSpoof::onClickReroll()
{
    hwspoof_reroll_seed();
    gSavedSettings.setString("HWSpoofSeed", hwspoof_get_seed());
    updateLabels();
}
