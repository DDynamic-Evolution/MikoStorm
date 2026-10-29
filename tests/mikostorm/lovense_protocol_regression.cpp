#include <boost/json/src.hpp>
#include "lovense/fslovenseprotocol.h"
#include <iostream>
#include <stdexcept>
using namespace MikoLovense;
static int checks = 0;
void       check(bool pass, const char* what)
{
    ++checks;
    if (!pass)
        throw std::runtime_error(what);
}
int main()
{
    try
    {
        auto e = endpoint("https://api.lovense-api.com/?token=abc%2F123", "/client");
        check(e && e->host == "api.lovense-api.com" && e->target == "/client/?EIO=3&transport=websocket&token=abc%2F123",
              "signed endpoint");
        check(endpoint("wss://Socket.Lovense.com:443", "/socket.io/").has_value(), "wss normalization");
        for (const auto& bad :
             { "http://api.lovense-api.com", "https://lovense-api.com.evil.test", "https://evillovense.com",
               "https://user@api.lovense-api.com", "https://127.0.0.1", "https://api.lovense-api.com:8443",
               "https://api.lovense-api.com/?EIO=4", "https://api.lovense-api.com/?transport=polling",
               "https://api.lovense-api.com/#fragment", "https://api.lovense-api.com/?sid=old", "https://api.lovense-api.com/?%45IO=4" })
            check(!endpoint(bad, "/socket.io"), "reject endpoint");
        check(!endpoint("https://api.lovense-api.com", "/../other"), "reject path traversal");
        check(!endpoint("https://api.lovense-api.com", "/socket?query"), "reject path query");
        check(!endpoint("https://api.lovense-api.com", "/%2e%2e"), "reject escaped path");
        check(pattern("0;3;20;2", 5) == std::optional<std::string>("0;3;5;2"), "strength cap");
        for (const auto& bad : { "", "1;", "1;;2", "-1", "21", "NaN", "1x", " 2", "2.5", "0;9999999999999999999999999" })
            check(!pattern(bad, 5), "reject pattern");
        check(!pattern("1", 0) && !pattern("1", 21), "reject limit");
        std::string many = "1";
        for (int i = 1; i < 65; ++i)
            many += ";1";
        check(!pattern(many, 5), "pattern count bound");
        std::vector<Toy> toys = { { "a", "One", 100, true }, { "b", "Two", 50, false }, { "c", "Three", 25, true } };
        check(targets({}, toys).empty(), "no default selection");
        check(targets({ "a", "a", "missing", "b", "c" }, toys) == std::vector<std::string>({ "a", "c" }),
              "connected explicit unique targets");
        check(targets({ "a" }, {}).empty(), "offline targets revoked");
        auto cmd = command("a", pattern("1;20", 5));
        check(cmd.at("toy") == "a" && cmd.at("strength") == "1;5", "target and cap on wire");
        check(integer(cmd, "timeSec") == RunSeconds && RunSeconds > 1 && RunSeconds <= 10, "bounded device expiry");
        auto stop = command("a");
        check(stop.at("action") == "Stop" && stop.at("toy") == "a", "targeted stop");
        bool rejected = false;
        try
        {
            command("");
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        check(rejected, "no broadcast command");
        auto frame = event("basicapi_send_toy_command_ts", cmd);
        auto wire  = parse(frame.substr(2));
        check(frame.rfind("42", 0) == 0 && wire.as_array()[1].as_object().at("toy") == "a", "socketio event encoding");
        auto parsed = parse(
            R"({"online":true,"toyList":[{"id":"a","name":"One","battery":200,"connected":true},{"id":"a","name":"Duplicate","connected":true},{"id":"invalid/id","connected":true},{"id":"b","nickname":"Nick","battery":-2,"connected":false}]})");
        auto devices = deviceList(parsed.as_object());
        check(devices.size() == 2, "device dedup and id check");
        check(devices[0].battery == 100 && devices[1].battery == 0 && !devices[1].connected && devices[1].name == "Nick",
              "device fields and battery clamp");
        rejected = false;
        try
        {
            parse(std::string(MaxMessage + 1, 'x'));
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        check(rejected, "message bound");
        rejected = false;
        try
        {
            parse("{malformed}");
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        check(rejected, "invalid JSON");
        check(validId("AB:12_cd-9") && !validId("a/b") && !validId(std::string(129, 'a')), "device ID syntax");
        std::cout << "Lovense protocol regression: " << checks << " checks passed\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed: " << e.what() << "\n";
        return 1;
    }
}
