#ifndef LL_LLMCPHTTP_H
#define LL_LLMCPHTTP_H

#include "llsd.h"
#include <string>
#include <atomic>
#include <thread>

class LLMCPHttpServer
{
public:
    static void start(U16 port, const std::string& auth_token);
    static void stop();
    static bool isRunning();

private:
    static void serverThread(LLMCPHttpServer* self);
    void run();
    void handleClient(int client_fd);
    std::string readHttpRequest(int fd, bool& too_large);
    void sendHttpResponse(int fd, int status, const std::string& status_text,
                          const std::string& body, const std::string& content_type = "application/json",
                          const std::string& origin = "");
    static std::string getAllowedOrigin(const std::string& header_part);

    // Maximum accepted HTTP body size. Larger requests are rejected with 413.
    static constexpr size_t kMaxBodySize = 1 * 1024 * 1024;

    static LLMCPHttpServer* sInstance;
    int mListenFd;
    U16 mPort;
    std::string mAuthToken;
    std::atomic<bool> mRunning;
    std::thread mThread;
};

#endif
