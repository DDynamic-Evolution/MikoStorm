// Copyright (C) 2026 MikoStorm contributors.
// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef FS_LOVENSE_PROTOCOL_H
#define FS_LOVENSE_PROTOCOL_H
// MikoStorm Lovense integration. LGPL-2.1-or-later; see LICENSE.
#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <set>
#include <string>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <boost/json.hpp>
namespace MikoLovense
{
namespace json                   = boost::json;
constexpr std::size_t MaxMessage = 64 * 1024;
constexpr std::size_t MaxToys    = 8;
constexpr int         RunSeconds = 5; // Commands expire even if the viewer/network disappears.
struct Toy
{
    std::string id, name;
    int         battery   = 0;
    bool        connected = false;
};
struct Endpoint
{
    std::string host, target;
};
inline bool suffix(std::string_view s, std::string_view tail)
{
    return s.size() >= tail.size() && s.substr(s.size() - tail.size()) == tail;
}
inline bool validId(std::string_view id)
{
    return !id.empty() && id.size() <= 128 &&
           std::all_of(
               id.begin(),
               id.end(),
               [](unsigned char c)
               { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ':' || c == '-' || c == '_'; });
}
inline std::optional<Endpoint> endpoint(std::string url, std::string path)
{
    // Connection information is untrusted API input. Never forward credentials
    // to arbitrary hosts, userinfo URLs, plaintext transports or redirect targets.
    if (url.size() > 4096 || path.empty() || path.size() > 512 || path.front() != '/')
        return {};
    const auto unsafe = [](unsigned char c)
    {
        return c <= 32 || c >= 127 || c == '#' || c == '\\';
    };
    if (std::any_of(url.begin(), url.end(), unsafe) || std::any_of(path.begin(), path.end(), unsafe) || path.find('?') != std::string::npos)
        return {};
    if (url.rfind("https://", 0) == 0)
        url.erase(0, 8);
    else if (url.rfind("wss://", 0) == 0)
        url.erase(0, 6);
    else
        return {};
    const auto  end  = url.find_first_of("/?");
    std::string host = url.substr(0, end);
    if (suffix(host, ":443"))
        host.resize(host.size() - 4);
    std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (host.empty() || host.size() > 253 || host.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-.") != std::string::npos)
        return {};
    if (!(host == "lovense.com" || suffix(host, ".lovense.com") || host == "lovense-api.com" || suffix(host, ".lovense-api.com") ||
          suffix(host, ".lovense.club")))
        return {};
    if (path.find("..") != std::string::npos || path.find('%') != std::string::npos)
        return {};
    std::string query;
    const auto  q = url.find('?');
    if (q != std::string::npos)
        query = url.substr(q + 1);
    // Preserve the signed query, but reject ambiguous transport parameters.
    for (std::size_t pos = 0; pos < query.size();)
    {
        auto next = query.find('&', pos);
        auto key  = query.substr(pos, next == std::string::npos ? next : next - pos);
        key       = key.substr(0, key.find('='));
        if (key == "EIO" || key == "transport" || key == "sid" || key.find('%') != std::string::npos)
            return {};
        if (next == std::string::npos)
            break;
        pos = next + 1;
    }
    if (path.back() != '/')
        path += '/';
    return Endpoint{ host, path + "?EIO=3&transport=websocket" + (query.empty() ? "" : "&" + query) };
}
inline std::optional<std::string> pattern(std::string_view source, int limit)
{
    if (source.empty() || source.size() > 256 || limit < 1 || limit > 20)
        return {};
    std::string result;
    std::size_t count = 0;
    while (!source.empty())
    {
        auto       sep    = source.find(';');
        auto       item   = source.substr(0, sep);
        int        level  = 0;
        const auto parsed = std::from_chars(item.data(), item.data() + item.size(), level);
        if (item.empty() || parsed.ec != std::errc{} || parsed.ptr != item.data() + item.size() || level < 0 || level > 20 || ++count > 64)
            return {};
        if (!result.empty())
            result += ';';
        result += std::to_string(std::min(level, limit));
        if (sep == std::string_view::npos)
            break;
        source.remove_prefix(sep + 1);
        if (source.empty())
            return {};
    }
    return result;
}
inline std::vector<std::string> targets(const std::vector<std::string>& requested, const std::vector<Toy>& toys)
{
    std::vector<std::string> out;
    for (const auto& id : requested)
        if (validId(id) && std::find(out.begin(), out.end(), id) == out.end() && out.size() < MaxToys &&
            std::any_of(toys.begin(), toys.end(), [&](const Toy& toy) { return toy.id == id && toy.connected; }))
            out.push_back(id);
    return out;
}
inline json::value parse(std::string_view input)
{
    if (input.size() > MaxMessage)
        throw std::runtime_error("message_limit");
    boost::system::error_code ec;
    auto                      value = json::parse(input, ec);
    if (ec)
        throw std::runtime_error("protocol");
    return value;
}
inline std::string text(const json::object& obj, const char* key, std::size_t max = 256)
{
    const auto* p = obj.if_contains(key);
    if (!p || !p->is_string())
        return {};
    std::string s(p->as_string());
    if (s.size() > max)
        return {};
    return s;
}
inline int integer(const json::object& obj, const char* key, int fallback = 0)
{
    const auto* p = obj.if_contains(key);
    if (!p)
        return fallback;
    if (p->is_int64() && p->as_int64() >= -1000000 && p->as_int64() <= 1000000)
        return static_cast<int>(p->as_int64());
    if (p->is_uint64() && p->as_uint64() <= 1000000)
        return static_cast<int>(p->as_uint64());
    return fallback;
}
inline bool flag(const json::object& obj, const char* key)
{
    const auto* p = obj.if_contains(key);
    return p && ((p->is_bool() && p->as_bool()) || integer(obj, key) == 1);
}
inline std::vector<Toy> deviceList(const json::object& obj)
{
    std::vector<Toy> out;
    const auto*      list = obj.if_contains("toyList");
    if (!list || !list->is_array())
        throw std::runtime_error("devices");
    for (const auto& v : list->as_array())
    {
        if (!v.is_object() || out.size() >= MaxToys)
            continue;
        const auto& t = v.as_object();
        Toy         toy;
        toy.id = text(t, "id", 128);
        if (!validId(toy.id))
            continue;
        if (std::any_of(out.begin(), out.end(), [&](const Toy& old) { return old.id == toy.id; }))
            continue;
        toy.name = text(t, "nickname", 128);
        if (toy.name.empty())
            toy.name = text(t, "name", 128);
        toy.battery   = std::clamp(integer(t, "battery"), 0, 100);
        toy.connected = flag(t, "connected");
        out.push_back(toy);
    }
    return out;
}
inline std::string event(const char* name, const json::value& data)
{
    return "42" + json::serialize(json::array{ name, data });
}
inline json::object command(const std::string& id, const std::optional<std::string>& run = {})
{
    if (!validId(id))
        throw std::runtime_error("target");
    // A toy ID is mandatory. Never use the API's implicit all-devices broadcast.
    if (run)
        return { { "command", "Pattern" }, { "rule", "V:1;F:v;S:100#" }, { "strength", *run }, { "timeSec", RunSeconds }, { "toy", id },
                 { "apiVer", 2 } };
    return { { "command", "Function" }, { "action", "Stop" }, { "timeSec", 0 }, { "toy", id }, { "apiVer", 2 } };
}
} // namespace MikoLovense
#endif
