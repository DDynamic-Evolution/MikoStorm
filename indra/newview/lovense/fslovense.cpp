// Copyright (C) 2026 MikoStorm contributors.
// SPDX-License-Identifier: LGPL-2.1-or-later
#include "../llviewerprecompiledheaders.h"
#include "fslovense.h"
#include <curl/curl.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <openssl/ssl.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
namespace MikoLovense
{
namespace net   = boost::asio;
namespace ssl   = net::ssl;
namespace beast = boost::beast;
namespace ws    = beast::websocket;
using tcp       = net::ip::tcp;
using Clock     = std::chrono::steady_clock;
struct Client::Impl
{
    net::io_context                                  io;
    ssl::context                                     tls{ ssl::context::tls_client };
    tcp::resolver                                    resolver{ io };
    ws::stream<beast::ssl_stream<beast::tcp_stream>> socket{ io, tls };
    net::steady_timer                                deadline{ io }, tick{ io };
    beast::flat_buffer                               buffer;
    std::thread                                      thread;
    std::atomic<bool>                                cancelled{ false }, done{ false };
    std::atomic<unsigned>                            commandEpoch{ 0 };
    mutable std::mutex                               mutex;
    Snapshot                                         state;
    Endpoint                                         address;
    std::deque<std::pair<std::string, bool>>         pending;
    std::shared_ptr<std::string>                     writing;
    std::vector<std::string>                         active;
    std::optional<std::string>                       runPattern;
    bool                                             open = false, stopping = false, engineReady = false, pongPending = false;
    int                                              pingInterval = 25000, pingTimeout = 20000;
    Clock::time_point                                nextPing = Clock::now(), pongDeadline = Clock::now(), nextRun = Clock::now();
    Snapshot                                         snapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return state;
    }
    template<typename F>
    void update(F fn)
    {
        std::lock_guard<std::mutex> lock(mutex);
        fn(state);
        ++state.revision;
    }
    void status(const char* key)
    {
        update([&](Snapshot& s) { s.status = key; });
    }
    void closeSocket()
    {
        open = false;
        resolver.cancel();
        deadline.cancel();
        tick.cancel();
        boost::system::error_code ec;
        beast::get_lowest_layer(socket).socket().cancel(ec);
        beast::get_lowest_layer(socket).socket().close(ec);
    }
    void fail(const char* key)
    {
        stopping = true;
        ++commandEpoch;
        active.clear();
        runPattern.reset();
        pending.clear();
        update(
            [&](Snapshot& s)
            {
                s.status    = cancelled ? "disconnected" : key;
                s.busy      = false;
                s.connected = false;
                s.online    = false;
                s.running   = false;
                s.qr.clear();
                s.toys.clear();
            });
        closeSocket();
    }
    struct HttpReply
    {
        std::string        body;
        std::atomic<bool>* cancelled;
    };
    static size_t receive(char* data, size_t size, size_t count, void* context)
    {
        auto& reply = *static_cast<HttpReply*>(context);
        if (size && count > MaxMessage / size)
            return 0;
        const auto bytes = size * count;
        if (reply.body.size() + bytes > MaxMessage)
            return 0;
        reply.body.append(data, bytes);
        return bytes;
    }
    static int progress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
    {
        return static_cast<HttpReply*>(context)->cancelled->load() ? 1 : 0;
    }
    Endpoint authorize(std::string& auth, const std::string& platform, const std::string& ca)
    {
        const auto cleanup = [](CURL* c)
        {
            if (c)
                curl_easy_cleanup(c);
        };
        std::unique_ptr<CURL, decltype(cleanup)> curl(curl_easy_init(), cleanup);
        if (!curl)
            throw std::runtime_error("network");
        std::string body = json::serialize(json::object{ { "platform", platform }, { "authToken", auth } });
        HttpReply   reply{ {}, &cancelled };
        curl_slist* headers     = curl_slist_append(nullptr, "Content-Type: application/json");
        const auto  freeHeaders = [](curl_slist* h)
        {
            curl_slist_free_all(h);
        };
        std::unique_ptr<curl_slist, decltype(freeHeaders)> headerOwner(headers, freeHeaders);
        auto*                                              c = curl.get();
        curl_easy_setopt(c, CURLOPT_URL, "https://api.lovense-api.com/api/basicApi/getSocketUrl");
        curl_easy_setopt(c, CURLOPT_POST, 1L);
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(c, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(c, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
        curl_easy_setopt(c, CURLOPT_CAINFO, ca.c_str());
        curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, 4000L);
        curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, 8000L);
        curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, &receive);
        curl_easy_setopt(c, CURLOPT_WRITEDATA, &reply);
        curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, &progress);
        curl_easy_setopt(c, CURLOPT_XFERINFODATA, &reply);
        const auto result = curl_easy_perform(c);
        long       code   = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
        std::fill(body.begin(), body.end(), '\0');
        std::fill(auth.begin(), auth.end(), '\0');
        auth.clear();
        if (cancelled)
            throw std::runtime_error("cancelled");
        if (result != CURLE_OK || code != 200)
            throw std::runtime_error("network");
        auto data = parse(reply.body);
        if (!data.is_object())
            throw std::runtime_error("protocol");
        auto& object = data.as_object();
        if (integer(object, "code", -1) != 0)
            throw std::runtime_error("token");
        auto* item = object.if_contains("data");
        if (!item || !item->is_object())
            throw std::runtime_error("protocol");
        auto endpointValue = endpoint(text(item->as_object(), "socketIoUrl", 4096), text(item->as_object(), "socketIoPath", 512));
        if (!endpointValue)
            throw std::runtime_error("endpoint");
        return *endpointValue;
    }
    void launch(std::string auth, std::string platform, std::string ca)
    {
        update(
            [](Snapshot& s)
            {
                s.status = "connecting";
                s.busy   = true;
            });
        thread = std::thread(
            [this, auth = std::move(auth), platform = std::move(platform), ca = std::move(ca)]() mutable
            {
                try
                {
                    address = authorize(auth, platform, ca);
                    if (cancelled)
                        throw std::runtime_error("cancelled");
                    tls.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::no_sslv3 |
                                    ssl::context::no_tlsv1 | ssl::context::no_tlsv1_1);
                    if (!SSL_set_min_proto_version(socket.next_layer().native_handle(), TLS1_2_VERSION))
                        throw std::runtime_error("tls");
                    tls.load_verify_file(ca);
                    socket.next_layer().set_verify_mode(ssl::verify_peer);
                    socket.next_layer().set_verify_callback(ssl::host_name_verification(address.host));
                    if (!SSL_set_tlsext_host_name(socket.next_layer().native_handle(), address.host.c_str()))
                        throw std::runtime_error("tls");
                    socket.read_message_max(MaxMessage);
                    deadline.expires_after(std::chrono::seconds(15));
                    deadline.async_wait(
                        [this](auto ec)
                        {
                            if (!ec)
                                fail("error_network");
                        });
                    resolver.async_resolve(address.host, "443",
                                           [this](auto ec, auto results)
                                           {
                                               if (ec || cancelled)
                                               {
                                                   fail("error_network");
                                                   return;
                                               }
                                               beast::get_lowest_layer(socket).expires_after(std::chrono::seconds(8));
                                               beast::get_lowest_layer(socket).async_connect(
                                                   results,
                                                   [this](auto error, auto)
                                                   {
                                                       if (error || cancelled)
                                                       {
                                                           fail("error_network");
                                                           return;
                                                       }
                                                       socket.next_layer().async_handshake(
                                                           ssl::stream_base::client,
                                                           [this](auto tlsError)
                                                           {
                                                               if (tlsError || cancelled)
                                                               {
                                                                   fail("error_network");
                                                                   return;
                                                               }
                                                               beast::get_lowest_layer(socket).expires_never();
                                                               socket.set_option(
                                                                   ws::stream_base::timeout::suggested(beast::role_type::client));
                                                               socket.async_handshake(address.host, address.target,
                                                                                      [this](auto wsError)
                                                                                      {
                                                                                          if (wsError || cancelled)
                                                                                          {
                                                                                              fail("error_network");
                                                                                              return;
                                                                                          }
                                                                                          open = true;
                                                                                          read();
                                                                                          scheduleTick();
                                                                                      });
                                                           });
                                                   });
                                           });
                    io.run();
                }
                catch (const std::exception& ex)
                {
                    // Never propagate exception text or remote bodies/tokens into logs/UI.
                    fail(std::string_view(ex.what()) == "token" ? "error_token" : "error_network");
                }
                done = true;
            });
    }
    void enqueue(std::string packet, bool isRun = false)
    {
        if (!open || (stopping && isRun))
            return;
        if (pending.size() >= 32)
        {
            fail("error_network");
            return;
        }
        pending.emplace_back(std::move(packet), isRun);
        writeNext();
    }
    void writeNext()
    {
        if (writing || pending.empty() || !open)
            return;
        writing = std::make_shared<std::string>(std::move(pending.front().first));
        pending.pop_front();
        auto lifetime = writing;
        socket.async_write(net::buffer(*lifetime),
                           [this, lifetime](auto ec, std::size_t)
                           {
                               writing.reset();
                               if (ec)
                               {
                                   fail("error_network");
                                   return;
                               }
                               writeNext();
                           });
    }
    void stopActive()
    {
        ++commandEpoch;
        runPattern.reset();
        pending.erase(std::remove_if(pending.begin(), pending.end(), [](const auto& p) { return p.second; }), pending.end());
        auto stopped = std::move(active);
        active.clear();
        for (const auto& id : stopped)
            enqueue(event("basicapi_send_toy_command_ts", command(id)));
        update(
            [](Snapshot& s)
            {
                s.running = false;
                if (s.connected)
                    s.status = s.online ? "paused" : "scan";
            });
    }
    void requestDisconnect()
    {
        if (stopping)
            return;
        stopping = true;
        stopActive();
        if (!open)
        {
            fail("disconnected");
            return;
        }
        // Give the explicit Stop packets a bounded chance to leave, then cancel
        // all operations. No synchronous TLS shutdown or indefinite main-thread wait.
        deadline.expires_after(std::chrono::milliseconds(350));
        deadline.async_wait(
            [this](auto ec)
            {
                if (!ec)
                    fail("disconnected");
            });
    }
    void start(const std::vector<std::string>& requested, const std::string& source, int limit, unsigned epoch)
    {
        if (cancelled || stopping || epoch != commandEpoch.load())
            return;
        auto current = snapshot();
        auto ids     = targets(requested, current.toys);
        auto p       = pattern(source, limit);
        if (!current.connected || !current.online || ids.empty() || ids.size() != requested.size() || !p)
            return;
        stopActive();
        active     = std::move(ids);
        runPattern = std::move(p);
        nextRun    = Clock::now();
        update(
            [](Snapshot& s)
            {
                s.running = true;
                s.status  = "running";
            });
    }
    void read()
    {
        socket.async_read(buffer,
                          [this](auto ec, std::size_t)
                          {
                              if (ec)
                              {
                                  fail("error_network");
                                  return;
                              }
                              std::string packet = beast::buffers_to_string(buffer.data());
                              buffer.consume(buffer.size());
                              try
                              {
                                  if (!stopping)
                                      onPacket(packet);
                              }
                              catch (const std::exception&)
                              {
                                  fail("error_protocol");
                                  return;
                              }
                              if (open)
                                  read();
                          });
    }
    void onPacket(const std::string& packet)
    {
        if (packet.empty())
            throw std::runtime_error("packet");
        if (packet.front() == '0')
        {
            if (engineReady)
                throw std::runtime_error("duplicate_open");
            auto hello = parse(std::string_view(packet).substr(1));
            if (!hello.is_object())
                throw std::runtime_error("open");
            pingInterval = integer(hello.as_object(), "pingInterval", -1);
            pingTimeout  = integer(hello.as_object(), "pingTimeout", -1);
            if (pingInterval < 1000 || pingInterval > 60000 || pingTimeout < 1000 || pingTimeout > 60000)
                throw std::runtime_error("heartbeat");
            engineReady = true;
            nextPing    = Clock::now() + std::chrono::milliseconds(pingInterval);
            return;
        }
        if (!engineReady)
            throw std::runtime_error("open_required");
        if (packet == "40" || packet.rfind("40{", 0) == 0)
        {
            deadline.cancel();
            update(
                [](Snapshot& s)
                {
                    s.connected = true;
                    s.status    = "scan";
                    s.busy      = false;
                });
            requestQR();
            return;
        }
        if (packet.front() == '1' || packet.rfind("41", 0) == 0 || packet.rfind("44", 0) == 0)
        {
            fail("error_network");
            return;
        }
        if (packet.front() == '3')
        {
            pongPending = false;
            nextPing    = Clock::now() + std::chrono::milliseconds(pingInterval);
            return;
        }
        if (packet.front() == '2')
        {
            enqueue("3" + packet.substr(1));
            return;
        }
        if (packet.rfind("42", 0) != 0)
            return;
        auto decoded = parse(std::string_view(packet).substr(2));
        if (!decoded.is_array())
            throw std::runtime_error("event");
        const auto& arr = decoded.as_array();
        if (arr.size() != 2 || !arr[0].is_string())
            throw std::runtime_error("event");
        const std::string name(arr[0].as_string());
        json::value       payload = arr[1];
        if (payload.is_string())
            payload = parse(std::string(payload.as_string()));
        if (!payload.is_object())
            throw std::runtime_error("data");
        const auto& data = payload.as_object();
        if (name == "basicapi_get_qrcode_tc")
        {
            if (integer(data, "code", -1) != 0)
            {
                status("error_qr");
                return;
            }
            const auto* value = data.if_contains("data");
            if (!value || !value->is_object())
                throw std::runtime_error("qr");
            const auto qr = text(value->as_object(), "qrcode", 2048);
            if (qr.empty())
                throw std::runtime_error("qr");
            update([&](Snapshot& s) { s.qr = qr; });
        }
        else if (name == "basicapi_update_device_info_tc")
        {
            auto       toys   = deviceList(data);
            const bool online = flag(data, "online");
            if (!online || targets(active, toys) != active)
                stopActive();
            update(
                [&](Snapshot& s)
                {
                    s.toys   = std::move(toys);
                    s.online = online;
                    if (!s.running)
                        s.status = online ? "ready" : "scan";
                    if (online)
                        s.qr.clear();
                });
        }
        else if (name == "basicapi_update_app_status_tc" || name == "basicapi_update_app_online_tc")
        {
            const bool online = integer(data, "status") == 1;
            if (!online)
                stopActive();
            update(
                [&](Snapshot& s)
                {
                    s.online = online;
                    if (!online)
                        s.toys.clear();
                    if (!s.running)
                        s.status = online ? "ready" : "scan";
                    if (online)
                        s.qr.clear();
                });
        }
    }
    void requestQR()
    {
        if (cancelled || stopping || !snapshot().connected)
            return;
        update([](Snapshot& s) { s.qr.clear(); });
        enqueue(event("basicapi_get_qrcode_ts", json::object{}));
    }
    void scheduleTick()
    {
        tick.expires_after(std::chrono::milliseconds(100));
        tick.async_wait(
            [this](auto ec)
            {
                if (ec || !open || stopping)
                    return;
                const auto now = Clock::now();
                if (engineReady)
                {
                    if (pongPending && now >= pongDeadline)
                    {
                        fail("error_network");
                        return;
                    }
                    if (!pongPending && now >= nextPing)
                    {
                        enqueue("2");
                        pongPending  = true;
                        pongDeadline = now + std::chrono::milliseconds(pingTimeout);
                        nextPing     = now + std::chrono::milliseconds(pingInterval);
                    }
                }
                if (runPattern && now >= nextRun)
                {
                    for (const auto& id : active)
                        enqueue(event("basicapi_send_toy_command_ts", command(id, runPattern)), true);
                    nextRun = now + std::chrono::seconds(2);
                }
                if (open)
                    scheduleTick();
            });
    }
};
Client& Client::instance()
{
    static Client client;
    return client;
}
Client::~Client()
{
    shutdown();
}
bool Client::connect(std::string auth, std::string platform, std::string ca)
{
    if (mImpl && !mImpl->done)
        return false;
    shutdown();
    if (auth.empty() || auth.size() > 4096 || platform.empty() || platform.size() > 128 || ca.empty())
        return false;
    mImpl = std::make_shared<Impl>();
    mImpl->launch(std::move(auth), std::move(platform), std::move(ca));
    return true;
}
void Client::disconnect()
{
    if (!mImpl || mImpl->done)
        return;
    mImpl->cancelled = true;
    ++mImpl->commandEpoch;
    net::post(mImpl->io, [p = mImpl.get()] { p->requestDisconnect(); });
}
void Client::shutdown()
{
    if (mImpl)
    {
        disconnect();
        if (mImpl->thread.joinable())
            mImpl->thread.join();
        mImpl.reset();
    }
}
void Client::pause()
{
    if (mImpl && !mImpl->done)
    {
        ++mImpl->commandEpoch;
        net::post(mImpl->io, [p = mImpl.get()] { p->stopActive(); });
    }
}
void Client::run(const std::vector<std::string>& ids, const std::string& source, int limit)
{
    if (mImpl && !mImpl->done)
    {
        const auto epoch = ++mImpl->commandEpoch;
        net::post(mImpl->io, [p = mImpl.get(), ids, source, limit, epoch] { p->start(ids, source, limit, epoch); });
    }
}
void Client::reloadQR()
{
    if (mImpl && !mImpl->done)
        net::post(mImpl->io, [p = mImpl.get()] { p->requestQR(); });
}
Snapshot Client::snapshot() const
{
    return mImpl ? mImpl->snapshot() : Snapshot{};
}
} // namespace MikoLovense
