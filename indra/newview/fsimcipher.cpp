#include "llviewerprecompiledheaders.h"

#include "fsimcipher.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <vector>

const std::string FSIMCipher::ENCRYPTED_MARKER = "[E2E]";

static const int PBKDF2_ITERATIONS = 100000;
static const int SALT_SIZE = 16;
static const int IV_SIZE = 12;
static const int TAG_SIZE = 16;
static const int KEY_SIZE = 32;

std::string FSIMCipher::encrypt(const std::string& plaintext, const std::string& password)
{
    if (plaintext.empty() || password.empty())
        return std::string();

    unsigned char salt[SALT_SIZE];
    unsigned char iv[IV_SIZE];
    unsigned char key[KEY_SIZE];

    if (!RAND_bytes(salt, SALT_SIZE) || !RAND_bytes(iv, IV_SIZE))
        return std::string();

    if (!PKCS5_PBKDF2_HMAC_SHA1(password.c_str(), (int)password.length(),
                                 salt, SALT_SIZE, PBKDF2_ITERATIONS,
                                 KEY_SIZE, key))
        return std::string();

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return std::string();

    std::vector<unsigned char> ciphertext(plaintext.length() + EVP_CIPHER_block_size(EVP_aes_256_gcm()));
    int outlen = 0, tmplen = 0;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL) != 1 ||
        EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1 ||
        EVP_EncryptUpdate(ctx, ciphertext.data(), &outlen,
                          (const unsigned char*)plaintext.c_str(), (int)plaintext.length()) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return std::string();
    }

    int ciphertext_len = outlen;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + outlen, &tmplen) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return std::string();
    }
    ciphertext_len += tmplen;

    unsigned char tag[TAG_SIZE];
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return std::string();
    }
    EVP_CIPHER_CTX_free(ctx);

    std::vector<unsigned char> result;
    result.push_back(0x01);
    result.insert(result.end(), salt, salt + SALT_SIZE);
    result.insert(result.end(), iv, iv + IV_SIZE);
    result.insert(result.end(), ciphertext.begin(), ciphertext.begin() + ciphertext_len);
    result.insert(result.end(), tag, tag + TAG_SIZE);

    return base64Encode(result.data(), result.size());
}

std::string FSIMCipher::decrypt(const std::string& ciphertext_b64, const std::string& password)
{
    if (ciphertext_b64.empty() || password.empty())
        return std::string();

    std::string decoded = base64Decode(ciphertext_b64);
    if (decoded.empty())
        return std::string();

    if (decoded.size() < 1 + SALT_SIZE + IV_SIZE + TAG_SIZE)
        return std::string();

    size_t offset = 0;
    unsigned char version = (unsigned char)decoded[offset++];
    if (version != 0x01)
        return std::string();

    const unsigned char* salt = (const unsigned char*)decoded.data() + offset;
    offset += SALT_SIZE;
    const unsigned char* iv = (const unsigned char*)decoded.data() + offset;
    offset += IV_SIZE;
    const unsigned char* ciphertext = (const unsigned char*)decoded.data() + offset;
    size_t ciphertext_len = decoded.size() - offset - TAG_SIZE;
    const unsigned char* tag = (const unsigned char*)decoded.data() + offset + ciphertext_len;

    unsigned char key[KEY_SIZE];
    if (!PKCS5_PBKDF2_HMAC_SHA1(password.c_str(), (int)password.length(),
                                 salt, SALT_SIZE, PBKDF2_ITERATIONS,
                                 KEY_SIZE, key))
        return std::string();

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return std::string();

    std::vector<unsigned char> plaintext(ciphertext_len + EVP_CIPHER_block_size(EVP_aes_256_gcm()));
    int outlen = 0, tmplen = 0;

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, IV_SIZE, NULL) != 1 ||
        EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1 ||
        EVP_DecryptUpdate(ctx, plaintext.data(), &outlen,
                          ciphertext, (int)ciphertext_len) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return std::string();
    }
    int plaintext_len = outlen;

    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE, (void*)tag))
    {
        EVP_CIPHER_CTX_free(ctx);
        return std::string();
    }

    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + outlen, &tmplen);
    EVP_CIPHER_CTX_free(ctx);

    if (ret <= 0)
        return std::string();

    plaintext_len += tmplen;

    return std::string((const char*)plaintext.data(), plaintext_len);
}

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string FSIMCipher::base64Encode(const unsigned char* data, size_t length)
{
    std::string result;
    result.reserve((length + 2) / 3 * 4);

    for (size_t i = 0; i < length; i += 3)
    {
        unsigned int b = ((unsigned int)data[i]) << 16;
        if (i + 1 < length) b |= ((unsigned int)data[i + 1]) << 8;
        if (i + 2 < length) b |= data[i + 2];

        result += b64_table[(b >> 18) & 0x3F];
        result += b64_table[(b >> 12) & 0x3F];
        result += (i + 1 < length) ? b64_table[(b >> 6) & 0x3F] : '=';
        result += (i + 2 < length) ? b64_table[b & 0x3F] : '=';
    }
    return result;
}

static int b64_index(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::string FSIMCipher::base64Decode(const std::string& input)
{
    if (input.empty() || input.length() % 4 != 0)
        return std::string();

    std::string result;
    result.reserve(input.length() / 4 * 3);

    for (size_t i = 0; i < input.length(); i += 4)
    {
        int b0 = b64_index(input[i]);
        int b1 = b64_index(input[i + 1]);
        int b2 = b64_index(input[i + 2]);
        int b3 = b64_index(input[i + 3]);

        if (b0 < 0 || b1 < 0)
            return std::string();

        unsigned int b = (b0 << 18) | (b1 << 12);
        if (b2 >= 0) b |= (b2 << 6);
        if (b3 >= 0) b |= b3;

        result += (char)((b >> 16) & 0xFF);
        if (b2 >= 0) result += (char)((b >> 8) & 0xFF);
        if (b3 >= 0) result += (char)(b & 0xFF);
    }
    return result;
}
