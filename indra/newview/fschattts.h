#ifndef FS_FSCHATTTS_H
#define FS_FSCHATTTS_H

#include "llsd.h"
#include "lluuid.h"
#include <queue>
#include <mutex>
#include <atomic>
#include <string>

class FSChatTTS
{
public:
    FSChatTTS();
    ~FSChatTTS();

    void onChatMessage(const LLSD& chat);

    static FSChatTTS& instance();

private:
    struct TTSRequest
    {
        std::string text;
        std::string from_name;
        LLUUID from_id;
    };

    void processQueue();
    void doTTS(const std::string& text);
    static std::string sanitizeText(const std::string& text);

    std::queue<TTSRequest> mQueue;
    std::mutex mMutex;
    std::atomic<bool> mProcessing{ false };

    std::string mLastText;
    LLUUID mLastSpeaker;
    F64 mLastTime{ 0.0 };

    static FSChatTTS* sInstance;
};

#endif
