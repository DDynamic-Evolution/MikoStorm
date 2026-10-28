#include "llviewerprecompiledheaders.h"
#include "llpositionalstreammgr.h"
#include "llstream3dnumeric.h"
#include "llvenuereverbdsp.h"
#include "llstring.h"
#include <algorithm>
#include <cctype>
namespace
{
    bool tryParseFloat(const std::string& s, F32& out)
    {
        if (s.empty())
        {
            return false;
        }
        return LLStream3DNumeric::parseFloat(s, out);
    }

    bool tryParseNonNegativeS32(const std::string& s, S32& out)
    {
        if (s.empty())
        {
            return false;
        }
        try
        {
            size_t consumed = 0;
            int val = std::stoi(s, &consumed, 10);
            if (consumed != s.size() || val < 0)
            {
                return false;
            }
            out = static_cast<S32>(val);
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    std::string toLowerAscii(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    // Case-insensitive substring search (ASCII).
    size_t findCaseInsensitive(const std::string& haystack, const std::string& needle)
    {
        if (needle.empty() || haystack.size() < needle.size())
        {
            return std::string::npos;
        }
        const size_t end = haystack.size() - needle.size();
        for (size_t i = 0; i <= end; ++i)
        {
            bool match = true;
            for (size_t j = 0; j < needle.size(); ++j)
            {
                unsigned char a = static_cast<unsigned char>(haystack[i + j]);
                unsigned char b = static_cast<unsigned char>(needle[j]);
                if (std::tolower(a) != std::tolower(b))
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                return i;
            }
        }
        return std::string::npos;
    }

    // Walks a body between [content_start, end) by directly scanning for
    // "{key:value}" blocks and invoking onPair(lowered_key, trimmed_val) for
    // each well-formed unit. Anything between blocks (comma, whitespace, or
    // nothing at all) is treated as a separator, so r5 unifies the previous
    // 3D Stream "comma required" and Parcel Hide "no separator" formats.
    template <typename F>
    void forEachKeyValue(const std::string& description,
                         size_t content_start, size_t end, F&& onPair)
    {
        size_t cursor = content_start;
        while (cursor < end)
        {
            size_t ob = description.find('{', cursor);
            if (ob == std::string::npos || ob >= end) break;
            size_t cb = description.find('}', ob + 1);
            if (cb == std::string::npos || cb > end) break;

            std::string inner = description.substr(ob + 1, cb - ob - 1);
            cursor = cb + 1;

            size_t colon = inner.find(':');
            if (colon == std::string::npos) continue;

            std::string key = inner.substr(0, colon);
            std::string val = inner.substr(colon + 1);
            LLStringUtil::trim(key);
            LLStringUtil::trim(val);
            key = toLowerAscii(key);

            onPair(key, val);
        }
    }

    // Locate the body of "[<prefix>...]" and return [content_start, end).
    // Returns false if prefix not found or no closing bracket.
    bool findTagBody(const std::string& description, const std::string& prefix,
                     size_t& out_content_start, size_t& out_end)
    {
        size_t begin = findCaseInsensitive(description, prefix);
        if (begin == std::string::npos) return false;
        size_t content_start = begin + prefix.size();
        size_t end = description.find(']', content_start);
        if (end == std::string::npos) return false;
        out_content_start = content_start;
        out_end = end;
        return true;
    }

}

// static
std::optional<LLPositionalStreamMgr::TagData>
LLPositionalStreamMgr::parseTag(const std::string& description)
{
    // r5: new prefix is `[3dstream:`; the legacy `[ayastream:` is kept as a
    // permanent alias so already-placed prims keep working without re-edit.
    static const std::string kPrefix    = "[3dstream:";
    static const std::string kPrefixOld = "[ayastream:";

    size_t content_start = 0, end = 0;
    if (!findTagBody(description, kPrefix, content_start, end) &&
        !findTagBody(description, kPrefixOld, content_start, end)) return std::nullopt;

    TagData data;
    bool got_url = false;
    forEachKeyValue(description, content_start, end,
        [&](const std::string& key, const std::string& val)
        {
            if (key == "url")      { data.url = val; got_url = !val.empty(); }
            else if (key == "min") { F32 f; if (tryParseFloat(val, f)) data.min = f; }
            else if (key == "max") { F32 f; if (tryParseFloat(val, f)) data.max = f; }
        });

    if (!got_url) return std::nullopt;
    return data;
}

// r12 P9: short-name aliases for r11 reverb/binaural keys and venue values.
// Object Description has a hard 127-byte cap (SL protocol). Long forms like
// {binaural:on}{venue:hall_medium}{wetgain:1.5} eat 45 bytes alone; short
// forms compress to 22 (saves 23 bytes, enough headroom for typical URLs).
// Parser accepts both old and new; LSL writer emits short by default.
namespace
{
    // venue value alias table: short token → canonical long name.
    // Keep in sync with LLVenueReverbDsp::knownVenues() and the LSL setup
    // script's VENUE_BUTTONS list.
    static constexpr std::pair<std::string_view, std::string_view> kVenueAliases[] = {
        {"d",  "dry"},
        {"rs", "room_small"},
        {"rm", "room_medium"},
        {"hs", "hall_small"},
        {"hm", "hall_medium"},
        {"hl", "hall_large"},
        {"cl", "club"},
        {"ct", "cathedral"},
        {"od", "outdoor"},
    };

    std::string resolveVenueAlias(const std::string& val)
    {
        for (const auto& [short_form, long_form] : kVenueAliases)
        {
            if (val == short_form) return std::string(long_form);
        }
        return val; // not an alias → pass through as-is
    }
}

// static
std::optional<LLPositionalStreamMgr::ChannelKind>
LLPositionalStreamMgr::parseChannelKind(std::string_view s)
{
    // The {ch:...} alphabet. r8/r9 used L/R/M only; r10 added the 5.1
    // placement values. Every other consumer (evaluator, downmix table)
    // reads through the enum and never re-parses the source string.
    static constexpr std::pair<std::string_view, ChannelKind> kTokens[] = {
        {"L",   ChannelKind::L},
        {"R",   ChannelKind::R},
        {"M",   ChannelKind::M},
        {"FL",  ChannelKind::FL},
        {"FR",  ChannelKind::FR},
        {"C",   ChannelKind::C},
        {"LFE", ChannelKind::LFE},
        {"SL",  ChannelKind::SL},
        {"SR",  ChannelKind::SR},
        {"BL",  ChannelKind::BL},
        {"BR",  ChannelKind::BR},
    };

    auto upcase = [](char c) -> char
    {
        return (c >= 'a' && c <= 'z') ? char(c - 'a' + 'A') : c;
    };

    for (const auto& [token, kind] : kTokens)
    {
        if (s.size() != token.size()) continue;
        bool match = true;
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (upcase(s[i]) != token[i]) { match = false; break; }
        }
        if (match) return kind;
    }
    return std::nullopt;
}

// static
LLPositionalStreamMgr::DistParseResult
LLPositionalStreamMgr::parseDistributedStereoTag(const std::string& description)
{
    // r5 で導入した [3dstream-stereo:...] と alias [ayastream-stereo:...] を
    // 同じ本体としてサポート (r8 で旧 {l:N}{r:N} 書式は廃止し、新書式のみ受ける)。
    // r31 では [3dstream:...] も distributed/linkset grammar として読む。
    // ただし [3dstream:{url:...}] 単独は evaluateLinkset() 側で既存 mono
    // fallback に戻すため、古い単一プリム配置は壊さない。
    static const std::string kPrefix    = "[3dstream-stereo:";
    static const std::string kPrefixOld = "[ayastream-stereo:";
    static const std::string kPrefixUnified = "[3dstream:";

    DistParseResult result;

    size_t content_start = 0, end = 0;
    bool unified_3dstream_prefix = false;
    if (!findTagBody(description, kPrefix, content_start, end) &&
        !findTagBody(description, kPrefixOld, content_start, end))
    {
        if (!findTagBody(description, kPrefixUnified, content_start, end))
        {
            return result; // no tag at all
        }
        unified_3dstream_prefix = true;
    }

    // Track which keys appeared (regardless of value validity) so a
    // {ch:X} with an invalid value still surfaces BadCh rather than
    // silently being treated as "no tag here".
    bool seen_ch_key = false;
    bool seen_url_key = false;
    bool seen_source_key = false;
    bool seen_media_source = false;

    DistStereoTagData data;
    data.unified_3dstream_prefix = unified_3dstream_prefix;
    F32 range_value = 0.f;
    bool range_valid = false;
    F32 volume_value = 0.f;
    bool volume_valid = false;

    auto setError = [&](DistParseError e, const std::string& v)
    {
        // First error wins — keeps the surfaced message stable when several
        // fields are bad (LSL editing typically fixes one at a time anyway).
        if (result.error == DistParseError::Ok)
        {
            result.error = e;
            result.bad_value = v;
        }
    };

    forEachKeyValue(description, content_start, end,
        [&](const std::string& key, const std::string& val)
        {
            if (key == "url")
            {
                seen_url_key = true;
                if (val.empty())
                {
                    setError(DistParseError::EmptyUrl, val);
                }
                else
                {
                    data.source_kind = DistSourceKind::Url;
                    data.url = val;
                }
            }
            else if (key == "source")
            {
                seen_source_key = true;
                std::string lowered = val;
                LLStringUtil::toLower(lowered);
                if (lowered == "media" || lowered == "media-stereo")
                {
                    seen_media_source = true;
                    data.source_kind = DistSourceKind::Media;
                    data.media_source_channels = 2;
                }
                else if (lowered == "media-5-1")
                {
                    seen_media_source = true;
                    data.source_kind = DistSourceKind::Media;
                    data.media_source_channels = 6;
                }
                else if (lowered == "media-7-1")
                {
                    seen_media_source = true;
                    data.source_kind = DistSourceKind::Media;
                    data.media_source_channels = 8;
                }
                else
                {
                    setError(DistParseError::BadSource, val);
                }
            }
            else if (key == "face")
            {
                S32 face = -1;
                if (tryParseNonNegativeS32(val, face))
                {
                    data.media_face = face;
                }
                else
                {
                    setError(DistParseError::BadFace, val);
                }
            }
            else if (key == "link")
            {
                S32 link = -1;
                if (tryParseNonNegativeS32(val, link))
                {
                    data.media_link = link;
                }
                else
                {
                    setError(DistParseError::BadLink, val);
                }
            }
            else if (key == "ch")
            {
                seen_ch_key = true;
                if (auto ck = parseChannelKind(val))
                {
                    data.ch = *ck;
                }
                else
                {
                    setError(DistParseError::BadCh, val);
                }
            }
            else if (key == "range")
            {
                F32 f;
                if (tryParseFloat(val, f) && f > 0.f)
                {
                    range_value = f;
                    range_valid = true;
                }
                else
                {
                    setError(DistParseError::BadRange, val);
                }
            }
            else if (key == "volume")
            {
                F32 f;
                if (tryParseFloat(val, f) && f >= 0.f && f <= 1.f)
                {
                    volume_value = f;
                    volume_valid = true;
                }
                else
                {
                    setError(DistParseError::BadVolume, val);
                }
            }
            else if (key == "binaural" || key == "bin")
            {
                // r11 P5: {binaural:on|off} (case-insensitive). Only
                // meaningful on the root prim (= same prim as {url}); we
                // still parse it on every prim so a malformed value
                // surfaces a chat error regardless of where the typo is.
                // r12 P9: short alias `bin` accepted for Desc 127-byte budget.
                std::string lowered = val;
                LLStringUtil::toLower(lowered);
                if (lowered == "on" || lowered == "true" || lowered == "1")
                {
                    data.binaural = true;
                }
                else if (lowered == "off" || lowered == "false" || lowered == "0")
                {
                    data.binaural = false;
                }
                else
                {
                    setError(DistParseError::BadBinaural, val);
                }
            }
            else if (key == "upmix")
            {
                // r12 P5: {upmix:on|off} (case-insensitive). Same shape
                // as binaural. Only meaningful on the root prim, but we
                // still parse on every prim so a typo surfaces a chat
                // error regardless of where it sits.
                std::string lowered = val;
                LLStringUtil::toLower(lowered);
                if (lowered == "on" || lowered == "true" || lowered == "1")
                {
                    data.upmix = true;
                }
                else if (lowered == "off" || lowered == "false" || lowered == "0")
                {
                    data.upmix = false;
                }
                else
                {
                    setError(DistParseError::BadUpmix, val);
                }
            }
            else if (key == "venue" || key == "v")
            {
                // r11 P8: {venue:NAME}. Validated against the bundled
                // catalog (LLVenueReverbDsp::knownVenues, includes "dry").
                // Spec §4.1 line 174 says unknown name is silent-ignore +
                // chat warn — i.e. NOT a tag-rejecting error like the
                // {binaural} branch above. So we capture the bad value
                // separately and let evaluateLinkset notify on its own
                // schedule, leaving data.venue at nullopt → effective
                // resolves to "dry".
                // r12 P9: short alias `v` accepted; venue value also
                // accepts 2-char alias (hm, ct, ...) per kVenueAliases.
                const std::string resolved = resolveVenueAlias(val);
                const auto& known = LLVenueReverbDsp::knownVenues();
                if (std::find(known.begin(), known.end(), resolved) != known.end())
                {
                    data.venue = resolved;
                }
                else
                {
                    data.bad_venue_value = val;
                }
            }
            else if (key == "wetgain" || key == "wg")
            {
                // r11 P9: {wetgain:N}. Spec §4.1 line 152 — F32 in
                // [0.0, 2.0], values outside the range are clamped (not
                // rejected). Non-numeric input is the only failure mode
                // surfaced as BadWetGain.
                // r12 P9: short alias `wg` accepted.
                F32 f;
                if (tryParseFloat(val, f))
                {
                    data.wetgain = std::clamp(f, 0.f, 2.f);
                }
                else
                {
                    setError(DistParseError::BadWetGain, val);
                }
            }
            else if (key == "lfegain" || key == "lg")
            {
                // r12.1: {lfegain:N}. F32 in [0.0, 3.0], values outside
                // the range are clamped (not rejected). Source-side /
                // root-only — child values silently ignored same as
                // {wetgain}/{venue}. Non-numeric input is BadLfeGain.
                F32 f;
                if (tryParseFloat(val, f))
                {
                    data.lfegain = std::clamp(f, 0.f, 3.f);
                }
                else
                {
                    setError(DistParseError::BadLfeGain, val);
                }
            }
            // Unknown keys (incl. removed-in-r8 {l}/{r}/{min}/{max}) are
            // silently ignored — the spec is permissive about extra fields.
        });

    // The tag is recognized when at least one of {url}, {source}, or {ch}
    // appeared.
    // A bracket body with neither is treated as "no tag here".
    const bool tag_recognized = seen_ch_key || seen_url_key || seen_source_key;
    if (!tag_recognized)
    {
        return result;
    }

    if (seen_url_key && seen_media_source)
    {
        setError(DistParseError::ConflictingSource, "url+source:media");
    }

    if (result.error != DistParseError::Ok)
    {
        return result; // data stays nullopt; caller can throttle/notify
    }

    // Fan {range} and {volume} into the role-specific slots. {range} on a
    // prim that is both source and speaker fills both range_default and
    // range_speaker from the same value (spec §4.3).
    if (range_valid)
    {
        if (data.source_kind.has_value()) data.range_default = range_value;
        if (data.ch.has_value())  data.range_speaker = range_value;
    }
    if (volume_valid && data.ch.has_value())
    {
        data.volume = volume_value;
    }

    result.data = data;
    return result;
}
