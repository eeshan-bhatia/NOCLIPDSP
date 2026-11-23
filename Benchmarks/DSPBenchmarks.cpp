#include "DSP/AudioEngine.h"
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
int main()
{
    constexpr int block = 256, iterations = 20000;
    std::array<float, block> source {}, left {}, right {};
    for (int i = 0; i < block; ++i) source[i] = 0.8f * std::sin(static_cast<float>(i) * 0.13f);
    float* channels[] { left.data(), right.data() };
    for (int profile = -2; profile < noclip::curveCount; ++profile)
    {
        noclip::Parameters p;
        p.clipEnabled = profile != -2;
        p.shape = profile == -1 ? 1.0f : 0.0f;
        p.curve = profile < 0 ? 3 : profile;
        p.driveDb = 12;
        noclip::AudioEngine engine;
        engine.prepare(48000, p);
        double checksum = 0;
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i)
        {
            left = source; right = source; // Refill avoids repeatedly distorting prior output.
            checksum += engine.process(channels, 2, block).outputRms;
        }
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        constexpr std::array<const char*, 7> names { "Disabled", "Hard", "Tanh", "Atan", "Cubic", "Knee", "Asymmetric" };
        std::cout << names[static_cast<size_t>(profile + 2)] << ": "
                  << seconds * 1e6 / iterations << " us/block; checksum " << checksum << '\n';
    }
}
