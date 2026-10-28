#include "llviewerprecompiledheaders.h"
#include "llmcpserver.h"
#include "llmcpcontext.h"
#include "llmcpvalidation.h"
#include "llmcphttp.h"
#include "llsdjson.h"
#include "llviewercontrol.h"
#include "llversioninfo.h"
#include "lldate.h"
#include "llthread.h"
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include <algorithm>
#include <functional>
#include <stdexcept>

namespace
{
std::string hashToken(const std::string& token)
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(token.data()), token.size(), digest);
    const char* hex = "0123456789abcdef";
    std::string result;
    for (unsigned char byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
}
bool equalHash(const std::string& left, const std::string& right)
{
    return left.size() == right.size() && !left.empty() && CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}
struct Finally
{
    std::function<void()> function;
    ~Finally() { function(); }
};
LLSD toolError(const std::string& message)
{
    const LLSD output = LLSDMap("data", LLSDMap("error", message));
    return LLSDMap("isError", true)("structuredContent", output)
        ("content", llsd::array(LLSDMap("type", "text")("text", boost::json::serialize(LlsdToJson(output)))));
}
}

std::string LLMCPServer::generateToken()
{
    unsigned char random[32];
    if (RAND_bytes(random, sizeof(random)) != 1) return {};
    const char* hex = "0123456789abcdef";
    std::string token;
    for (unsigned char byte : random) { token += hex[byte >> 4]; token += hex[byte & 15]; }
    return token;
}
void LLMCPServer::refreshAccess()
{
    const auto records = gSavedSettings.getLLSD("MCPClients");
    const auto token = gSavedSettings.getString("MCPAuthToken");
    std::lock_guard<std::mutex> lock(mMutex);
    mDefaultPermissions = gSavedSettings.getU32("MCPDefaultPermissions") & 31;
    mMediaPermissions = gSavedSettings.getU32("MCPMediaPermissions") & 31;
    mDefaultTokenHash = token.empty() ? std::string() : hashToken(token);
    mClients.clear();
    if (records.isMap()) for (auto it = records.beginMap(); it != records.endMap(); ++it)
    {
        if (mClients.size() >= 16) break;
        if (it->first == "default" || it->first == "media-plugin") continue;
        auto digest = it->second["token_hash"].asString();
        if (digest.size() != 64 || digest.find_first_not_of("0123456789abcdef") != std::string::npos) continue;
        mClients[it->first] = {it->second["name"].asString(), digest, static_cast<U32>(it->second["permissions"].asInteger()) & 31};
    }
}
bool LLMCPServer::authenticate(const std::string& token, bool supplied, std::string& client) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (!supplied && mDefaultTokenHash.empty()) { client = "default"; return true; }
    if (!supplied || token.empty() || token.size() > 256) return false;
    const auto digest = hashToken(token);
    for (const auto& entry : mClients)
        if (equalHash(entry.second.tokenHash, digest)) { client = entry.first; return true; }
    if (equalHash(mDefaultTokenHash, digest)) { client = "default"; return true; }
    return false;
}
bool LLMCPServer::permitted(const std::string& client, U32 group) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    U32 mask = 0;
    if (client == "default") mask = mDefaultPermissions;
    else if (client == "media-plugin") mask = mMediaPermissions;
    else { auto it = mClients.find(client); if (it == mClients.end()) return false; mask = it->second.permissions; }
    return (mask & group) == group;
}
U32 LLMCPServer::permissionForTool(const std::string& name)
{
    if (name == "operation_status" || name == "operation_cancel") return 0;
    if (name == "camera_get_state" || name == "camera_list_joints" || name == "camera_list_presets" || name == "timeline_get_state") return Read;
    if (name.find("camera_") == 0 || name.find("timeline_") == 0 || name == "audio_mute_source") return Camera;
    if (name.find("avatar_") == 0) return Movement;
    if (name.find("chat_") == 0 || name.find("im_") == 0) return Chat;
    if (name == "notecard_write") return InventoryWrite;
    return Read;
}
std::string LLMCPServer::currentClient() const { return gMCPContext ? gMCPContext->client : "media-plugin"; }
bool LLMCPServer::supportsProtocol(const std::string& version)
{
    return version == "2025-11-25" || version == "2025-06-18";
}
bool LLMCPServer::validSession(const std::string& id, const std::string& client) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto found = mSessions.find(id);
    return found != mSessions.end() && found->second.client == client && Clock::now() - found->second.touched < std::chrono::minutes(20);
}
std::string LLMCPServer::protocolForSession(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto found = mSessions.find(id);
    return found == mSessions.end() ? std::string() : found->second.protocol;
}
void LLMCPServer::closeSession(const std::string& id, const std::string& client)
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto found = mSessions.find(id);
    if (found == mSessions.end() || found->second.client != client) return;
    mSessions.erase(found);
    const auto prefix = client + "\n" + id + "\n";
    for (auto& entry : mRequests) if (entry.first.find(prefix) == 0) entry.second->store(false);
    for (auto& entry : mOperations) if (entry.second.session == id && entry.second.client == client && (entry.second.state == "running" || entry.second.state == "cancelling"))
    { entry.second.cancelRequested = true; entry.second.state = "cancelling"; entry.second.message = "Client session closed"; }
}
LLSD LLMCPServer::getStatus() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return LLSDMap("running", mRunning.load() && LLMCPHttpServer::isRunning())("port", static_cast<S32>(mPort))
        ("sessions", static_cast<S32>(mSessions.size()))("operations", static_cast<S32>(mOperations.size()))
        ("authentication", !mDefaultTokenHash.empty() || !mClients.empty())("last_error", mLastError);
}
LLSD LLMCPServer::getClientProfiles() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    LLSD profiles = llsd::array(LLSDMap("id", "default")("name", "Standard / global token")("permissions", static_cast<S32>(mDefaultPermissions)),
        LLSDMap("id", "media-plugin")("name", "Media plugins")("permissions", static_cast<S32>(mMediaPermissions)));
    for (const auto& entry : mClients)
        profiles.append(LLSDMap("id", entry.first)("name", entry.second.name)("permissions", static_cast<S32>(entry.second.permissions)));
    return profiles;
}
void LLMCPServer::setClientPermissions(const std::string& id, U32 mask)
{
    mask &= 31;
    if (id == "default") gSavedSettings.setU32("MCPDefaultPermissions", mask);
    else if (id == "media-plugin") gSavedSettings.setU32("MCPMediaPermissions", mask);
    else
    {
        LLSD profiles = gSavedSettings.getLLSD("MCPClients");
        if (!profiles.has(id)) return;
        profiles[id]["permissions"] = static_cast<S32>(mask);
        gSavedSettings.setLLSD("MCPClients", profiles);
    }
    refreshAccess();
}
std::string LLMCPServer::createClient(const std::string& name, U32 mask)
{
    LLSD profiles = gSavedSettings.getLLSD("MCPClients");
    if (name.empty() || name.size() > 128 || profiles.size() >= 16) return {};
    auto token = generateToken();
    if (token.empty()) return {};
    const auto id = LLUUID::generateNewID().asString();
    profiles[id] = LLSDMap("name", name)("token_hash", hashToken(token))("permissions", static_cast<S32>(mask & 31));
    gSavedSettings.setLLSD("MCPClients", profiles);
    refreshAccess();
    return token;
}
void LLMCPServer::removeClient(const std::string& id)
{
    if (id == "default" || id == "media-plugin") return;
    LLSD profiles = gSavedSettings.getLLSD("MCPClients");
    profiles.erase(id);
    gSavedSettings.setLLSD("MCPClients", profiles);
    refreshAccess();
}
void LLMCPServer::audit(const std::string& tool, const std::string& client, const std::string& status)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (mAudit.size() >= 100) mAudit.pop_front();
    // Deliberately do not retain arguments, tokens, chat text or IM content.
    mAudit.push_back(LLSDMap("time", LLDate::now().asString())("tool", tool)("client", client)("status", status));
}
LLSD LLMCPServer::getAudit() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    LLSD list = LLSD::emptyArray();
    for (const auto& entry : mAudit) list.append(entry);
    return list;
}

LLSD LLMCPServer::handleRequest(const LLSD& request, Context context, std::string* created_session)
{
    if (!request.isMap() || request["jsonrpc"].asString() != "2.0" || !request["method"].isString())
        return makeError(-32600, "Expected a JSON-RPC 2.0 request object");
    const auto method = request["method"].asString();
    const bool has_id = request.has("id") && request["id"].isDefined();
    const LLSD params = request.has("params") ? request["params"] : LLSD::emptyMap();
    auto response = [&](LLSD value) { value["id"] = request["id"]; return value; };
    if (method.empty() || method.size() > 128) return has_id ? response(makeError(-32600, "Invalid method name")) : LLSD();
    if (!params.isMap()) return has_id ? response(makeError(-32602, "params must be an object")) : LLSD();
    if (method == "initialize")
    {
        if (!has_id) return LLSD();
        if (!params["protocolVersion"].isString() || !params["capabilities"].isMap() ||
            !params["clientInfo"].isMap() || !params["clientInfo"]["name"].isString() ||
            params["clientInfo"]["name"].asString().empty() ||
            params["clientInfo"]["name"].asString().size() > 128 || !params["clientInfo"]["version"].isString())
            return response(makeError(-32602, "Invalid initialize parameters"));
        if (!context.session.empty()) return response(makeError(-32600, "Initialize must create a new session"));
        const auto id = generateToken();
        if (id.empty()) return response(makeError(-32603, "Secure random generator unavailable"));
        const auto requested = params["protocolVersion"].asString();
        {
            std::lock_guard<std::mutex> lock(mMutex);
            for (auto it = mSessions.begin(); it != mSessions.end();)
                if (Clock::now() - it->second.touched >= std::chrono::minutes(20)) it = mSessions.erase(it); else ++it;
            if (mSessions.size() >= 32) return response(makeError(-32000, "Too many client sessions"));
            Session session;
            session.client = context.client;
            session.protocol = supportsProtocol(requested) ? requested : "2025-11-25";
            mSessions.emplace(id, session);
        }
        context.session = id;
        if (created_session) *created_session = id;
        LLMCPContextScope scope(context);
        return response(handleInitialize(params));
    }
    if (!validSession(context.session, context.client)) return has_id ? response(makeError(-32001, "Unknown or expired session")) : LLSD();
    if (method == "notifications/cancelled")
    {
        if (has_id) return response(makeError(-32600, "Cancellation must be a notification"));
        if (context.cancelKey.empty()) return LLSD();
        const auto key = context.client + "\n" + context.session + "\n" + context.cancelKey;
        std::lock_guard<std::mutex> lock(mMutex);
        auto found = mRequests.find(key);
        if (found != mRequests.end()) found->second->store(false);
        return LLSD();
    }
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto foundSession = mSessions.find(context.session);
        if (foundSession == mSessions.end()) return has_id ? response(makeError(-32001, "Session closed")) : LLSD();
        auto& session = foundSession->second;
        session.touched = Clock::now();
        if (method == "notifications/initialized")
        {
            if (has_id) return response(makeError(-32600, "Initialized must be a notification"));
            session.initialized = true; return LLSD();
        }
        if (method != "ping" && !session.initialized)
            return has_id ? response(makeError(-32000, "Send notifications/initialized before using the session")) : LLSD();
        if (Clock::now() - session.window > std::chrono::seconds(1)) { session.window = Clock::now(); session.calls = 0; }
        if (++session.calls > 30) return has_id ? response(makeError(-32000, "Client rate limit exceeded")) : LLSD();
    }
    // Only defined MCP notifications are accepted without an ID; never run a tool accidentally.
    if (!has_id) return LLSD();
    if (!context.active) context.active = std::make_shared<std::atomic<bool>>(true);
    const auto key = context.client + "\n" + context.session + "\n" + context.requestKey;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mRequests.size() >= 64 || mRequests.count(key)) return response(makeError(-32600, "Duplicate or excessive in-flight request"));
        mRequests[key] = context.active;
    }
    Finally cleanup{[this, key]() { std::lock_guard<std::mutex> lock(mMutex); mRequests.erase(key); }};
    LLMCPContextScope scope(context);
    LLSD result;
    if (method == "ping") result = handlePing(params);
    else if (method == "tools/list") result = handleToolsList(params);
    else if (method == "tools/call") result = handleToolsCall(params);
    else if (method == "resources/list") result = handleResourcesList(params);
    else if (method == "resources/read") result = handleResourcesRead(params);
    else if (method == "logging/setLevel")
        result = permitted(context.client, Read) ? handleSetLoggerLevel(params) : makeError(-32001, "Read permission denied");
    else result = makeError(-32601, "Unknown method: " + method);
    if (!context.active->load()) return LLSD();
    return response(result);
}

LLSD LLMCPServer::dispatchTool(const std::string& name, const LLSD& args)
{
    if (!on_main_thread()) return toolError("Tool execution requires the viewer main thread");
    Tool tool;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto found = mTools.find(name);
        if (found == mTools.end()) return toolError("Unknown tool: " + name);
        tool = found->second;
    }
    const auto client = currentClient();
    const auto permission = permissionForTool(name);
    // These checks also run on the main thread, after a possibly delayed queue handoff.
    if (!mRunning || !permitted(client, permission))
    { audit(name, client, "denied"); return toolError("Tool permission denied or server stopped"); }
    std::string validation;
    if (!LLMCPValidation::validate(args, tool.input_schema, validation))
    { audit(name, client, "invalid_arguments"); return toolError(validation); }
    if (gMCPContext && currentClient() != "media-plugin" && !validSession(gMCPContext->session, currentClient()))
        return toolError("Client session closed before execution");
    LLSD result;
    try { result = tool.handler(args); }
    catch (const std::exception& e) { result = toolError(std::string("Tool failed: ") + e.what()); }
    catch (...) { result = toolError("Tool failed"); }
    LLSD data = LLSD::emptyMap();
    if (result["structuredContent"].isMap()) data = result["structuredContent"].has("data") ? result["structuredContent"]["data"] : result["structuredContent"];
    else if (result["content"].isArray() && result["content"].size())
    {
        const auto text = result["content"][0]["text"].asString();
        boost::system::error_code error;
        auto parsed = boost::json::parse(text, error);
        if (!error)
        {
            auto value = LlsdFromJson(parsed);
            if (value.isMap()) data = value;
            else data["items"] = value;
        }
        else data["message"] = text;
    }
    result["structuredContent"] = LLSDMap("data", data);
    result["content"] = llsd::array(LLSDMap("type", "text")("text", boost::json::serialize(LlsdToJson(result["structuredContent"]))));
    audit(name, client, result["isError"].asBoolean() ? "failed" : "completed_or_started");
    return result;
}
