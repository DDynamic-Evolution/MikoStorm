#ifndef LL_LLMCPSERVER_H
#define LL_LLMCPSERVER_H

#include "llsingleton.h"
#include "llsd.h"
#include <string>
#include <map>
#include <functional>
#include <mutex>
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

    LLSD handleRequest(const LLSD& request);

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
    LLSD inventorySearch(const LLSD& params) const;
    LLSD collectFriendsOnline() const;
    LLSD collectIMSessions() const;

    struct Tool {
        std::string name;
        std::string description;
        LLSD input_schema;
        ToolHandler handler;
    };

    std::map<std::string, Tool> mTools;
    bool mRunning;
    U16 mPort;
    std::string mAuthToken;
    mutable std::mutex mMutex;
    bool mInitialized;

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
