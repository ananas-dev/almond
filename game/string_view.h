#pragma once
#include <cassert>
#include <cstring>

struct StringView {
    StringView(const char* data, size_t length)
        : data(data)
        , length(length)
    {
    }

    [[nodiscard]] StringView substring(size_t start, size_t end) const
    {
        assert(end > start && end <= length);
        return { data + start, end - start };
    }

    [[nodiscard]] float to_float() const
    {
        float result = 0.0f;
        float sign = 1.0f;
        bool encountered_radix = false;
        int decimal_places = 0;

        for (size_t i = 0; i < length; i++) {
            char c = data[i];

            if (c == '-' && i == 0) {
                sign = -1.0f;
            } else if (c == '.') {
                if (encountered_radix)
                    return 0.0f;

                encountered_radix = true;
            } else if (c >= '0' && c <= '9') {
                int digit = c - '0';
                result = result * 10.0f + static_cast<float>(digit);

                if (encountered_radix) {
                    decimal_places++;
                }
            } else {
                return 0.0f;
            }
        }

        if (decimal_places > 0) {
            float divisor = 1.0f;
            for (int i = 0; i < decimal_places; i++) {
                divisor *= 10.0f;
            }
            result /= divisor;
        }

        return sign * result;
    }

    const char* data = nullptr;
    size_t length;
};

[[nodiscard]] inline StringView operator""_sv(char const* cstring, size_t length)
{
    return { cstring, length };
}

[[nodiscard]] inline bool operator==(const StringView& lhs, const StringView& rhs)
{
    if (lhs.length != rhs.length)
        return false;
    return memcmp(lhs.data, rhs.data, lhs.length) == 0;
}
