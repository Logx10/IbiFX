// Testes do Tuner.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <iostream>
#include <vector>

#include "Tuner.h"
#include "test_helpers.h"

namespace
{
constexpr double kSampleRate = 44100.0;

// Gera samples suficientes pra completar pelo menos uma janela de análise
// do Tuner (2048 amostras) — 4096 dá folga pra sobrar uma janela cheia
// mesmo que a primeira comece "no meio" de um período.
std::vector<float> generateSine(double frequency, float amplitude = 0.5f, std::size_t sampleCount = 4096)
{
    std::vector<float> signal(sampleCount);

    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        const double phase = 2.0 * 3.14159265358979323846 * frequency * static_cast<double>(i) / kSampleRate;
        signal[i] = amplitude * static_cast<float>(std::sin(phase));
    }

    return signal;
}

// Uma nota "de guitarra de verdade" para efeitos de teste: fundamental
// FRACA, harmônicos (2×, 3×) mais FORTES que ela — o que motivou trocar
// autocorrelação simples por YIN (ver Tuner.h). Amplitudes escolhidas pra
// exagerar o problema de propósito: se o detector confundisse fundamental
// com harmônico, é exatamente aqui que apareceria.
std::vector<float> generateNoteWithStrongHarmonics(double fundamentalHz, std::size_t sampleCount = 4096)
{
    std::vector<float> signal(sampleCount, 0.0f);

    const struct { double multiplier; float amplitude; } partials[] = {
        {1.0, 0.15f},  // fundamental, fraca de propósito
        {2.0, 0.5f},   // 2º harmônico, mais forte que a fundamental
        {3.0, 0.3f},   // 3º harmônico
    };

    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        float sample = 0.0f;

        for (const auto& partial : partials)
        {
            const double phase = 2.0 * 3.14159265358979323846 * fundamentalHz * partial.multiplier
                                * static_cast<double>(i) / kSampleRate;
            sample += partial.amplitude * static_cast<float>(std::sin(phase));
        }

        signal[i] = sample;
    }

    return signal;
}
}

// ---------------------------------------------------------------------

// Silêncio não produz leitura nenhuma — isValid() precisa continuar falso,
// não inventar uma nota a partir de ruído de quantização.
void testSilenceIsNotValid()
{
    std::cout << "silencio nao produz leitura\n";

    std::vector<float> buffer(4096, 0.0f);

    Tuner tuner;
    tuner.prepare(kSampleRate, 64);
    tuner.process(buffer);

    check(!tuner.isValid(), "isValid() e falso com silencio");
    checkClose(tuner.frequencyHz(), 0.0f, "frequencyHz() e 0.0 com silencio");
}

// Um tom de 440 Hz é detectado como A4, perto de 0 cents.
void testDetectsA440()
{
    std::cout << "detecta 440 Hz como A4\n";

    std::vector<float> buffer = generateSine(440.0);

    Tuner tuner;
    tuner.prepare(kSampleRate, 64);
    tuner.process(buffer);

    check(tuner.isValid(), "isValid() e verdadeiro com um tom limpo");
    check(tuner.noteName() == "A4", "nota detectada e A4");
    check(std::fabs(tuner.frequencyHz() - 440.0f) < 3.0f, "frequencia perto de 440 Hz");
    check(std::fabs(tuner.centsOff()) < 15.0f, "cents perto de 0 (afinado)");
}

// A corda mi grave aberta de uma guitarra (~82.41 Hz) é detectada como E2 —
// prova de que a janela de análise é longa o bastante pra notas graves
// também, não só pra frequências de teste convenientes como 440 Hz.
void testDetectsLowE()
{
    std::cout << "detecta mi grave (~82.41 Hz) como E2\n";

    std::vector<float> buffer = generateSine(82.41);

    Tuner tuner;
    tuner.prepare(kSampleRate, 64);
    tuner.process(buffer);

    check(tuner.isValid(), "isValid() e verdadeiro");
    check(tuner.noteName() == "E2", "nota detectada e E2");
    check(std::fabs(tuner.frequencyHz() - 82.41f) < 3.0f, "frequencia perto de 82.41 Hz");
}

// Uma nota desafinada pra cima (450 Hz, ~39 cents acima de A4) ainda é
// reconhecida como A4 (é a nota mais próxima), mas com cents claramente
// positivo — prova de que centsOff() reflete a direção do desvio.
void testSharpNoteShowsPositiveCents()
{
    std::cout << "nota desafinada para cima mostra cents positivo\n";

    std::vector<float> buffer = generateSine(450.0);

    Tuner tuner;
    tuner.prepare(kSampleRate, 64);
    tuner.process(buffer);

    check(tuner.isValid(), "isValid() e verdadeiro");
    check(tuner.noteName() == "A4", "450 Hz ainda e mais perto de A4 que de A#4");
    check(tuner.centsOff() > 10.0f, "cents e claramente positivo (desafinado pra cima)");
}

// O CASO QUE MOTIVOU TROCAR AUTOCORRELAÇÃO POR YIN: testado ao vivo, o mi
// grave saía detectado como outra nota a cada janela — o sinal real tem
// harmônicos mais fortes que a fundamental, e a autocorrelação simples
// confundia um harmônico com o período de verdade. Reproduzido aqui com um
// sinal sintético onde o 2º harmônico é deliberadamente mais forte que a
// fundamental.
void testDetectsFundamentalDespiteStrongHarmonics()
{
    std::cout << "acha a fundamental mesmo com harmonicos mais fortes\n";

    std::vector<float> buffer = generateNoteWithStrongHarmonics(82.41);

    Tuner tuner;
    tuner.prepare(kSampleRate, 64);
    tuner.process(buffer);

    check(tuner.isValid(), "isValid() e verdadeiro");
    check(tuner.noteName() == "E2", "detecta E2 (a fundamental), nao E3 nem B3 (harmonicos)");
    check(std::fabs(tuner.frequencyHz() - 82.41f) < 3.0f, "frequencia perto de 82.41 Hz, nao do dobro/triplo");
}

// process() não altera o áudio que passa por ele — o Tuner só escuta.
void testDoesNotModifyTheSignal()
{
    std::cout << "nao altera o sinal (passthrough)\n";

    std::vector<float> buffer = generateSine(440.0);
    const std::vector<float> original = buffer;

    Tuner tuner;
    tuner.prepare(kSampleRate, 64);
    tuner.process(buffer);

    bool identico = true;

    for (std::size_t i = 0; i < buffer.size(); ++i)
    {
        if (buffer[i] != original[i])
        {
            identico = false;
            break;
        }
    }

    check(identico, "o buffer sai byte a byte igual ao que entrou");
}

// reset() apaga tanto a leitura quanto a janela parcialmente preenchida.
void testResetClearsTheReading()
{
    std::cout << "reset apaga a leitura\n";

    std::vector<float> buffer = generateSine(440.0);

    Tuner tuner;
    tuner.prepare(kSampleRate, 64);
    tuner.process(buffer);

    check(tuner.isValid(), "tinha uma leitura valida antes do reset");

    tuner.reset();

    check(!tuner.isValid(), "isValid() e falso depois do reset");
    checkClose(tuner.frequencyHz(), 0.0f, "frequencyHz() volta a 0.0 depois do reset");
    check(tuner.noteName().empty(), "noteName() volta a vazio depois do reset");
}

// Buffer vazio não pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    Tuner tuner;
    tuner.prepare(kSampleRate, 64);
    tuner.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

int main()
{
    std::cout << "\n=== testes do Tuner ===\n\n";

    testSilenceIsNotValid();
    testDetectsA440();
    testDetectsLowE();
    testSharpNoteShowsPositiveCents();
    testDetectsFundamentalDespiteStrongHarmonics();
    testDoesNotModifyTheSignal();
    testResetClearsTheReading();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
