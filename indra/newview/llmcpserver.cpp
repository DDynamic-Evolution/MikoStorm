#include "llmcpserver.h"
#include "llmcphttp.h"
#include "llviewercontrol.h"
#include "llsdjson.h"
#include "llchat.h"
#include "llagent.h"
#include "llagentpilot.h"
#include "llviewerregion.h"
#include "llworld.h"
#include "llvoavatar.h"
#include "llavatarappearance.h"
#include "llinventorymodel.h"
#include "llviewerinventory.h"
#include "llappviewer.h"
#include "workqueue.h"
#include "fsnearbychathub.h"

// create_new_item is defined in llviewerinventory.cpp
void create_new_item(const std::string& name,
                     const LLUUID& parent_id,
                     LLAssetType::EType asset_type,
                     LLInventoryType::EType inv_type,
                     U32 next_owner_perm,
                     std::function<void(const LLUUID&)> created_cb = nullptr);
#include "llviewerassetupload.h"
#include <future>
#include <boost/json.hpp>

static std::string toJsonString(const LLSD& data)
{
    return boost::json::serialize(LlsdToJson(data));
}

LLMCPServer::LLMCPServer()
    : mRunning(false)
    , mPort(13231)
    , mInitialized(false)
{
}

LLMCPServer::~LLMCPServer()
{
    stop();
}

void LLMCPServer::start()
{
    if (mRunning) return;

    mPort = (U16)gSavedSettings.getU32("MCPPort");
    mAuthToken = gSavedSettings.getString("MCPAuthToken");
    mInitialized = false;

    registerDefaultTools();

    mRunning = true;
    LLMCPHttpServer::start(mPort, mAuthToken);
    LL_INFOS("MCP") << "MCP Server started on port " << mPort << LL_ENDL;
}

void LLMCPServer::stop()
{
    if (!mRunning) return;
    mRunning = false;
    LLMCPHttpServer::stop();
    LL_INFOS("MCP") << "MCP Server stopped" << LL_ENDL;
}

void LLMCPServer::registerTool(const std::string& name,
                               const std::string& description,
                               const LLSD& input_schema,
                               ToolHandler handler)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mTools[name] = { name, description, input_schema, std::move(handler) };
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
        "Sit down on the ground or nearby object",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
            if (gAgent.isSitting())
            {
                gAgent.standUp();
                return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", "Standing up")));
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
            LLVector3 local((F32)p["x"].asReal(), (F32)p["y"].asReal(), (F32)p["z"].asReal());
            if (gAgent.getRegion())
            {
                LLVector3d global = gAgent.getRegion()->getOriginGlobal() + LLVector3d(local);
                gAgent.startAutoPilotGlobal(global);
            }
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text",
                llformat("Walking to (%.1f, %.1f, %.1f)", p["x"].asReal(), p["y"].asReal(), p["z"].asReal()))));
        });

    registerTool("avatar_teleport",
        "Teleport to a location in any region",
        LLSDMap("type", "object")(
            "properties", LLSDMap("region", LLSDMap("type", "string")("description", "Region name"))(
                "x", LLSDMap("type", "number")("description", "X coordinate"))(
                "y", LLSDMap("type", "number")("description", "Y coordinate"))(
                "z", LLSDMap("type", "number")("description", "Z coordinate"))
        )("required", llsd::array("region", "x", "y", "z")),
        [](const LLSD& p) -> LLSD {
            LLVector3 local((F32)p["x"].asReal(), (F32)p["y"].asReal(), (F32)p["z"].asReal());
            std::string region_name = p["region"].asString();
            LLViewerRegion* region = nullptr;
            for (const auto& r : LLWorld::getInstance()->getRegionList())
            {
                if (r->getName() == region_name)
                {
                    region = r;
                    break;
                }
            }
            if (region)
            {
                gAgent.teleportViaLocation(region->getPosGlobalFromRegion(local));
            }
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text",
                llformat("Teleporting to %s at (%.0f, %.0f, %.0f)", region_name.c_str(), p["x"].asReal(), p["y"].asReal(), p["z"].asReal()))));
        });

    registerTool("avatar_fly",
        "Enable or disable flying",
        LLSDMap("type", "object")(
            "properties", LLSDMap("enabled", LLSDMap("type", "boolean")("description", "Whether to fly"))
        )("required", llsd::array("enabled")),
        [](const LLSD& p) -> LLSD {
            gAgent.setFlying(p["enabled"].asBoolean());
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
            data["handle"] = (LLSD::Integer)regionp->getHandle();
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", toJsonString(data))));
        });

    registerTool("get_nearby_agents",
        "List nearby avatars with positions and distances",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
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
            }
            LLSD data = LLSD::emptyMap();
            data["agents"] = agents;
            data["count"] = (LLSD::Integer)agents.size();
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", toJsonString(data))));
        });

    registerTool("notecard_write",
        "Create a new notecard in your inventory with the given name and text content",
        LLSDMap("type", "object")(
            "properties", LLSDMap("name", LLSDMap("type", "string")("description", "Notecard title"))(
                "content", LLSDMap("type", "string")("description", "Notecard body text"))
        )("required", llsd::array("name", "content")),
        [](const LLSD& p) -> LLSD {
            std::string name = p["name"].asString();
            std::string content = p["content"].asString();
            if (name.empty() || content.empty())
                return LLSDMap("isError", true)("content", llsd::array(LLSDMap("type", "text")("text", "Name and content are required")));
            LLViewerRegion* region = gAgent.getRegion();
            if (!region)
                return LLSDMap("isError", true)("content", llsd::array(LLSDMap("type", "text")("text", "Not connected to a region")));
            std::string agent_url = region->getCapability("UpdateNotecardAgentInventory");
            if (agent_url.empty())
                return LLSDMap("isError", true)("content", llsd::array(LLSDMap("type", "text")("text", "Region does not support notecard updates")));
            create_new_item(name,
                gInventory.findCategoryUUIDForType(LLFolderType::FT_NOTECARD),
                LLAssetType::AT_NOTECARD,
                LLInventoryType::IT_NOTECARD,
                PERM_ALL,
                [name, content, agent_url](const LLUUID& item_id)
                {
                    if (item_id.isNull()) return;
                    auto uploadInfo = std::make_shared<LLBufferedAssetUploadInfo>(
                        item_id, LLAssetType::AT_NOTECARD, content,
                        [name](LLUUID itemId, LLUUID newAssetId, LLUUID newItemId, LLSD)
                        {
                            LLUUID targetId = newItemId.notNull() ? newItemId : itemId;
                            LLViewerInventoryItem* item = gInventory.getItem(targetId);
                            if (item)
                            {
                                item->setAssetUUID(newAssetId);
                                gInventory.updateItem(item);
                                gInventory.notifyObservers();
                            }
                            LL_INFOS("MCP") << "Notecard '" << name << "' saved (asset=" << newAssetId << ")" << LL_ENDL;
                        },
                        [name](LLUUID, LLUUID, LLSD, std::string reason) -> bool {
                            LL_WARNS("MCP") << "Failed to save notecard '" << name << "': " << reason << LL_ENDL;
                            return false;
                        });
                    LLViewerAssetUpload::EnqueueInventoryUpload(agent_url, uploadInfo);
                });
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text",
                llformat("Creating notecard '%s'...", name.c_str()))));
        });

    registerTool("inventory_list",
        "List inventory folders and items at the root level",
        LLSDMap("type", "object")("properties", LLSD::emptyMap())("required", llsd::array()),
        [](const LLSD&) -> LLSD {
            LLSD items = LLSD::emptyArray();
            LLInventoryModel::cat_array_t* cats = nullptr;
            LLInventoryModel::item_array_t* items_arr = nullptr;
            gInventory.getDirectDescendentsOf(LLUUID::null, cats, items_arr);
            if (cats)
            {
                for (size_t i = 0; i < cats->size(); ++i)
                {
                    LLSD entry = LLSD::emptyMap();
                    entry["name"] = (*cats)[i]->getName();
                    entry["uuid"] = (*cats)[i]->getUUID().asString();
                    entry["type"] = "folder";
                    items.append(entry);
                }
            }
            if (items_arr)
            {
                for (size_t i = 0; i < items_arr->size(); ++i)
                {
                    LLSD entry = LLSD::emptyMap();
                    entry["name"] = (*items_arr)[i]->getName();
                    entry["uuid"] = (*items_arr)[i]->getUUID().asString();
                    entry["type"] = "item";
                    items.append(entry);
                }
            }
            return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", toJsonString(items))));
        });
}

LLSD LLMCPServer::handleRequest(const LLSD& request)
{
    if (!request.isMap() || !request.has("method"))
    {
        return makeError(-32600, "Invalid Request: missing 'method'");
    }

    std::string method = request["method"].asString();
    LLSD params = request.has("params") ? request["params"] : LLSD::emptyMap();
    LLSD id = request.has("id") ? request["id"] : LLSD();

    LLSD result;
    bool needs_id = id.isDefined() && !id.isUndefined();

    if (method == "initialize")
        result = handleInitialize(params);
    else if (method == "ping")
        result = handlePing(params);
    else if (method == "tools/list")
        result = handleToolsList(params);
    else if (method == "tools/call")
        result = handleToolsCall(params);
    else if (method == "resources/list")
        result = handleResourcesList(params);
    else if (method == "resources/read")
        result = handleResourcesRead(params);
    else if (method == "logging/setLevel")
        result = handleSetLoggerLevel(params);
    else if (method == "notifications/initialized")
    {
        mInitialized = true;
        return LLSD();
    }
    else if (method == "notifications/roots/list_changed")
    {
        return LLSD();
    }
    else
        result = makeError(-32601, llformat("Method '%s' not found", method.c_str()));

    if (needs_id)
    {
        LLSD response = result;
        response["id"] = id;
        return response;
    }
    return LLSD();
}

LLSD LLMCPServer::handleInitialize(const LLSD& params)
{
    mInitialized = true;

    LLSD caps = LLSD::emptyMap();
    LLSD tool_cap = LLSD::emptyMap();
    tool_cap["listChanged"] = false;
    caps["tools"] = tool_cap;

    LLSD res_cap = LLSD::emptyMap();
    res_cap["subscribe"] = false;
    res_cap["listChanged"] = false;
    caps["resources"] = res_cap;

    LLSD server_info = LLSD::emptyMap();
    server_info["name"] = "MikoStorm";
    server_info["version"] = "1.0.0";

    LLSD result = LLSD::emptyMap();
    result["protocolVersion"] = "2025-11-25";
    result["capabilities"] = caps;
    result["serverInfo"] = server_info;

    return makeResult(result);
}

LLSD LLMCPServer::handlePing(const LLSD&)
{
    return makeResult(LLSDMap("pong", true));
}

LLSD LLMCPServer::handleToolsList(const LLSD&)
{
    std::lock_guard<std::mutex> lock(mMutex);
    LLSD tools = LLSD::emptyArray();
    for (const auto& pair : mTools)
    {
        const Tool& tool = pair.second;
        LLSD t = LLSD::emptyMap();
        t["name"] = tool.name;
        t["description"] = tool.description;
        t["inputSchema"] = tool.input_schema;
        tools.append(t);
    }
    LLSD result = LLSD::emptyMap();
    result["tools"] = tools;
    return makeResult(result);
}

LLSD LLMCPServer::handleToolsCall(const LLSD& params)
{
    std::string name = params["name"].asString();
    LLSD args = params.has("arguments") ? params["arguments"] : LLSD::emptyMap();

    ToolHandler handler;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto it = mTools.find(name);
        if (it == mTools.end())
            return makeError(-32602, llformat("Tool '%s' not found", name.c_str()));
        handler = it->second.handler;
    }

    std::promise<LLSD> promise;
    auto future = promise.get_future();

    auto mainloop = LL::WorkQueue::getInstance("mainloop");
    if (!mainloop)
        return makeError(-32000, "Main loop work queue not available");

    try
    {
        mainloop->post([&promise, handler, args]()
        {
            LLSD result = handler(args);
            promise.set_value(result);
        });
    }
    catch (...)
    {
        return makeError(-32000, "Failed to post tool call to main thread");
    }

    return future.get();
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

    LLSD result = LLSD::emptyMap();
    result["resources"] = resources;
    return makeResult(result);
}

LLSD LLMCPServer::handleResourcesRead(const LLSD& params)
{
    std::string uri = params["uri"].asString();
    LLSD content_data;

    if (uri == "mikostorm://position")
    {
        content_data = LLSD::emptyMap();
        content_data["region"] = gAgent.getRegion() ? gAgent.getRegion()->getName() : "";
        LLVector3 pos = gAgent.getPositionAgent();
        content_data["x"] = pos.mV[VX];
        content_data["y"] = pos.mV[VY];
        content_data["z"] = pos.mV[VZ];
    }
    else if (uri == "mikostorm://region")
    {
        LLViewerRegion* regionp = gAgent.getRegion();
        if (!regionp)
            return makeError(-32000, "Not connected to a region");
        content_data = LLSD::emptyMap();
        content_data["name"] = regionp->getName();
        content_data["handle"] = (LLSD::Integer)regionp->getHandle();
    }
    else if (uri == "mikostorm://nearby")
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
        }
        content_data = LLSD::emptyMap();
        content_data["agents"] = agents;
    }
    else
    {
        return makeError(-32602, llformat("Resource '%s' not found", uri.c_str()));
    }

    LLSD contents_entry = LLSD::emptyMap();
    contents_entry["uri"] = uri;
    contents_entry["mimeType"] = "application/json";
    contents_entry["text"] = toJsonString(content_data);

    LLSD contents_arr = LLSD::emptyArray();
    contents_arr.append(contents_entry);

    LLSD result = LLSD::emptyMap();
    result["contents"] = contents_arr;
    return makeResult(result);
}

LLSD LLMCPServer::handleSetLoggerLevel(const LLSD& params)
{
    std::string level = params["level"].asString();
    LL_INFOS("MCP") << "Set logger level to: " << level << LL_ENDL;
    return makeResult(LLSDMap("level", level));
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
