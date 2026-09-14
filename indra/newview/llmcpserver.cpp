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

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        LL_WARNS("MCP") << "WSAStartup failed" << LL_ENDL;
        return;
    }
#endif

    mPort = (U16)gSavedSettings.getU32("MCPPort");
    mAuthToken = gSavedSettings.getString("MCPAuthToken");
    mInitialized = false;

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

    // Let media plugins consume MCP tools (e.g. CEF/VLC/GStreamer). The bridge
    // must run on the main thread so it can safely invoke the exposed tools.
    LLPluginClassMedia::setMCPBridge(
        [this](const std::string& command, const LLSD& params) -> LLSD
        {
            return LLMainThreadTask::dispatch([this, command, params]()
            {
                return handlePluginMCPCommand(command, params);
            });
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
    LLMCPHttpServer::start(mPort, mAuthToken);
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

    mRunning = false;
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
            if (!region)
            {
                return errorResult(llformat("Region '%s' not found in the current grid", region_name.c_str()));
            }
            gAgent.teleportViaLocation(region->getPosGlobalFromRegion(local));
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
            return textResult(toJsonString(LLMCPServer::instance().collectNearbyAgents()));
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
    if(command == "get_tools_list")
    {
        return getToolsList();
    }
    else if(command == "call_tool")
    {
        std::string name = params["name"].asString();
        LLSD args = params.has("arguments") ? params["arguments"] : LLSD::emptyMap();

        ToolHandler handler;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto it = mTools.find(name);
            if(it == mTools.end())
            {
                LLSD err = LLSD::emptyMap();
                err["_error"] = llformat("Tool '%s' not found", name.c_str());
                return err;
            }
            handler = it->second.handler;
        }

        if(!on_main_thread())
        {
            LL_WARNS("MCP") << "Plugin tool call escaped the main thread" << LL_ENDL;
            LLSD err = LLSD::emptyMap();
            err["_error"] = "Tool execution is restricted to the main thread";
            return err;
        }

        try
        {
            return handler(args);
        }
        catch(const std::exception& e)
        {
            LL_WARNS("MCP") << "Plugin tool '" << name << "' threw: " << e.what() << LL_ENDL;
            LLSD err = LLSD::emptyMap();
            err["_error"] = std::string("Handler failed: ") + e.what();
            return err;
        }
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
    std::lock_guard<std::mutex> lock(mMutex);
    LLSD tools = LLSD::emptyArray();
    for(const auto& pair : mTools)
    {
        const Tool& tool = pair.second;
        LLSD t = LLSD::emptyMap();
        t["name"] = tool.name;
        t["description"] = tool.description;
        t["inputSchema"] = tool.input_schema;
        tools.append(t);
    }
    return tools;
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
    server_info["version"] = LLVersionInfo::instance().getVersion();

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

    return runOnMain([handler, args]() { return handler(args); });
}

LLSD LLMCPServer::runOnMain(std::function<LLSD()> fn)
{
    auto mainloop = LL::WorkQueue::getInstance("mainloop");
    if (!mainloop)
        return makeError(-32000, "Main loop work queue not available");

    std::promise<LLSD> promise;
    auto future = promise.get_future();

    try
    {
        mainloop->post([&promise, fn = std::move(fn)]()
        {
            LLSD result;
            try
            {
                result = fn();
            }
            catch (const std::exception& e)
            {
                LL_WARNS("MCP") << "Main thread handler threw: " << e.what() << LL_ENDL;
                result = errorResult(std::string("Handler failed: ") + e.what());
            }
            catch (...)
            {
                LL_WARNS("MCP") << "Main thread handler threw" << LL_ENDL;
                result = errorResult("Handler failed");
            }
            promise.set_value(result);
        });
    }
    catch (...)
    {
        return makeError(-32000, "Failed to post to main thread");
    }

    return future.get();
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
    }

    LLSD data = LLSD::emptyMap();
    data["friends"] = friends;
    data["count"] = (LLSD::Integer)friends.size();
    data["online_count"] = online_count;
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
    }
    LLSD data = LLSD::emptyMap();
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
    for (S32 i = 0; i < gObjectList.getNumObjects(); ++i)
    {
        if ((S32)objects.size() >= max_results) break;
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
    data["objects"] = objects;
    data["count"] = (LLSD::Integer)objects.size();
    return data;
}

LLSD LLMCPServer::inventorySearch(const LLSD& params) const
{
    std::string query = params.has("query") ? params["query"].asString() : "";
    if (query.empty())
        return LLSDMap("error", "Query is required");
    LLStringUtil::toLower(query);

    LLUUID start_folder = params.has("folder") ? LLUUID(params["folder"].asString()) : LLUUID::null;
    std::string type_filter = params.has("type") ? params["type"].asString() : "";
    S32 max_results = params.has("max_results") ? (S32)params["max_results"].asInteger() : 50;
    if (max_results <= 0) max_results = 50;

    LLInventoryType::EType filter_type = LLInventoryType::IT_NONE;
    if (!type_filter.empty())
    {
        filter_type = LLInventoryType::lookup(type_filter);
        if (filter_type == LLInventoryType::IT_NONE)
            return LLSDMap("error", llformat("Unknown inventory type '%s'", type_filter.c_str()));
    }

    LLSD results = LLSD::emptyArray();
    std::function<void(const LLUUID&, const std::string&)> walk;
    walk = [&](const LLUUID& cat_id, const std::string& path)
    {
        if ((S32)results.size() >= max_results) return;

        LLInventoryModel::cat_array_t* cats = nullptr;
        LLInventoryModel::item_array_t* items = nullptr;
        gInventory.getDirectDescendentsOf(cat_id, cats, items);

        if (cats)
        {
            for (size_t i = 0; i < cats->size(); ++i)
            {
                if ((S32)results.size() >= max_results) break;
                std::string cname = (*cats)[i]->getName();
                std::string lower = cname;
                LLStringUtil::toLower(lower);
                if (type_filter.empty() && lower.find(query) != std::string::npos)
                {
                    LLSD entry = LLSD::emptyMap();
                    entry["name"] = cname;
                    entry["uuid"] = (*cats)[i]->getUUID().asString();
                    entry["type"] = "folder";
                    entry["folder"] = path;
                    results.append(entry);
                }
                std::string sub_path = path.empty() ? cname : path + "/" + cname;
                walk((*cats)[i]->getUUID(), sub_path);
            }
        }

        if (items)
        {
            for (size_t i = 0; i < items->size(); ++i)
            {
                if ((S32)results.size() >= max_results) break;
                std::string iname = (*items)[i]->getName();
                std::string lower = iname;
                LLStringUtil::toLower(lower);
                if (lower.find(query) == std::string::npos) continue;
                LLInventoryType::EType itype = (*items)[i]->getInventoryType();
                if (filter_type != LLInventoryType::IT_NONE && itype != filter_type) continue;
                LLSD entry = LLSD::emptyMap();
                entry["name"] = iname;
                entry["uuid"] = (*items)[i]->getUUID().asString();
                entry["type"] = LLInventoryType::lookup(itype);
                entry["folder"] = path;
                results.append(entry);
            }
        }
    };
    walk(start_folder, "");

    LLSD data = LLSD::emptyMap();
    data["results"] = results;
    data["count"] = (LLSD::Integer)results.size();
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
    result["resources"] = resources;
    return makeResult(result);
}

LLSD LLMCPServer::handleResourcesRead(const LLSD& params)
{
    std::string uri = params["uri"].asString();

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
            content_data["handle"] = (LLSD::Integer)regionp->getHandle();
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
        loader = []() {
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
            return items;
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

    LLSD result = runOnMain(loader);
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
    std::string level = params["level"].asString();
    std::string upper = level;
    std::transform(upper.begin(), upper.end(), upper.begin(), toupper);
    if (upper != "DEBUG" && upper != "INFO" && upper != "WARN" && upper != "WARNING"
        && upper != "ERROR" && upper != "NONE")
    {
        return makeError(-32602, llformat("Unrecognized logging level '%s'", level.c_str()));
    }
    if (upper == "WARNING")
    {
        upper = "WARN";
    }
    LLError::setDefaultLevel(LLError::decodeLevel(upper));
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
