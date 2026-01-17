#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <cstdint>

class WavWriter {
public:
    static void writeHeader(std::ofstream& file, uint32_t sampleRate, uint16_t channels, uint16_t bitsPerSample) {
        file.write("RIFF", 4);
        uint32_t fileSize = 0; // Placeholder
        file.write(reinterpret_cast<const char*>(&fileSize), 4);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        uint32_t fmtSize = 16;
        file.write(reinterpret_cast<const char*>(&fmtSize), 4);
        uint16_t format = 1; // PCM
        file.write(reinterpret_cast<const char*>(&format), 2);
        file.write(reinterpret_cast<const char*>(&channels), 2);
        file.write(reinterpret_cast<const char*>(&sampleRate), 4);
        uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
        file.write(reinterpret_cast<const char*>(&byteRate), 4);
        uint16_t blockAlign = channels * bitsPerSample / 8;
        file.write(reinterpret_cast<const char*>(&blockAlign), 2);
        file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
        file.write("data", 4);
        uint32_t dataSize = 0; // Placeholder
        file.write(reinterpret_cast<const char*>(&dataSize), 4);
    }

    static void updateHeader(std::ofstream& file) {
        auto currentPos = file.tellp();
        uint32_t fileSize = static_cast<uint32_t>(currentPos) - 8;
        uint32_t dataSize = static_cast<uint32_t>(currentPos) - 44;
        
        file.seekp(4);
        file.write(reinterpret_cast<const char*>(&fileSize), 4);
        file.seekp(40);
        file.write(reinterpret_cast<const char*>(&dataSize), 4);
        file.seekp(currentPos);
    }
};
