#ifndef LL_LLMCPSERVER_H
#define LL_LLMCPSERVER_H

#include "llsingleton.h"
#include "llsd.h"
#include <string>
#include <map>
#include <functional>
#include <mutex>
#include <atomic>
#include <memory>
#include <chrono>
#include <vector>
#include "lluuid.h"
#include "v3dmath.h"
#include <deque>
#include <set>
#include <boost/signals2.hpp>

class LLMCPServer : public LLSingleton<LLMCPServer>
{
    LLSINGLETON(LLMCPServer);
public:
    using ToolHandler = std::function<LLSD(const LLSD& params)>;

    void start();
    void stop();
    bool isRunning() const { return mRunning; }
    U16 getPort() const { return mPort; }

    void registerTool(const std::string& name,
                      const std::string& description,
                      const LLSD& input_schema,
                      ToolHandler handler);

    enum Permission : U32 { Read = 1, Camera = 2, Movement = 4, Chat = 8, InventoryWrite = 16 };
    struct Context
    {
        std::string client = "default", session, requestKey, cancelKey;
        std::shared_ptr<std::atomic<bool>> active;
    };
    LLSD handleRequest(const LLSD& request, Context context, std::string* created_session = nullptr);
    bool authenticate(const std::string& token, bool supplied, std::string& client) const;
    bool validSession(const std::string& session, const std::string& client) const;
    std::string protocolForSession(const std::string& session) const;
    static bool supportsProtocol(const std::string& version);
    void closeSession(const std::string& session, const std::string& client);
    void refreshAccess();
    LLSD getStatus() const;
    LLSD getAudit() const;
    LLSD getClientProfiles() const;
    void setClientPermissions(const std::string& client, U32 mask);
    std::string createClient(const std::string& name, U32 mask);
    void removeClient(const std::string& client);
    static std::string generateToken();
    static void configureLive();
    static void idle(void* data);
    LLSD dispatchTool(const std::string& name, const LLSD& args);
    bool permitted(const std::string& client, U32 group) const;
    std::string beginOperation(const std::string& kind, U32 group);
    bool operationActive(const std::string& id);
    void updateOperation(const std::string& id, const std::string& state, const std::string& message,
                         F32 progress = 0.f, const LLSD& result = LLSD());
    LLSD operationStatus(const std::string& id) const;
    LLSD cancelOperation(const std::string& id);
    std::string currentClient() const;
    void registerViewerTools();
    LLSD startTeleport(const LLSD& params);
    LLSD writeNotecard(const LLSD& params);
    void setOperationTarget(const std::string& id, const LLVector3d& target);

    void registerDefaultTools();

    // Chat buffer: stores recent chat messages for MCP read access
    static constexpr size_t kChatBufferSize = 100;
    void pushChatMessage(const LLSD& msg);
    LLSD getChatMessages(S32 limit = 50) const;

    // IM buffer: stores recent IM messages for MCP read access
    static constexpr size_t kImBufferSize = 100;
    void pushIMMessage(const LLSD& msg);
    LLSD getIMMessages(S32 limit = 50) const;

    // Trigger word watch: stores chat messages matching trigger words
    static constexpr size_t kTriggerBufferSize = 50;
    void checkTriggerWords(const LLSD& chat);
    LLSD getTriggerMatches() const;
    void setTriggerWords(const std::vector<std::string>& words);
    void clearTriggerBuffer();

    // Bridge used by media plugins to consume MCP tools.
    LLSD handlePluginMCPCommand(const std::string& command, const LLSD& params);

    // Exposed tool registry (called on the main thread only).
    LLSD getToolsList() const;

private:
    ~LLMCPServer();

    LLSD handleInitialize(const LLSD& params);
    LLSD handlePing(const LLSD& params);
    LLSD handleToolsList(const LLSD& params);
    LLSD handleToolsCall(const LLSD& params);
    LLSD handleResourcesList(const LLSD& params);
    LLSD handleResourcesRead(const LLSD& params);
    LLSD handleSetLoggerLevel(const LLSD& params);

    LLSD makeError(int code, const std::string& message, const LLSD& data = LLSD());
    LLSD makeResult(const LLSD& result);

    LLSD runOnMain(std::function<LLSD()> fn);
    LLSD collectNearbyAgents() const;
    LLSD collectAttachments() const;
    LLSD collectParcelInfo() const;
    LLSD collectSelfInfo() const;
    LLSD collectNearbyObjects(const LLSD& params) const;
    LLSD inventorySearch(const LLSD& params);
    LLSD collectFriendsOnline() const;
    LLSD collectIMSessions() const;

    struct Tool {
        std::string name;
        std::string description;
        LLSD input_schema;
        ToolHandler handler;
    };

    std::map<std::string, Tool> mTools;
    std::atomic<bool> mRunning{false};
    std::shared_ptr<std::atomic<U32>> mMainRequests = std::make_shared<std::atomic<U32>>(0);
    std::shared_ptr<std::atomic<bool>> mSession = std::make_shared<std::atomic<bool>>(false);
    U16 mPort;
    mutable std::mutex mMutex;
    using Clock = std::chrono::steady_clock;
    struct Session
    {
        std::string client, protocol, logLevel = "info";
        bool initialized = false;
        Clock::time_point touched = Clock::now(), window = Clock::now();
        U32 calls = 0;
    };
    struct Client { std::string name, tokenHash; U32 permissions = 0; };
    struct Operation
    {
        std::string client, session, kind, state = "running", message;
        U32 permission = 0;
        F32 progress = 0.f;
        LLSD result;
        Clock::time_point started = Clock::now();
        LLVector3d target, localTarget;
        U64 destinationHandle = 0;
        std::string regionName;
        bool hasTarget = false, sawTeleport = false, uploadSubmitted = false, cancelRequested = false, cancellationSent = false;
    };
    U32 mDefaultPermissions = 31, mMediaPermissions = 31;
    std::string mDefaultTokenHash, mLastError;
    std::map<std::string, Client> mClients;
    std::map<std::string, Session> mSessions;
    std::map<std::string, std::shared_ptr<std::atomic<bool>>> mRequests;
    std::map<std::string, Operation> mOperations;
    Clock::time_point mLastOperationTick{};
    struct SearchFrame { LLUUID folder; std::string path; size_t cat = 0, item = 0; };
    struct SearchCursor
    {
        std::string client, session, query, type;
        LLUUID root;
        Clock::time_point touched = Clock::now();
        std::vector<SearchFrame> stack;
        std::set<LLUUID> visited;
    };
    // Only accessed on the main thread, like gInventory itself.
    std::map<std::string, SearchCursor> mSearchCursors;
    std::deque<LLSD> mAudit;
    void audit(const std::string& tool, const std::string& client, const std::string& status);
    static U32 permissionForTool(const std::string& name);
    void updateOperations();
    void requestOperationCancellation(const std::string& id, const std::string& reason);

    // Chat buffer (protected by mMutex)
    std::deque<LLSD> mChatBuffer;
    boost::signals2::connection mChatConnection;

    // IM buffer (protected by mMutex)
    std::deque<LLSD> mImBuffer;
    boost::signals2::connection mImConnection;

    // Trigger word watch (protected by mMutex)
    std::set<std::string> mTriggerWords;
    std::deque<LLSD> mTriggerBuffer;
};

#endif
