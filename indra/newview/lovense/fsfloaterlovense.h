// Copyright (C) 2026 MikoStorm contributors.
// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef FS_FLOATER_LOVENSE_H
#define FS_FLOATER_LOVENSE_H
#include "llfloater.h"
#include "lltimer.h"
#include "fslovense.h"
#include "QrCode.hpp"
#include <boost/signals2/connection.hpp>
class FSFloaterLovense : public LLFloater
{
    friend class LLFloaterReg;

public:
    explicit FSFloaterLovense(const LLSD& key) : LLFloater(key) {}
    ~FSFloaterLovense() override { MikoLovense::Client::instance().disconnect(); }
    bool postBuild() override;
    void draw() override;
    void onClose(bool app_quitting) override;

private:
    void                                            refresh();
    void                                            connect();
    void                                            disconnect();
    void                                            start();
    void                                            changed();
    bool                                            mUpdatingSelection = false;
    std::vector<boost::signals2::scoped_connection> mSettingsConnections;
    LLTimer                                         mRefresh;
    MikoLovense::Snapshot                           mSnapshot;
    std::string                                     mQRText;
    std::unique_ptr<qrcodegen::QrCode>              mQR;
};
#endif
