#ifndef FS_IMCIPHER_H
#define FS_IMCIPHER_H

#include <string>

class FSIMCipher
{
public:
    static std::string encrypt(const std::string& plaintext, const std::string& password);
    static std::string decrypt(const std::string& ciphertext_b64, const std::string& password);

    static const std::string ENCRYPTED_MARKER;

private:
    static std::string base64Encode(const unsigned char* data, size_t length);
    static std::string base64Decode(const std::string& input);
};

#endif
