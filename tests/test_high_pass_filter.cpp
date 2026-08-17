// Testes do HighPassFilter.
//
// Filtro é um dos poucos módulos em que o teste precisa MEDIR uma
// propriedade, e não conferir um valor: alimenta-se uma senoide de frequência
// conhecida e compara-se a amplitude que sai com a que entrou.
//
// A infra de verificação vive em test_helpers.h.

#include <cmath>
#include <iostream>
#include <vector>

#include "HighPassFilter.h"
#include "test_helpers.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

// Gera uma senoide e devolve a amplitude que sobrou depois do filtro.
//
// As primeiras amostras são descartadas: o filtro parte da memória zerada e
// leva um tempo até se acomodar. Medir durante essa acomodação daria um
// número que não corresponde ao comportamento em regime.
float measureGain(HighPassFilter& filter, double frequency, int cycles = 40)
{
    const int samplesPerCycle = static_cast<int>(kSampleRate / frequency);
    const int total = samplesPerCycle * cycles;
    const int skip = total / 2;

    std::vector<float> buffer(static_cast<std::size_t>(total));

    for (int i = 0; i < total; ++i)
    {
        buffer[static_cast<std::size_t>(i)] =
            static_cast<float>(std::sin(kTwoPi * frequency * i / kSampleRate));
    }

    filter.process(buffer);

    float peak = 0.0f;
    for (int i = skip; i < total; ++i)
    {
        peak = std::max(peak, std::fabs(buffer[static_cast<std::size_t>(i)]));
    }

    return peak;
}
}

// ---------------------------------------------------------------------

// Os parâmetros nascem com id e faixa esperados.
void testParameters()
{
    std::cout << "parametros do filtro\n";

    HighPassFilter filter;

    check(filter.parameterCount() == 1, "tem 1 parametro");
    check(filter.parameterAt(0).id() == "frequency", "chamado frequency");
    checkClose(filter.frequency(), 100.0f, "padrao e 100 Hz");
}

// Sem prepare, o sinal atravessa intacto.
void testWithoutPrepareIsTransparent()
{
    std::cout << "sem prepare, passa direto\n";

    HighPassFilter filter;

    std::vector<float> buffer = {1.0f, 0.5f, -0.25f};
    filter.process(buffer);

    checkClose(buffer[0], 1.0f, "1.0 continua 1.0");
    checkClose(buffer[1], 0.5f, "0.5 continua 0.5");
    checkClose(buffer[2], -0.25f, "-0.25 continua -0.25");
}

// Frequência muito acima do corte passa quase inteira.
void testHighFrequencyPasses()
{
    std::cout << "agudo passa\n";

    HighPassFilter filter;
    filter.prepare(kSampleRate, 512);
    filter.setFrequency(100.0f);

    const float gain = measureGain(filter, 2000.0);

    check(gain > 0.95f, "2000 Hz com corte em 100 Hz sai com mais de 95%");
}

// Frequência muito abaixo do corte é fortemente atenuada.
void testLowFrequencyIsAttenuated()
{
    std::cout << "grave e cortado\n";

    HighPassFilter filter;
    filter.prepare(kSampleRate, 512);
    filter.setFrequency(500.0f);

    const float gain = measureGain(filter, 50.0);

    check(gain < 0.2f, "50 Hz com corte em 500 Hz sai com menos de 20%");
}

// NA FREQUÊNCIA DE CORTE, a amplitude cai para cerca de 0.707.
//
// É a definição do corte: o ponto onde a POTÊNCIA cai à metade. Como potência
// é proporcional ao quadrado da amplitude, metade da potência corresponde a
// 1/raiz(2) = 0.707 de amplitude. É o famoso "-3 dB".
//
// Este é o teste que prova que o coeficiente foi calculado direito: um erro
// na fórmula deslocaria o corte e apareceria aqui.
void testCutoffFrequencyIsMinusThreeDecibels()
{
    std::cout << "no corte, a amplitude cai para 0.707\n";

    HighPassFilter filter;
    filter.prepare(kSampleRate, 512);
    filter.setFrequency(200.0f);

    const float gain = measureGain(filter, 200.0);

    check(gain > 0.68f && gain < 0.74f, "em 200 Hz com corte em 200 Hz, ganho perto de 0.707");
}

// A inclinação é de 6 dB por oitava.
//
// Uma oitava abaixo do corte a amplitude deve ficar por volta da metade. É a
// assinatura de um filtro de um polo, e o que distingue de um mais íngreme.
void testSlopeIsSixDecibelsPerOctave()
{
    std::cout << "inclinacao de 6 dB por oitava\n";

    HighPassFilter filter;
    filter.prepare(kSampleRate, 512);
    filter.setFrequency(400.0f);

    const float umaOitavaAbaixo = measureGain(filter, 200.0);

    filter.reset();
    const float duasOitavasAbaixo = measureGain(filter, 100.0);

    check(umaOitavaAbaixo > 0.40f && umaOitavaAbaixo < 0.52f,
          "uma oitava abaixo sai perto de metade");
    check(duasOitavasAbaixo < umaOitavaAbaixo * 0.65f,
          "duas oitavas abaixo cai bem mais");
}

// Sinal contínuo é removido por completo.
//
// Offset DC não é som — é o alto-falante deslocado do repouso. Ele come
// headroom e causa estalo ao ligar e desligar efeitos. Um filtro passa-alta
// elimina isso por construção, e é um uso comum dele mesmo em corte baixo.
void testRemovesDirectCurrent()
{
    std::cout << "remove sinal continuo\n";

    HighPassFilter filter;
    filter.prepare(kSampleRate, 512);
    filter.setFrequency(100.0f);

    std::vector<float> buffer(8000, 0.5f);
    filter.process(buffer);

    check(std::fabs(buffer[7999]) < 0.01f, "um sinal fixo em 0.5 vira quase zero");
}

// Frequência fora da faixa para na borda.
void testFrequencyIsClamped()
{
    std::cout << "frequencia fora da faixa\n";

    HighPassFilter filter;

    filter.setFrequency(50000.0f);
    checkClose(filter.frequency(), 2000.0f, "50000 Hz para em 2000");

    filter.setFrequency(-10.0f);
    checkClose(filter.frequency(), 20.0f, "-10 Hz para em 20");
}

// O estado atravessa a fronteira dos blocos.
void testStateSurvivesAcrossBlocks()
{
    std::cout << "o estado atravessa blocos\n";

    HighPassFilter inteiro;
    inteiro.prepare(kSampleRate, 512);
    inteiro.setFrequency(200.0f);

    HighPassFilter fatiado;
    fatiado.prepare(kSampleRate, 512);
    fatiado.setFrequency(200.0f);

    std::vector<float> completo(600);
    for (std::size_t i = 0; i < completo.size(); ++i)
    {
        completo[i] = static_cast<float>(std::sin(kTwoPi * 300.0 * static_cast<double>(i) / kSampleRate));
    }

    std::vector<float> emBlocos = completo;

    inteiro.process(completo);

    // Mesmo sinal, processado em pedaços de 64.
    for (std::size_t start = 0; start < emBlocos.size(); start += 64)
    {
        const std::size_t count = std::min<std::size_t>(64, emBlocos.size() - start);
        std::vector<float> slice(emBlocos.begin() + static_cast<std::ptrdiff_t>(start),
                                 emBlocos.begin() + static_cast<std::ptrdiff_t>(start + count));
        fatiado.process(slice);
        std::copy(slice.begin(), slice.end(), emBlocos.begin() + static_cast<std::ptrdiff_t>(start));
    }

    bool iguais = true;
    for (std::size_t i = 0; i < completo.size(); ++i)
    {
        if (std::fabs(completo[i] - emBlocos[i]) > 1e-5f)
        {
            iguais = false;
        }
    }

    check(iguais, "processar de uma vez ou em blocos de 64 da o mesmo resultado");
}

// reset() apaga a memória do filtro.
void testResetClearsMemory()
{
    std::cout << "reset limpa a memoria\n";

    HighPassFilter filter;
    filter.prepare(kSampleRate, 512);

    std::vector<float> aquecimento(100, 1.0f);
    filter.process(aquecimento);

    filter.reset();

    std::vector<float> depois(3, 0.0f);
    filter.process(depois);

    checkClose(depois[0], 0.0f, "silencio entra, silencio sai");
    checkClose(depois[1], 0.0f, "sem residuo da memoria antiga");
}

// Buffer vazio nao pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    HighPassFilter filter;
    filter.prepare(kSampleRate, 512);

    std::vector<float> buffer;
    filter.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

int main()
{
    std::cout << "\n=== testes do HighPassFilter ===\n\n";

    testParameters();
    testWithoutPrepareIsTransparent();
    testHighFrequencyPasses();
    testLowFrequencyIsAttenuated();
    testCutoffFrequencyIsMinusThreeDecibels();
    testSlopeIsSixDecibelsPerOctave();
    testRemovesDirectCurrent();
    testFrequencyIsClamped();
    testStateSurvivesAcrossBlocks();
    testResetClearsMemory();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
