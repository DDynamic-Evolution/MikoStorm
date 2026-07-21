#include "llviewerprecompiledheaders.h"

#include "fschattts.h"

#include "llagent.h"
#include "llchat.h"
#include "llframetimer.h"
#include "llviewercontrol.h"
#include "llcoros.h"
#include "fscorehttputil.h"
#include "llhttpconstants.h"

#include <queue>
#include <vector>
#include <boost/json.hpp>

FSChatTTS* FSChatTTS::sInstance = nullptr;

FSChatTTS::FSChatTTS()
{
    sInstance = this;
}

FSChatTTS::~FSChatTTS()
{
    sInstance = nullptr;
}

FSChatTTS& FSChatTTS::instance()
{
    if (!sInstance)
    {
        sInstance = new FSChatTTS();
    }
    return *sInstance;
}

void FSChatTTS::onChatMessage(const LLSD& chat)
{
    if (!gSavedPerAccountSettings.getBOOL("FSVoiceBoxTTSEnabled"))
    {
        return;
    }

    S32 source = chat["source"].asInteger();
    S32 chat_type = chat["chat_type"].asInteger();

    bool is_im = (chat_type == CHAT_TYPE_IM || chat_type == CHAT_TYPE_IM_GROUP);
    if (is_im && !gSavedPerAccountSettings.getBOOL("FSVoiceBoxTTSReadIMs"))
    {
        return;
    }

    if (source != CHAT_SOURCE_AGENT && !is_im)
    {
        return;
    }

    if (!is_im)
    {
        switch (chat_type)
        {
        case CHAT_TYPE_WHISPER:
            if (!gSavedPerAccountSettings.getBOOL("FSVoiceBoxTTSReadWhisper"))
                return;
            break;
        case CHAT_TYPE_NORMAL:
            if (!gSavedPerAccountSettings.getBOOL("FSVoiceBoxTTSReadNormal"))
                return;
            break;
        case CHAT_TYPE_SHOUT:
            if (!gSavedPerAccountSettings.getBOOL("FSVoiceBoxTTSReadShout"))
                return;
            break;
        default:
            break;
        }
    }

    LLUUID from_id = chat["from_id"].asUUID();
    if (from_id.isNull())
    {
        return;
    }

    if (from_id == gAgentID)
    {
        return;
    }

    std::string text = sanitizeText(chat["message"].asString());
    if (text.empty())
    {
        return;
    }

    // Dedup: skip if same speaker and same text within 30 seconds
    F64 now = LLFrameTimer::getElapsedSeconds();
    if (from_id == mLastSpeaker && text == mLastText && (now - mLastTime) < 30.0)
    {
        return;
    }
    mLastSpeaker = from_id;
    mLastText = text;
    mLastTime = now;

    std::string from_name = chat["from"].asString();

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mQueue.push({text, from_name, from_id});
    }

    processQueue();
}

void FSChatTTS::processQueue()
{
    bool expected = false;
    if (!mProcessing.compare_exchange_strong(expected, true))
    {
        return;
    }

    LLCoros::instance().launch("FSChatTTSCoro",
        [this]()
        {
            while (true)
            {
                TTSRequest req;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (mQueue.empty())
                    {
                        mProcessing = false;
                        return;
                    }
                    req = mQueue.front();
                    mQueue.pop();
                }

                std::string name_format = gSavedPerAccountSettings.getString("FSVoiceBoxTTSNameFormat");
                std::string text;
                if (req.from_name.empty() || name_format == "none")
                {
                    text = req.text;
                }
                else if (name_format == "first")
                {
                    std::string first_name = req.from_name.substr(0, req.from_name.find(' '));
                    text = first_name + " says " + req.text;
                }
                else
                {
                    text = req.from_name + " says " + req.text;
                }
                LL_DEBUGS("TTS") << "Speaking: " << text << LL_ENDL;

                doTTS(text);
            }
        });
}

std::string FSChatTTS::sanitizeText(const std::string& text)
{
    static const std::string url_prefixes[] = {
        "http://", "https://", "secondlife://", "sl://"
    };

    std::string result = text;

    for (const auto& prefix : url_prefixes)
    {
        size_t pos = 0;
        while ((pos = result.find(prefix, pos)) != std::string::npos)
        {
            size_t end = result.find(' ', pos);
            if (end == std::string::npos)
                end = result.length();
            result.erase(pos, end - pos);
        }
    }

    size_t pos = 0;
    while ((pos = result.find("  ", pos)) != std::string::npos)
    {
        result.replace(pos, 2, " ");
    }

    size_t start = result.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = result.find_last_not_of(" \t\r\n");
    return result.substr(start, end - start + 1);
}

void FSChatTTS::doTTS(const std::string& text)
{
    doVoiceboxTTS(text);
}

std::string FSChatTTS::jsonEscape(const std::string& text)
{
    std::string result;
    result.reserve(text.size() + 16);
    for (char c : text)
    {
        switch (c)
        {
        case '"':  result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b";  break;
        case '\f': result += "\\f";  break;
        case '\n': result += "\\n";  break;
        case '\r': result += "\\r";  break;
        case '\t': result += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                result += buf;
            }
            else
            {
                result += c;
            }
        }
    }
    return result;
}

void FSChatTTS::doVoiceboxTTS(const std::string& text)
{
    std::string server_url = gSavedPerAccountSettings.getString("FSVoiceBoxServerURL");
    std::string voice = gSavedPerAccountSettings.getString("FSVoiceBoxVoiceName");

    if (server_url.empty())
    {
        server_url = "http://127.0.0.1:17493";
    }

    if (server_url.back() == '/')
    {
        server_url.pop_back();
    }

    std::string url = server_url + "/speak";

    std::string json = "{\"text\":\"" + jsonEscape(text) + "\"";
    if (!voice.empty())
    {
        json += ",\"profile\":\"" + jsonEscape(voice) + "\"";
    }
    json += "}";

    LLCore::HttpHeaders::ptr_t headers = std::make_shared<LLCore::HttpHeaders>();
    headers->append(HTTP_OUT_HEADER_ACCEPT, HTTP_CONTENT_JSON);
    headers->append(HTTP_OUT_HEADER_CONTENT_TYPE, HTTP_CONTENT_JSON);
    headers->append("X-Voicebox-Client-Id", "mikostorm-viewer");

    LLCore::HttpOptions::ptr_t opts = std::make_shared<LLCore::HttpOptions>();
    opts->setTimeout(10);

    FSCoreHttpUtil::callbackHttpPostRaw(url, json,
        [](const LLSD& result)
        {
            LL_INFOS("TTS") << "Voicebox speak request sent" << LL_ENDL;
        },
        [](const LLSD& result)
        {
            LL_WARNS("TTS") << "Voicebox speak request failed" << LL_ENDL;
        },
        headers, opts);
}
