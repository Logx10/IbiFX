#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "GearLibrary.h"
#include "ModuleChain.h"
#include "Preset.h"
#include "WavFile.h"

namespace
{
constexpr double kRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;
constexpr std::size_t kBlock = 128;

std::vector<float> sine(float amplitude, double frequency = 220.0)
{
    std::vector<float> signal(static_cast<std::size_t>(kRate));
    for (std::size_t n = 0; n < signal.size(); ++n)
        signal[n] = amplitude * static_cast<float>(std::sin(kTwoPi * frequency * n / kRate));
    return signal;
}

std::vector<float> render(const GearLibrary& library, const std::string& id,
                          const std::vector<float>& input, const std::string& cabinet = {}, int mode = 0)
{
    ModuleChain chain;
    std::vector<std::string> equipment = {id};
    if (!cabinet.empty()) equipment.push_back(cabinet);
    preset::apply(library.buildSignalChain(equipment, "Audit"), chain);
    if (auto* lead = chain.moduleAt(0).findParameter("lead"))
    {
        lead->setValue(mode == 2 ? 1.0f : 0.0f);
        chain.moduleAt(0).findParameter("clean")->setValue(mode == 0 ? 1.0f : 0.0f);
    }
    chain.prepare(kRate, static_cast<int>(kBlock));
    auto output = input;
    std::vector<float> block;
    block.reserve(kBlock);
    for (std::size_t start = 0; start < output.size(); start += kBlock)
    {
        const auto count = std::min(kBlock, output.size() - start);
        block.assign(output.begin() + static_cast<std::ptrdiff_t>(start),
                     output.begin() + static_cast<std::ptrdiff_t>(start + count));
        chain.process(block);
        std::copy(block.begin(), block.end(), output.begin() + static_cast<std::ptrdiff_t>(start));
    }
    return output;
}

struct Metrics
{
    double rms = 0.0;
    double peak = 0.0;
    double mean = 0.0;
    double thd = 0.0;
};

Metrics measure(const std::vector<float>& signal)
{
    Metrics result;
    const auto start = signal.size() / 2;
    const auto count = signal.size() - start;
    for (std::size_t n = start; n < signal.size(); ++n)
    {
        const double x = signal[n];
        if (!std::isfinite(x)) throw std::runtime_error("saida nao finita");
        result.rms += x * x;
        result.mean += x;
        result.peak = std::max(result.peak, std::fabs(x));
    }
    result.rms = std::sqrt(result.rms / static_cast<double>(count));
    result.mean /= static_cast<double>(count);
    double fundamental = 0.0, harmonics = 0.0;
    // 220 Hz tem um número inteiro de ciclos na janela final de 0,5 s.
    // Projeta H1..H10 sem confundir atraso/fase do oversampling com THD.
    for (int harmonic = 1; harmonic <= 10; ++harmonic)
    {
        double real = 0.0, imag = 0.0;
        for (std::size_t n = start; n < signal.size(); ++n)
        {
            const double angle = kTwoPi * 220.0 * harmonic * n / kRate;
            real += signal[n] * std::cos(angle);
            imag += signal[n] * std::sin(angle);
        }
        const double power = real * real + imag * imag;
        if (harmonic == 1) fundamental = power;
        else harmonics += power;
    }
    result.thd = fundamental > 0.0 ? 100.0 * std::sqrt(harmonics / fundamental) : 0.0;
    return result;
}

double db(double value) { return 20.0 * std::log10(std::max(value, 1e-12)); }

std::vector<float> phrase()
{
    // Mesma frase sintética para ouvir todos: notas fracas/fortes e acorde.
    // Não se apresenta este sinal como uma gravação DI de guitarra real.
    std::vector<float> signal(static_cast<std::size_t>(kRate * 4.0));
    for (std::size_t n = 0; n < signal.size(); ++n)
    {
        const double time = n / kRate;
        double x = 0.0;
        for (int note = 0; note < 4; ++note)
        {
            const double age = time - (0.1 + 0.85 * note);
            if (age < 0.0) continue;
            const double attack = std::min(age / 0.004, 1.0);
            const double amplitude = note < 2 ? 0.025 : 0.1;
            for (int string = 0; string < (note == 3 ? 3 : 1); ++string)
            {
                const double frequency = string == 0 ? 110.0 : (string == 1 ? 164.81 : 220.0);
                for (int harmonic = 1; harmonic <= 6; ++harmonic)
                    x += amplitude / harmonic * attack * std::exp(-(1.5 + 0.4 * harmonic) * age)
                         * std::sin(kTwoPi * frequency * harmonic * age);
            }
        }
        const double fade = std::clamp((4.0 - time) / 0.1, 0.0, 1.0);
        signal[n] = static_cast<float>(x * fade);
    }
    return signal;
}
}

int main(int argc, char** argv)
{
    try
    {
        // Sem argumento, só escreve CSV no terminal. Com uma pasta nova,
        // exporta também a frase seca, uma versão por amp e uma sequência.
        if (argc > 2) throw std::runtime_error("uso: ibifx_amp_audit [nova-pasta-de-audios]");
        const std::filesystem::path directory = argc == 2 ? argv[1] : "";
        if (!directory.empty() && !std::filesystem::create_directories(directory))
            throw std::runtime_error("use uma pasta de destino nova para nao sobrescrever audios");
        GearLibrary library;
        library.scanImpulseResponses("irs");
        const auto* cab = library.find("cab.factory.brit-4x12");
        if (!directory.empty() && cab == nullptr)
            throw std::runtime_error("execute da raiz do projeto: IR Factory Brit 4x12 ausente");

        std::cout << "id,rms_soft_dbfs,rms_medium_dbfs,rms_hot_dbfs,dynamic_span_db,peak_medium,peak_hot,dc_medium,thd_h2_h10_percent\n";
        std::cout << std::fixed << std::setprecision(6);
        const auto source = phrase();
        WavFile sequence;
        sequence.sampleRate = kRate;
        sequence.channels.resize(1);
        if (!directory.empty()) wav::write((directory / "00-dry.wav").string(), WavFile{{source}, kRate});
        for (const auto* amp : library.modelsIn(GearCategory::Amp))
        {
          const int modes = amp->id.starts_with("amp.ironheart") ? 3 : 1;
          for (int mode = 0; mode < modes; ++mode)
          {
            const std::string id = amp->id + (modes == 1 ? "" : (mode == 0 ? "-clean" : (mode == 1 ? "-rhythm" : "-lead")));
            const auto soft = measure(render(library, amp->id, sine(0.01f), {}, mode));
            const auto medium = measure(render(library, amp->id, sine(0.08f), {}, mode));
            const auto hot = measure(render(library, amp->id, sine(0.3f), {}, mode));
            std::cout << id << ',' << db(soft.rms) << ',' << db(medium.rms) << ',' << db(hot.rms)
                      << ',' << db(hot.rms / soft.rms) << ',' << medium.peak << ',' << hot.peak << ',' << medium.mean
                      << ',' << medium.thd << '\n';
            if (directory.empty()) continue;
            auto output = render(library, amp->id, source, cab->id, mode);
            double sum = 0.0, peak = 0.0;
            for (float x : output) { sum += static_cast<double>(x) * x; peak = std::max(peak, std::fabs(static_cast<double>(x))); }
            const double rms = std::sqrt(sum / static_cast<double>(output.size()));
            // Mesmo RMS para a escuta não favorecer simplesmente o mais alto.
            // Limite de pico protege o WAV PCM16; não há limiter mascarando o teste.
            const double gain = rms > 0.0 ? std::min(0.07 / rms, 0.95 / std::max(peak, 1e-12)) : 1.0;
            for (auto& x : output) x = static_cast<float>(x * gain);
            wav::write((directory / (id + ".wav")).string(), WavFile{{output}, kRate});
            auto& samples = sequence.channels[0];
            if (!samples.empty()) samples.insert(samples.end(), static_cast<std::size_t>(0.5 * kRate), 0.0f);
            samples.insert(samples.end(), output.begin(), output.end());
          }
        }
        if (!directory.empty()) wav::write((directory / "comparativo.wav").string(), sequence);
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
