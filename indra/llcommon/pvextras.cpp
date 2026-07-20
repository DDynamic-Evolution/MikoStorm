
#include "pvextras.h"

static unsigned pv_flags = PV_CONVENIENCE | PV_ENHANCED_EXPORT;
static unsigned pv_mask = 0;

static unsigned default_on_flags[] = {
    PV_CONVENIENCE,
    PV_ENHANCED_EXPORT
};

static unsigned new_flags;

void pv_set_flags(unsigned flags, unsigned mask)
{
    for (unsigned x : default_on_flags)
    {
        if (!(mask & x))
        {
            flags |= x;
            new_flags |= x;
        }
    }

    pv_flags = flags;
    pv_mask = mask | PV_FEATURE_MASK;
}

unsigned pv_get_flags()
{
    return pv_flags;
}

unsigned pv_get_mask()
{
    return pv_mask;
}

unsigned pv_new_defaulted_flags()
{
    return new_flags;
}

void pv_enable_flag(unsigned flag)
{
    pv_flags |= flag;
}

void pv_disable_flag(unsigned flag)
{
    pv_flags &= ~flag;
}

bool pv_check_flag(unsigned flag)
{
    return ((pv_flags & flag) == flag);
}

static std::pair<int, int> pv_find_jpeg2000_comment(const unsigned char* buf, std::size_t len)
{
    bool in_header = false;

    if (len < 6)
        return {0, 0};

    for (int i = 0; i < len - 3; ++i)
    {
        if (buf[i] == 0xff)
        {
            if (buf[i+1] == 0x4f)
            {
                in_header = true;
            }
            else if (buf[i+1] == 0x90 || buf[i+1] == 0xd9)
            {
        return {0, 0};
            }
            else if (in_header && buf[i+1] == 0x64)
            {
                int comment_len = (buf[i + 2] << 8) | buf[i + 3];

                if (comment_len > len - i)
                    comment_len = (int)len - i;

                return {i, comment_len + 2};
            }
        }
    }

    return {0, 0};
}

void pv_strip_jpeg2000_comment(std::string& str)
{
    auto range = pv_find_jpeg2000_comment((const unsigned char*)str.data(), str.size());
    if (range.first > 0 || range.second > 0)
    {
        auto it = str.begin() + range.first;
        str.erase(it, it + range.second);
    }
}

void pv_strip_jpeg2000_comment(std::vector<unsigned char>& str)
{
    auto range = pv_find_jpeg2000_comment((const unsigned char*)str.data(), str.size());
    if (range.first > 0 || range.second > 0)
    {
        auto it = str.begin() + range.first;
        str.erase(it, it + range.second);
    }
}


