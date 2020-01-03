/* Public Domain from https://gist.github.com/ynov/aead6c4a4208e1808fb2, modified */

#include "base64.hpp"

#include <cstdint>
#include <cstring>

namespace base64 {
    
static bool dlookupok = false;
static char dlookup[256];
const static char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const static char padchar = '=';

const char *getdlookup()
{
    if (!dlookupok) {
        memset(dlookup, 0, sizeof(dlookup));
        for (size_t i = 0; i < strlen(lookup); i++) {
            dlookup[(int) lookup[i]] = i;
        }
    }

    dlookupok = true;
    return dlookup;
}

std::string encode(const std::vector<byte> &src)
{
    std::string encoded;

    int outputSize = 4 * ((src.size() / 3) + (src.size() % 3 > 0 ? 1 : 0));
    encoded.reserve(outputSize);

    uint32_t tmp;
    int i, padding;
    int srcsz = src.size();

    for (i = 0; i < srcsz; i += 3) {
        padding = (i + 3) - srcsz;
        tmp = 0;

        switch (padding) {
        case 2:
            tmp |= (src[i] << 16);
            break;
        case 1:
            tmp |= (src[i] << 16) | (src[i + 1] << 8);
            break;
        default:
            tmp |= (src[i] << 16) | (src[i + 1] << 8) | (src[i + 2]);
            break;
        }

        encoded +=                         lookup[(tmp & 0xfc0000) >> 18];
        encoded +=                         lookup[(tmp & 0x03f000) >> 12];
        encoded += padding > 1 ? padchar : lookup[(tmp & 0x000fc0) >> 6];
        encoded += padding > 0 ? padchar : lookup[(tmp & 0x00003f)];
    }

    return encoded;
}

std::vector<byte> decode(const std::string &src)
{
    const char *dlookup = getdlookup();
    std::vector<byte> decoded;

    int outputSize = (src.size() / 4) * 3;
    decoded.reserve(outputSize);

    char ch[4];
    uint32_t tmp;
    size_t i, j;
    int padding = 0;

    for (i = 0; i < src.size(); i += 4) {
        for (j = 0; j < 4; j++) {
            if (src[i + j] == padchar) {
                ch[j] = '\0';
                padding++;
            } else {
                ch[j] = dlookup[static_cast<int>(src[i + j])];
            }
        }

        tmp = 0;
        tmp |= (ch[0] << 18) | (ch[1] << 12) | (ch[2] << 6) | ch[3];

        decoded.push_back((tmp & 0xff0000) >> 16);
        decoded.push_back((tmp & 0xff00) >> 8);
        decoded.push_back((tmp & 0xff));
    }

    return decoded;
}

}