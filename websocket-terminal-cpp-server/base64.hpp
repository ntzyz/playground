#pragma once

#include <iostream>
#include <string>
#include <vector>

using byte = unsigned char;

namespace base64 {

std::string encode(const std::vector<byte> &src);

}