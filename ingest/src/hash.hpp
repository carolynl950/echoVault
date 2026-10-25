#pragma once

#include <string>

namespace echovault {

// SHA-256 of `data`, as 64 lowercase hex characters.
std::string sha256Hex(const std::string& data);

}  // namespace echovault
