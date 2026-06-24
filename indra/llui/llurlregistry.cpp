/**
 * @file llurlregistry.cpp
 * @author Martin Reddy
 * @brief Contains a set of Url types that can be matched in a string
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "llregex.h"
#include "llurlregistry.h"
#include "lluriparser.h"

// <FS:PP> Option to disable square-bracket links
#include "llui.h"
// </FS:PP>

#include <boost/algorithm/string/find.hpp> //for boost::ifind_first -KC

// default dummy callback that ignores any label updates from the server
void LLUrlRegistryNullCallback(const std::string &url, const std::string &label, const std::string& icon)
{
}

LLUrlRegistry::LLUrlRegistry()
{
//  mUrlEntry.reserve(20);
// [RLVa:KB] - Checked: 2010-11-01 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
    mUrlEntry.reserve(31);
// [/RLVa:KB]

    // Urls are matched in the order that they were registered
    mUrlEntryNoLink = new LLUrlEntryNoLink();
    registerUrl(mUrlEntryNoLink);
    mUrlEntryIcon = new LLUrlEntryIcon();
    registerUrl(mUrlEntryIcon);
    mLLUrlEntryInvalidSLURL = new LLUrlEntryInvalidSLURL();
    registerUrl(mLLUrlEntryInvalidSLURL);
    registerUrl(new LLUrlEntrySLURL());

    // decorated links for host names like: secondlife.com and lindenlab.com
    // <FS:Ansariel> Normalize only trusted URL
    //registerUrl(new LLUrlEntrySeconlifeURL());
    mUrlEntryTrustedUrl = new LLUrlEntrySecondlifeURL();
    registerUrl(mUrlEntryTrustedUrl);
    // </FS:Ansariel>
    registerUrl(new LLUrlEntrySimpleSecondlifeURL());

    registerUrl(new LLUrlEntryHTTP());
    mUrlEntryHTTPLabel = new LLUrlEntryHTTPLabel();
    registerUrl(mUrlEntryHTTPLabel);
    registerUrl(new LLUrlEntryAgentCompleteName());
    registerUrl(new LLUrlEntryAgentLegacyName());
    registerUrl(new LLUrlEntryAgentDisplayName());
    registerUrl(new LLUrlEntryAgentUserName());
// [RLVa:KB] - Checked: 2010-11-01 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
    registerUrl(new LLUrlEntryAgentRLVAnonymizedName());
// [/RLVa:KB]
    registerUrl(new FSUrlEntryAgentSelf());// <FS:Ansariel> FIRE-30611: "You" in transcript is underlined
    // LLUrlEntryAgent*Name must appear before LLUrlEntryAgent since
    // LLUrlEntryAgent is a less specific (catchall for agent urls)
    mUrlEntryAgentMention = new LLUrlEntryAgentMention();
    registerUrl(mUrlEntryAgentMention);
    registerUrl(new LLUrlEntryAgent());
    registerUrl(new LLUrlEntryChat());
    registerUrl(new LLUrlEntryGroup());
    registerUrl(new LLUrlEntryParcel());
    registerUrl(new LLUrlEntryTeleport());
    registerUrl(new LLUrlEntryRegion());
    registerUrl(new LLUrlEntryWorldMap());
    registerUrl(new LLUrlEntryObjectIM());
    registerUrl(new LLUrlEntryPlace());
    registerUrl(new LLUrlEntryInventory());
    registerUrl(new LLUrlEntryExperienceProfile());
    mUrlEntryKeybinding = new LLUrlEntryKeybinding();
    registerUrl(mUrlEntryKeybinding);
    registerUrl(new FSHelpDebugUrlEntrySL()); // <FS:Ansariel> FS Help SLUrl
    // <FS:Ansariel> Wear folder SLUrl
    mUrlEntryWear = new FSUrlEntryWear();
    registerUrl(mUrlEntryWear);
    //LLUrlEntrySL and LLUrlEntrySLLabel have more common pattern,
    //so it should be registered in the end of list
    registerUrl(new LLUrlEntrySL());
    mUrlEntrySLLabel = new LLUrlEntrySLLabel();
    registerUrl(mUrlEntrySLLabel);
    // <FS:Ansariel> Allow URLs with no protocol again
    registerUrl(new LLUrlEntryHTTPNoProtocol());
    registerUrl(new LLUrlEntryEmail());
    registerUrl(new LLUrlEntryIPv6());
    // parse jira issue names to links -KC
    registerUrl(new LLUrlEntryJira());
}

LLUrlRegistry::~LLUrlRegistry()
{
    // free all of the LLUrlEntryBase objects we are holding
    std::vector<LLUrlEntryBase *>::iterator it;
    for (it = mUrlEntry.begin(); it != mUrlEntry.end(); ++it)
    {
        delete *it;
    }
}

void LLUrlRegistry::registerUrl(LLUrlEntryBase *url, bool force_front)
{
    if (url)
    {
        if (force_front)  // IDEVO
            mUrlEntry.insert(mUrlEntry.begin(), url);
        else
        mUrlEntry.push_back(url);
    }
}

static bool matchRegex(const char *text, boost::regex regex, U32 &start, U32 &end)
{
    boost::cmatch result;
    bool found;

    found = ll_regex_search(text, result, regex);

    if (! found)
    {
        return false;
    }

    // return the first/last character offset for the matched substring
    start = static_cast<U32>(result[0].first - text);
    end = static_cast<U32>(result[0].second - text) - 1;

    // we allow certain punctuation to terminate a Url but not match it,
    // e.g., "http://foo.com/." should just match "http://foo.com/"
    if (text[end] == '.' || text[end] == ',')
    {
        end--;
    }
    // ignore a terminating ')' when Url contains no matching '('
    // see DEV-19842 for details
    else if (text[end] == ')' && std::string(text+start, end-start).find('(') == std::string::npos)
    {
        end--;
    }

    else if (text[end] == ']' && std::string(text+start, end-start).find('[') == std::string::npos)
    {
            end--;
    }

    return true;
}

static bool stringHasUrl(const std::string &text)
{
    // fast heuristic test for a URL in a string. This is used
    // to avoid lots of costly regex calls, BUT it needs to be
    // kept in sync with the LLUrlEntry regexes we support.
    return (text.find("://") != std::string::npos ||
            // text.find("www.") != std::string::npos ||
            // text.find(".com") != std::string::npos ||
            // allow ALLCAPS urls -KC
            boost::ifind_first(text, "www.") ||
            boost::ifind_first(text, ".com") ||
            boost::ifind_first(text, ".net") ||
            boost::ifind_first(text, ".edu") ||
            boost::ifind_first(text, ".org") ||
            text.find("<nolink>") != std::string::npos ||
            text.find("<icon") != std::string::npos ||
            text.find("@") != std::string::npos);
}

static bool stringHasJira(const std::string &text)
{
    // same as above, but for jiras
    // <FS:CR> Please make sure to sync these with the items in LLUrlEntryJira::LLUrlEntryJira() if you make a change
    return (text.find("ARVD") != std::string::npos ||
            text.find("BUG") != std::string::npos ||
            text.find("CHOP") != std::string::npos ||
            text.find("CHUIBUG") != std::string::npos ||
            text.find("CTS") != std::string::npos ||
            text.find("DOC") != std::string::npos ||
            text.find("DN") != std::string::npos ||
            text.find("ECC") != std::string::npos ||
            text.find("EXP") != std::string::npos ||
            text.find("FIRE") != std::string::npos ||
            text.find("FITMESH") != std::string::npos ||
            text.find("LEAP") != std::string::npos ||
            text.find("LLSD") != std::string::npos ||
            text.find("MATBUG") != std::string::npos ||
            text.find("MISC") != std::string::npos ||
            text.find("OPEN") != std::string::npos ||
            text.find("PATHBUG") != std::string::npos ||
            text.find("PLAT") != std::string::npos ||
            text.find("PYO") != std::string::npos ||
            text.find("SCR") != std::string::npos ||
            text.find("SH") != std::string::npos ||
            text.find("SINV") != std::string::npos ||
            text.find("SLS") != std::string::npos ||
            text.find("SNOW") != std::string::npos ||
            text.find("SOCIAL") != std::string::npos ||
            text.find("STORM") != std::string::npos ||
            text.find("SUN") != std::string::npos ||
            text.find("SUP") != std::string::npos ||
            text.find("SVC") != std::string::npos ||
            text.find("TPV") != std::string::npos ||
            text.find("VWR") != std::string::npos ||
            text.find("WEB") != std::string::npos);
}

// <FS:PP> Option to disable bracket links needs is_nearby_chat here
// bool LLUrlRegistry::findUrl(const std::string &text, LLUrlMatch &match, const LLUrlLabelCallback &cb, bool is_content_trusted, bool skip_non_mentions)
bool LLUrlRegistry::findUrl(const std::string &text, LLUrlMatch &match, const LLUrlLabelCallback &cb, bool is_content_trusted, bool skip_non_mentions, bool is_nearby_chat)
// </FS:PP>
{
    // avoid costly regexes if there is clearly no URL in the text
    if (! (stringHasUrl(text) || stringHasJira(text)))
    {
        return false;
    }

    // find the first matching regex from all url entries in the registry
    U32 match_start = 0, match_end = 0;
    LLUrlEntryBase *match_entry = NULL;

    std::vector<LLUrlEntryBase *>::iterator it;
    for (it = mUrlEntry.begin(); it != mUrlEntry.end(); ++it)
    {
        //Skip for url entry icon if content is not trusted
        if((mUrlEntryIcon == *it) && ((text.find("Hand") != std::string::npos) || !is_content_trusted))
        {
            continue;
        }

        if (skip_non_mentions && (mUrlEntryAgentMention != *it))
        {
            continue;
        }

        LLUrlEntryBase *url_entry = *it;

        U32 start = 0, end = 0;
        if (matchRegex(text.c_str(), url_entry->getPattern(), start, end))
        {
            // does this match occur in the string before any other match
            if (start < match_start || match_entry == NULL)
            {

                if (mLLUrlEntryInvalidSLURL == *it)
                {
                    if(url_entry && url_entry->isSLURLvalid(text.substr(start, end - start + 1)))
                    {
                        continue;
                    }
                }

                if((mUrlEntryHTTPLabel == *it) || (mUrlEntrySLLabel == *it))
                {
                    if(url_entry && !url_entry->isWikiLinkCorrect(text.substr(start, end - start + 1)))
                    {
                        continue;
                    }
                }

                match_start = start;
                match_end = end;
                match_entry = url_entry;

                // <FS:Ansariel> Wear folder SLUrl
                if (mUrlEntryWear == *it)
                {
                    break;
                }
                // </FS:Ansariel>
            }
        }
    }

    // did we find a match? if so, return its details in the match object
    if (match_entry)
    {
        // Skip if link is an email with an empty username (starting with @). See MAINT-5371.
        if (match_start > 0 && text.substr(match_start - 1, 1) == "@")
            return false;

        // fill in the LLUrlMatch object and return it
        std::string url = text.substr(match_start, match_end - match_start + 1);

        // <FS:Ansariel> Fix the "nolink>" fail; Fix from Alchemy viewer, courtesy of Drake Arconis
        //if (match_entry == mUrlEntryTrusted)
        //{
        //  LLUriParser up(url);
        //  if (up.normalize() == 0)
        //    {
        //        url = up.normalizedUri();
        //    }
        //}
        if (match_entry != mUrlEntryNoLink && match_entry == mUrlEntryTrustedUrl)
        {
            LLUriParser up(url);
            if (up.normalize())
            {
                url = up.normalizedUri();
            }
        }
        // </FS:Ansariel>

        match.setValues(match_start, match_end,
                        match_entry->getUrl(url),
                        match_entry->getLabel(url, cb),
                        match_entry->getQuery(url),
                        match_entry->getTooltip(url),
                        match_entry->getIcon(url),
                        match_entry->getStyle(url),
                        match_entry->getMenuName(),
                        match_entry->getLocation(url),
                        // <FS:Ansariel> Store matched text
                        text.substr(match_start, match_end - match_start + 1),
                        match_entry->getID(url),
                        match_entry->getUnderline(url),
                        match_entry->isTrusted(),
                        match_entry->getSkipProfileIcon(url));

        // <FS:PP> Preview real URLs of bracket links
        static LLUICachedControl<bool> sDisableLabeledLinks("FSDisableLabeledChatLinks", false);
        static LLUICachedControl<bool> sDisableLabeledLinksNearby("FSDisableLabeledChatLinksNearbyChat", false);
        if (!is_content_trusted && (match_entry == mUrlEntryHTTPLabel) && (is_nearby_chat ? sDisableLabeledLinksNearby : sDisableLabeledLinks) && match.getLabel() != match.getUrl())
        {
            match.setLabeledLinkMasked(true);
            if (mUrlEntryTrustedUrl)
            {
                U32 trusted_start = 0, trusted_end = 0;
                const std::string& real_url = match.getUrl();
                bool url_trusted = matchRegex(real_url.c_str(), mUrlEntryTrustedUrl->getPattern(), trusted_start, trusted_end) && (trusted_start == 0);
                if (!url_trusted)
                {
                    const std::string slashed_url = real_url + "/";
                    url_trusted = matchRegex(slashed_url.c_str(), mUrlEntryTrustedUrl->getPattern(), trusted_start, trusted_end) && (trusted_start == 0);
                }
                if (url_trusted)
                {
                    match.setLabeledLinkTrusted(true);
                }
            }
        }
        // </FS:PP>

        // URL security check
        {
            std::string checked_url = match_entry->getUrl(url);
            std::string warning;
            ESecurityStatus security = checkUrlSecurity(checked_url, warning);
            match.setSecurityStatus(security);
            match.setSecurityMessage(warning);
        }

        return true;
    }

    return false;
}

bool LLUrlRegistry::findUrl(const LLWString &text, LLUrlMatch &match, const LLUrlLabelCallback &cb)
{
    // boost::regex_search() only works on char or wchar_t
    // types, but wchar_t is only 2-bytes on Win32 (not 4).
    // So we use UTF-8 to make this work the same everywhere.
    std::string utf8_text = wstring_to_utf8str(text);
    if (findUrl(utf8_text, match, cb))
    {
        // we cannot blindly return the start/end offsets from
        // the UTF-8 string because it is a variable-length
        // character encoding, so we need to update the start
        // and end values to be correct for the wide string.
        // <FS:Ansariel> Fix for LLUrlEntryHTTPLabel and
        // LLUrlEntrySLLabel: Cannot simply replace the URL,
        //need to replace the matched text!
        //LLWString wurl = utf8str_to_wstring(match.getUrl());
        LLWString wurl = utf8str_to_wstring(match.getMatchedText());
        // </FS:Ansariel>
        size_t start = text.find(wurl);
        if (start == std::string::npos)
        {
            return false;
        }
        auto end = start + wurl.size() - 1;

        match.setValues(static_cast<U32>(start), static_cast<U32>(end), match.getUrl(),
                        match.getLabel(),
                        match.getQuery(),
                        match.getTooltip(),
                        match.getIcon(),
                        match.getStyle(),
                        match.getMenuName(),
                        match.getLocation(),
                        // <FS:Ansariel> Store matched text
                        match.getMatchedText(),
                        match.getID(),
                        match.getUnderline(),
                        false,
                        match.getSkipProfileIcon());
        return true;
    }
    return false;
}

// URL Security Checking Implementation

// ---------------------------------------------------------------------------
// SL-related domains that might be typosquatted for phishing
// Note: entries ending with ".*" are wildcard prefixes
// (e.g., "lindenlab.com.*" matches any subdomain of lindenlab.com)
// ---------------------------------------------------------------------------
static const char* sSLDomains[] = {
    // Core
    "secondlife.com",
    "secondlife.net",
    "secondlifegrid.net",
    "secondlife.org",
    "secondlife.io",
    "secondlife.community",
    "lindenlab.com",
    "slurl.com",

    // secondlife.com subdomains (explicit enumeration)
    "www.secondlife.com",
    "marketplace.secondlife.com",
    "community.secondlife.com",
    "wiki.secondlife.com",
    "maps.secondlife.com",
    "support.secondlife.com",
    "status.secondlife.com",
    "accounts.secondlife.com",
    "my.secondlife.com",
    "join.secondlife.com",
    "premium.secondlife.com",
    "go.secondlife.com",
    "blog.secondlife.com",
    "engage.secondlife.com",

    // lindenlab.com subdomains (wildcard patterns)
    "www.lindenlab.com",
    "lindenlab.com.*",      // *.lindenlab.com wildcard
    "agni.lindenlab.com.*", // *.agni.lindenlab.com wildcard
    "aditi.lindenlab.com.*",// *.aditi.lindenlab.com wildcard

    NULL
};

// ---------------------------------------------------------------------------
// Homoglyph mapping: Cyrillic / lookalike Unicode chars -> ASCII
// ---------------------------------------------------------------------------
struct HomoglyphMap { llwchar homoglyph; char ascii; };
static const HomoglyphMap sHomoglyphs[] = {
    { 0x0430, 'a' }, // Cyrillic small a
    { 0x0435, 'e' }, // Cyrillic small ie
    { 0x043E, 'o' }, // Cyrillic small o
    { 0x0440, 'p' }, // Cyrillic small er
    { 0x0441, 'c' }, // Cyrillic small es
    { 0x0445, 'x' }, // Cyrillic small ha
    { 0x0432, 'b' }, // Cyrillic small ve
    { 0x043D, 'h' }, // Cyrillic small en
    { 0x043A, 'k' }, // Cyrillic small ka
    { 0x043C, 'm' }, // Cyrillic small em
    { 0x0438, 'i' }, // Cyrillic small i
    { 0x0456, 'i' }, // Cyrillic small Byelorussian/Ukrainian i
    { 0x04CF, 'l' }, // Cyrillic small palochka (used in Caucasus languages)
    { 0x00ED, 'i' }, // Latin i with acute
    { 0x0442, 't' }, // Cyrillic small te
    { 0x0443, 'y' }, // Cyrillic small u
    { 0, 0 }
};

// ---------------------------------------------------------------------------
// Normalize a domain: lowercase, replace homoglyph chars with ASCII
// ---------------------------------------------------------------------------
std::string LLUrlRegistry::normalizeDomain(const std::string& domain)
{
    LLWString wdomain = utf8str_to_wstring(domain);
    std::string result;
    result.reserve(wdomain.size());

    for (U32 i = 0; i < wdomain.size(); ++i)
    {
        llwchar wc = wdomain[i];

        if (wc >= 'A' && wc <= 'Z')
        {
            result += (char)(wc - 'A' + 'a');
        }
        else
        {
            bool replaced = false;
            for (S32 j = 0; sHomoglyphs[j].homoglyph != 0; ++j)
            {
                if (wc == sHomoglyphs[j].homoglyph)
                {
                    result += sHomoglyphs[j].ascii;
                    replaced = true;
                    break;
                }
            }
            if (!replaced && wc < 128)
            {
                result += (char)wc;
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Extract eTLD+1 (simplified): last 2-3 labels handling common multi-part TLDs
// ---------------------------------------------------------------------------
std::string LLUrlRegistry::extractETLDPlusOne(const std::string& domain)
{
    // Split domain by '.'
    std::vector<std::string> labels;
    std::string label;
    for (size_t i = 0; i < domain.size(); ++i)
    {
        if (domain[i] == '.')
        {
            if (!label.empty())
            {
                labels.push_back(label);
                label.clear();
            }
        }
        else
        {
            label += domain[i];
        }
    }
    if (!label.empty())
        labels.push_back(label);

    if (labels.size() < 2)
        return domain;

    // Common two-part TLDs (simplified list)
    static const char* sTwoPartTLDs[] = {
        "co.uk", "org.uk", "ac.uk", "gov.uk", "net.uk", "nhs.uk",
        "com.au", "net.au", "org.au", "gov.au", "edu.au",
        "co.nz", "net.nz", "org.nz",
        "co.jp", "ne.jp", "or.jp",
        "co.kr", "ne.kr", "or.kr",
        "com.cn", "net.cn", "org.cn", "gov.cn",
        "co.in", "net.in", "org.in", "gov.in",
        "co.za", "net.za", "org.za", "gov.za",
        "com.br", "net.br", "org.br", "gov.br",
        "com.mx", "org.mx",
        "co.il", "org.il", "net.il", "ac.il", "gov.il",
        "com.pl", "net.pl", "org.pl",
        "co.th", "or.th", "go.th",
        "com.tr", "net.tr", "org.tr", "gov.tr",
        "com.hk", "net.hk", "org.hk", "gov.hk",
        "com.sg", "net.sg", "org.sg", "gov.sg",
        "co.at", "or.at",
        "com.tw", "net.tw", "org.tw", "gov.tw",
        "co.hu", "net.hu", "org.hu",
        "co.ve", "com.ve", "net.ve", "org.ve", "gov.ve",
        "com.eg", "net.eg", "org.eg", "gov.eg",
        NULL
    };

    if (labels.size() >= 3)
    {
        std::string last_two = labels[labels.size() - 2] + "." + labels[labels.size() - 1];
        for (S32 i = 0; sTwoPartTLDs[i] != NULL; ++i)
        {
            if (last_two == sTwoPartTLDs[i])
            {
                // Return last three labels
                return labels[labels.size() - 3] + "." + last_two;
            }
        }
    }

    // Default: return last two labels
    return labels[labels.size() - 2] + "." + labels[labels.size() - 1];
}

// ---------------------------------------------------------------------------
// Levenshtein distance (iterative, O(n*m))
// ---------------------------------------------------------------------------
U32 LLUrlRegistry::levenshteinDistance(const std::string& a, const std::string& b)
{
    size_t n = a.size();
    size_t m = b.size();

    if (n == 0) return (U32)m;
    if (m == 0) return (U32)n;

    // Use two rows to reduce memory
    std::vector<U32> prev(m + 1);
    std::vector<U32> cur(m + 1);

    for (size_t j = 0; j <= m; ++j)
        prev[j] = (U32)j;

    for (size_t i = 1; i <= n; ++i)
    {
        cur[0] = (U32)i;
        for (size_t j = 1; j <= m; ++j)
        {
            U32 cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            cur[j] = llmin(prev[j] + 1,              // deletion
                      llmin(cur[j - 1] + 1,          // insertion
                            prev[j - 1] + cost));    // substitution
        }
        prev.swap(cur);
    }

    return prev[m];
}

// ---------------------------------------------------------------------------
// Check if domain (normalized eTLD+1) is a known SL domain.
// Supports wildcard entries ending with ".*" which match any subdomain.
// ---------------------------------------------------------------------------
bool LLUrlRegistry::isSLDomain(const std::string& domain)
{
    for (S32 i = 0; sSLDomains[i] != NULL; ++i)
    {
        std::string entry(sSLDomains[i]);

        // Wildcard pattern: "prefix.*" matches any subdomain of prefix
        if (entry.size() >= 2 && entry.substr(entry.size() - 2) == ".*")
        {
            std::string prefix = entry.substr(0, entry.size() - 2);
            // Domain must have at least one extra label before the prefix
            if (domain.size() > prefix.size() + 1
                && domain.compare(domain.size() - prefix.size(), prefix.size(), prefix) == 0
                && domain[domain.size() - prefix.size() - 1] == '.')
            {
                return true;
            }
        }
        else if (domain == entry)
        {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Check if the domain looks like a typosquat of an SL domain
// ---------------------------------------------------------------------------
bool LLUrlRegistry::isSLTyposquatAttempt(const std::string& domain)
{
    for (S32 i = 0; sSLDomains[i] != NULL; ++i)
    {
        U32 dist = levenshteinDistance(domain, std::string(sSLDomains[i]));
        // Distance 1 is extremely suspicious (single char diff)
        // Distance 2 could be coincidence for short domains
        if (dist <= 1)
            return true;
        if (dist <= 2 && strlen(sSLDomains[i]) > 4)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Subdomain trick detection: SL domain used as a subdomain in a different
// registered domain. e.g., "secondlife.login.evil.com" or
// "secondlife.com.evil.com"
// Also catches typosquat/impersonation domains in subdomain chain
// ---------------------------------------------------------------------------
bool LLUrlRegistry::isSLSubdomainTrick(const std::string& domain, const std::string& registrable_domain)
{
    std::vector<std::string> labels;
    std::string label;
    for (size_t i = 0; i < domain.size(); ++i)
    {
        if (domain[i] == '.')
        {
            if (!label.empty())
            {
                labels.push_back(label);
                label.clear();
            }
        }
        else
        {
            label += domain[i];
        }
    }
    if (!label.empty())
        labels.push_back(label);

    if (labels.size() < 3)
        return false;

    // Check each subdomain label sequence for SL domain matches
    for (size_t i = 0; i < labels.size() - 1; ++i)
    {
        std::string candidate;
        for (size_t j = i; j < labels.size() - 1; ++j)
        {
            if (!candidate.empty())
                candidate += ".";
            candidate += labels[j];

            if (candidate == registrable_domain)
                continue;

            if (isSLDomain(candidate))
                return true;

            if (isSLTyposquatAttempt(candidate))
                return true;
            if (isSLBrandImpersonation(candidate))
                return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Brand impersonation: domain name contains "secondlife", "lindenlab", or
// "slurl" as a substring of its registrable name (leftmost label of eTLD+1)
// e.g. "secondlife-marketplace.com", "mylindenlabs.com"
// ---------------------------------------------------------------------------
bool LLUrlRegistry::isSLBrandImpersonation(const std::string& domain)
{
    // Extract the name part (first label of the eTLD+1)
    // e.g. "secondlife-marketplace" from "secondlife-marketplace.com"
    std::string::size_type dot = domain.find('.');
    if (dot == std::string::npos)
        return false;
    std::string name = domain.substr(0, dot);

    static const char* sBrandKeywords[] = {
        "secondlife",
        "lindenlab",
        "slurl",
        NULL
    };

    for (S32 i = 0; sBrandKeywords[i] != NULL; ++i)
    {
        if (name.find(sBrandKeywords[i]) != std::string::npos)
            return true;
        // Check if name is a typosquat of the brand keyword
        if (levenshteinDistance(name, sBrandKeywords[i]) <= 2)
            return true;
        // Also check each hyphen-separated segment
        std::string::size_type start = 0;
        std::string::size_type hyphen;
        while ((hyphen = name.find('-', start)) != std::string::npos)
        {
            std::string segment = name.substr(start, hyphen - start);
            if (!segment.empty() && levenshteinDistance(segment, sBrandKeywords[i]) <= 2)
                return true;
            start = hyphen + 1;
        }
        std::string last_segment = name.substr(start);
        if (!last_segment.empty() && levenshteinDistance(last_segment, sBrandKeywords[i]) <= 2)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Main security check entry point
// ---------------------------------------------------------------------------
ESecurityStatus LLUrlRegistry::checkUrlSecurity(const std::string& url, std::string& warning_msg)
{
    // Extract hostname from URL
    std::string hostname;
    std::string::size_type scheme_end = url.find("://");
    std::string::size_type host_start = (scheme_end != std::string::npos) ? scheme_end + 3 : 0;

    // Skip past scheme
    std::string::size_type host_end = url.find_first_of("/?#", host_start);
    if (host_end == std::string::npos)
        host_end = url.size();

    hostname = url.substr(host_start, host_end - host_start);

    // Remove userinfo (user:pass@host)
    std::string::size_type at_pos = hostname.find('@');
    if (at_pos != std::string::npos)
        hostname = hostname.substr(at_pos + 1);

    // Remove port
    std::string::size_type port_pos = hostname.find(':');
    if (port_pos != std::string::npos)
        hostname = hostname.substr(0, port_pos);

    if (hostname.empty())
        return SECURITY_NONE;

    // Check if URL security checking is enabled (user preference)
    static LLUICachedControl<bool> sEnableURLChecking("FSEnableURLChecking", true);
    if (!sEnableURLChecking)
        return SECURITY_NONE;

    // Check for homoglyph-based impersonation BEFORE allowlist check.
    // If the domain contains non-ASCII characters and normalizing it
    // reveals an SL domain, it's a homoglyph attack.
    bool has_non_ascii = false;
    for (size_t i = 0; i < hostname.size(); ++i)
    {
        if ((unsigned char)hostname[i] >= 128)
        {
            has_non_ascii = true;
            break;
        }
    }
    if (has_non_ascii)
    {
        std::string normalized_full = normalizeDomain(hostname);
        std::string normalized_etld1 = extractETLDPlusOne(normalized_full);
        if (isSLDomain(normalized_etld1))
        {
            warning_msg = "Suspicious link: domain uses lookalike characters to impersonate Second Life";
            return SECURITY_WARNING;
        }
    }

    // Extract eTLD+1 and check if it's a known SL domain (exact match)
    std::string raw_etld1 = extractETLDPlusOne(hostname);
    if (isSLDomain(raw_etld1))
        return SECURITY_NONE;

    // Normalize (lowercase + homoglyph replacement) for further checks
    std::string normalized = normalizeDomain(hostname);
    std::string etld1 = extractETLDPlusOne(normalized);

    // Check subdomain tricks (SL domain used as subdomain in a non-SL domain)
    if (isSLSubdomainTrick(normalized, etld1))
    {
        warning_msg = "Suspicious link: may impersonate a Second Life website";
        return SECURITY_WARNING;
    }

    // Check typosquatting of SL domains
    if (isSLTyposquatAttempt(etld1))
    {
        warning_msg = "Suspicious link: domain looks similar to Second Life";
        return SECURITY_WARNING;
    }

    // Check brand impersonation (domain name contains SL keyword as substring)
    if (isSLBrandImpersonation(etld1))
    {
        warning_msg = "Suspicious link: domain name references Second Life";
        return SECURITY_WARNING;
    }

    return SECURITY_NONE;
}

bool LLUrlRegistry::hasUrl(const std::string &text)
{
    LLUrlMatch match;
    return findUrl(text, match);
}

bool LLUrlRegistry::hasUrl(const LLWString &text)
{
    LLUrlMatch match;
    return findUrl(text, match);
}

bool LLUrlRegistry::isUrl(const std::string &text)
{
    LLUrlMatch match;
    if (findUrl(text, match))
    {
        return (match.getStart() == 0 && match.getEnd() >= text.size()-1);
    }
    return false;
}

bool LLUrlRegistry::isUrl(const LLWString &text)
{
    LLUrlMatch match;
    if (findUrl(text, match))
    {
        return (match.getStart() == 0 && match.getEnd() >= text.size()-1);
    }
    return false;
}

void LLUrlRegistry::setKeybindingHandler(LLKeyBindingToStringHandler* handler)
{
    LLUrlEntryKeybinding *entry = (LLUrlEntryKeybinding*)mUrlEntryKeybinding;
    entry->setHandler(handler);
}

bool LLUrlRegistry::containsAgentMention(const std::string& text)
{
    // avoid costly regexes if there is clearly no URL in the text
    if (!stringHasUrl(text))
    {
        return false;
    }

    try
    {
        boost::sregex_iterator it(text.begin(), text.end(), mUrlEntryAgentMention->getPattern());
        boost::sregex_iterator end;
        for (; it != end; ++it)
        {
            if (mUrlEntryAgentMention->isAgentID(it->str()))
            {
               return true;
            }
        }
    }
    catch (boost::regex_error&)
    {
        LL_INFOS() << "Regex error for: " << text << LL_ENDL;
    }
    return false;
}
