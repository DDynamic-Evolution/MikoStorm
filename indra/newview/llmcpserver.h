#ifndef LL_LLMCPSERVER_H
#define LL_LLMCPSERVER_H

#include "llsingleton.h"
#include "llsd.h"
#include <string>
#include <map>
#include <functional>
#include <mutex>

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
    std::mutex mMutex;
    bool mInitialized;
};

#endif
