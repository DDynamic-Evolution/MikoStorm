// Copyright (C) 2026 MikoStorm contributors.
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "../llviewerprecompiledheaders.h"
#include "fsfloaterlovense.h"
#include "../llviewercontrol.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llpanel.h"
#include "llrender.h"
#include "llgl.h"
#include "lldir.h"
bool FSFloaterLovense::postBuild()
{
    getChild<LLButton>("connect")->setCommitCallback([this](LLUICtrl*, const LLSD&) { connect(); });
    getChild<LLButton>("disconnect")->setCommitCallback([this](LLUICtrl*, const LLSD&) { disconnect(); });
    getChild<LLButton>("reload")->setCommitCallback([](LLUICtrl*, const LLSD&) { MikoLovense::Client::instance().reloadQR(); });
    getChild<LLButton>("start")->setCommitCallback([this](LLUICtrl*, const LLSD&) { start(); });
    getChild<LLButton>("pause")->setCommitCallback([](LLUICtrl*, const LLSD&) { MikoLovense::Client::instance().pause(); });
    getChild<LLButton>("stop")->setCommitCallback([](LLUICtrl*, const LLSD&) { MikoLovense::Client::instance().pause(); });
    getChild<LLScrollListCtrl>("toys")->setCommitCallback([this](LLUICtrl*, const LLSD&) { changed(); });
    getChild<LLLineEditor>("pattern")->setCommitCallback([this](LLUICtrl*, const LLSD&) { changed(); });
    getChild<LLSpinCtrl>("strength")->setCommitCallback([this](LLUICtrl*, const LLSD&) { changed(); });
    getChild<LLCheckBoxCtrl>("enabled")->setCommitCallback(
        [this](LLUICtrl*, const LLSD&)
        {
            if (!gSavedSettings.getBOOL("MikoLovenseEnabled"))
                disconnect();
            refresh();
        });
    getChild<LLCheckBoxCtrl>("help")->setCommitCallback([this](LLUICtrl*, const LLSD&) { refresh(); });
    // Debug Settings and other settings editors must revoke control as well.
    for (const auto* name : { "MikoLovenseEnabled", "MikoLovensePattern", "MikoLovenseMaxStrength" })
        mSettingsConnections.emplace_back(gSavedSettings.getControl(name)->getSignal()->connect(
            [](LLControlVariable*, const LLSD&, const LLSD&)
            {
                if (!gSavedSettings.getBOOL("MikoLovenseEnabled"))
                    MikoLovense::Client::instance().disconnect();
                else
                    MikoLovense::Client::instance().pause();
            }));
    refresh();
    return true;
}
void FSFloaterLovense::connect()
{
    if (!gSavedSettings.getBOOL("MikoLovenseEnabled"))
        return;
    auto* editor = getChild<LLLineEditor>("auth_token");
    auto  token  = editor->getText();
    editor->clear();
    const auto platform = getChild<LLLineEditor>("platform")->getText();
    if (!MikoLovense::Client::instance().connect(std::move(token), platform, gDirUtilp->getCAFile()))
        getChild<LLTextBox>("feedback")->setText(getString("need_token"));
    else
        getChild<LLTextBox>("feedback")->setText(std::string{});
    refresh();
}
void FSFloaterLovense::disconnect()
{
    MikoLovense::Client::instance().disconnect();
    getChild<LLLineEditor>("auth_token")->clear();
    getChild<LLScrollListCtrl>("toys")->deselectAllItems();
    mQR.reset();
    mQRText.clear();
    refresh();
}
void FSFloaterLovense::changed()
{
    if (mUpdatingSelection)
        return;
    // Changing targets/parameters never starts or silently resumes a device.
    MikoLovense::Client::instance().pause();
    getChild<LLTextBox>("feedback")->setText(std::string{});
}
void FSFloaterLovense::start()
{
    if (!gSavedSettings.getBOOL("MikoLovenseEnabled"))
        return;
    std::vector<std::string> selected;
    for (auto* item : getChild<LLScrollListCtrl>("toys")->getAllSelected())
        selected.push_back(item->getValue().asString());
    const auto value = getChild<LLLineEditor>("pattern")->getText();
    const int  limit = static_cast<int>(getChild<LLSpinCtrl>("strength")->getValue().asInteger());
    if (selected.empty() || !MikoLovense::pattern(value, limit))
    {
        getChild<LLTextBox>("feedback")->setText(getString("invalid_pattern"));
        return;
    }
    MikoLovense::Client::instance().run(selected, value, limit);
    getChild<LLTextBox>("feedback")->setText(std::string{});
}
void FSFloaterLovense::onClose(bool quitting)
{
    disconnect();
    LLFloater::onClose(quitting);
}
void FSFloaterLovense::refresh()
{
    auto  snapshot       = MikoLovense::Client::instance().snapshot();
    auto* list           = getChild<LLScrollListCtrl>("toys");
    bool  devicesChanged = snapshot.toys.size() != mSnapshot.toys.size();
    for (std::size_t i = 0; !devicesChanged && i < snapshot.toys.size(); ++i)
    {
        const auto& a  = snapshot.toys[i];
        const auto& b  = mSnapshot.toys[i];
        devicesChanged = a.id != b.id || a.name != b.name || a.battery != b.battery || a.connected != b.connected;
    }
    if (devicesChanged)
    {
        std::vector<std::string> selected;
        for (auto* item : list->getAllSelected())
            selected.push_back(item->getValue().asString());
        mUpdatingSelection = true;
        list->clearRows();
        for (const auto& toy : snapshot.toys)
        {
            if (!toy.connected)
                continue;
            LLSD row;
            row["value"]                = toy.id;
            row["columns"][0]["column"] = "name";
            row["columns"][0]["value"]  = toy.name;
            row["columns"][1]["column"] = "battery";
            row["columns"][1]["value"]  = std::to_string(toy.battery) + "%";
            list->addElement(row);
            if (std::find(selected.begin(), selected.end(), toy.id) != selected.end())
                list->setSelectedByValue(toy.id, true);
        }
    }
    mUpdatingSelection = false;
    if (snapshot.qr != mQRText)
    {
        mQR.reset();
        mQRText = snapshot.qr;
        if (!mQRText.empty())
            try
            {
                mQR = std::make_unique<qrcodegen::QrCode>(qrcodegen::QrCode::encodeText(mQRText.c_str(), qrcodegen::QrCode::Ecc::MEDIUM));
            }
            catch (const std::exception&)
            {
                getChild<LLTextBox>("feedback")->setText(getString("error_qr"));
            }
    }
    mSnapshot = std::move(snapshot);
    getChild<LLTextBox>("status")->setText(getString(mSnapshot.status));
    const bool enabled = gSavedSettings.getBOOL("MikoLovenseEnabled");
    const bool active  = mSnapshot.busy || mSnapshot.connected;
    getChild<LLButton>("connect")->setEnabled(enabled && !active);
    getChild<LLLineEditor>("auth_token")->setEnabled(enabled && !active);
    getChild<LLLineEditor>("platform")->setEnabled(enabled && !active);
    getChild<LLButton>("disconnect")->setEnabled(active);
    getChild<LLButton>("reload")->setEnabled(mSnapshot.connected && !mSnapshot.online);
    getChild<LLButton>("start")->setEnabled(enabled && mSnapshot.online && mSnapshot.connected && !mSnapshot.running);
    getChild<LLButton>("pause")->setEnabled(mSnapshot.running);
    getChild<LLButton>("stop")->setEnabled(mSnapshot.running);
    getChild<LLTextBox>("instructions")->setVisible(gSavedSettings.getBOOL("MikoLovenseShowHelp"));
    mRefresh.reset();
}
void FSFloaterLovense::draw()
{
    if (mRefresh.getElapsedTimeF32() >= .25f)
        refresh();
    LLFloater::draw();
    if (mQR)
    {
        const auto rect  = getChild<LLPanel>("qr_code")->getRect();
        const int  count = mQR->getSize() + 8;
        const int  scale = std::min(rect.getWidth(), rect.getHeight()) / count;
        if (scale < 1)
            return;
        const int width = count * scale;
        const int x     = rect.mLeft + (rect.getWidth() - width) / 2;
        const int y     = rect.mBottom + (rect.getHeight() - width) / 2;
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
        gl_rect_2d(x, y + width, x + width, y, LLColor4::white, true);
        for (int row = 0; row < mQR->getSize(); ++row)
            for (int col = 0; col < mQR->getSize(); ++col)
                if (mQR->getModule(col, row))
                    gl_rect_2d(x + (col + 4) * scale, y + width - (row + 4) * scale, x + (col + 5) * scale, y + width - (row + 5) * scale,
                               LLColor4::black, true);
    }
}
