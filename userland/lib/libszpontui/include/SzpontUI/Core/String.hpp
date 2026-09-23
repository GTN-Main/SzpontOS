#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace SzpontUI {

class StringUtils {
public:
    // Decodes next UTF-8 codepoint, advancing index
    static uint32_t decode_utf8(std::string_view str, size_t &index) {
        if (index >= str.size()) return 0;
        unsigned char c = static_cast<unsigned char>(str[index++]);
        if (c < 0x80) return c;

        uint32_t cp = 0;
        int remaining = 0;
        if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            remaining = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            remaining = 2;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            remaining = 3;
        } else {
            return 0xFFFD; // Replacement char
        }

        while (remaining-- > 0 && index < str.size()) {
            unsigned char next = static_cast<unsigned char>(str[index++]);
            if ((next & 0xC0) != 0x80) return 0xFFFD;
            cp = (cp << 6) | (next & 0x3F);
        }
        return cp;
    }

    static std::vector<uint32_t> to_codepoints(std::string_view str) {
        std::vector<uint32_t> cps;
        size_t idx = 0;
        while (idx < str.size()) {
            cps.push_back(decode_utf8(str, idx));
        }
        return cps;
    }

    static std::string trim(std::string_view s) {
        size_t start = 0;
        while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' || s[start] == '\r')) {
            start++;
        }
        size_t end = s.size();
        while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\n' || s[end - 1] == '\r')) {
            end--;
        }
        return std::string(s.substr(start, end - start));
    }
};

} // namespace SzpontUI
