// Copyright (C) 2026 MikoStorm contributors.
// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef FS_LOVENSE_H
#define FS_LOVENSE_H
#include "fslovenseprotocol.h"
#include <memory>
namespace MikoLovense
{
struct Snapshot
{
    std::string      status = "disconnected", qr;
    std::vector<Toy> toys;
    bool             connected = false, online = false, running = false, busy = false;
    unsigned         revision = 0;
};
// Public methods are called only on the viewer main thread. The worker owns all
// networking and posts copied snapshots; it never calls viewer/UI singletons.
class Client
{
public:
    static Client& instance();
    ~Client();
    bool     connect(std::string authToken, std::string platform, std::string caFile);
    void     disconnect();
    void     shutdown();
    void     run(const std::vector<std::string>& ids, const std::string& pattern, int maxStrength);
    void     pause();
    void     reloadQR();
    Snapshot snapshot() const;

private:
    Client() = default;
    struct Impl;
    std::shared_ptr<Impl> mImpl;
};
} // namespace MikoLovense
#endif
