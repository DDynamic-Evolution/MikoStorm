#ifndef LL_METADATA_TAGS_H
#define LL_METADATA_TAGS_H

#include "llsd.h"
#include <string>

struct FMOD_TAG;

namespace LLMetadataTags
{
    // Normalise an FMOD tag name to the canonical form used by
    // LLStreamingAudioInterface's metadata LLSD (e.g. "TIT2" -> "TITLE",
    // "TPE1" -> "ARTIST", "title" -> "TITLE").
    // Returns the (possibly-renamed) name when the tag should be stored,
    // or an empty string when it should be skipped (e.g. FMOD internal
    // tags such as "Sample Rate Change").
    std::string normaliseName(const FMOD_TAG& tag);

    // Convert an FMOD tag's data to an LLSD value, handling all
    // FMOD_TAGDATATYPE variants (INT, FLOAT, STRING, UTF8, UTF16,
    // UTF16BE). Returns the converted value.
    LLSD convertValue(const FMOD_TAG& tag);
}

#endif // LL_METADATA_TAGS_H
