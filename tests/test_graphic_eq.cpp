// Testes do GraphicEQ.
//
// Os ganhos são medidos como o ouvido mede: um seno puro numa frequência,
// passando pelo EQ, comparando o RMS da saída com o da entrada (em dB),
// depois que o filtro já assentou (só a segunda metade do sinal conta).
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "GearLibrary.h"
#include "GraphicEQ.h"
#include "ModuleChain.h"
#include "Preset.h"
#include "test_helpers.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

// Ganho, em dB, que o EQ aplica num seno de frequency Hz.
double gainAtDb(GraphicEQ& eq, double frequency)
{
    eq.prepare(kSampleRate, 512);
    eq.reset();

    std::vector<float> signal(static_cast<std::size_t>(kSampleRate / 2));
    for (std::size_t i = 0; i < signal.size(); ++i)
        signal[i] = 0.1f * static_cast<float>(std::sin(kTwoPi * frequency * static_cast<double>(i) / kSampleRate));

    eq.process(signal);

    double sum = 0.0;
    for (std::size_t i = signal.size() / 2; i < signal.size(); ++i)
        sum += static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
    const double rms = std::sqrt(sum / static_cast<double>(signal.size() - signal.size() / 2));
    return 20.0 * std::log10(rms / (0.1 / std::sqrt(2.0)));
}

bool near(double value, double expected, double tolerance)
{
    return std::fabs(value - expected) < tolerance;
}
}

// Com tudo em 0 dB o pedal é flat: nenhuma frequência da guitarra muda.
void testFlatEqDoesNothing()
{
    std::cout << "tudo em 0 dB nao muda o som\n";

    GraphicEQ eq;
    for (double frequency : {82.0, 300.0, 1000.0, 5000.0})
        check(near(gainAtDb(eq, frequency), 0.0, 0.01),
              "flat em " + std::to_string(static_cast<int>(frequency)) + " Hz");
}

// No centro da banda, o ganho do sino é exatamente o do knob — é como o
// filtro do Cookbook é definido.
void testBandGainAtItsCenter()
{
    std::cout << "cada banda aplica o proprio ganho no centro\n";

    for (std::size_t band = 0; band < GraphicEQ::kBandCount; ++band)
    {
        GraphicEQ boost;
        boost.setBandGain(band, 12.0f);
        GraphicEQ cut;
        cut.setBandGain(band, -12.0f);

        const double frequency = GraphicEQ::bandFrequency(band);
        const std::string name = std::to_string(static_cast<int>(frequency)) + " Hz";

        check(near(gainAtDb(boost, frequency), 12.0, 0.2), "+12 dB em " + name);
        check(near(gainAtDb(cut, frequency), -12.0, 0.2), "-12 dB em " + name);
    }
}

// Uma banda não mexe nas que estão longe dela: subir o 100 Hz deixa o
// 3,2 kHz em paz, e vice-versa.
void testBandsAreIndependent()
{
    std::cout << "uma banda nao mexe nas frequencias longe dela\n";

    GraphicEQ grave;
    grave.setBandGain(0, 15.0f);
    check(near(gainAtDb(grave, 3200.0), 0.0, 0.2), "100 Hz no maximo nao muda 3,2 kHz");

    GraphicEQ agudo;
    agudo.setBandGain(5, 15.0f);
    check(near(gainAtDb(agudo, 100.0), 0.0, 0.2), "3,2 kHz no maximo nao muda 100 Hz");
}

// O Level é um volume em dB, igual para todas as frequências.
void testLevelIsAFlatVolume()
{
    std::cout << "level sobe o volume por igual\n";

    GraphicEQ eq;
    eq.setLevel(6.0f);
    check(near(gainAtDb(eq, 200.0), 6.0, 0.05), "+6 dB em 200 Hz");
    check(near(gainAtDb(eq, 4000.0), 6.0, 0.05), "+6 dB em 4 kHz");
}

void testParameterRoundTrip()
{
    std::cout << "setters, getters e faixas\n";

    GraphicEQ eq;
    check(eq.parameterCount() == 8, "sete bandas mais o Level");
    checkClose(eq.bandGain(3), 0.0f, "banda padrao em 0 dB");

    eq.setBandGain(3, -7.5f);
    checkClose(eq.bandGain(3), -7.5f, "800 Hz vira -7.5");

    eq.setBandGain(6, 40.0f);
    checkClose(eq.bandGain(6), 15.0f, "ganho acima de +15 fica em +15");

    eq.setLevel(-3.0f);
    checkClose(eq.level(), -3.0f, "level vira -3");
}

// O pedal do catálogo cria o módulo e sobrevive a salvar/carregar preset.
void testCatalogPedalBuildsTheModule()
{
    std::cout << "pedal do catalogo monta o GraphicEQ\n";

    GearLibrary library;
    const GearModel* model = library.find("stomp.seven-band");
    check(model != nullptr, "Seven Band EQ aparece no catalogo");
    if (model == nullptr)
        return;
    check(model->controls.size() == 8, "oito knobs no painel");

    ModuleChain chain;
    preset::apply(library.buildSignalChain({"stomp.seven-band"}, "teste"), chain);
    check(chain.moduleAt(0).name() == std::string("GraphicEQ"), "rig cria o modulo GraphicEQ");
}

int main()
{
    std::cout << "\n=== testes do GraphicEQ ===\n\n";

    testFlatEqDoesNothing();
    testBandGainAtItsCenter();
    testBandsAreIndependent();
    testLevelIsAFlatVolume();
    testParameterRoundTrip();
    testCatalogPedalBuildsTheModule();

    return reportResults();
}
