#include "llviewerprecompiledheaders.h"
#include "llmcpserver.h"
#include "llmcpcontext.h"
#include "llagent.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llworldmapmessage.h"
#include "llworldmap.h"
#include "llsdutil.h"
#include "llregionhandle.h"
#include "llinventorymodel.h"
#include "llviewerinventory.h"
#include "llviewerassetupload.h"
#include "llsdjson.h"
#include "llnotecard.h"
#include <sstream>

void create_new_item(const std::string&, const LLUUID&, LLAssetType::EType,
                     LLInventoryType::EType, U32, std::function<void(const LLUUID&)>);
namespace
{
LLSD dataResult(const LLSD& data)
{
    return LLSDMap("content", llsd::array(LLSDMap("type", "text")("text", boost::json::serialize(LlsdToJson(data)))));
}
LLSD failure(const std::string& text)
{
    LLSD result = dataResult(LLSDMap("error", text)); result["isError"] = true; return result;
}
}
void LLMCPServer::configureLive()
{
    if (!instanceExists()) return;
    auto& server = instance();
    if (!gSavedSettings.getBOOL("MCPEnabled")) { server.stop(); return; }
    server.stop();
    server.start();
}
void LLMCPServer::idle(void* data) { static_cast<LLMCPServer*>(data)->updateOperations(); }
std::string LLMCPServer::beginOperation(const std::string& kind, U32 group)
{
    if (!gMCPContext || !permitted(currentClient(), group)) return {};
    std::lock_guard<std::mutex> lock(mMutex);
    // Movement and region-name resolution are global viewer operations.
    // A second client must not displace an operation already in progress.
    if (kind == "teleport")
        for (const auto& pair : mOperations)
            if (pair.second.kind == kind && (pair.second.state == "running" || pair.second.state == "cancelling")) return {};
    for (auto it = mOperations.begin(); mOperations.size() >= 128 && it != mOperations.end(); )
        if (it->second.state != "running" && it->second.state != "cancelling") it = mOperations.erase(it); else ++it;
    if (mOperations.size() >= 128) return {};
    LLUUID uuid; uuid.generate();
    Operation operation;
    operation.client = currentClient(); operation.session = gMCPContext->session;
    operation.kind = kind; operation.permission = group;
    mOperations.emplace(uuid.asString(), operation);
    return uuid.asString();
}
bool LLMCPServer::operationActive(const std::string& id)
{
    Operation operation;
    { std::lock_guard<std::mutex> lock(mMutex); auto it = mOperations.find(id);
      if (it == mOperations.end() || it->second.state != "running" || it->second.cancelRequested) return false; operation = it->second; }
    if (!mRunning || !permitted(operation.client, operation.permission) ||
        (operation.client != "media-plugin" && !validSession(operation.session, operation.client)))
    { requestOperationCancellation(id, "Session closed, permission revoked or server stopped"); return false; }
    return true;
}
void LLMCPServer::setOperationTarget(const std::string& id, const LLVector3d& target)
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mOperations.find(id); if (it == mOperations.end()) return;
    it->second.target = target; it->second.hasTarget = true;
}
void LLMCPServer::updateOperation(const std::string& id, const std::string& state,
                                 const std::string& message, F32 progress, const LLSD& result)
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mOperations.find(id); if (it == mOperations.end()) return;
    auto& operation = it->second;
    // A late upload callback may report an already submitted server-side effect
    // even after cancellation. It must not silently turn cancellation into success.
    if ((operation.state == "cancelled" || operation.state == "failed") && state == "succeeded")
    { operation.message += "; submitted server action completed later"; operation.result = result; return; }
    operation.state = state; operation.message = message; operation.progress = progress;
    if (!result.isUndefined()) operation.result = result;
}
LLSD LLMCPServer::operationStatus(const std::string& id) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mOperations.find(id);
    if (it == mOperations.end() || !gMCPContext || it->second.client != currentClient() || it->second.session != gMCPContext->session)
        return LLSDMap("error", "Unknown operation for this client session");
    const auto& op = it->second;
    return LLSDMap("operation_id", id)("kind", op.kind)("state", op.state)("progress", op.progress)
        ("message", op.message)("result", op.result)("elapsed_seconds", std::chrono::duration<F64>(Clock::now() - op.started).count());
}
LLSD LLMCPServer::cancelOperation(const std::string& id)
{
    Operation operation;
    { std::lock_guard<std::mutex> lock(mMutex); auto it = mOperations.find(id);
      if (it == mOperations.end() || !gMCPContext || it->second.client != currentClient() || it->second.session != gMCPContext->session)
          return LLSDMap("error", "Unknown operation for this client session");
      operation = it->second; }
    if (operation.state != "running" && operation.state != "cancelling") return operationStatus(id);
    requestOperationCancellation(id, "Client requested cancellation");
    return operationStatus(id);
}
void LLMCPServer::requestOperationCancellation(const std::string& id, const std::string& reason)
{
    bool teleport = false;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto it = mOperations.find(id); if (it == mOperations.end()) return;
        auto& op = it->second;
        if (op.state != "running" && op.state != "cancelling") return;
        op.cancelRequested = true;
        teleport = op.kind == "teleport" && op.hasTarget && gAgent.getTeleportState() != LLAgent::TELEPORT_NONE;
        op.state = teleport ? "cancelling" : "cancelled";
        op.message = reason + (teleport ? "; simulator cancellation requested" : op.uploadSubmitted ? "; submitted upload cannot be undone" : "; no further action will be submitted");
        op.cancellationSent = true;
    }
    if (teleport) gAgent.teleportCancel();
}

void LLMCPServer::updateOperations()
{
    if (Clock::now() - mLastOperationTick < std::chrono::milliseconds(200)) return;
    mLastOperationTick = Clock::now();
    std::vector<std::pair<std::string, Operation>> operations;
    { std::lock_guard<std::mutex> lock(mMutex); for (const auto& pair : mOperations)
        if (pair.second.state == "running" || pair.second.state == "cancelling") operations.push_back(pair); }
    for (const auto& [id, op] : operations)
    {
        if (op.cancelRequested && !op.cancellationSent)
        { requestOperationCancellation(id, op.message); continue; }
        if (op.state == "running" && !operationActive(id)) continue;
        if (Clock::now() - op.started > std::chrono::seconds(120))
        { updateOperation(id, "failed", "Operation timed out; an already submitted server action may still finish", op.progress); continue; }
        if (op.kind != "teleport") continue;
        if (!op.hasTarget)
        {
            // Resolve against the world-map cache after requesting its data.
            // This does not replace the viewer's one global SLURL callback.
            auto* sim = LLWorldMap::getInstance()->simInfoFromName(op.regionName);
            if (!sim)
            {
                if (Clock::now() - op.started > std::chrono::seconds(20)) updateOperation(id, "failed", "Destination region lookup timed out");
                continue;
            }
            if (sim->isDown()) { updateOperation(id, "failed", "Destination region is offline"); continue; }
            if (op.localTarget.mdV[VX] >= sim->getSizeX() || op.localTarget.mdV[VY] >= sim->getSizeY())
            { updateOperation(id, "failed", "Coordinates lie outside the destination region"); continue; }
            if (gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
            { updateOperation(id, "failed", "Another teleport has started"); continue; }
            auto target = from_region_handle(sim->getHandle()) + op.localTarget;
            setOperationTarget(id, target);
            { std::lock_guard<std::mutex> lock(mMutex); mOperations.at(id).destinationHandle = sim->getHandle(); }
            updateOperation(id, "running", "Teleport requested", .3f);
            gAgent.teleportViaLocation(target);
            if (gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
            { std::lock_guard<std::mutex> lock(mMutex); mOperations.at(id).sawTeleport = true; }
            else if ((gAgent.getPositionGlobal() - target).length() < 32.0) updateOperation(id, "succeeded", "Already at destination", 1.f);
            else updateOperation(id, "failed", "Viewer refused teleport request");
            continue;
        }
        const auto state = gAgent.getTeleportState();
        if (state != LLAgent::TELEPORT_NONE)
        {
            std::lock_guard<std::mutex> lock(mMutex);
            auto& current = mOperations.at(id); current.sawTeleport = true;
            current.progress = state == LLAgent::TELEPORT_ARRIVING ? .9f : .5f;
        }
        else if (gAgent.getRegion() && gAgent.getRegion()->getHandle() == op.destinationHandle &&
                 (op.sawTeleport || (gAgent.getPositionGlobal() - op.target).length() < 32.0))
        {
            const auto position = gAgent.getPositionAgent();
            const F64 distance = (gAgent.getPositionGlobal() - op.target).length();
            updateOperation(id, "succeeded", op.state == "cancelling" ? "Teleport completed despite cancellation request" :
                distance < 32.0 ? "Arrived near requested destination" : "Arrived in destination region at a different landing point", 1.f,
                LLSDMap("region", gAgent.getRegion()->getName())("x", position.mV[VX])("y", position.mV[VY])("z", position.mV[VZ])("distance_from_requested_m", distance));
        }
        else if (op.sawTeleport || op.state == "cancelling")
            updateOperation(id, op.state == "cancelling" ? "cancelled" : "failed", "Teleport ended without arriving at the requested destination", op.progress);
    }
}
LLSD LLMCPServer::startTeleport(const LLSD& p)
{
    if (!gAgent.getRegion() || gAgent.getTeleportState() != LLAgent::TELEPORT_NONE) return failure("Not connected or teleport already in progress");
    const auto id = beginOperation("teleport", Movement);
    if (id.empty()) return failure("Another teleport or region lookup is in progress, or operation limit reached");
    const LLVector3d local(p["x"].asReal(), p["y"].asReal(), p["z"].asReal());
    updateOperation(id, "running", "Resolving destination region", .1f);
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto& op = mOperations.at(id); op.regionName = p["region"].asString(); op.localTarget = local;
    }
    LLWorldMapMessage::getInstance()->sendNamedRegionRequest(p["region"].asString());
    return dataResult(operationStatus(id));
}
LLSD LLMCPServer::writeNotecard(const LLSD& p)
{
    if (!gAgent.getRegion()) return failure("Not connected to a region");
    const auto url = gAgent.getRegion()->getCapability("UpdateNotecardAgentInventory");
    if (url.empty()) return failure("Region does not support notecard updates");
    const auto body = p["content"].asString();
    if (body.size() > LLNotecard::MAX_SIZE) return failure("Notecard body exceeds the 65536-byte viewer limit");
    LLNotecard card; card.setText(body); std::ostringstream serialized;
    if (!card.exportStream(serialized)) return failure("Could not serialize notecard");
    const auto id = beginOperation("notecard", InventoryWrite);
    if (id.empty()) return failure("Operation limit reached");
    auto lifetime = mSession;
    const auto name = p["name"].asString(), content = serialized.str();
    updateOperation(id, "running", "Creating inventory item", .1f);
    create_new_item(name, gInventory.findCategoryUUIDForType(LLFolderType::FT_NOTECARD),
        LLAssetType::AT_NOTECARD, LLInventoryType::IT_NOTECARD, PERM_ALL,
        [this, lifetime, id, content, url](const LLUUID& itemID) {
            if (!lifetime->load()) return;
            if (!operationActive(id))
            { updateOperation(id, "cancelled", "Creation was already submitted; inventory item may exist", 0.f, LLSDMap("item_id", itemID.asString())); return; }
            if (itemID.isNull()) { updateOperation(id, "failed", "Inventory item creation failed"); return; }
            { std::lock_guard<std::mutex> lock(mMutex); mOperations.at(id).uploadSubmitted = true; }
            updateOperation(id, "running", "Uploading notecard content", .5f, LLSDMap("item_id", itemID.asString()));
            auto upload = std::make_shared<LLBufferedAssetUploadInfo>(itemID, LLAssetType::AT_NOTECARD, content,
                [this, lifetime, id](LLUUID item, LLUUID asset, LLUUID replacement, LLSD) {
                    if (!lifetime->load()) return;
                    const auto target = replacement.notNull() ? replacement : item;
                    if (auto* entry = gInventory.getItem(target))
                    { entry->setAssetUUID(asset); gInventory.updateItem(entry); gInventory.notifyObservers(); }
                    updateOperation(id, "succeeded", "Notecard saved", 1.f, LLSDMap("item_id", target.asString())("asset_id", asset.asString()));
                },
                [this, lifetime, id](LLUUID, LLUUID, LLSD, std::string) -> bool {
                    if (lifetime->load()) updateOperation(id, "failed", "Notecard upload failed"); return false;
                });
            LLViewerAssetUpload::EnqueueInventoryUpload(url, upload);
        });
    return dataResult(operationStatus(id));
}
