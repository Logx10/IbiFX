// Testes do processamento offline.
//
// Verificam o que acontece entre o arquivo e a cadeia: fatiamento em blocos,
// isolamento entre canais e o gerador de sinal de teste.
//
// A infra de verificação vive em test_helpers.h.

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include "Delay.h"
#include "GainProcessor.h"
#include "ModuleChain.h"
#include "offline.h"
#include "test_helpers.h"

namespace
{
ModuleChain makeGainChain(float gain)
{
    ModuleChain chain;

    auto processor = std::make_unique<GainProcessor>();
    processor->setGain(gain);
    chain.add(std::move(processor));

    return chain;
}
}

// ---------------------------------------------------------------------

// O arquivo inteiro passa pela cadeia, mantendo formato e tamanho.
void testProcessesWholeFile()
{
    std::cout << "processa o arquivo inteiro\n";

    WavFile input;
    input.sampleRate = 1000.0;
    input.channels = {{0.1f, 0.2f, 0.3f, 0.4f}};

    ModuleChain chain = makeGainChain(2.0f);
    const WavFile output = offline::processFile(input, chain, 2);

    check(output.channelCount() == 1, "manteve 1 canal");
    check(output.frameCount() == 4, "manteve 4 frames");
    checkClose(static_cast<float>(output.sampleRate), 1000.0f, "manteve o sample rate");

    checkClose(output.channels[0][0], 0.2f, "0.1 virou 0.2");
    checkClose(output.channels[0][3], 0.8f, "0.4 virou 0.8");
}

// O tamanho de bloco não pode alterar o resultado de um módulo sem estado.
//
// Este é o teste que pega erros de fatiamento: uma amostra pulada ou
// duplicada na fronteira apareceria como divergência entre os dois tamanhos.
void testBlockSizeDoesNotChangeStatelessResult()
{
    std::cout << "o tamanho de bloco nao muda modulo sem estado\n";

    WavFile input;
    input.sampleRate = 1000.0;
    input.channels.assign(1, std::vector<float>(37, 0.0f));

    for (std::size_t i = 0; i < 37; ++i)
    {
        input.channels[0][i] = static_cast<float>(i) / 100.0f;
    }

    ModuleChain chainPequeno = makeGainChain(2.0f);
    ModuleChain chainGrande = makeGainChain(2.0f);

    // 37 não é múltiplo de 4 nem de 64: o último bloco fica incompleto nos
    // dois casos, que é justamente onde erros de borda aparecem.
    const WavFile comBloco4 = offline::processFile(input, chainPequeno, 4);
    const WavFile comBloco64 = offline::processFile(input, chainGrande, 64);

    check(comBloco4.frameCount() == 37, "bloco 4 devolveu 37 frames");
    check(comBloco64.frameCount() == 37, "bloco 64 devolveu 37 frames");

    bool iguais = true;
    for (std::size_t i = 0; i < 37; ++i)
    {
        if (std::fabs(comBloco4.channels[0][i] - comBloco64.channels[0][i]) > 1e-6f)
        {
            iguais = false;
        }
    }

    check(iguais, "os dois tamanhos de bloco dao o mesmo resultado");
}

// Cada canal começa do zero.
//
// Sem o reset entre canais, o eco do esquerdo apareceria no direito — um
// vazamento sutil, audível como imagem estéreo errada.
void testChannelsDoNotBleedIntoEachOther()
{
    std::cout << "os canais nao vazam um no outro\n";

    WavFile input;
    input.sampleRate = 100.0;

    // Esquerdo tem um impulso; direito é puro silêncio.
    std::vector<float> esquerdo(20, 0.0f);
    esquerdo[0] = 1.0f;
    std::vector<float> direito(20, 0.0f);

    input.channels = {esquerdo, direito};

    ModuleChain chain;
    auto echo = std::make_unique<Delay>();
    echo->setTime(0.05f);
    echo->setFeedback(0.8f);
    echo->setMix(1.0f);
    chain.add(std::move(echo));

    const WavFile output = offline::processFile(input, chain, 8);

    bool esquerdoTemEco = false;
    for (float sample : output.channels[0])
    {
        if (std::fabs(sample) > 0.01f)
            esquerdoTemEco = true;
    }

    bool direitoEmSilencio = true;
    for (float sample : output.channels[1])
    {
        if (sample != 0.0f)
            direitoEmSilencio = false;
    }

    check(esquerdoTemEco, "o canal esquerdo tem eco");
    check(direitoEmSilencio, "o direito continua em silencio absoluto");
}

// Um módulo com estado atravessa a fronteira dos blocos corretamente.
void testStatefulModuleWorksAcrossBlocks()
{
    std::cout << "modulo com estado atravessa blocos\n";

    WavFile input;
    input.sampleRate = 100.0;
    input.channels.assign(1, std::vector<float>(30, 0.0f));
    input.channels[0][0] = 1.0f;

    ModuleChain chain;
    auto echo = std::make_unique<Delay>();
    echo->setTime(0.1f);   // 10 amostras a 100 Hz
    echo->setFeedback(0.0f);
    echo->setMix(1.0f);
    chain.add(std::move(echo));

    // Blocos de 4 fazem o eco cair no meio do terceiro bloco.
    const WavFile output = offline::processFile(input, chain, 4);

    checkClose(output.channels[0][10], 1.0f, "o eco chega na amostra 10");
    checkClose(output.channels[0][9], 0.0f, "e nao antes");
}

// Cadeia vazia devolve o arquivo intacto.
void testEmptyChainIsTransparent()
{
    std::cout << "cadeia vazia nao altera o arquivo\n";

    WavFile input;
    input.sampleRate = 1000.0;
    input.channels = {{0.3f, -0.7f, 0.1f}};

    ModuleChain chain;
    const WavFile output = offline::processFile(input, chain, 2);

    checkClose(output.channels[0][0], 0.3f, "0.3 continua 0.3");
    checkClose(output.channels[0][1], -0.7f, "-0.7 continua -0.7");
    checkClose(output.channels[0][2], 0.1f, "0.1 continua 0.1");
}

// Tamanho de bloco inválido é erro de programação.
void testInvalidBlockSizeThrows()
{
    std::cout << "tamanho de bloco invalido\n";

    WavFile input;
    input.sampleRate = 1000.0;
    input.channels = {{0.1f}};

    ModuleChain chain;

    bool lancou = false;
    try
    {
        offline::processFile(input, chain, 0);
    }
    catch (const std::invalid_argument&)
    {
        lancou = true;
    }

    check(lancou, "blockSize zero lanca invalid_argument");
}

// O sinal de teste tem as propriedades que o tornam útil.
void testGeneratedSignalProperties()
{
    std::cout << "propriedades do sinal de teste\n";

    const WavFile signal = offline::generateTestSignal(1000.0, 2.0);

    check(signal.channelCount() == 1, "e mono");
    check(signal.frameCount() == 2000, "tem 2000 frames");
    checkClose(static_cast<float>(signal.durationSeconds()), 2.0f, "dura 2 segundos");

    float peak = 0.0f;
    for (float sample : signal.channels[0])
    {
        peak = std::max(peak, std::fabs(sample));
    }

    check(peak > 0.05f, "tem sinal de verdade, nao silencio");
    check(peak <= 1.0f, "nao estoura a faixa");

    // O envelope decai: o começo de uma nota é mais forte que o fim dela.
    float energiaInicio = 0.0f;
    float energiaFim = 0.0f;

    for (std::size_t i = 0; i < 100; ++i)
    {
        energiaInicio += std::fabs(signal.channels[0][i]);
        energiaFim += std::fabs(signal.channels[0][400 + i]);
    }

    check(energiaInicio > energiaFim, "o envelope decai ao longo da nota");
}

// Parâmetros inválidos no gerador são recusados.
void testGeneratorRejectsInvalidArguments()
{
    std::cout << "gerador recusa argumentos invalidos\n";

    bool lancouTaxa = false;
    try
    {
        offline::generateTestSignal(0.0, 1.0);
    }
    catch (const std::invalid_argument&)
    {
        lancouTaxa = true;
    }

    bool lancouDuracao = false;
    try
    {
        offline::generateTestSignal(44100.0, 0.0);
    }
    catch (const std::invalid_argument&)
    {
        lancouDuracao = true;
    }

    check(lancouTaxa, "sample rate zero lanca");
    check(lancouDuracao, "duracao zero lanca");
}

int main()
{
    std::cout << "\n=== testes do processamento offline ===\n\n";

    testProcessesWholeFile();
    testBlockSizeDoesNotChangeStatelessResult();
    testChannelsDoNotBleedIntoEachOther();
    testStatefulModuleWorksAcrossBlocks();
    testEmptyChainIsTransparent();
    testInvalidBlockSizeThrows();
    testGeneratedSignalProperties();
    testGeneratorRejectsInvalidArguments();

    return reportResults();
}
