#include "hash.hpp"

#include <array>
#include <stdexcept>

#include <openssl/evp.h>

namespace echovault {

std::string sha256Hex(const std::string& data) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    if (EVP_Digest(data.data(), data.size(), digest.data(), &length, EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("sha256 failed");
    }

    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(length * 2);
    for (unsigned int i = 0; i < length; ++i) {
        out += hex[digest[i] >> 4];
        out += hex[digest[i] & 0x0f];
    }
    return out;
}

}  // namespace echovault
