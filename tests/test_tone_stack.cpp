// Testes do ToneStack.
//
// Não temos como comparar contra os números exatos do SPICE que o artigo de
// Yeh & Smith usa pra verificação (estão em gráficos, não em tabelas) — mas
// dá para testar propriedades REAIS do circuito, derivadas do próprio
// artigo, e verificar que o código as reproduz:
//
//   - o circuito é passivo e estável: nenhuma entrada razoável deveria
//     fazer a saída explodir, em nenhum ajuste dos três controles;
//   - "existe um zero em DC" (citação do artigo): um sinal constante
//     deveria ir sumindo até quase zero, porque o circuito é todo
//     capacitivo em série — corrente contínua não atravessa capacitor;
//   - treble sobe grave/agudo na direção certa, mesmo sem saber o valor
//     exato — testado com tons de verdade, grave e agudo.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <iostream>
#include <vector>

#include "ToneStack.h"
#include "test_helpers.h"

namespace
{
constexpr double kSampleRate = 44100.0;

std::vector<float> generateSine(double frequency, std::size_t sampleCount, float amplitude = 0.5f)
{
    std::vector<float> signal(sampleCount);

    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        const double phase = 2.0 * 3.14159265358979323846 * frequency * static_cast<double>(i) / kSampleRate;
        signal[i] = amplitude * static_cast<float>(std::sin(phase));
    }

    return signal;
}

// RMS (root mean square) da segunda metade do sinal — pula o transiente do
// filtro assentando e mede só o regime permanente.
float rmsOfSecondHalf(const std::vector<float>& signal)
{
    const std::size_t start = signal.size() / 2;
    double soma = 0.0;

    for (std::size_t i = start; i < signal.size(); ++i)
    {
        soma += static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
    }

    return static_cast<float>(std::sqrt(soma / static_cast<double>(signal.size() - start)));
}
}

// ---------------------------------------------------------------------

// Circuito puramente capacitivo em série com a entrada: corrente contínua
// não atravessa capacitor, então nenhuma energia de DC deveria sobreviver
// no regime permanente — em qualquer ajuste dos três controles.
//
// Isso também é uma checagem indireta da transformada bilinear em si: DC
// mapeia exatamente em z=1, e a soma dos coeficientes do numerador digital
// (B0+B1+B2+B3) se cancela par a par por construção da fórmula — não
// importa quem são b1, b2 e b3. Se esse cancelamento estivesse errado no
// código, todo ajuste de controle vazaria um pouco de DC.
//
// 20000 AMOSTRAS, NÃO UM NÚMERO QUALQUER
// Com bass/mid/treble no máximo, a constante de tempo do circuito fica
// maior — fisicamente esperado, mais resistência no caminho significa
// decaimento mais lento — e medi diretamente quantas amostras eram
// necessárias antes de escrever este número: a configuração mais lenta
// (bass=mid=treble=1.0) levou perto de 9000 amostras (~200 ms) pra cair
// abaixo de 0.01. 20000 dá margem confortável sem o teste ficar lento.
void testDCIsBlockedAtAnyControlSetting()
{
    std::cout << "DC e bloqueado em qualquer ajuste dos controles\n";

    const float configuracoes[][3] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}};

    for (const auto& config : configuracoes)
    {
        std::vector<float> dc(20000, 1.0f);

        ToneStack toneStack;
        toneStack.setBass(config[0]);
        toneStack.setMid(config[1]);
        toneStack.setTreble(config[2]);
        toneStack.prepare(kSampleRate, 64);
        toneStack.process(dc);

        check(std::fabs(dc.back()) < 0.01f, "DC convergiu perto de zero nesta configuracao");
    }
}

// O circuito é passivo (só resistores e capacitores, sem ganho nenhum) —
// não deveria amplificar sem limite nem oscilar. Testado nos quatro cantos
// e no centro do cubo de controles, com um tom real, não um impulso.
void testStaysStableAcrossControlRange()
{
    std::cout << "permanece estavel em todo o range dos controles\n";

    const float configuracoes[][3] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.5f, 0.5f, 0.5f}};

    for (const auto& config : configuracoes)
    {
        std::vector<float> sinal = generateSine(440.0, 4000);

        ToneStack toneStack;
        toneStack.setBass(config[0]);
        toneStack.setMid(config[1]);
        toneStack.setTreble(config[2]);
        toneStack.prepare(kSampleRate, 64);
        toneStack.process(sinal);

        for (float sample : sinal)
        {
            check(std::isfinite(sample), "amostra e finita");
            check(std::fabs(sample) < 5.0f, "amostra nao explodiu (bem abaixo de 5x a entrada)");
        }
    }
}

// Treble em 1.0 deixa passar mais agudo que treble em 0.0 — testado com um
// tom de 5 kHz (a citação do artigo: "a resposta de um alto-falante de
// guitarra vai de 100 Hz a 6000 Hz", então 5 kHz é bem "agudo" nesse
// contexto), bass e mid fixos no meio.
void testTrebleBoostsHighFrequencies()
{
    std::cout << "treble alto deixa passar mais agudo\n";

    auto medirComTreble = [](float trebleValue) -> float
    {
        std::vector<float> sinal = generateSine(5000.0, 4000);

        ToneStack toneStack;
        toneStack.setBass(0.5f);
        toneStack.setMid(0.5f);
        toneStack.setTreble(trebleValue);
        toneStack.prepare(kSampleRate, 64);
        toneStack.process(sinal);

        return rmsOfSecondHalf(sinal);
    };

    const float rmsBaixo = medirComTreble(0.0f);
    const float rmsAlto = medirComTreble(1.0f);

    check(rmsAlto > rmsBaixo, "treble 1.0 deixa mais energia de 5 kHz passar que treble 0.0");
}

// Bass em 1.0 deixa passar mais grave que bass em 0.0 — testado com 80 Hz,
// mid e treble fixos no meio.
void testBassBoostsLowFrequencies()
{
    std::cout << "bass alto deixa passar mais grave\n";

    auto medirComBass = [](float bassValue) -> float
    {
        std::vector<float> sinal = generateSine(80.0, 4000);

        ToneStack toneStack;
        toneStack.setBass(bassValue);
        toneStack.setMid(0.5f);
        toneStack.setTreble(0.5f);
        toneStack.prepare(kSampleRate, 64);
        toneStack.process(sinal);

        return rmsOfSecondHalf(sinal);
    };

    const float rmsBaixo = medirComBass(0.0f);
    const float rmsAlto = medirComBass(1.0f);

    check(rmsAlto > rmsBaixo, "bass 1.0 deixa mais energia de 80 Hz passar que bass 0.0");
}

// reset() apaga a memória do filtro (3 amostras de entrada, 3 de saída):
// depois de processar sinal de verdade e resetar, um bloco silencioso tem
// que sair silencioso — nada de eco numérico do que veio antes.
void testResetClearsFilterHistory()
{
    std::cout << "reset apaga a memoria do filtro\n";

    ToneStack toneStack;
    toneStack.prepare(kSampleRate, 64);

    std::vector<float> tom = generateSine(440.0, 1000);
    toneStack.process(tom);

    toneStack.reset();

    std::vector<float> silencio(100, 0.0f);
    toneStack.process(silencio);

    for (float sample : silencio)
    {
        checkClose(sample, 0.0f, "amostra continua 0.0 depois do reset");
    }
}

// Os getters devolvem o que os setters guardaram.
void testParameterRoundTrip()
{
    std::cout << "setters e getters\n";

    ToneStack toneStack;

    checkClose(toneStack.bass(), 0.5f, "bass padrao e 0.5");
    checkClose(toneStack.mid(), 0.5f, "mid padrao e 0.5");
    checkClose(toneStack.treble(), 0.5f, "treble padrao e 0.5");

    toneStack.setBass(0.2f);
    toneStack.setMid(0.8f);
    toneStack.setTreble(0.1f);

    checkClose(toneStack.bass(), 0.2f, "bass vira 0.2");
    checkClose(toneStack.mid(), 0.8f, "mid vira 0.8");
    checkClose(toneStack.treble(), 0.1f, "treble vira 0.1");
}

// Buffer vazio não pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    ToneStack toneStack;
    toneStack.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

int main()
{
    std::cout << "\n=== testes do ToneStack ===\n\n";

    testDCIsBlockedAtAnyControlSetting();
    testStaysStableAcrossControlRange();
    testTrebleBoostsHighFrequencies();
    testBassBoostsLowFrequencies();
    testResetClearsFilterHistory();
    testParameterRoundTrip();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
