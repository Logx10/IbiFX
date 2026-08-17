// Testes da CommandQueue e da segurança entre threads.
//
// Boa parte destes testes roda com DUAS THREADS DE VERDADE. Testar código
// concorrente numa thread só prova muito pouco: a corrida simplesmente não
// acontece. Aqui as threads disputam de fato, com milhares de operações, e o
// que se verifica é que nada se perde, nada se duplica e nada chega corrompido.
//
// Testes assim não são prova matemática — uma corrida pode não se manifestar
// numa execução. São rede de segurança: falham de forma intermitente quando o
// código está errado, e é por isso que o volume de operações é alto.
//
// A infra de verificação vive em test_helpers.h.

#include <atomic>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "CommandQueue.h"
#include "GainProcessor.h"
#include "ModuleChain.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// A garantia de que tudo isto vale a pena.
//
// Se atomic<float> ou atomic<size_t> precisassem de cadeado interno, a thread
// de áudio poderia BLOQUEAR ao ler um parâmetro — exatamente o que estamos
// tentando evitar. Em toda plataforma que nos interessa eles são lock-free,
// mas isso é propriedade da implementação, não promessa do padrão.
void testAtomicsAreLockFree()
{
    std::cout << "os atomicos usados sao lock-free\n";

    check(std::atomic<float>::is_always_lock_free, "atomic<float> e lock-free");
    check(std::atomic<std::size_t>::is_always_lock_free, "atomic<size_t> e lock-free");
}

// Uma fila nova está vazia.
void testStartsEmpty()
{
    std::cout << "fila comeca vazia\n";

    CommandQueue queue(8);

    check(queue.empty(), "empty() e verdadeiro");
    check(queue.size() == 0, "size() e zero");
    check(queue.capacity() == 8, "capacidade e a pedida");
}

// O que entra é o que sai, na mesma ordem.
void testPushAndPopPreserveOrder()
{
    std::cout << "ordem preservada\n";

    CommandQueue queue(8);

    for (int i = 0; i < 5; ++i)
    {
        Command command;
        command.type = Command::Type::SetParameter;
        command.moduleIndex = static_cast<std::size_t>(i);
        command.value = static_cast<float>(i) * 10.0f;

        check(queue.push(command), "deposito bem-sucedido");
    }

    check(queue.size() == 5, "5 comandos pendentes");

    bool ordemCorreta = true;
    for (int i = 0; i < 5; ++i)
    {
        Command out;
        if (!queue.pop(out) || out.moduleIndex != static_cast<std::size_t>(i))
        {
            ordemCorreta = false;
        }
    }

    check(ordemCorreta, "saiu na mesma ordem em que entrou");
    check(queue.empty(), "a fila voltou a ficar vazia");
}

// Retirar de fila vazia devolve false, sem esperar.
void testPopOnEmptyReturnsFalse()
{
    std::cout << "retirar de fila vazia\n";

    CommandQueue queue(4);

    Command out;
    check(!queue.pop(out), "pop devolve false");
}

// Fila cheia recusa em vez de bloquear.
//
// Bloquear aqui devolveria justamente o problema que a fila existe para
// evitar. Quem chama decide o que fazer com a recusa.
void testFullQueueRefusesWithoutBlocking()
{
    std::cout << "fila cheia recusa sem bloquear\n";

    CommandQueue queue(3);

    Command command;
    check(queue.push(command), "1o cabe");
    check(queue.push(command), "2o cabe");
    check(queue.push(command), "3o cabe");
    check(!queue.push(command), "o 4o e recusado");

    check(queue.size() == 3, "a fila tem exatamente a capacidade");
}

// O índice dá a volta corretamente.
//
// Um erro no cálculo circular só aparece depois de a fila dar a primeira
// volta, então este teste passa muito mais comandos do que a capacidade.
void testWrapsAroundCorrectly()
{
    std::cout << "o indice circular da a volta\n";

    CommandQueue queue(4);

    bool tudoCerto = true;

    for (int volta = 0; volta < 50; ++volta)
    {
        Command command;
        command.moduleIndex = static_cast<std::size_t>(volta);

        if (!queue.push(command))
        {
            tudoCerto = false;
        }

        Command out;
        if (!queue.pop(out) || out.moduleIndex != static_cast<std::size_t>(volta))
        {
            tudoCerto = false;
        }
    }

    check(tudoCerto, "50 idas e voltas sem erro numa fila de 4");
}

// Capacidade zero é erro de programação.
void testZeroCapacityThrows()
{
    std::cout << "capacidade zero lanca\n";

    bool lancou = false;
    try
    {
        CommandQueue queue(0);
    }
    catch (const std::invalid_argument&)
    {
        lancou = true;
    }

    check(lancou, "capacidade zero lanca invalid_argument");
}

// DUAS THREADS DE VERDADE.
//
// Uma deposita 100 mil comandos numerados; a outra retira. Ao fim, todos
// precisam ter chegado, em ordem e sem repetição. Se houvesse corrida nos
// índices, apareceriam números fora de ordem ou perdidos.
void testProducerConsumerAcrossThreads()
{
    std::cout << "produtor e consumidor em threads separadas\n";

    constexpr std::size_t total = 100000;

    CommandQueue queue(64);

    std::atomic<bool> ordemQuebrada{false};
    std::atomic<std::size_t> recebidos{0};

    std::thread consumidor([&]
    {
        std::size_t esperado = 0;

        while (esperado < total)
        {
            Command out;

            if (queue.pop(out))
            {
                if (out.moduleIndex != esperado)
                {
                    ordemQuebrada = true;
                }

                ++esperado;
                recebidos.store(esperado, std::memory_order_relaxed);
            }
        }
    });

    for (std::size_t i = 0; i < total; ++i)
    {
        Command command;
        command.moduleIndex = i;

        // Fila cheia: tenta de novo. Um produtor de verdade poderia
        // descartar, mas aqui queremos verificar que nada se perde.
        while (!queue.push(command))
        {
            std::this_thread::yield();
        }
    }

    consumidor.join();

    check(!ordemQuebrada.load(), "nenhum comando chegou fora de ordem");
    check(recebidos.load() == total, "todos os 100000 comandos chegaram");
}

// Ajustar um parâmetro de outra thread enquanto o áudio processa.
//
// É o caso real: alguém girando um knob enquanto a guitarra toca. Sem o
// atomic no Parameter isto seria corrida de dados — comportamento indefinido,
// não apenas um valor desatualizado.
void testParameterChangesWhileProcessing()
{
    std::cout << "parametro mudando durante o processamento\n";

    GainProcessor processor;
    processor.prepare(48000.0, 128);

    std::atomic<bool> parar{false};
    std::atomic<std::size_t> blocos{0};

    // "Thread de áudio": processa sem parar.
    std::thread audio([&]
    {
        std::vector<float> buffer(128, 0.5f);

        while (!parar.load(std::memory_order_relaxed))
        {
            buffer.assign(128, 0.5f);
            processor.process(buffer);
            blocos.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // "Thread de controle": gira o knob de um lado para o outro.
    for (int i = 0; i < 20000; ++i)
    {
        processor.setGain((i % 2 == 0) ? 0.5f : 2.0f);
    }

    parar.store(true);
    audio.join();

    check(blocos.load() > 0, "a thread de audio processou blocos");

    const float finalGain = processor.gain();
    check(finalGain == 0.5f || finalGain == 2.0f, "o ganho final e um dos dois valores validos");
    check(finalGain >= -8.0f && finalGain <= 8.0f, "e continua dentro da faixa");
}

// A cadeia aceita comandos de outra thread enquanto processa.
//
// O teste anterior cobriu o parâmetro isolado. Aqui a mudança passa pela
// fila, que é o caminho para bypass e reset — coisas que não cabem num
// atomic<float>.
void testChainAcceptsCommandsWhileProcessing()
{
    std::cout << "cadeia recebendo comandos durante o processamento\n";

    ModuleChain chain;

    auto gain = std::make_unique<GainProcessor>();
    gain->setGain(1.0f);
    chain.add(std::move(gain));

    chain.prepare(48000.0, 128);

    std::atomic<bool> parar{false};
    std::atomic<std::size_t> blocos{0};

    std::thread audio([&]
    {
        std::vector<float> buffer(128, 0.5f);

        while (!parar.load(std::memory_order_relaxed))
        {
            buffer.assign(128, 0.5f);
            chain.process(buffer);
            blocos.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::size_t enviados = 0;
    std::size_t recusados = 0;

    for (int i = 0; i < 20000; ++i)
    {
        Command command;
        command.type = Command::Type::SetBypass;
        command.moduleIndex = 0;
        command.value = (i % 2 == 0) ? 1.0f : 0.0f;

        if (chain.pushCommand(command))
        {
            ++enviados;
        }
        else
        {
            ++recusados;
        }
    }

    parar.store(true);
    audio.join();

    check(blocos.load() > 0, "a thread de audio processou blocos");
    check(enviados > 0, "comandos foram aceitos");
    check(chain.size() == 1, "a cadeia continua intacta");

    // Recusa não é falha: significa que o controle andou mais rápido que o
    // áudio e a fila encheu. O importante é não ter travado nem corrompido.
    std::cout << "          (" << enviados << " aceitos, " << recusados << " recusados por fila cheia)\n";
}

// Comando com índice inexistente é ignorado, e não derruba nada.
//
// Na thread de áudio não dá para lançar. Um comando pode ter sido criado
// antes de um módulo ser removido, e ignorar é a resposta menos danosa.
void testCommandWithInvalidIndexIsIgnored()
{
    std::cout << "comando com indice invalido e ignorado\n";

    ModuleChain chain;
    chain.add(std::make_unique<GainProcessor>());
    chain.prepare(48000.0, 8);

    Command command;
    command.type = Command::Type::SetParameter;
    command.moduleIndex = 99;
    command.parameterIndex = 0;
    command.value = 5.0f;

    chain.pushCommand(command);

    std::vector<float> buffer(8, 1.0f);
    chain.process(buffer);

    check(chain.size() == 1, "a cadeia continua intacta");
    checkClose(chain.moduleAt(0).parameterAt(0).value(), 1.0f, "nenhum parametro foi alterado");
}

// O comando chega ao módulo certo.
void testCommandReachesTheRightModule()
{
    std::cout << "o comando chega ao modulo certo\n";

    ModuleChain chain;
    chain.add(std::make_unique<GainProcessor>());
    chain.add(std::make_unique<GainProcessor>());
    chain.prepare(48000.0, 8);

    Command command;
    command.type = Command::Type::SetParameter;
    command.moduleIndex = 1;
    command.parameterIndex = 0;
    command.value = 3.0f;

    chain.pushCommand(command);
    check(chain.pendingCommandCount() == 1, "o comando ficou pendente");

    std::vector<float> buffer(8, 0.0f);
    chain.process(buffer);

    check(chain.pendingCommandCount() == 0, "a fila foi esvaziada pelo process");
    checkClose(chain.moduleAt(0).parameterAt(0).value(), 1.0f, "o modulo 0 nao mudou");
    checkClose(chain.moduleAt(1).parameterAt(0).value(), 3.0f, "o modulo 1 recebeu o valor");
}

int main()
{
    std::cout << "\n=== testes da CommandQueue e de concorrencia ===\n\n";

    testAtomicsAreLockFree();
    testStartsEmpty();
    testPushAndPopPreserveOrder();
    testPopOnEmptyReturnsFalse();
    testFullQueueRefusesWithoutBlocking();
    testWrapsAroundCorrectly();
    testZeroCapacityThrows();
    testProducerConsumerAcrossThreads();
    testParameterChangesWhileProcessing();
    testChainAcceptsCommandsWhileProcessing();
    testCommandWithInvalidIndexIsIgnored();
    testCommandReachesTheRightModule();

    return reportResults();
}
