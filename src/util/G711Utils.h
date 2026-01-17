#pragma once

#include <cstdint>
#include <vector>

class G711Utils {
public:
    static int16_t ulawToLinear(uint8_t ulaw) {
        ulaw = ~ulaw;
        int t = ((ulaw & 0x0F) << 3) + 0x84;
        t <<= (ulaw & 0x70) >> 4;
        return (ulaw & 0x80) ? (0x84 - t) : (t - 0x84);
    }

    static int16_t alawToLinear(uint8_t alaw) {
        alaw ^= 0x55;
        int t = (alaw & 0x0F) << 4;
        int seg = (alaw & 0x70) >> 4;
        if (seg) t = (t + 0x108) << (seg - 1);
        else t += 8;
        return (alaw & 0x80) ? t : -t;
    }

    static uint8_t linearToULaw(int16_t pcm) {
        int mask = (pcm < 0) ? 0xFF : 0x7F;
        if (pcm < 0) pcm = -pcm;
        pcm += 128 + 4;
        if (pcm > 32767) pcm = 32767;
        int seg = 0;
        int t = pcm >> 7;
        while (t > 1) {
            t >>= 1;
            seg++;
        }
        return ~(mask ^ ((seg << 4) | ((pcm >> (seg + 3)) & 0x0F)));
    }

    static uint8_t linearToALaw(int16_t pcm) {
        int mask = (pcm < 0) ? 0x7F : 0xFF;
        if (pcm < 0) pcm = -pcm;
        int seg = 0;
        int t = pcm >> 8;
        while (t > 0) {
            t >>= 1;
            seg++;
        }
        uint8_t alaw;
        if (seg == 0) alaw = (pcm >> 4) & 0x0F;
        else alaw = (seg << 4) | ((pcm >> (seg + 3)) & 0x0F);
        return alaw ^ 0x55 ^ mask;
    }

    static void decodeULaw(const std::vector<char>& input, std::vector<int16_t>& output) {
        output.resize(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            output[i] = ulawToLinear(static_cast<uint8_t>(input[i]));
        }
    }

    static void decodeALaw(const std::vector<char>& input, std::vector<int16_t>& output) {
        output.resize(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            output[i] = alawToLinear(static_cast<uint8_t>(input[i]));
        }
    }

    static void encodeULaw(const std::vector<int16_t>& input, std::vector<char>& output) {
        output.resize(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            output[i] = static_cast<char>(linearToULaw(input[i]));
        }
    }

    static void encodeALaw(const std::vector<int16_t>& input, std::vector<char>& output) {
        output.resize(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            output[i] = static_cast<char>(linearToALaw(input[i]));
        }
    }
};
