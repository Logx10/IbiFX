#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "GearLibrary.h"
#include "Serialization.h"
#include "offline.h"

namespace
{
namespace fs = std::filesystem;
constexpr double kRate = 48000.0;
constexpr int kBlock = 128;
constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr float kLevel = 1.2f;

struct Setting
{
    const char* series;
    const char* position;
    float drive;
    float toneHz;
};

constexpr std::array kSettings{
    Setting{"tone", "min", 4.0f, 500.0f},
    Setting{"tone", "meio", 4.0f, 4250.0f},
    Setting{"tone", "max", 4.0f, 8000.0f},
    Setting{"drive", "min", 0.5f, 4250.0f},
    Setting{"drive", "padrao", 4.0f, 4250.0f},
    Setting{"drive", "max", 12.0f, 4250.0f},
};

double db(double amplitude)
{
    return 20.0 * std::log10(std::max(amplitude, 1e-12));
}

WavFile sine(double frequency, float amplitude)
{
    WavFile signal{{std::vector<float>(static_cast<std::size_t>(kRate))}, kRate};
    auto& samples = signal.channels.front();
    for (std::size_t n = 0; n < samples.size(); ++n)
        samples[n] = static_cast<float>(amplitude * std::sin(kTwoPi * frequency * n / kRate));
    return signal;
}

double harmonic(const std::vector<float>& samples, double frequency)
{
    // Frequências pares mantêm número inteiro de ciclos na janela final de 0,5 s.
    const std::size_t start = samples.size() / 2;
    double real = 0.0;
    double imag = 0.0;
    for (std::size_t n = start; n < samples.size(); ++n)
    {
        const double phase = kTwoPi * frequency * n / kRate;
        real += samples[n] * std::cos(phase);
        imag += samples[n] * std::sin(phase);
    }
    return 2.0 * std::hypot(real, imag) / static_cast<double>(samples.size() - start);
}

struct SignalLevel
{
    double rms;
    double peak;
};

SignalLevel measureLevel(const std::vector<float>& samples, std::size_t start = 0)
{
    double energy = 0.0;
    double peak = 0.0;
    for (std::size_t n = start; n < samples.size(); ++n)
    {
        const double sample = samples[n];
        if (!std::isfinite(sample)) throw std::runtime_error("amostra nao finita");
        energy += sample * sample;
        peak = std::max(peak, std::abs(sample));
    }
    return {std::sqrt(energy / static_cast<double>(samples.size() - start)), peak};
}

Preset setControls(Preset recipe, const GearModel& pedal, const Setting& setting)
{
    const std::pair<const char*, float> settings[] = {
        {"Drive", setting.drive}, {"Tone", setting.toneHz}, {"Level", kLevel}
    };
    for (const auto& [label, value] : settings)
    {
        const auto control = std::find_if(pedal.controls.begin(), pedal.controls.end(), [&](const GearControl& entry) {
            return entry.label == label;
        });
        if (control == pedal.controls.end() || value < control->minValue || value > control->maxValue)
            throw std::runtime_error(std::string("controle incompativel com a auditoria: ") + label);
        auto& parameters = recipe.modules.at(control->moduleOffset).parameters;
        const auto it = std::find_if(parameters.begin(), parameters.end(), [&](const auto& parameter) {
            return parameter.first == control->parameterId;
        });
        if (it == parameters.end()) throw std::runtime_error("controle ausente na receita: " + control->label);
        it->second = value;
    }
    return recipe;
}

WavFile render(const Preset& recipe, const WavFile& input)
{
    ModuleChain chain;
    preset::apply(recipe, chain);
    return offline::processFile(input, chain, kBlock);
}

void measureTones(std::ostream& csv, const GearLibrary& library, const GearModel& pedal)
{
    const Preset base = library.buildSignalChain({pedal.id}, pedal.name);
    for (const Setting& setting : kSettings)
    {
        const Preset recipe = setControls(base, pedal, setting);
        for (float amplitude : {0.01f, 0.08f, 0.25f})
        {
            for (double hz : {82.0, 220.0, 500.0, 1000.0, 2000.0, 4000.0, 6000.0})
            {
                const auto output = render(recipe, sine(hz, amplitude));
                const auto& samples = output.channels.front();
                const auto level = measureLevel(samples, samples.size() / 2);
                const double fundamental = harmonic(samples, hz);
                csv << pedal.id << ',' << setting.series << ',' << setting.position << ','
                    << setting.drive << ',' << setting.toneHz << ',' << hz << ',' << amplitude << ','
                    << db(fundamental / amplitude) << ',' << db(harmonic(samples, 2 * hz) / fundamental) << ','
                    << db(harmonic(samples, 3 * hz) / fundamental) << ',' << db(level.rms) << '\n';
            }
        }
    }
}

void exportComparisons(const fs::path& directory, std::ostream& csv, const GearLibrary& library,
                       const GearModel& pedal, const WavFile& input)
{
    const Preset base = library.buildSignalChain({pedal.id, "amp.american-clean", "cab.factory.brit-4x12"}, pedal.name);
    for (std::size_t first : {0u, 3u})
    {
        const std::string prefix = pedal.id + "-" + kSettings[first].series;
        const auto silence = static_cast<std::size_t>(input.sampleRate * 0.5);
        WavFile comparison{{{}}, input.sampleRate};
        std::array<WavFile, 3> outputs;
        std::array<SignalLevel, 3> levels{};
        double targetRms = 0.04;
        double rawGain = 1.0;
        for (std::size_t i = 0; i < outputs.size(); ++i)
        {
            const auto& setting = kSettings[first + i];
            const auto recipe = setControls(base, pedal, setting);
            outputs[i] = render(recipe, input);
            levels[i] = measureLevel(outputs[i].channels.front());
            if (levels[i].rms <= 0.0) throw std::runtime_error("saida silenciosa na comparacao");
            targetRms = std::min(targetRms, 0.95 * levels[i].rms / levels[i].peak);
            rawGain = std::min(rawGain, 1.0 / levels[i].peak);
            std::ofstream saved(directory / (prefix + "-" + setting.position + ".preset"));
            saved << preset::serialize(recipe);
            if (!saved) throw std::runtime_error("falha ao gravar preset");
        }

        auto& sequence = comparison.channels.front();
        for (std::size_t i = 0; i < outputs.size(); ++i)
        {
            const auto& setting = kSettings[first + i];
            auto& samples = outputs[i].channels.front();
            const double listeningGain = targetRms / levels[i].rms;
            if (i != 0) sequence.insert(sequence.end(), silence, 0.0f);
            for (float& sample : samples)
            {
                sequence.push_back(static_cast<float>(sample * listeningGain));
                sample = static_cast<float>(sample * rawGain);
            }
            wav::write((directory / (prefix + "-" + setting.position + "-bruto.wav")).string(), outputs[i]);
            csv << pedal.id << ',' << setting.series << ',' << setting.position << ','
                << db(levels[i].rms) << ',' << db(levels[i].peak) << ',' << db(rawGain) << ',' << db(listeningGain) << '\n';
        }
        wav::write((directory / (prefix + "-comparativo.wav")).string(), comparison);
    }
}
}

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2 || argc > 3)
            throw std::runtime_error("uso: ibifx_screamer_audit <nova-pasta> [guitarra-sem-efeitos-mono.wav]");
        const auto input = argc == 3 ? wav::read(argv[2]) : offline::generateTestSignal(kRate, 4.0);
        if (input.channelCount() != 1 || input.channels.front().empty() || measureLevel(input.channels.front()).rms <= 0.0)
            throw std::runtime_error("use um WAV mono com audio");

        GearLibrary library;
        library.scanImpulseResponses("irs");
        if (library.find("cab.factory.brit-4x12") == nullptr)
            throw std::runtime_error("execute da raiz do projeto para localizar o gabinete Factory Brit 4x12");

        const fs::path directory = argv[1];
        if (!fs::create_directories(directory)) throw std::runtime_error("use uma pasta de destino nova");
        if (argc == 3)
            fs::copy_file(argv[2], directory / "entrada.wav");
        else
            wav::write((directory / "entrada.wav").string(), input);
        std::ofstream tones(directory / "tons.csv");
        tones << "modelo,serie,posicao,drive,tone_hz,frequencia_hz,pico_entrada,h1_ganho_db,h2_dbc,h3_dbc,rms_dbfs\n"
              << std::fixed << std::setprecision(6);
        std::ofstream audio(directory / "niveis-audio.csv");
        audio << "modelo,serie,posicao,rms_bruto_dbfs,pico_bruto_dbfs,ajuste_wav_bruto_db,ajuste_escuta_db\n"
              << std::fixed << std::setprecision(6);
        for (const char* id : {"stomp.green-screamer", "stomp.green-screamer-symmetric"})
        {
            const auto* pedal = library.find(id);
            if (pedal == nullptr) throw std::runtime_error(std::string("pedal ausente: ") + id);
            measureTones(tones, library, *pedal);
            exportComparisons(directory, audio, library, *pedal, input);
        }
        if (!tones || !audio) throw std::runtime_error("falha ao gravar CSVs");
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
