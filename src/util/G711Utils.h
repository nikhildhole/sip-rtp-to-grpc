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
        int mask = (pcm < 0) ? 0x7F : 0xFF;
        if (pcm < 0) pcm = -pcm;
        if (pcm > 32635) pcm = 32635;
        pcm += 0x84;

        int seg = 0;
        if (pcm <= 0xFF) seg = 0;
        else if (pcm <= 0x1FF) seg = 1;
        else if (pcm <= 0x3FF) seg = 2;
        else if (pcm <= 0x7FF) seg = 3;
        else if (pcm <= 0xFFF) seg = 4;
        else if (pcm <= 0x1FFF) seg = 5;
        else if (pcm <= 0x3FFF) seg = 6;
        else seg = 7;

        uint8_t u_val = (seg << 4) | ((pcm >> (seg + 3)) & 0x0F);
        return u_val ^ mask;
    }

    static uint8_t linearToALaw(int16_t pcm) {
        int mask;
        if (pcm >= 0) {
            mask = 0xD5;
        } else {
            pcm = -pcm;
            mask = 0x55;
        }
        if (pcm > 32767) pcm = 32767;

        int seg;
        if (pcm <= 0xFF) seg = 0;
        else if (pcm <= 0x1FF) seg = 1;
        else if (pcm <= 0x3FF) seg = 2;
        else if (pcm <= 0x7FF) seg = 3;
        else if (pcm <= 0xFFF) seg = 4;
        else if (pcm <= 0x1FFF) seg = 5;
        else if (pcm <= 0x3FFF) seg = 6;
        else seg = 7;

        uint8_t a_val = (seg << 4);
        if (seg < 2) a_val |= (pcm >> 4) & 0x0F;
        else a_val |= (pcm >> (seg + 3)) & 0x0F;
        return a_val ^ mask;
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
