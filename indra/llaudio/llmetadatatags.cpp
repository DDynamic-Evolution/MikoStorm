#include "linden_common.h"

#include "llmetadatatags.h"

#include "llstring.h"

#include "fmodstudio/fmod.hpp"

namespace LLMetadataTags
{

std::string normaliseName(const FMOD_TAG& tag)
{
    std::string name = tag.name;

    switch (tag.type)
    {
    case FMOD_TAGTYPE_ID3V2:
    {
        if (name == "TIT2")
            name = "TITLE";
        else if (name == "TPE1")
            name = "ARTIST";
        break;
    }
    case FMOD_TAGTYPE_ASF:
    {
        if (name == "Title")
            name = "TITLE";
        else if (name == "WM/AlbumArtist")
            name = "ARTIST";
        break;
    }
    case FMOD_TAGTYPE_VORBISCOMMENT:
    {
        if (name == "title")
            name = "TITLE";
        else if (name == "artist")
            name = "ARTIST";
        break;
    }
    case FMOD_TAGTYPE_FMOD:
    {
        // FMOD internal tags (e.g. "Sample Rate Change") are not
        // user-facing metadata; signal the caller to skip them.
        return {};
    }
    default:
        break;
    }

    return name;
}

LLSD convertValue(const FMOD_TAG& tag)
{
    switch (tag.datatype)
    {
    case FMOD_TAGDATATYPE_INT:
    {
        return LLSD::Integer(*(reinterpret_cast<const S32*>(tag.data)));
    }
    case FMOD_TAGDATATYPE_FLOAT:
    {
        return LLSD::Real(*(reinterpret_cast<const F32*>(tag.data)));
    }
    case FMOD_TAGDATATYPE_STRING:
    {
        return LLSD::String(rawstr_to_utf8(std::string(
            static_cast<const char*>(tag.data), tag.datalen)));
    }
    case FMOD_TAGDATATYPE_STRING_UTF8:
    {
        return LLSD::String(static_cast<const char*>(tag.data));
    }
    case FMOD_TAGDATATYPE_STRING_UTF16:
    {
        return LLSD::String(utf16str_to_utf8str(
            reinterpret_cast<const U16*>(tag.data), tag.datalen / 2));
    }
    case FMOD_TAGDATATYPE_STRING_UTF16BE:
    {
        // Swap high & low bytes
        const U32 len = tag.datalen / 2;
        std::vector<U16> buffer(len);
        const U16* src = reinterpret_cast<const U16*>(tag.data);
        for (U32 j = 0; j < len; ++j)
            buffer[j] = ((src[j] & 0xff) << 8) | ((src[j] & 0xff00) >> 8);
        return LLSD::String(utf16str_to_utf8str(buffer.data(), len));
    }
    default:
        break;
    }
    return LLSD();
}

} // namespace LLMetadataTags
