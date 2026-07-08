#include "llviewerprecompiledheaders.h"

#include "fschattts.h"

#include "llagent.h"
#include "llchat.h"
#include "lldir.h"
#include "llfile.h"
#include "llframetimer.h"
#include "llaudioengine.h"
#include "llviewercontrol.h"
#include "llcoros.h"
#include "fscorehttputil.h"
#include "llhttpconstants.h"

#include <queue>
#include <vector>
#include <boost/json.hpp>

#ifdef LL_ESPEAK_NG
# include <espeak-ng/speak_lib.h>
#endif

FSChatTTS* FSChatTTS::sInstance = nullptr;

FSChatTTS::FSChatTTS()
{
    sInstance = this;
}

FSChatTTS::~FSChatTTS()
{
#ifdef LL_ESPEAK_NG
    espeak_Terminate();
#endif
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

#ifdef LL_ESPEAK_NG

static std::vector<short> sPcmBuffer;

static int espeak_callback(short *wav, int numsamples, espeak_EVENT *events)
{
    if (wav == nullptr)
        return 0;
    sPcmBuffer.insert(sPcmBuffer.end(), wav, wav + numsamples);
    return 0;
}

static bool init_espeak()
{
    std::string data_path;
    std::string path;

    path = gDirUtilp->getExecutableDir() + "/../espeak-ng-data";
    LL_INFOS("TTS") << "Looking for espeak-ng-data at: " << path << LL_ENDL;
    if (LLFile::isdir(path))
        data_path = path;

    if (data_path.empty())
    {
        path = gDirUtilp->getExecutableDir() + "/../../espeak-ng/espeak-ng-data";
        LL_INFOS("TTS") << "Looking for espeak-ng-data at: " << path << LL_ENDL;
        if (LLFile::isdir(path))
            data_path = path;
    }

    LL_INFOS("TTS") << "espeak-ng-data path: " << (data_path.empty() ? "(default)" : data_path) << LL_ENDL;

    int sample_rate = espeak_Initialize(AUDIO_OUTPUT_RETRIEVAL, 200,
        data_path.empty() ? nullptr : data_path.c_str(),
        espeakINITIALIZE_DONT_EXIT);
    if (sample_rate < 0)
    {
        LL_WARNS("TTS") << "espeak_Initialize failed, sample_rate=" << sample_rate << LL_ENDL;
        return false;
    }

    LL_INFOS("TTS") << "espeak_Initialize OK, sample_rate=" << sample_rate << LL_ENDL;

    espeak_SetSynthCallback(espeak_callback);
    return true;
}

#endif

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

void FSChatTTS::doEspeakTTS(const std::string& text)
{
#ifdef LL_ESPEAK_NG
    static bool initialized = init_espeak();
    if (!initialized)
        return;

    std::string voice = gSavedPerAccountSettings.getString("FSChatTTSEspeakVoice");
    std::string variant = gSavedPerAccountSettings.getString("FSChatTTSEspeakVariant");
    S32 speed = gSavedPerAccountSettings.getS32("FSChatTTSEspeakSpeed");
    S32 pitch = gSavedPerAccountSettings.getS32("FSChatTTSEspeakPitch");
    S32 amplitude = gSavedPerAccountSettings.getS32("FSChatTTSEspeakAmplitude");
    S32 wordgap = gSavedPerAccountSettings.getS32("FSChatTTSEspeakWordGap");

    {
        std::string voice_name = voice;
        if (!variant.empty())
            voice_name = voice + "+" + variant;
        espeak_ERROR voice_err = espeak_SetVoiceByName(voice_name.c_str());
        if (voice_err != EE_OK)
        {
            LL_WARNS("TTS") << "espeak_SetVoiceByName(" << voice_name << ") failed: " << (int)voice_err << LL_ENDL;
        }
    }
    espeak_SetParameter(espeakRATE, speed, 0);
    espeak_SetParameter(espeakPITCH, pitch, 0);
    espeak_SetParameter(espeakVOLUME, amplitude, 0);
    espeak_SetParameter(espeakWORDGAP, wordgap, 0);

    sPcmBuffer.clear();

    unsigned int flags = espeakCHARS_UTF8;
    espeak_ERROR synth_result = espeak_Synth(text.c_str(), text.size() + 1,
        0, POS_CHARACTER, 0, flags, nullptr, nullptr);

    if (synth_result != EE_OK)
    {
        LL_WARNS("TTS") << "espeak_Synth failed: " << (int)synth_result << LL_ENDL;
        return;
    }

    espeak_Synchronize();

    if (sPcmBuffer.empty())
    {
        LL_WARNS("TTS") << "No PCM data generated" << LL_ENDL;
        return;
    }

    int sample_rate = 22050; // default espeak sample rate

    LLUUID audio_uuid;
    audio_uuid.generate();
    std::string uuid_str;
    audio_uuid.toString(uuid_str);
    std::string cache_path = gDirUtilp->getExpandedFilename(LL_PATH_FS_SOUND_CACHE, uuid_str) + ".dsf";

    {
        llofstream outfile(cache_path, llofstream::binary);
        if (!outfile.is_open())
        {
            LL_WARNS("TTS") << "Failed to open output file: " << cache_path << LL_ENDL;
            return;
        }

        S32 data_size = (S32)(sPcmBuffer.size() * sizeof(short));
        S32 file_size = 36 + data_size;
        S16 audio_format = 1;
        S16 num_channels = 1;
        S32 byte_rate = sample_rate * num_channels * (S32)sizeof(short);
        S16 block_align = num_channels * (S16)sizeof(short);
        S16 bits_per_sample = 16;

        outfile.write("RIFF", 4);
        outfile.write((const char*)&file_size, 4);
        outfile.write("WAVE", 4);
        outfile.write("fmt ", 4);
        S32 fmt_size = 16;
        outfile.write((const char*)&fmt_size, 4);
        outfile.write((const char*)&audio_format, 2);
        outfile.write((const char*)&num_channels, 2);
        outfile.write((const char*)&sample_rate, 4);
        outfile.write((const char*)&byte_rate, 4);
        outfile.write((const char*)&block_align, 2);
        outfile.write((const char*)&bits_per_sample, 2);
        outfile.write("data", 4);
        outfile.write((const char*)&data_size, 4);
        outfile.write((const char*)sPcmBuffer.data(), data_size);
    }

    if (gAudiop && gAudiop->preloadSound(audio_uuid))
    {
        gAudiop->triggerSound(audio_uuid, gAgent.getID(), 1.0f, LLAudioEngine::AUDIO_TYPE_TTS);
        LL_INFOS("TTS") << "Playing eSpeak audio" << LL_ENDL;
    }
    else
    {
        LL_WARNS("TTS") << "Failed to preload eSpeak audio" << LL_ENDL;
    }
#else
    LL_WARNS("TTS") << "eSpeak TTS not compiled in" << LL_ENDL;
#endif
}
