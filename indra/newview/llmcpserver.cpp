#include "llmcpserver.h"
#include "llsdutil.h"
#include "llmcptask.h"
#include "llmcpcontext.h"
#include "llcallbacklist.h"
#include "llmcphttp.h"
#include "llviewercontrol.h"
#include "llsdjson.h"
#include "llchat.h"
#include "llagent.h"
#include "llagentpilot.h"
#include "llviewerregion.h"
#include "llworld.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llavatarappearance.h"
#include "llinventorymodel.h"
#include "llinventorytype.h"
#include "llviewerinventory.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llparcel.h"
#include "llviewerjointattachment.h"
#include "llimview.h"
#include "llnamevalue.h"
#include "llstring.h"
#include "llappviewer.h"
#include "llversioninfo.h"
#include "llerrorcontrol.h"
#include "workqueue.h"
#include "llpluginclassmedia.h"
#include "llmainthreadtask.h"
#include "fsnearbychathub.h"
#include "llfloaterimnearbychathandler.h"
#include "llnotificationmanager.h"
#include "llcallingcard.h"
#include "llavatarnamecache.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

// create_new_item is defined in llviewerinventory.cpp
void create_new_item(const std::string& name,
                     const LLUUID& parent_id,
                     LLAssetType::EType asset_type,
                     LLInventoryType::EType inv_type,
                     U32 next_owner_perm,
                     std::function<void(const LLUUID&)> created_cb = nullptr);
#include "llviewerassetupload.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <future>
#include <boost/json.hpp>

static std::string toJsonString(const LLSD& data)
{
    return boost::json::serialize(LlsdToJson(data));
}

static LLSD errorResult(const std::string& message)
{
    return LLSDMap("isError", true)("content", llsd::array(LLSDMap("type", "text")("text", message)));
}

static LLSD textResult(const std::string& text)
{
    return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", text)));
}

LLMCPServer::LLMCPServer()
    : mRunning(false)
    , mPort(13231)
{
}

LLMCPServer::~LLMCPServer()
{
    stop();
}

void LLMCPServer::start()
{
    if (mRunning) return;
    const U32 port = gSavedSettings.getU32("MCPPort");
    if (port == 0 || port > 65535)
    {
        mLastError = "MCPPort must be between 1 and 65535";
        LL_WARNS("MCP") << "MCPPort must be between 1 and 65535" << LL_ENDL;
        return;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        mLastError = "Windows socket initialization failed";
        LL_WARNS("MCP") << "WSAStartup failed" << LL_ENDL;
        return;
    }
#endif

    mPort = static_cast<U16>(port);
    refreshAccess();
    mLastError.clear();

    // Load trigger words from settings (comma-separated)
    std::string trigger_cfg = gSavedSettings.getString("MCPTriggerWords");
    std::vector<std::string> trigger_words;
    if (!trigger_cfg.empty())
    {
        std::stringstream ss(trigger_cfg);
        std::string word;
        while (std::getline(ss, word, ','))
        {
            LLStringUtil::trim(word);
            if (!word.empty())
                trigger_words.push_back(word);
        }
    }
    setTriggerWords(trigger_words);

    registerDefaultTools();
    registerViewerTools();

    mSession = std::make_shared<std::atomic<bool>>(true);

    // Let media plugins consume MCP tools (e.g. CEF/VLC/GStreamer). The bridge
    // must run on the main thread so it can safely invoke the exposed tools.
    LLPluginClassMedia::setMCPBridge(
        [this, lifetime = mSession, capacity = mMainRequests](const std::string& command, const LLSD& params) -> LLSD
        {
            if (!lifetime->load()) return errorResult("MCP server stopped");
            if (on_main_thread()) return handlePluginMCPCommand(command, params);
            // A plugin worker must retain no reference to server members while
            // waiting: plugin workers are not joined by the HTTP server's stop.
            if (capacity->fetch_add(1) >= 6)
            { capacity->fetch_sub(1); return errorResult("Main-thread request capacity reached"); }
            struct Pending { std::shared_ptr<std::atomic<U32>> count; ~Pending() { count->fetch_sub(1); } } pending{capacity};
            auto queue = LL::WorkQueue::getInstance("mainloop");
            if (!queue) return errorResult("Main loop unavailable");
            auto task = std::make_shared<LLMCPCancellableTask<LLSD>>(lifetime);
            try
            {
                queue->post([task, this, command, params]() {
                    task->execute([this, command, params]() { return handlePluginMCPCommand(command, params); });
                });
                if (!task->wait(std::chrono::seconds(10))) return errorResult("MCP bridge request cancelled or timed out");
                return task->get();
            }
            catch (...) { return errorResult("MCP bridge request failed"); }
        });

    // Hook into chat to capture messages for MCP read access
    auto chat_handler = LLNotificationsUI::LLNotificationManager::instance().getChatHandler();
    if (chat_handler)
    {
        mChatConnection = chat_handler->addNewChatCallback(
            [this](const LLSD& chat) {
                pushChatMessage(chat);
                checkTriggerWords(chat);
            });
        LL_INFOS("MCP") << "Chat capture hook installed" << LL_ENDL;
    }

    // Hook into IM system to capture incoming IMs for MCP read access
    mImConnection = LLIMModel::instance().addNewMsgCallback(
        [this](const LLSD& arg) { pushIMMessage(arg); });
    LL_INFOS("MCP") << "IM capture hook installed" << LL_ENDL;

    mRunning = true;
    if (!LLMCPHttpServer::start(mPort))
    {
        mLastError = "Could not bind MCP listener; check port and log";
        stop();
        return;
    }
    gIdleCallbacks.addFunction(idle, this);
    LL_INFOS("MCP") << "MCP Server started on port " << mPort << LL_ENDL;
}

void LLMCPServer::stop()
{
    if (!mRunning) return;

    if (mChatConnection.connected())
    {
        mChatConnection.disconnect();
        mChatConnection = boost::signals2::connection();
    }

    if (mImConnection.connected())
    {
        mImConnection.disconnect();
        mImConnection = boost::signals2::connection();
    }

    LLPluginClassMedia::setMCPBridge(nullptr);

    bool cancelTeleport = false;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (const auto& [id, op] : mOperations)
            if (op.kind == "teleport" && op.hasTarget && (op.state == "running" || op.state == "cancelling")) cancelTeleport = true;
    }
    if (cancelTeleport) gAgent.teleportCancel();
    mSearchCursors.clear();
    mRunning = false;
    mSession->store(false); // Release a worker waiting for the main loop before joining it.
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (auto& request : mRequests) request.second->store(false);
        for (auto& operation : mOperations) if (operation.second.state == "running" || operation.second.state == "cancelling")
        { operation.second.state = "cancelled"; operation.second.message = "Server stopped"; }
        mSessions.clear();
    }
    gIdleCallbacks.deleteFunction(idle, this);
    LLMCPHttpServer::stop();
    LL_INFOS("MCP") << "MCP Server stopped" << LL_ENDL;
#ifdef _WIN32
    WSACleanup();
#endif
}

void LLMCPServer::pushChatMessage(const LLSD& msg)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (mChatBuffer.size() >= kChatBufferSize)
    {
        mChatBuffer.pop_front();
    }
    mChatBuffer.push_back(msg);
}

LLSD LLMCPServer::getChatMessages(S32 limit) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    LLSD messages = LLSD::emptyArray();
    if (limit <= 0 || limit > (S32)mChatBuffer.size())
        limit = (S32)mChatBuffer.size();
    size_t start = mChatBuffer.size() - limit;
    for (size_t i = start; i < mChatBuffer.size(); ++i)
    {
        messages.append(mChatBuffer[i]);
    }
    return messages;
}

void LLMCPServer::pushIMMessage(const LLSD& msg)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (mImBuffer.size() >= kImBufferSize)
    {
        mImBuffer.pop_front();
    }
    mImBuffer.push_back(msg);
}

LLSD LLMCPServer::getIMMessages(S32 limit) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    LLSD messages = LLSD::emptyArray();
    if (limit <= 0 || limit > (S32)mImBuffer.size())
        limit = (S32)mImBuffer.size();
    size_t start = mImBuffer.size() - limit;
    for (size_t i = start; i < mImBuffer.size(); ++i)
    {
        messages.append(mImBuffer[i]);
    }
    return messages;
}

void LLMCPServer::checkTriggerWords(const LLSD& chat)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (mTriggerWords.empty()) return;
    std::string text = chat["message"].asString();
    std::string lower = text;
    LLStringUtil::toLower(lower);
    for (const std::string& word : mTriggerWords)
    {
        if (lower.find(word) != std::string::npos)
        {
            if (mTriggerBuffer.size() >= kTriggerBufferSize)
            {
                mTriggerBuffer.pop_front();
            }
            mTriggerBuffer.push_back(chat);
            break;
        }
    }
}

LLSD LLMCPServer::getTriggerMatches() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    LLSD messages = LLSD::emptyArray();
    for (const auto& msg : mTriggerBuffer)
    {
        messages.append(msg);
    }
    LLSD data = LLSD::emptyMap();
    data["messages"] = messages;
    data["count"] = (LLSD::Integer)messages.size();
    data["trigger_words"] = llsd::array();
    for (const auto& w : mTriggerWords)
    {
        data["trigger_words"].append(w);
    }
    return data;
}

void LLMCPServer::setTriggerWords(const std::vector<std::string>& words)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mTriggerWords.clear();
    for (const std::string& w : words)
    {
        std::string lower = w;
        LLStringUtil::toLower(lower);
        if (!lower.empty())
        {
            mTriggerWords.insert(lower);
        }
    }
}

void LLMCPServer::clearTriggerBuffer()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mTriggerBuffer.clear();
}

void LLMCPServer::registerTool(const std::string& name,
                               const std::string& description,
                               const LLSD& input_schema,
                               ToolHandler handler)
{
    std::lock_guard<std::mutex> lock(mMutex);
    LLSD schema = input_schema;
    schema["additionalProperties"] = false;
    for (auto it = schema["properties"].beginMap(); it != schema["properties"].endMap(); ++it)
    {
        auto& field = schema["properties"][it->first];
        if (field["type"].asString() == "string" && !field.has("maxLength")) field["maxLength"] = it->first == "content" ? 65536 : 8192;
        if (it->first == "max_results" || it->first == "limit")
        { field["type"] = "integer"; field["minimum"] = 1; field["maximum"] = 200; }
        if (it->first == "offset") { field["type"] = "integer"; field["minimum"] = 0; field["maximum"] = 2147480000; }
        if (it->first == "channel") { field["type"] = "integer"; field["minimum"] = -2147483648.0; field["maximum"] = 2147483647.0; }
        if (it->first == "x" || it->first == "y") { field["minimum"] = 0; field["maximum"] = 65536; }
        if (it->first == "z") { field["minimum"] = -4096; field["maximum"] = 65536; }
    }
    for (const auto& key : {"folder", "source_id"})
        if (schema["properties"].has(key)) schema["properties"][key]["format"] = "uuid";
    for (auto it = schema["properties"].beginMap(); it != schema["properties"].endMap(); ++it)
        if (it->second["type"].asString() == "array" && it->second["items"]["type"].asString() == "string")
            schema["properties"][it->first]["items"]["maxLength"] = 8192;
    if (name == "inventory_search") schema["properties"]["cursor"] = LLSDMap("type", "string")("maxLength", 64);
    if (name == "get_nearby_objects") schema["properties"]["offset"] = LLSDMap("type", "integer")("minimum", 0)("maximum", 2147480000);
    if (name == "chat_read" || name == "im_read") schema["properties"]["limit"]["maximum"] = 100;
    for (const auto& key : {"name", "query", "region", "message", "target"})
        if (schema["properties"].has(key)) schema["properties"][key]["minLength"] = 1;
    if (schema["properties"].has("region")) schema["properties"]["region"]["maxLength"] = 255;
    if (name == "notecard_write") { schema["properties"]["name"]["maxLength"] = 128; schema["properties"]["content"]["minLength"] = 1; }
    mTools[name] = { name, description, schema, std::move(handler) };
}

void LLMCPServer::registerDefaultTools()
{
    registerTool("chat_say",
        "Send a message in local chat",
        LLSDMap("type", "object")(
            "properties", LLSDMap("message", LLSDMap("type", "string")("description", "Text to say"))(
                "channel", LLSDMap("type", "number")("description", "Chat channel (default 0)"))
        )("required", llsd::array("message")),
        [](const LLSD& p) -> LLSD {
            std::string msg = p["message"].asString();
            S32 ch = p.has("channel") ? (S32)p["channel"].asInteger() : 0;
            if (msg.empty())
                return LLSDMap("isError", true)("content", llsd::array(LLSDMap("type", "text")("text", "Message cannot be empty")));
            FSNearbyChat::sendChatFromViewer(utf8str_to_wstring(msg), utf8str_to_wstring(msg), CHAT_TYPE_NORMAL, false, ch);
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", "Message sent")));
        });

    registerTool("chat_shout",
        "Send a shout in local chat (up to 100m range)",
        LLSDMap("type", "object")(
            "properties", LLSDMap("message", LLSDMap("type", "string")("description", "Text to shout"))(
                "channel", LLSDMap("type", "number")("description", "Chat channel (default 0)"))
        )("required", llsd::array("message")),
        [](const LLSD& p) -> LLSD {
            std::string msg = p["message"].asString();
            S32 ch = p.has("channel") ? (S32)p["channel"].asInteger() : 0;
            if (msg.empty())
                return LLSDMap("isError", true)("content", llsd::array(LLSDMap("type", "text")("text", "Message cannot be empty")));
            FSNearbyChat::sendChatFromViewer(utf8str_to_wstring(msg), utf8str_to_wstring(msg), CHAT_TYPE_SHOUT, false, ch);
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", "Message shouted")));
        });

    registerTool("avatar_sit",
        "Sit on the ground; a repeated request leaves an already seated avatar seated",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
            if (gAgent.isSitting())
            {
                return textResult("Already sitting");
            }
            gAgent.sitDown();
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", "Sitting down")));
        });

    registerTool("avatar_stand",
        "Stand up from sitting position",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
            gAgent.standUp();
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", "Standing up")));
        });

    registerTool("avatar_walk_to",
        "Walk to a specific position in the current region",
        LLSDMap("type", "object")(
            "properties", LLSDMap("x", LLSDMap("type", "number")("description", "X coordinate"))(
                "y", LLSDMap("type", "number")("description", "Y coordinate"))(
                "z", LLSDMap("type", "number")("description", "Z coordinate"))
        )("required", llsd::array("x", "y", "z")),
        [](const LLSD& p) -> LLSD {
            if (!gAgent.getRegion()) return errorResult("Not connected to a region");
            LLVector3 local((F32)p["x"].asReal(), (F32)p["y"].asReal(), (F32)p["z"].asReal());
            if (local.mV[VX] >= gAgent.getRegion()->getWidth() || local.mV[VY] >= gAgent.getRegion()->getWidth())
                return errorResult("Walking destination lies outside the current region");
            if (gAgent.getRegion())
            {
                LLVector3d global = gAgent.getRegion()->getOriginGlobal() + LLVector3d(local);
                gAgent.startAutoPilotGlobal(global);
            }
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text",
                llformat("Walking to (%.1f, %.1f, %.1f)", p["x"].asReal(), p["y"].asReal(), p["z"].asReal()))));
        });

    registerTool("avatar_teleport",
        "Resolve a region in the current grid and start a tracked teleport operation",
        LLSDMap("type", "object")(
            "properties", LLSDMap("region", LLSDMap("type", "string")("description", "Region name"))(
                "x", LLSDMap("type", "number")("description", "X coordinate"))(
                "y", LLSDMap("type", "number")("description", "Y coordinate"))(
                "z", LLSDMap("type", "number")("description", "Z coordinate"))
        )("required", llsd::array("region", "x", "y", "z")),
        [this](const LLSD& p) -> LLSD { return startTeleport(p); });

    registerTool("avatar_fly",
        "Enable or disable flying",
        LLSDMap("type", "object")(
            "properties", LLSDMap("enabled", LLSDMap("type", "boolean")("description", "Whether to fly"))
        )("required", llsd::array("enabled")),
        [](const LLSD& p) -> LLSD {
            gAgent.setFlying(p["enabled"].asBoolean());
            if (gAgent.getFlying() != p["enabled"].asBoolean()) return errorResult("Flying was refused by viewer or region policy");
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text",
                p["enabled"].asBoolean() ? "Flying" : "Not flying")));
        });

    registerTool("get_position",
        "Get current avatar position, region, and rotation",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
            LLSD data = LLSD::emptyMap();
            data["region"] = gAgent.getRegion() ? gAgent.getRegion()->getName() : "";
            LLVector3 pos = gAgent.getPositionAgent();
            data["x"] = pos.mV[VX];
            data["y"] = pos.mV[VY];
            data["z"] = pos.mV[VZ];
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", toJsonString(data))));
        });

    registerTool("get_region_info",
        "Get information about the current region",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
            LLViewerRegion* regionp = gAgent.getRegion();
            if (!regionp)
                return LLSDMap("isError", true)("content", llsd::array(LLSDMap("type", "text")("text", "Not connected to a region")));
            LLSD data = LLSD::emptyMap();
            data["name"] = regionp->getName();
            data["handle"] = std::to_string(regionp->getHandle());
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", toJsonString(data))));
        });

    registerTool("get_nearby_agents",
        "List nearby avatars with positions and distances",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
            return textResult(toJsonString(LLMCPServer::instance().collectNearbyAgents()));
        });

    registerTool("notecard_write",
        "Create a new notecard in your inventory with the given name and text content",
        LLSDMap("type", "object")(
            "properties", LLSDMap("name", LLSDMap("type", "string")("description", "Notecard title"))(
                "content", LLSDMap("type", "string")("description", "Notecard body text"))
        )("required", llsd::array("name", "content")),
        [this](const LLSD& p) -> LLSD { return writeNotecard(p); });

    registerTool("inventory_list", "Page through the cached contents of an inventory folder",
        LLSDMap("type", "object")("properties", LLSDMap("folder", LLSDMap("type", "string"))
            ("offset", LLSDMap("type", "integer"))("max_results", LLSDMap("type", "integer"))),
        [](const LLSD& p) -> LLSD {
            LLUUID folder = p.has("folder") ? LLUUID(p["folder"].asString()) : gInventory.getRootFolderID();
            if (!gInventory.getCategory(folder)) return errorResult("Inventory folder is not cached");
            LLInventoryModel::cat_array_t* cats = nullptr;
            LLInventoryModel::item_array_t* items = nullptr;
            gInventory.getDirectDescendentsOf(folder, cats, items);
            const S32 offset = p.has("offset") ? p["offset"].asInteger() : 0;
            const S32 limit = p.has("max_results") ? p["max_results"].asInteger() : 50;
            const S32 catCount = cats ? static_cast<S32>(cats->size()) : 0;
            const S32 count = catCount + (items ? static_cast<S32>(items->size()) : 0);
            LLSD page = LLSD::emptyArray();
            for (S32 index = offset; index < count && page.size() < limit; ++index)
            {
                if (index < catCount)
                { const auto& entry = (*cats)[index]; page.append(LLSDMap("uuid", entry->getUUID().asString())("name", entry->getName())("type", "folder")); }
                else
                { const auto& entry = (*items)[index - catCount]; page.append(LLSDMap("uuid", entry->getUUID().asString())("name", entry->getName())("type", LLInventoryType::lookup(entry->getInventoryType()))); }
            }
            return textResult(toJsonString(LLSDMap("items", page)("count", page.size())("cached_total", count)
                ("next_offset", offset + page.size() < count ? LLSD(offset + page.size()) : LLSD())));
        });

    registerTool("chat_im",
        "Send an instant message to another avatar",
        LLSDMap("type", "object")(
            "properties", LLSDMap("target", LLSDMap("type", "string")("description", "Avatar UUID or display name"))(
                "message", LLSDMap("type", "string")("description", "Message text"))
        )("required", llsd::array("target", "message")),
        [](const LLSD& p) -> LLSD {
            std::string target = p["target"].asString();
            std::string msg = p["message"].asString();
            if (target.empty() || msg.empty())
                return errorResult("Target and message are required");

            LLUUID agent_id(target);
            std::string name = target;
            if (agent_id.isNull())
            {
                std::string tname = target;
                LLStringUtil::toLower(tname);
                for (LLCharacter* ch : LLCharacter::sInstances)
                {
                    LLVOAvatar* av = dynamic_cast<LLVOAvatar*>(ch);
                    if (!av || av->isSelf()) continue;
                    std::string fullname = av->getFullname();
                    std::string fname = fullname;
                    LLStringUtil::toLower(fname);
                    if (fullname == target || fname == tname)
                    {
                        agent_id = av->getID();
                        name = fullname;
                        break;
                    }
                }
                if (agent_id.isNull())
                    return errorResult(llformat("Could not resolve avatar '%s'", target.c_str()));
            }

            LLUUID session_id = gIMMgr->addSession(name, IM_NOTHING_SPECIAL, agent_id);
            if (session_id.isNull())
                return errorResult("Failed to create IM session");
            LLIMModel::sendMessage(msg, session_id, agent_id, IM_NOTHING_SPECIAL);
            return textResult("Message sent");
        });

    registerTool("chat_read",
        "Read recent local chat messages",
        LLSDMap("type", "object")(
            "properties", LLSDMap("limit", LLSDMap("type", "number")("description", "Maximum number of recent messages to return (default 50, max 100)"))
        )("required", llsd::array()),
        [](const LLSD& p) -> LLSD {
            S32 limit = p.has("limit") ? (S32)p["limit"].asInteger() : 50;
            if (limit <= 0) limit = 50;
            if (limit > 100) limit = 100;
            LLSD messages = LLMCPServer::instance().getChatMessages(limit);
            LLSD data = LLSD::emptyMap();
            data["messages"] = messages;
            data["count"] = (LLSD::Integer)messages.size();
            return textResult(toJsonString(data));
        });

    registerTool("inventory_search",
        "Search inventory folders and items by name",
        LLSDMap("type", "object")(
            "properties", LLSDMap("query", LLSDMap("type", "string")("description", "Search term (substring, case-insensitive)"))(
                "folder", LLSDMap("type", "string")("description", "Folder UUID to search within (optional, default: root)"))(
                "type", LLSDMap("type", "string")("description", "Optional type filter: texture, sound, landmark, object, notecard, animation, gesture, material, wearable, etc."))(
                "max_results", LLSDMap("type", "number")("description", "Maximum number of results (default 50)"))
        )("required", llsd::array("query")),
        [](const LLSD& p) -> LLSD {
            LLSD data = LLMCPServer::instance().inventorySearch(p);
            if (data.has("error"))
                return errorResult(data["error"].asString());
            return textResult(toJsonString(data));
        });

    registerTool("attachment_list",
        "List currently worn attachments with attach points",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
            return textResult(toJsonString(LLMCPServer::instance().collectAttachments()));
        });

    registerTool("get_parcel_info",
        "Get information about the current parcel",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
            return textResult(toJsonString(LLMCPServer::instance().collectParcelInfo()));
        });

    registerTool("get_nearby_objects",
        "List nearby objects with positions and distances",
        LLSDMap("type", "object")(
            "properties", LLSDMap("max_results", LLSDMap("type", "number")("description", "Maximum number of objects (default 50)"))(
                "include_attachments", LLSDMap("type", "boolean")("description", "Include worn attachments (default false)"))
        )("required", llsd::array()),
        [](const LLSD& p) -> LLSD {
            return textResult(toJsonString(LLMCPServer::instance().collectNearbyObjects(p)));
        });

    registerTool("get_self_info",
        "Get information about your own avatar and state",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
            return textResult(toJsonString(LLMCPServer::instance().collectSelfInfo()));
        });

    registerTool("get_friends_online",
        "List all friends with their online status",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
            return textResult(toJsonString(LLMCPServer::instance().collectFriendsOnline()));
        });

    registerTool("im_read",
        "Read recent incoming IM messages from any IM session",
        LLSDMap("type", "object")(
            "properties", LLSDMap("limit", LLSDMap("type", "number")("description", "Maximum number of recent messages to return (default 50, max 100)"))
        )("required", llsd::array()),
        [](const LLSD& p) -> LLSD {
            S32 limit = p.has("limit") ? (S32)p["limit"].asInteger() : 50;
            if (limit <= 0) limit = 50;
            if (limit > 100) limit = 100;
            LLSD data = LLSD::emptyMap();
            data["messages"] = LLMCPServer::instance().getIMMessages(limit);
            data["count"] = (LLSD::Integer)data["messages"].size();
            return textResult(toJsonString(data));
        });

    registerTool("im_sessions",
        "List all active IM sessions",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
            return textResult(toJsonString(LLMCPServer::instance().collectIMSessions()));
        });

    registerTool("chat_watch",
        "Set trigger words and read chat messages matching them (uses case-insensitive substring matching)",
        LLSDMap("type", "object")(
            "properties", LLSDMap(
                "words", LLSDMap("type", "array")("items", LLSDMap("type", "string"))("description", "Trigger words (case-insensitive). Empty array clears all triggers."))(
                "clear", LLSDMap("type", "boolean")("description", "Clear the trigger match buffer (default false)"))(
                "count", LLSDMap("type", "boolean")("description", "Set true to check how many matches are in the buffer"))
        )("required", llsd::array()),
        [](const LLSD& p) -> LLSD {
            LLMCPServer& server = LLMCPServer::instance();
            if (p.has("words"))
            {
                std::vector<std::string> words;
                for (LLSD::array_const_iterator it = p["words"].beginArray();
                     it != p["words"].endArray(); ++it)
                {
                    words.push_back(it->asString());
                }
                server.setTriggerWords(words);
                std::string joined;
                for (size_t i = 0; i < words.size(); ++i)
                {
                    if (i > 0) joined += ",";
                    joined += words[i];
                }
                gSavedSettings.setString("MCPTriggerWords", joined);
            }
            if (p.has("clear") && p["clear"].asBoolean())
            {
                server.clearTriggerBuffer();
            }
            LLSD data = server.getTriggerMatches();
            if (p.has("count") && p["count"].asBoolean())
            {
                return textResult(toJsonString(LLSDMap("count", data["count"])));
            }
            return textResult(toJsonString(data));
        });
}

LLSD LLMCPServer::handlePluginMCPCommand(const std::string& command, const LLSD& params)
{
    if (!on_main_thread() || !mRunning) return errorResult("MCP bridge is unavailable");
    if(command == "get_tools_list")
    {
        return getToolsList();
    }
    else if(command == "call_tool")
    {
        std::string name = params["name"].asString();
        LLSD args = params.has("arguments") ? params["arguments"] : LLSD::emptyMap();

        if (!on_main_thread()) return errorResult("Tool execution is restricted to the main thread");
        Context context;
        context.client = "media-plugin";
        context.session = "plugin";
        LLMCPContextScope scope(context);
        return dispatchTool(name, args);
    }
    else
    {
        LLSD err = LLSD::emptyMap();
        err["_error"] = "Unknown MCP bridge command";
        return err;
    }
}

LLSD LLMCPServer::getToolsList() const
{
    Context pluginContext; pluginContext.client = "media-plugin";
    LLMCPContextScope pluginScope(pluginContext);
    std::map<std::string, Tool> registry;
    { std::lock_guard<std::mutex> lock(mMutex); registry = mTools; }
    LLSD tools = LLSD::emptyArray();
    for(const auto& pair : registry)
    {
        const Tool& tool = pair.second;
        LLSD t = LLSD::emptyMap();
        t["name"] = tool.name;
        t["description"] = tool.description;
        if (!permitted(currentClient(), permissionForTool(tool.name))) continue;
        t["inputSchema"] = tool.input_schema;
        t["outputSchema"] = LLSDMap("type", "object")("properties", LLSDMap("data", LLSDMap("type", "object")))("required", llsd::array("data"));
        t["annotations"] = LLSDMap("readOnlyHint", (permissionForTool(tool.name) == Read || tool.name == "operation_status" || tool.name == "chat_read" || tool.name == "im_read" || tool.name == "im_sessions"))
            ("idempotentHint", tool.name == "avatar_sit" || tool.name == "avatar_stand" || tool.name == "avatar_fly");
        tools.append(t);
    }
    return tools;
}

LLSD LLMCPServer::handleInitialize(const LLSD& params)
{
    LLSD caps = LLSD::emptyMap();
    LLSD tool_cap = LLSD::emptyMap();
    tool_cap["listChanged"] = false;
    caps["tools"] = tool_cap;
    caps["logging"] = LLSD::emptyMap();

    LLSD res_cap = LLSD::emptyMap();
    res_cap["subscribe"] = false;
    res_cap["listChanged"] = false;
    caps["resources"] = res_cap;

    LLSD server_info = LLSD::emptyMap();
    server_info["name"] = "MikoStorm";
    server_info["version"] = LLVersionInfo::instance().getVersion();

    LLSD result = LLSD::emptyMap();
    result["protocolVersion"] = gMCPContext ? protocolForSession(gMCPContext->session) : "2025-11-25";
    result["capabilities"] = caps;
    result["serverInfo"] = server_info;

    return makeResult(result);
}

LLSD LLMCPServer::handlePing(const LLSD&)
{
    return makeResult(LLSD::emptyMap());
}

LLSD LLMCPServer::handleToolsList(const LLSD&)
{
    std::map<std::string, Tool> registry;
    { std::lock_guard<std::mutex> lock(mMutex); registry = mTools; }
    LLSD tools = LLSD::emptyArray();
    for (const auto& pair : registry)
    {
        const Tool& tool = pair.second;
        LLSD t = LLSD::emptyMap();
        t["name"] = tool.name;
        t["description"] = tool.description;
        if (!permitted(currentClient(), permissionForTool(tool.name))) continue;
        t["inputSchema"] = tool.input_schema;
        t["outputSchema"] = LLSDMap("type", "object")("properties", LLSDMap("data", LLSDMap("type", "object")))("required", llsd::array("data"));
        t["annotations"] = LLSDMap("readOnlyHint", (permissionForTool(tool.name) == Read || tool.name == "operation_status" || tool.name == "chat_read" || tool.name == "im_read" || tool.name == "im_sessions"))
            ("idempotentHint", tool.name == "avatar_sit" || tool.name == "avatar_stand" || tool.name == "avatar_fly");
        tools.append(t);
    }
    LLSD result = LLSD::emptyMap();
    result["tools"] = tools;
    return makeResult(result);
}

LLSD LLMCPServer::handleToolsCall(const LLSD& params)
{
    if (!params["name"].isString() || (params.has("arguments") && !params["arguments"].isMap()))
        return makeError(-32602, "Tool name must be a string and arguments an object");
    const auto name = params["name"].asString();
    { std::lock_guard<std::mutex> lock(mMutex); if (!mTools.count(name)) return makeError(-32602, "Unknown tool"); }
    const LLSD args = params.has("arguments") ? params["arguments"] : LLSD::emptyMap();
    LLSD value = runOnMain([this, name, args]() { return dispatchTool(name, args); });
    if (value.has("jsonrpc")) return value;
    return makeResult(value);
}

LLSD LLMCPServer::runOnMain(std::function<LLSD()> fn)
{
    // Leave HTTP worker capacity for cancellation and lifecycle requests.
    const auto capacity = mMainRequests;
    if (capacity->fetch_add(1) >= 6)
    { capacity->fetch_sub(1); return makeError(-32000, "Main-thread request capacity reached; retry later"); }
    struct Pending { std::shared_ptr<std::atomic<U32>> count; ~Pending() { count->fetch_sub(1); } } pending{capacity};
    auto mainloop = LL::WorkQueue::getInstance("mainloop");
    if (!mainloop)
        return makeError(-32000, "Main loop work queue not available");

    const Context context = gMCPContext ? *gMCPContext : Context{};
    const auto lifetime = context.client == "media-plugin" && context.active ? context.active : mSession;
    auto task = std::make_shared<LLMCPCancellableTask<LLSD>>(lifetime, context.active);
    try
    {
        mainloop->post([task, context, fn = std::move(fn)]() { LLMCPContextScope scope(context); task->execute(fn); });
    }
    catch (...)
    {
        return makeError(-32000, "Failed to post to main thread");
    }
    if (!task->wait(std::chrono::seconds(10)))
        return makeError(-32000, "Request cancelled or main thread timed out");
    try { return task->get(); }
    catch (const std::exception& e) { return makeError(-32603, e.what()); }
    catch (...) { return makeError(-32603, "Main thread handler failed"); }
}

LLSD LLMCPServer::collectNearbyAgents() const
{
    LLSD agents = LLSD::emptyArray();
    LLVector3 myPos = gAgent.getPositionAgent();
    for (LLCharacter* ch : LLCharacter::sInstances)
    {
        LLVOAvatar* av = dynamic_cast<LLVOAvatar*>(ch);
        if (!av || av->isSelf()) continue;
        LLSD entry = LLSD::emptyMap();
        entry["name"] = av->getFullname();
        LLVector3 avPos = av->getPositionAgent();
        entry["x"] = avPos.mV[VX];
        entry["y"] = avPos.mV[VY];
        entry["z"] = avPos.mV[VZ];
        entry["distance"] = dist_vec(myPos, avPos);
        agents.append(entry);
        if (agents.size() >= 200) break;
    }
    LLSD data = LLSD::emptyMap();
    data["agents"] = agents;
    data["count"] = (LLSD::Integer)agents.size();
    return data;
}

LLSD LLMCPServer::collectAttachments() const
{
    LLSD attachments = LLSD::emptyArray();
    if (gAgentAvatarp)
    {
        for (const auto& pair : gAgentAvatarp->mAttachmentPoints)
        {
            LLViewerJointAttachment* joint = pair.second;
            if (!joint || joint->mAttachedObjects.empty()) continue;
            for (const auto& objectp : joint->mAttachedObjects)
            {
                if (!objectp) continue;
                LLSD entry = LLSD::emptyMap();
                entry["name"] = objectp->getAttachmentItemName();
                entry["item_id"] = objectp->getAttachmentItemID().asString();
                entry["object_id"] = objectp->getID().asString();
                entry["attach_point"] = joint->getName();
                LLVector3 pos = objectp->getPositionAgent();
                entry["x"] = pos.mV[VX];
                entry["y"] = pos.mV[VY];
                entry["z"] = pos.mV[VZ];
                attachments.append(entry);
            }
        }
    }
    LLSD data = LLSD::emptyMap();
    data["attachments"] = attachments;
    data["count"] = (LLSD::Integer)attachments.size();
    return data;
}

LLSD LLMCPServer::collectParcelInfo() const
{
    LLSD data = LLSD::emptyMap();
    LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
    if (!parcel)
    {
        data["error"] = "Not in a parcel";
        return data;
    }
    data["name"] = parcel->getName();
    data["description"] = parcel->getDesc();
    data["owner_id"] = parcel->getOwnerID().asString();
    data["area"] = parcel->getArea();
    data["flags"] = (LLSD::Integer)parcel->getParcelFlags();
    data["region"] = gAgent.getRegion() ? gAgent.getRegion()->getName() : "";
    return data;
}

LLSD LLMCPServer::collectSelfInfo() const
{
    LLSD data = LLSD::emptyMap();
    data["name"] = gAgentAvatarp ? gAgentAvatarp->getFullname() : std::string("");
    data["agent_id"] = gAgent.getID().asString();
    data["group"] = gAgent.getGroupName();
    data["region"] = gAgent.getRegion() ? gAgent.getRegion()->getName() : "";
    data["sitting"] = gAgent.isSitting();
    data["flying"] = gAgent.getFlying();
    LLVector3 pos = gAgent.getPositionAgent();
    data["x"] = pos.mV[VX];
    data["y"] = pos.mV[VY];
    data["z"] = pos.mV[VZ];
    return data;
}

LLSD LLMCPServer::collectFriendsOnline() const
{
    LLAvatarTracker& tracker = LLAvatarTracker::instance();
    LLAvatarTracker::buddy_map_t buddies;
    tracker.copyBuddyList(buddies);

    LLSD friends = LLSD::emptyArray();
    S32 online_count = 0;

    for (const auto& pair : buddies)
    {
        const LLUUID& buddy_id = pair.first;
        LLRelationship* rel = pair.second;
        if (!rel) continue;

        LLAvatarName av_name;
        LLAvatarNameCache::get(buddy_id, &av_name);

        LLSD entry = LLSD::emptyMap();
        entry["name"] = av_name.getDisplayName();
        entry["username"] = av_name.getUserName();
        entry["uuid"] = buddy_id.asString();
        entry["online"] = rel->isOnline();
        entry["can_see_online"] = rel->isRightGrantedFrom(LLRelationship::GRANT_ONLINE_STATUS);
        entry["can_see_map"] = rel->isRightGrantedFrom(LLRelationship::GRANT_MAP_LOCATION);
        entry["can_modify_objects"] = rel->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS);
        friends.append(entry);
        if (rel->isOnline()) online_count++;
        if (friends.size() >= 200) break;
    }

    LLSD data = LLSD::emptyMap();
    data["friends"] = friends;
    data["count"] = (LLSD::Integer)friends.size();
    data["online_count"] = online_count;
    data["truncated"] = friends.size() < static_cast<S32>(buddies.size());
    return data;
}

LLSD LLMCPServer::collectIMSessions() const
{
    LLSD sessions = LLSD::emptyArray();
    const auto& session_map = LLIMModel::getInstance()->mId2SessionMap;
    for (const auto& pair : session_map)
    {
        LLIMModel::LLIMSession* session = pair.second;
        if (!session) continue;
        LLSD entry = LLSD::emptyMap();
        entry["session_id"] = pair.first.asString();
        entry["name"] = session->mName;
        entry["type"] = (LLSD::Integer)session->mType;
        entry["session_type"] = (LLSD::Integer)session->mSessionType;
        entry["other_participant_id"] = session->mOtherParticipantID.asString();
        entry["num_unread"] = session->mNumUnread;
        entry["num_messages"] = (LLSD::Integer)session->mMsgs.size();
        sessions.append(entry);
        if (sessions.size() >= 200) break;
    }
    LLSD data = LLSD::emptyMap();
    data["truncated"] = sessions.size() < static_cast<S32>(session_map.size());
    data["sessions"] = sessions;
    data["count"] = (LLSD::Integer)sessions.size();
    return data;
}

LLSD LLMCPServer::collectNearbyObjects(const LLSD& params) const
{
    S32 max_results = params.has("max_results") ? (S32)params["max_results"].asInteger() : 50;
    bool include_attachments = params.has("include_attachments") ? params["include_attachments"].asBoolean() : false;
    if (max_results <= 0) max_results = 50;

    LLSD objects = LLSD::emptyArray();
    LLVector3 myPos = gAgent.getPositionAgent();
    S32 index = params.has("offset") ? params["offset"].asInteger() : 0;
    const S32 scanEnd = llmin(gObjectList.getNumObjects(), index + 2048);
    for (; index < scanEnd; ++index)
    {
        if ((S32)objects.size() >= max_results) break;
        const S32 i = index;
        LLViewerObject* obj = gObjectList.getObject(i);
        if (!obj) continue;
        if (obj->isAttachment() && !include_attachments) continue;
        if (obj->getID() == gAgent.getID()) continue;
        LLPCode pcode = obj->getPCode();
        if (pcode == LLViewerObject::LL_VO_SURFACE_PATCH ||
            pcode == LLViewerObject::LL_VO_PART_GROUP ||
            pcode == LLViewerObject::LL_VO_WATER ||
            pcode == LLViewerObject::LL_VO_VOID_WATER ||
            pcode == LLViewerObject::LL_VO_WL_SKY ||
            pcode == LLViewerObject::LL_VO_SKY ||
            pcode == LLViewerObject::LL_VO_CLOUDS)
        {
            continue;
        }
        LLSD entry = LLSD::emptyMap();
        LLNameValue* name_nv = obj->getNVPair("Name");
        entry["name"] = (name_nv && name_nv->getString()) ? name_nv->getString() : "";
        entry["object_id"] = obj->getID().asString();
        entry["type"] = LLPrimitive::pCodeToString(pcode);
        LLVector3 pos = obj->getPositionAgent();
        entry["x"] = pos.mV[VX];
        entry["y"] = pos.mV[VY];
        entry["z"] = pos.mV[VZ];
        entry["distance"] = dist_vec(myPos, pos);
        objects.append(entry);
    }
    LLSD data = LLSD::emptyMap();
    data["next_offset"] = index < gObjectList.getNumObjects() ? LLSD(index) : LLSD();
    data["objects"] = objects;
    data["count"] = (LLSD::Integer)objects.size();
    return data;
}

LLSD LLMCPServer::inventorySearch(const LLSD& params)
{
    const auto now = Clock::now();
    for (auto it = mSearchCursors.begin(); it != mSearchCursors.end(); )
        if (now - it->second.touched > std::chrono::minutes(2)) it = mSearchCursors.erase(it); else ++it;
    std::string query = params["query"].asString(); LLStringUtil::toLower(query);
    const std::string type = params["type"].asString();
    const auto filter = type.empty() ? LLInventoryType::IT_NONE : LLInventoryType::lookup(type);
    if (!type.empty() && filter == LLInventoryType::IT_NONE) return LLSDMap("error", "Unknown inventory type");
    const LLUUID root = params.has("folder") ? LLUUID(params["folder"].asString()) : gInventory.getRootFolderID();
    std::string cursorID = params["cursor"].asString();
    if (cursorID.empty())
    {
        if (!gInventory.getCategory(root)) return LLSDMap("error", "Inventory folder is not cached");
        if (mSearchCursors.size() >= 32) return LLSDMap("error", "Too many inventory searches; finish an existing cursor or wait two minutes");
        LLUUID uuid; uuid.generate(); cursorID = uuid.asString();
        SearchCursor cursor;
        cursor.client = currentClient(); cursor.session = gMCPContext ? gMCPContext->session : "";
        cursor.query = query; cursor.type = type; cursor.root = root;
        cursor.stack.push_back({root, "", 0, 0}); cursor.visited.insert(root);
        mSearchCursors.emplace(cursorID, std::move(cursor));
    }
    auto it = mSearchCursors.find(cursorID);
    if (it == mSearchCursors.end() || it->second.client != currentClient() || !gMCPContext ||
        it->second.session != gMCPContext->session || it->second.query != query || it->second.type != type || it->second.root != root)
        return LLSDMap("error", "Unknown, expired or incompatible inventory cursor");
    auto& cursor = it->second; cursor.touched = now;
    const S32 limit = params.has("max_results") ? params["max_results"].asInteger() : 50;
    LLSD results = LLSD::emptyArray();
    S32 inspected = 0;
    // A continuation retains folder/item positions: an empty page is still
    // resumable and does not repeatedly scan the beginning of a large inventory.
    while (!cursor.stack.empty() && inspected < 2048 && results.size() < limit)
    {
        auto& frame = cursor.stack.back();
        LLInventoryModel::cat_array_t* cats = nullptr;
        LLInventoryModel::item_array_t* items = nullptr;
        gInventory.getDirectDescendentsOf(frame.folder, cats, items);
        if (cats && frame.cat < cats->size())
        {
            const auto category = (*cats)[frame.cat++]; ++inspected;
            const auto name = category->getName(); auto lower = name; LLStringUtil::toLower(lower);
            const auto path = frame.path;
            if (type.empty() && lower.find(query) != std::string::npos)
                results.append(LLSDMap("name", name)("uuid", category->getUUID().asString())("type", "folder")("folder", path));
            if (cursor.stack.size() < 64 && cursor.visited.size() < 20000 && cursor.visited.insert(category->getUUID()).second)
                cursor.stack.push_back({category->getUUID(), path.empty() ? name : path + "/" + name, 0, 0});
            else if (cursor.stack.size() >= 64 || cursor.visited.size() >= 20000)
            { mSearchCursors.erase(it); return LLSDMap("results", results)("truncated", true)("complete", false)("message", "Inventory depth/folder safety limit reached"); }
        }
        else if (items && frame.item < items->size())
        {
            const auto item = (*items)[frame.item++]; ++inspected;
            auto lower = item->getName(); LLStringUtil::toLower(lower);
            if (lower.find(query) != std::string::npos && (type.empty() || item->getInventoryType() == filter))
                results.append(LLSDMap("name", item->getName())("uuid", item->getUUID().asString())
                    ("type", LLInventoryType::lookup(item->getInventoryType()))("folder", frame.path));
        }
        else { cursor.stack.pop_back(); ++inspected; }
    }
    const bool complete = cursor.stack.empty();
    LLSD data = LLSDMap("results", results)("count", results.size())("inspected", inspected)("complete", complete)
        ("cursor", complete ? "" : cursorID)("cached_only", true);
    if (complete) mSearchCursors.erase(it);
    return data;
}

LLSD LLMCPServer::handleResourcesList(const LLSD&)
{
    LLSD resources = LLSD::emptyArray();

    LLSD r1 = LLSD::emptyMap();
    r1["uri"] = "mikostorm://position";
    r1["name"] = "Current Position";
    r1["description"] = "Current avatar position and region";
    r1["mimeType"] = "application/json";
    resources.append(r1);

    LLSD r2 = LLSD::emptyMap();
    r2["uri"] = "mikostorm://region";
    r2["name"] = "Region Info";
    r2["description"] = "Current region information";
    r2["mimeType"] = "application/json";
    resources.append(r2);

    LLSD r3 = LLSD::emptyMap();
    r3["uri"] = "mikostorm://nearby";
    r3["name"] = "Nearby Agents";
    r3["description"] = "List of nearby avatars";
    r3["mimeType"] = "application/json";
    resources.append(r3);

    LLSD r4 = LLSD::emptyMap();
    r4["uri"] = "mikostorm://attachments";
    r4["name"] = "Worn Attachments";
    r4["description"] = "Currently worn attachments";
    r4["mimeType"] = "application/json";
    resources.append(r4);

    LLSD r5 = LLSD::emptyMap();
    r5["uri"] = "mikostorm://parcel";
    r5["name"] = "Parcel Info";
    r5["description"] = "Current parcel information";
    r5["mimeType"] = "application/json";
    resources.append(r5);

    LLSD r6 = LLSD::emptyMap();
    r6["uri"] = "mikostorm://inventory";
    r6["name"] = "Root Inventory";
    r6["description"] = "Root-level inventory folders and items";
    r6["mimeType"] = "application/json";
    resources.append(r6);

    LLSD r7 = LLSD::emptyMap();
    r7["uri"] = "mikostorm://chat";
    r7["name"] = "Chat History";
    r7["description"] = "Recent local chat messages";
    r7["mimeType"] = "application/json";
    resources.append(r7);

    LLSD r8 = LLSD::emptyMap();
    r8["uri"] = "mikostorm://friends";
    r8["name"] = "Friends Online";
    r8["description"] = "Friends list with online status";
    r8["mimeType"] = "application/json";
    resources.append(r8);

    LLSD r9 = LLSD::emptyMap();
    r9["uri"] = "mikostorm://im";
    r9["name"] = "IM Messages";
    r9["description"] = "Recent incoming IM messages";
    r9["mimeType"] = "application/json";
    resources.append(r9);

    LLSD result = LLSD::emptyMap();
    LLSD visible = LLSD::emptyArray();
    for (const auto& resource : llsd::inArray(resources))
    {
        const auto uri = resource["uri"].asString();
        if (permitted(currentClient(), uri == "mikostorm://chat" || uri == "mikostorm://im" ? Chat : Read)) visible.append(resource);
    }
    result["resources"] = visible;
    return makeResult(result);
}

LLSD LLMCPServer::handleResourcesRead(const LLSD& params)
{
    if (!params["uri"].isString()) return makeError(-32602, "Resource URI must be a string");
    std::string uri = params["uri"].asString();
    const U32 group = uri == "mikostorm://chat" || uri == "mikostorm://im" ? Chat : Read;

    std::function<LLSD()> loader;
    if (uri == "mikostorm://position")
    {
        loader = []() {
            LLSD content_data = LLSD::emptyMap();
            content_data["region"] = gAgent.getRegion() ? gAgent.getRegion()->getName() : "";
            LLVector3 pos = gAgent.getPositionAgent();
            content_data["x"] = pos.mV[VX];
            content_data["y"] = pos.mV[VY];
            content_data["z"] = pos.mV[VZ];
            return content_data;
        };
    }
    else if (uri == "mikostorm://region")
    {
        loader = []() {
            LLViewerRegion* regionp = gAgent.getRegion();
            LLSD content_data = LLSD::emptyMap();
            if (!regionp)
            {
                content_data["error"] = "Not connected to a region";
                return content_data;
            }
            content_data["name"] = regionp->getName();
            content_data["handle"] = std::to_string(regionp->getHandle());
            return content_data;
        };
    }
    else if (uri == "mikostorm://nearby")
    {
        loader = [this]() { return collectNearbyAgents(); };
    }
    else if (uri == "mikostorm://attachments")
    {
        loader = [this]() { return collectAttachments(); };
    }
    else if (uri == "mikostorm://parcel")
    {
        loader = [this]() { return collectParcelInfo(); };
    }
    else if (uri == "mikostorm://inventory")
    {
        loader = [this]() {
            const auto response = dispatchTool("inventory_list", LLSD::emptyMap());
            return response["structuredContent"]["data"];
        };
    }

    else if (uri == "mikostorm://chat")
    {
        loader = [this]() {
            LLSD data = LLSD::emptyMap();
            data["messages"] = getChatMessages(100);
            data["count"] = (LLSD::Integer)kChatBufferSize;
            return data;
        };
    }
    else if (uri == "mikostorm://friends")
    {
        loader = [this]() { return collectFriendsOnline(); };
    }
    else if (uri == "mikostorm://im")
    {
        loader = [this]() {
            LLSD data = LLSD::emptyMap();
            data["messages"] = getIMMessages(100);
            data["count"] = (LLSD::Integer)data["messages"].size();
            return data;
        };
    }
    else
    {
        return makeError(-32602, llformat("Resource '%s' not found", uri.c_str()));
    }

    LLSD result = runOnMain([this, loader, group]() {
        if (!mRunning || !permitted(currentClient(), group)) return makeError(-32001, "Resource permission denied");
        return loader();
    });
    if (result.isMap() && result.has("jsonrpc"))
    {
        return result;
    }

    LLSD content_data = result;

    LLSD contents_entry = LLSD::emptyMap();
    contents_entry["uri"] = uri;
    contents_entry["mimeType"] = "application/json";
    contents_entry["text"] = toJsonString(content_data);

    LLSD contents_arr = LLSD::emptyArray();
    contents_arr.append(contents_entry);

    LLSD out = LLSD::emptyMap();
    out["contents"] = contents_arr;
    return makeResult(out);
}

LLSD LLMCPServer::handleSetLoggerLevel(const LLSD& params)
{
    const std::string level = params["level"].asString();
    static const std::set<std::string> levels = {"debug", "info", "notice", "warning", "error", "critical", "alert", "emergency"};
    if (!params["level"].isString() || !levels.count(level)) return makeError(-32602, "Unsupported MCP logging level");
    std::lock_guard<std::mutex> lock(mMutex);
    auto session = gMCPContext ? mSessions.find(gMCPContext->session) : mSessions.end();
    if (session == mSessions.end()) return makeError(-32001, "Session closed");
    session->second.logLevel = level;
    // JSON-only transport currently emits no unsolicited log notifications.
    return makeResult(LLSD::emptyMap());
}

LLSD LLMCPServer::makeError(int code, const std::string& message, const LLSD& data)
{
    LLSD err = LLSD::emptyMap();
    err["code"] = code;
    err["message"] = message;
    if (data.isDefined())
        err["data"] = data;
    return LLSDMap("jsonrpc", "2.0")("error", err);
}

LLSD LLMCPServer::makeResult(const LLSD& result)
{
    return LLSDMap("jsonrpc", "2.0")("result", result);
}
