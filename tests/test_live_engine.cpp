// Testes do dispositivo de áudio e do LiveEngine.
//
// TODOS usam o backend NULO do miniaudio, em que a biblioteca gera os blocos
// pelo relógio, sem placa de som. Isso permite verificar automaticamente que
// o dispositivo abre, que o callback roda na thread de áudio, que o DSP é
// executado e que os comandos chegam — sem depender de hardware, de permissão
// de microfone nem de alguém escutando.
//
// O que estes testes NÃO provam: que sai som pelo alto-falante. Isso só se
// verifica ouvindo, e o §50 do AI_GUIDELINES já diz que testes não substituem
// audição.
//
// A infra de verificação vive em test_helpers.h.

#include <chrono>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "AudioDevice.h"
#include "Delay.h"
#include "GainProcessor.h"
#include "LiveEngine.h"
#include "test_helpers.h"

namespace
{
// Espera até o dispositivo processar pelo menos `wanted` blocos, ou desistir.
//
// Dormir por um tempo fixo tornaria o teste lento e ainda assim frágil em
// máquina carregada. Esperar por uma condição com prazo máximo é mais rápido
// no caso comum e mais robusto no caso ruim.
bool waitForBlocks(const AudioDevice& device, std::size_t wanted, int maxMillis = 3000)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(maxMillis);

    while (std::chrono::steady_clock::now() < deadline)
    {
        if (device.processedBlocks() >= wanted)
        {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return false;
}

bool waitForEngineBlocks(const LiveEngine& engine, std::size_t wanted, int maxMillis = 3000)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(maxMillis);

    while (std::chrono::steady_clock::now() < deadline)
    {
        if (engine.processedBlocks() >= wanted)
        {
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return false;
}
}

// ---------------------------------------------------------------------

// O dispositivo abre e chama o callback repetidamente.
void testDeviceStartsAndCallsBack()
{
    std::cout << "o dispositivo abre e chama o callback\n";

    AudioDevice device;

    const bool started = device.start(
        [](float* output, const float*, std::size_t frames, std::size_t channels)
        {
            for (std::size_t i = 0; i < frames * channels; ++i)
            {
                output[i] = 0.0f;
            }
        },
        AudioDevice::Mode::Null, 48000.0, 128);

    check(started, "start devolveu true");

    if (!started)
    {
        std::cout << "          erro: " << device.lastError() << "\n";
        return;
    }

    check(device.isRunning(), "isRunning e verdadeiro");
    check(device.sampleRate() > 0.0, "negociou um sample rate");
    check(device.channelCount() > 0, "negociou pelo menos um canal");

    check(waitForBlocks(device, 5), "o callback rodou pelo menos 5 blocos");

    device.stop();
    check(!device.isRunning(), "parou depois do stop");
}

// stop() pode ser chamado sem start(), e duas vezes seguidas.
void testStopIsSafeToRepeat()
{
    std::cout << "stop repetido e seguro\n";

    AudioDevice device;

    device.stop();
    device.stop();

    check(!device.isRunning(), "continua parado");
}

// Iniciar duas vezes é recusado com mensagem.
void testDoubleStartIsRefused()
{
    std::cout << "iniciar duas vezes e recusado\n";

    AudioDevice device;

    const auto silence = [](float* output, const float*, std::size_t frames, std::size_t channels)
    {
        for (std::size_t i = 0; i < frames * channels; ++i)
        {
            output[i] = 0.0f;
        }
    };

    if (!device.start(silence, AudioDevice::Mode::Null))
    {
        check(false, "o primeiro start deveria ter funcionado");
        return;
    }

    check(!device.start(silence, AudioDevice::Mode::Null), "o segundo start devolve false");
    check(!device.lastError().empty(), "e explica o motivo");

    device.stop();
}

// Callback vazio é recusado antes de abrir qualquer coisa.
void testEmptyCallbackIsRefused()
{
    std::cout << "callback vazio e recusado\n";

    AudioDevice device;

    check(!device.start(nullptr, AudioDevice::Mode::Null), "start com callback nulo devolve false");
    check(!device.lastError().empty(), "com mensagem de erro");
}

// Parâmetros inválidos são recusados.
void testInvalidParametersAreRefused()
{
    std::cout << "parametros invalidos sao recusados\n";

    AudioDevice device;

    const auto silence = [](float* output, const float*, std::size_t frames, std::size_t channels)
    {
        for (std::size_t i = 0; i < frames * channels; ++i)
        {
            output[i] = 0.0f;
        }
    };

    check(!device.start(silence, AudioDevice::Mode::Null, 0.0, 128), "sample rate zero e recusado");
    check(!device.start(silence, AudioDevice::Mode::Null, 48000.0, 0), "bloco zero e recusado");
}

// O engine roda a cadeia dentro do callback.
void testEngineRunsTheChain()
{
    std::cout << "o engine roda a cadeia\n";

    LiveEngine engine;

    auto gain = std::make_unique<GainProcessor>();
    gain->setGain(2.0f);
    engine.chain().add(std::move(gain));

    const bool started = engine.start(AudioDevice::Mode::Null, 48000.0, 128);

    check(started, "o engine iniciou");

    if (!started)
    {
        std::cout << "          erro: " << engine.lastError() << "\n";
        return;
    }

    check(engine.isRunning(), "esta rodando");
    check(engine.sampleRate() > 0.0, "tem sample rate negociado");
    check(waitForEngineBlocks(engine, 5), "processou pelo menos 5 blocos");

    engine.stop();
    check(!engine.isRunning(), "parou");
}

// O sample rate negociado chega aos módulos.
//
// O driver pode devolver um valor diferente do pedido, e é o negociado que
// precisa alimentar o prepare() — senão o tempo do delay sairia errado.
void testNegotiatedSampleRateReachesModules()
{
    std::cout << "o sample rate negociado chega aos modulos\n";

    LiveEngine engine;

    auto echo = std::make_unique<Delay>();
    echo->setTime(0.5f);
    engine.chain().add(std::move(echo));

    if (!engine.start(AudioDevice::Mode::Null, 48000.0, 128))
    {
        check(false, "o engine deveria ter iniciado");
        return;
    }

    // O buffer do delay é dimensionado pelo tempo máximo vezes o sample rate.
    // Se o prepare tivesse recebido zero, ele estaria vazio.
    const auto& delay = static_cast<const Delay&>(engine.chain().moduleAt(0));

    check(delay.bufferSize() > 0, "o delay alocou o buffer circular");

    const std::size_t esperado = static_cast<std::size_t>(2.0 * engine.sampleRate()) + 1;
    check(delay.bufferSize() == esperado, "o tamanho corresponde ao sample rate negociado");

    engine.stop();
}

// Comandos chegam ao módulo com o áudio rodando.
//
// É o caso real: girar um knob enquanto a guitarra toca. O comando atravessa
// a fila sem bloqueio e é aplicado no início de um bloco.
void testCommandsReachModulesWhileRunning()
{
    std::cout << "comandos chegam com o audio rodando\n";

    LiveEngine engine;

    auto gain = std::make_unique<GainProcessor>();
    gain->setGain(1.0f);
    engine.chain().add(std::move(gain));

    if (!engine.start(AudioDevice::Mode::Null, 48000.0, 128))
    {
        check(false, "o engine deveria ter iniciado");
        return;
    }

    waitForEngineBlocks(engine, 3);

    check(engine.setParameter(0, 0, 4.0f), "o comando foi aceito pela fila");

    // Espera blocos suficientes para o comando ser consumido.
    const std::size_t antes = engine.processedBlocks();
    waitForEngineBlocks(engine, antes + 5);

    checkClose(engine.chain().moduleAt(0).parameterAt(0).value(), 4.0f,
               "o parametro chegou ao modulo");

    check(engine.setBypassed(0, true), "o bypass foi aceito");
    check(engine.resetModule(0), "o reset foi aceito");

    engine.stop();
}

// A cadeia continua íntegra depois de rodar.
void testChainSurvivesRunning()
{
    std::cout << "a cadeia sobrevive ao processamento\n";

    LiveEngine engine;

    engine.chain().add(std::make_unique<GainProcessor>());
    engine.chain().add(std::make_unique<Delay>());

    if (!engine.start(AudioDevice::Mode::Null, 48000.0, 128))
    {
        check(false, "o engine deveria ter iniciado");
        return;
    }

    waitForEngineBlocks(engine, 10);
    engine.stop();

    check(engine.chain().size() == 2, "os 2 modulos continuam la");
    check(std::string(engine.chain().moduleAt(0).name()) == "Gain", "o primeiro e o Gain");
    check(std::string(engine.chain().moduleAt(1).name()) == "Delay", "o segundo e o Delay");
}

// Um engine sem módulo nenhum roda sem quebrar.
void testEmptyChainRuns()
{
    std::cout << "engine sem modulos\n";

    LiveEngine engine;

    if (!engine.start(AudioDevice::Mode::Null, 48000.0, 128))
    {
        check(false, "o engine deveria ter iniciado");
        return;
    }

    check(waitForEngineBlocks(engine, 5), "processou blocos com a cadeia vazia");

    engine.stop();
}

// Iniciar e parar várias vezes não vaza nem quebra.
void testRestartCycles()
{
    std::cout << "ciclos de start e stop\n";

    LiveEngine engine;
    engine.chain().add(std::make_unique<GainProcessor>());

    bool todosIniciaram = true;

    for (int i = 0; i < 3; ++i)
    {
        if (!engine.start(AudioDevice::Mode::Null, 48000.0, 128))
        {
            todosIniciaram = false;
            break;
        }

        waitForEngineBlocks(engine, 2);
        engine.stop();
    }

    check(todosIniciaram, "3 ciclos de start/stop funcionaram");
    check(!engine.isRunning(), "terminou parado");
}

int main()
{
    std::cout << "\n=== testes do dispositivo de audio e do LiveEngine ===\n\n";

    testDeviceStartsAndCallsBack();
    testStopIsSafeToRepeat();
    testDoubleStartIsRefused();
    testEmptyCallbackIsRefused();
    testInvalidParametersAreRefused();
    testEngineRunsTheChain();
    testNegotiatedSampleRateReachesModules();
    testCommandsReachModulesWhileRunning();
    testChainSurvivesRunning();
    testEmptyChainRuns();
    testRestartCycles();

    return reportResults();
}
