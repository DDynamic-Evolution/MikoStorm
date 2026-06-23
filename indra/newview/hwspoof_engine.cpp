#include "llviewerprecompiledheaders.h"

#include "hwspoof_engine.h"

#include "llmd5.h"
#include "llrand.h"
#include "llstring.h"
#include "lluuid.h"

static std::string lo_seed;
static std::string lo_username;

static std::string real_serial;
static std::string real_macid_str;
static unsigned char real_nodeid[6] = {};
static unsigned char real_machineid[6] = {};
static std::string real_nodeid_str;
static std::string real_machineid_str;

static std::string spoofed_id0;
static std::string spoofed_macid;
static unsigned char faux_nodeid[6] = {};
static unsigned char faux_machineid[6] = {};
static std::string faux_nodeid_str;
static std::string faux_machineid_str;

static std::string format_mac(unsigned char mac[6])
{
    return llformat("%02x-%02x-%02x-%02x-%02x-%02x",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static std::string simulate_macid(unsigned char nodeid[6], unsigned char machineid[6])
{
    U32 sum_nodeid = 0, sum_machineid = 0;
    for (int i = 0; i < 6; ++i) sum_nodeid    += nodeid[i];
    for (int i = 0; i < 6; ++i) sum_machineid += machineid[i];

    unsigned char* input = 0;
    if (sum_machineid != 0)      input = machineid;
    else if (sum_nodeid != 0)    input = nodeid;

    char digest[33] = {};
    if (input)
    {
        LLMD5 hash;
        hash.update(input, 6);
        hash.finalize();
        hash.hex_digest(digest);
    }
    else
    {
        strcpy(digest, "00000000000000000000000000000000");
    }
    return digest;
}

static void regen_seed()
{
    LLMD5 seedgen;
    for (int i = 0; i < 4; ++i)
    {
        S32 r = ll_rand();
        seedgen.update((unsigned char*)&r, sizeof(r));
    }
    seedgen.update((unsigned char*)real_serial.data(), real_serial.size());
    seedgen.update(real_nodeid, sizeof(real_nodeid));
    seedgen.update(real_machineid, sizeof(real_machineid));
    seedgen.finalize();

    lo_seed.resize(33);
    seedgen.hex_digest((char*)&lo_seed[0]);
    lo_seed.resize(16);
}

const std::string& hwspoof_get_seed()
{
    if (lo_seed.empty()) regen_seed();
    return lo_seed;
}

const std::string& hwspoof_get_username() { return lo_username; }

static void regen_spoofed_ids()
{
    const std::string& seed     = hwspoof_get_seed();
    const std::string& username = hwspoof_get_username();

    {
        LLMD5 idgen;
        idgen.update((unsigned char*)"id0", 3);
        idgen.update((unsigned char*)seed.data(), seed.size());
        idgen.update((unsigned char*)username.data(), username.size());
        idgen.finalize();
        spoofed_id0.resize(33);
        idgen.hex_digest((char*)&spoofed_id0[0]);
        spoofed_id0.resize(32);
    }

    {
        LLMD5 idgen;
        unsigned char digest[16];
        idgen.update((unsigned char*)"fauxids", 7);
        idgen.update((unsigned char*)seed.data(), seed.size());
        idgen.update((unsigned char*)username.data(), username.size());
        idgen.finalize();
        idgen.raw_digest(digest);

        int i = 0;
        if ((real_nodeid[0] + real_nodeid[1] + real_nodeid[2]) != 0)
        {
            for (; i < 3; ++i) faux_nodeid[i] = real_nodeid[i];
        }
        else
        {
            faux_nodeid[0] = 0x00;
            faux_nodeid[1] = digest[1] & 0x1F;
            i = 2;
        }
        for (; i < 6; ++i) faux_nodeid[i] = digest[i];
        faux_nodeid_str = format_mac(faux_nodeid);

        for (int j = 0; j < 6; ++j) faux_machineid[j] = digest[6 + j];
        faux_machineid_str = format_mac(faux_machineid);
    }

    spoofed_macid = simulate_macid(faux_nodeid, faux_machineid);
}

void hwspoof_reroll_seed()          { regen_seed(); regen_spoofed_ids(); }
void hwspoof_set_seed(const std::string& seed) { lo_seed = seed; regen_spoofed_ids(); }
void hwspoof_set_username(const std::string& username)
    { lo_username = utf8str_tolower(username); regen_spoofed_ids(); }

void hwspoof_set_real_serial(std::string serial) { real_serial = serial; }

void hwspoof_set_real_nodeid(unsigned char nodeid[6])
{
    for (int i = 0; i < 6; ++i) real_nodeid[i] = nodeid[i];
    real_nodeid_str = format_mac(real_nodeid);
    real_macid_str  = simulate_macid(real_nodeid, real_machineid);
}

void hwspoof_set_real_machineid(unsigned char machineid[6])
{
    for (int i = 0; i < 6; ++i) real_machineid[i] = machineid[i];
    real_machineid_str = format_mac(real_machineid);
    real_macid_str     = simulate_macid(real_nodeid, real_machineid);
}

const std::string& hwspoof_get_real_serial()        { return real_serial; }
const std::string& hwspoof_get_real_nodeid_str()     { return real_nodeid_str; }
const std::string& hwspoof_get_real_machineid_str()  { return real_machineid_str; }
const std::string& hwspoof_get_real_macid_str()      { return real_macid_str; }

const std::string& hwspoof_get_id0()    { return spoofed_id0; }
const std::string& hwspoof_get_macid()  { return spoofed_macid; }

void hwspoof_get_faux_nodeid(unsigned char out[6])
    { for (int i = 0; i < 6; ++i) out[i] = faux_nodeid[i]; }
const std::string& hwspoof_get_faux_nodeid_str()    { return faux_nodeid_str; }

void hwspoof_get_faux_machineid(unsigned char out[6])
    { for (int i = 0; i < 6; ++i) out[i] = faux_machineid[i]; }
const std::string& hwspoof_get_faux_machineid_str() { return faux_machineid_str; }

bool hwspoof_is_initialized() { return !faux_nodeid_str.empty() && !faux_machineid_str.empty(); }

void hwspoof_fake_support_info(LLSD& info, std::string build_type_string)
{
#if LL_WINDOWS
    info["BUILD_DATE"] = "Feb  9 2026"; info["BUILD_TIME"] = "18:30:30";
    info["COMPILER"] = "MSVC";          info["COMPILER_VERSION"] = "1944";
#else
    info["BUILD_DATE"] = "Feb  9 2026"; info["BUILD_TIME"] = "20:01:31";
    info["COMPILER"] = "GCC";           info["COMPILER_VERSION"] = "130100";
#endif
    info["J2C_VERSION"]          = "KDU v8.6.1";
    info["AUDIO_DRIVER_VERSION"] = "FMOD Studio 2.03.07";
    info["SIMD"]       = "AVX2";
    info["BUILD_TYPE"] = build_type_string;
}
