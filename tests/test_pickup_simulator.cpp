#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "PickupSimulator.h"

namespace
{
int failures = 0;
constexpr double kSampleRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FALHOU: " << message << '\n';
        ++failures;
    }
}

double gain(PickupSimulator::Mode mode, double frequency)
{
    PickupSimulator simulator;
    simulator.setMode(mode);
    simulator.prepare(kSampleRate);

    std::vector<float> signal(48000);
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = static_cast<float>(0.1 * std::sin(kTwoPi * frequency * i / kSampleRate));

    simulator.process(signal);
    double energy = 0.0;
    for (std::size_t i = signal.size() / 2; i < signal.size(); ++i)
        energy += static_cast<double>(signal[i]) * signal[i];
    return std::sqrt(energy / (signal.size() / 2)) / (0.1 / std::sqrt(2.0));
}
}

int main()
{
    PickupSimulator simulator;
    simulator.prepare(kSampleRate);
    std::vector<float> original = {0.125f, -0.5f, 0.0f, 0.75f};
    auto processed = original;
    simulator.process(processed);
    check(processed == original, "Original deve passar sem alterar amostras");

    const double humLow = gain(PickupSimulator::Mode::SingleToHumbucker, 100.0);
    const double humHigh = gain(PickupSimulator::Mode::SingleToHumbucker, 7000.0);
    const double singleLow = gain(PickupSimulator::Mode::HumbuckerToSingle, 100.0);
    const double singleHigh = gain(PickupSimulator::Mode::HumbuckerToSingle, 7000.0);
    check(humLow > 1.7 && humLow < 1.85, "Single para humbucker eleva o nivel em cerca de 5 dB");
    check(humHigh / humLow < 0.75, "Single para humbucker reduz agudos relativos");
    check(singleLow > 0.53 && singleLow < 0.60, "Humbucker para single reduz o nivel em cerca de 5 dB");
    check(singleHigh / singleLow > 1.35, "Humbucker para single aumenta agudos relativos");
    check(std::fabs(humLow * singleLow - 1.0) < 0.02, "Conversoes opostas sao reciprocas");

    simulator.setMode(PickupSimulator::Mode::SingleToHumbucker);
    std::vector<float> steady(960, 0.1f);
    simulator.process(steady);
    check(std::isfinite(steady.back()), "Troca de modo produz sinal finito");
    check(steady.front() < steady.back(), "Modo muda gradualmente");
    check(steady.back() > 0.17f && steady.back() < 0.19f, "Transicao termina no ganho alvo");

    std::cout << (failures == 0 ? "pickup simulator: OK\n" : "pickup simulator: falhou\n");
    return failures == 0 ? 0 : 1;
}
