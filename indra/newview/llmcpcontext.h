#ifndef LL_MCP_CONTEXT_H
#define LL_MCP_CONTEXT_H
#include "llmcpserver.h"
inline thread_local const LLMCPServer::Context* gMCPContext = nullptr;
class LLMCPContextScope
{
public:
    explicit LLMCPContextScope(const LLMCPServer::Context& context) : mPrevious(gMCPContext) { gMCPContext = &context; }
    ~LLMCPContextScope() { gMCPContext = mPrevious; }
private:
    const LLMCPServer::Context* mPrevious;
};
#endif
