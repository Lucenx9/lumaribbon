// SPDX-License-Identifier: GPL-3.0-or-later
#include <cmath>
#include <cstdint>
#include <fstream>

int main(int argc, char **argv) {
    if (argc != 2) return 1;
    std::ofstream file(argv[1], std::ios::binary);
    const auto word = [&file](uint32_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) file.put(char((value >> (8 * i)) & 255));
    };
    constexpr uint32_t rate = 48000, count = rate * 35, bytes = count * 4;
    file.write("RIFF", 4); word(bytes + 36, 4); file.write("WAVEfmt ", 8);
    word(16, 4); word(1, 2); word(2, 2); word(rate, 4); word(rate * 4, 4); word(4, 2); word(16, 2);
    file.write("data", 4); word(bytes, 4);
    for (uint32_t i = 0; i < count; ++i) {
        const double t = double(i) / rate;
        const double envelope = 0.6 + 0.4 * std::exp(-std::fmod(t, 0.5) * 12);
        const double s = envelope * (0.16 * std::sin(6.2831853 * 100 * t) + 0.08 * std::sin(6.2831853 * 1200 * t));
        const auto sample = uint16_t(int16_t(s * 32767));
        word(sample, 2); word(sample, 2);
    }
    return file ? 0 : 1;
}
