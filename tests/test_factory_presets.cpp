#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "ModuleChain.h"
#include "PresetManager.h"
#include "test_helpers.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

double testPreset(const std::string& filename)
{
    const auto path = std::filesystem::path(IBIFX_SOURCE_DIR) / "presets" / "factory" / filename;
    const Preset loaded = preset::load(path.string());
    ModuleChain chain;
    preset::apply(loaded, chain);
    chain.prepare(kSampleRate, 128);

    std::size_t represented = 0;
    for (const auto& item : loaded.chainItems)
        represented += item.moduleCount;
    check(represented == chain.size(), filename + ": layout cobre a cadeia");
    check(chain.size() > 2 && chain.moduleAt(chain.size() - 1).name() == std::string("Limiter"),
          filename + ": termina no limiter");

    double energy = 0.0;
    float peak = 0.0f;
    bool finite = true;
    for (std::size_t start = 0; start < 12000; start += 128)
    {
        std::vector<float> block(std::min<std::size_t>(128, 12000 - start));
        for (std::size_t i = 0; i < block.size(); ++i)
        {
            const double t = static_cast<double>(start + i) / kSampleRate;
            block[i] = static_cast<float>(0.08 * std::sin(kTwoPi * 220.0 * t)
                                          + 0.025 * std::sin(kTwoPi * 440.0 * t));
        }
        chain.process(block);
        for (float sample : block)
        {
            finite &= std::isfinite(sample);
            energy += static_cast<double>(sample) * sample;
            peak = std::max(peak, std::fabs(sample));
        }
    }
    const double rms = std::sqrt(energy / 12000.0);
    check(finite, filename + ": saída finita");
    check(rms > 0.005 && rms < 0.5, filename + ": nível audível e controlado");
    check(peak < 1.01f, filename + ": saída sem clipping numérico");
    std::cout << "  RMS=" << rms << " peak=" << peak << '\n';
    return rms;
}
}

int main()
{
    std::vector<double> levels;
    for (const char* filename : {
             "01-hotel-california-lead.ibifxpreset",
             "02-sir-psycho-sexy-clean.ibifxpreset",
             "03-high-voltage-crunch.ibifxpreset",
             "04-estranged-lead.ibifxpreset",
             "05-youthanasia-rhythm.ibifxpreset",
             "06-reflective-lead.ibifxpreset"})
        levels.push_back(testPreset(filename));

    const auto [quietest, loudest] = std::minmax_element(levels.begin(), levels.end());
    check(*loudest / *quietest < 1.5, "presets mantêm volumes próximos para a mesma entrada");
    return reportResults();
}
