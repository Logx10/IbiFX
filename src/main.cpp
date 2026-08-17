#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "AudioGraph.h"
#include "LiveEngine.h"
#include "PedalboardUI.h"
#include "Clipper.h"
#include "Delay.h"
#include "GainProcessor.h"
#include "HighPassFilter.h"
#include "ModuleChain.h"
#include "SoftClipper.h"
#include "WavFile.h"
#include "offline.h"

// Demo do IbiFX.
//
// Monta uma cadeia em tempo de execução e a manipula sem recompilar: lista os
// parâmetros que existem, ajusta um deles pelo id, desliga um módulo e
// reordena a cadeia.
//
// A parte importante é o que o setParameter() abaixo NÃO faz: ele não
// menciona GainProcessor, Clipper nem SoftClipper. Recebe dois textos e um
// número, e é só disso que um preset, um controlador MIDI ou um knob de tela
// dispõem.

// Imprime um buffer alinhado, para comparação visual.
void printBuffer(const char* label, const std::vector<float>& buffer)
{
    std::cout << std::left << std::setw(24) << label << std::right;

    for (float sample : buffer)
        std::cout << std::fixed << std::setprecision(2) << std::setw(8) << sample;

    std::cout << '\n';
}

// Descreve a cadeia em uma linha, marcando os módulos em bypass.
void printChain(const ModuleChain& chain)
{
    std::cout << "cadeia: ";

    for (std::size_t i = 0; i < chain.size(); ++i)
    {
        if (i > 0)
            std::cout << " -> ";

        std::cout << chain.moduleAt(i).name();

        if (chain.isBypassed(i))
            std::cout << " (bypass)";
    }

    std::cout << "\n\n";
}

// Lista todos os parâmetros da cadeia, com valor, faixa e forma normalizada.
void printParameters(const ModuleChain& chain)
{
    std::cout << std::left << std::setw(14) << "MODULO"
              << std::setw(12) << "ID"
              << std::setw(10) << "VALOR"
              << std::setw(16) << "FAIXA"
              << "NORMALIZADO\n";

    for (std::size_t i = 0; i < chain.size(); ++i)
    {
        const AudioModule& module = chain.moduleAt(i);

        for (std::size_t p = 0; p < module.parameterCount(); ++p)
        {
            const Parameter& parameter = module.parameterAt(p);

            const std::string range = std::to_string(static_cast<int>(parameter.minValue()))
                                    + " .. "
                                    + std::to_string(static_cast<int>(parameter.maxValue()));

            std::cout << std::left << std::setw(14) << module.name()
                      << std::setw(12) << parameter.id()
                      << std::fixed << std::setprecision(2)
                      << std::setw(10) << parameter.value()
                      << std::setw(16) << range
                      << parameter.normalized() << '\n';
        }
    }

    std::cout << '\n';
}

// Ajusta um parâmetro sem saber de que tipo é o módulo.
//
// Percorre a cadeia procurando o módulo pelo nome e o parâmetro pelo id. É
// exatamente a operação que um preset ou um mapeamento MIDI precisa fazer.
bool setParameter(ModuleChain& chain,
                  const std::string& moduleName,
                  const std::string& parameterId,
                  float value)
{
    for (std::size_t i = 0; i < chain.size(); ++i)
    {
        if (moduleName != chain.moduleAt(i).name())
            continue;

        Parameter* parameter = chain.moduleAt(i).findParameter(parameterId);

        if (parameter == nullptr)
            return false;

        parameter->setValue(value);
        return true;
    }

    return false;
}

// Processa uma cópia do sinal original e imprime entrada e saída.
void runChain(ModuleChain& chain, const std::vector<float>& original)
{
    printChain(chain);

    std::vector<float> buffer = original;
    printBuffer("entrada:", buffer);

    chain.process(buffer);
    printBuffer("saida:", buffer);
}

// Demonstração no terminal, com buffers pequenos e valores conferíveis.
void runTerminalDemo()
{

    // Valores escolhidos para o ganho 4.0 produzir os três casos de uma vez:
    // amostras que continuam na faixa, uma que cai exatamente no teto
    // (0.25 * 4 = 1.0) e várias que estouram.
    const std::vector<float> original = {0.0f, 0.1f, 0.2f, 0.25f, 0.3f, 0.5f, -0.4f, -0.9f};

    ModuleChain chain;
    chain.add(std::make_unique<GainProcessor>());
    chain.add(std::make_unique<Clipper>());

    std::cout << "1. PARAMETROS DA CADEIA — descobertos, nao codificados\n\n";
    printParameters(chain);

    std::cout << "2. AJUSTE GENERICO — setParameter(\"Gain\", \"gain\", 4.0)\n\n";
    setParameter(chain, "Gain", "gain", 4.0f);
    runChain(chain, original);

    std::cout << "\n3. FORA DA FAIXA — o teto do Clipper vai ate 2.0\n\n";
    setParameter(chain, "Clipper", "threshold", 99.0f);
    std::cout << "pedimos 99.0 e o parametro guardou "
              << chain.moduleAt(1).parameterAt(0).value() << "\n\n";
    setParameter(chain, "Clipper", "threshold", 1.0f);

    std::cout << "4. BYPASS — o clipper continua na cadeia, mas e pulado\n\n";
    chain.setBypassed(1, true);
    runChain(chain, original);

    std::cout << "\n5. CORTE TROCADO — o clipper sai, o softclipper entra\n\n";
    chain.remove(1);
    chain.add(std::make_unique<SoftClipper>());
    runChain(chain, original);

    std::cout << "\nno hard clipping, 1.20 / 2.00 / -3.60 viraram todos o mesmo valor.\n";
    std::cout << "no soft, eles continuam distinguiveis entre si.\n";

    // -----------------------------------------------------------------
    // O DELAY — o primeiro modulo com memoria.
    // -----------------------------------------------------------------
    //
    // Sample rate de 10 Hz é absurdo para áudio e ideal para ver o efeito:
    // com time = 0.3 s o atraso dá exatamente 3 amostras, e os ecos cabem
    // numa linha de terminal.
    //
    // A entrada é um impulso — um único 1.0 seguido de silêncio. Onde ele
    // reaparecer na saída é, literalmente, o atraso do módulo.
    std::cout << "\n\n6. DELAY — um impulso e seus ecos (10 Hz, 0.3 s, feedback 0.5)\n\n";

    ModuleChain echoChain;
    echoChain.add(std::make_unique<Delay>());
    echoChain.prepare(10.0, 16);

    setParameter(echoChain, "Delay", "time", 0.3f);
    setParameter(echoChain, "Delay", "feedback", 0.5f);
    setParameter(echoChain, "Delay", "mix", 1.0f);

    std::vector<float> impulso(13, 0.0f);
    impulso[0] = 1.0f;

    printBuffer("impulso:", impulso);
    echoChain.process(impulso);
    printBuffer("ecos:", impulso);

    std::cout << "\ncada eco vale metade do anterior: 1.00, 0.50, 0.25, 0.12...\n";
    std::cout << "e por isso que feedback >= 1.0 nunca pararia de crescer.\n";

    // O eco continua vivo dentro do módulo: o buffer circular ainda guarda o
    // que foi gravado. Processar silêncio agora traz o resto dos ecos.
    std::cout << "\n7. O ESTADO ATRAVESSA OS BLOCOS — agora entra so silencio\n\n";

    std::vector<float> silencio(13, 0.0f);

    printBuffer("silencio:", silencio);
    echoChain.process(silencio);
    printBuffer("ecos:", silencio);

    std::cout << "\nnada entrou, e mesmo assim saiu som: e a memoria do delay.\n";

    // reset() descarta essa memória sem tocar nos parâmetros.
    std::cout << "\n8. RESET — descarta o eco pendente\n\n";

    echoChain.reset();

    std::vector<float> depoisDoReset(13, 0.0f);
    echoChain.process(depoisDoReset);
    printBuffer("apos reset:", depoisDoReset);

    std::cout << "\nsilencio absoluto: o reset esvaziou o buffer circular.\n";

    // -----------------------------------------------------------------
    // SMOOTHING — por que mudar um parametro de uma vez estala.
    // -----------------------------------------------------------------
    //
    // A entrada é um sinal constante de 1.0, o mais simples possível de ler:
    // qualquer coisa que apareça na saída veio do parâmetro, não do sinal.
    //
    // 500 Hz faz a rampa padrão de 20 ms durar exatamente 10 amostras.
    std::cout << "\n\n9. SMOOTHING — ganho indo de 1.0 para 0.0\n\n";

    GainProcessor semRampa;
    semRampa.setGain(0.0f);

    std::vector<float> abrupto(12, 1.0f);
    semRampa.process(abrupto);

    GainProcessor comRampa;
    comRampa.prepare(500.0, 16);
    comRampa.setGain(0.0f);

    std::vector<float> suave(12, 1.0f);
    comRampa.process(suave);

    printBuffer("sem prepare:", abrupto);
    printBuffer("com rampa:", suave);

    std::cout << "\nsem rampa o valor cai de 1.00 para 0.00 entre duas amostras vizinhas.\n";
    std::cout << "esse degrau nao estava no sinal: o ouvido escuta um clique.\n";
    std::cout << "com rampa a queda leva 10 amostras e a onda continua continua.\n";

    // -----------------------------------------------------------------
    // O GRAFO — roteamento que a cadeia linear nao consegue fazer.
    // -----------------------------------------------------------------
    //
    // Na cadeia, cada modulo recebe a saida do anterior. Aqui o sinal se
    // divide em dois caminhos independentes e volta a se somar:
    //
    //              ┌── Gain x2 ──┐
    //     entrada ─┤             ├─ saida
    //              └── Gain x3 ──┘
    //
    // Numa fila isso seria impossivel: o segundo caminho receberia a saida
    // do primeiro em vez do sinal original, e o resultado seria x6 em vez
    // de x2 + x3 = x5.
    std::cout << "\n\n10. GRAFO — dois caminhos paralelos somados\n\n";

    AudioGraph graph;

    auto caminhoA = std::make_unique<GainProcessor>();
    caminhoA->setGain(2.0f);
    const AudioGraph::NodeId noA = graph.addNode(std::move(caminhoA));

    auto caminhoB = std::make_unique<GainProcessor>();
    caminhoB->setGain(3.0f);
    const AudioGraph::NodeId noB = graph.addNode(std::move(caminhoB));

    graph.connectFromInput(noA);
    graph.connectFromInput(noB);
    graph.connectToOutput(noA);
    graph.connectToOutput(noB);

    graph.prepare(1000.0, 16);

    std::vector<float> paralelo = {1.0f, 0.5f, -0.25f, 0.1f};
    printBuffer("entrada:", paralelo);
    graph.process(paralelo);
    printBuffer("A(x2) + B(x3):", paralelo);

    std::cout << "\ncada caminho recebeu o sinal ORIGINAL, nao a saida do outro.\n";
    std::cout << "1.00 virou 2.00 + 3.00 = 5.00, e nao 1.00 x 2 x 3 = 6.00.\n";

    // Um ciclo nao tem ordem valida: para processar A e preciso B, e para
    // processar B e preciso A. O connect() recusa antes de criar.
    std::cout << "\n11. CICLO — o grafo recusa realimentacao sem atraso\n\n";

    auto extra = std::make_unique<GainProcessor>();
    const AudioGraph::NodeId noC = graph.addNode(std::move(extra));
    graph.connect(noA, noC);

    std::cout << "conectar C -> A fecharia um ciclo? "
              << (graph.wouldCreateCycle(noC, noA) ? "sim" : "nao") << "\n";

    try
    {
        graph.connect(noC, noA);
        std::cout << "conexao aceita (isto seria um bug)\n";
    }
    catch (const std::exception& error)
    {
        std::cout << "recusada: " << error.what() << "\n";
    }

    std::cout << "\nordem de processamento: ";
    for (AudioGraph::NodeId id : graph.processingOrder())
    {
        std::cout << graph.moduleAt(id).name() << "(" << id << ") ";
    }
    std::cout << "\n";
}

// Ajustes da cadeia, com os valores padrão.
//
// Os padrões são deliberadamente agressivos: foram escolhidos para a
// diferença entre entrada e saída ficar óbvia, não para soar bonito.
struct ChainSettings
{
    float highPass = 100.0f;
    float gain = 6.0f;
    float drive = 4.0f;
    float delayTime = 0.28f;
    float feedback = 0.45f;
    float mix = 0.35f;

    bool useHighPass = true;
    bool useDrive = true;
    bool useDelay = true;
};

// Monta a cadeia — um pedal de drive seguido de eco.
//
// A ordem é a clássica de pedaleira: a distorção vem ANTES do delay, para que
// os ecos repitam o som já distorcido. Invertida, o delay produziria ecos
// limpos que depois seriam distorcidos juntos, e o resultado vira uma pasta.
void buildChain(ModuleChain& chain, const ChainSettings& settings)
{
    // O filtro vem PRIMEIRO, antes de qualquer ganho ou distorção.
    //
    // Saturação mistura as frequências que entram, e grave forte ocupa a
    // curva inteira do saturador, empastando tudo que vem junto. Cortar o
    // grave depois não conserta: a mistura já aconteceu. É a mesma ordem que
    // todo amplificador de guitarra usa.
    if (settings.useHighPass)
    {
        auto filter = std::make_unique<HighPassFilter>();
        filter->setFrequency(settings.highPass);
        chain.add(std::move(filter));
    }

    auto gain = std::make_unique<GainProcessor>();
    gain->setGain(settings.gain);
    chain.add(std::move(gain));

    if (settings.useDrive)
    {
        auto drive = std::make_unique<SoftClipper>();
        drive->setDrive(settings.drive);
        chain.add(std::move(drive));
    }

    if (settings.useDelay)
    {
        auto echo = std::make_unique<Delay>();
        echo->setTime(settings.delayTime);
        echo->setFeedback(settings.feedback);
        echo->setMix(settings.mix);
        chain.add(std::move(echo));
    }
}

void buildDefaultChain(ModuleChain& chain)
{
    buildChain(chain, ChainSettings{});
}

// Interpreta as opções de linha de comando a partir de `first`.
//
// Lança com mensagem clara em caso de opção desconhecida, valor faltando ou
// número inválido. Falhar aqui é barato; falhar depois, com um parâmetro
// silenciosamente errado, custaria uma sessão de depuração.
ChainSettings parseSettings(int argc, char** argv, int first)
{
    ChainSettings settings;

    for (int i = first; i < argc; ++i)
    {
        const std::string option = argv[i];

        if (option == "--no-highpass")
        {
            settings.useHighPass = false;
            continue;
        }

        if (option == "--no-drive")
        {
            settings.useDrive = false;
            continue;
        }

        if (option == "--no-delay")
        {
            settings.useDelay = false;
            continue;
        }

        if (i + 1 >= argc)
        {
            throw std::runtime_error("a opcao " + option + " precisa de um valor");
        }

        const std::string raw = argv[++i];
        float value = 0.0f;

        try
        {
            value = std::stof(raw);
        }
        catch (const std::exception&)
        {
            throw std::runtime_error("valor invalido para " + option + ": '" + raw + "'");
        }

        if (option == "--highpass")      settings.highPass = value;
        else if (option == "--gain")     settings.gain = value;
        else if (option == "--drive")    settings.drive = value;
        else if (option == "--time")     settings.delayTime = value;
        else if (option == "--feedback") settings.feedback = value;
        else if (option == "--mix")      settings.mix = value;
        else throw std::runtime_error("opcao desconhecida: " + option);
    }

    return settings;
}

// Mostra os valores que os módulos realmente guardaram.
//
// Não são necessariamente os pedidos: a faixa de cada parâmetro limita o que
// entra. Imprimir o valor efetivo evita a confusão de pedir feedback 2.0 e
// não entender por que o som não cresce sem parar.
void printChainSettings(const ModuleChain& chain)
{
    for (std::size_t i = 0; i < chain.size(); ++i)
    {
        const AudioModule& module = chain.moduleAt(i);

        for (std::size_t p = 0; p < module.parameterCount(); ++p)
        {
            const Parameter& parameter = module.parameterAt(p);

            std::cout << "  " << std::left << std::setw(14) << module.name()
                      << std::setw(10) << parameter.id()
                      << std::fixed << std::setprecision(2) << parameter.value() << "\n";
        }
    }

    std::cout << "\n";
}

void printUsage(const char* program)
{
    std::cout << "uso:\n"
              << "  " << program << "                          demonstracao no terminal\n"
              << "  " << program << " --generate saida.wav     gera um sinal de teste\n"
              << "  " << program << " entrada.wav saida.wav    processa um arquivo\n"
              << "  " << program << " --live [segundos]        toca ao vivo pela placa de som\n"
              << "  " << program << " --devices                testa o dispositivo sem hardware\n"
              << "  " << program << " --ui                     pedaleira interativa no terminal\n"
              << "  " << program << " --ui-demo                a pedaleira sem placa de som\n"
              << "\n"
              << "opcoes do processamento de arquivo:\n"
              << "  --highpass N   corta grave antes do drive  (20 a 2000, padrao 100)\n"
              << "  --gain N       volume antes da distorcao   (-8 a 8,   padrao 6.0)\n"
              << "  --drive N      quantidade de distorcao     (0 a 100,  padrao 4.0)\n"
              << "  --time N       atraso do eco em segundos   (0 a 2,    padrao 0.28)\n"
              << "  --feedback N   quantas repeticoes          (0 a 0.95, padrao 0.45)\n"
              << "  --mix N        quanto do eco na saida      (0 a 1,    padrao 0.35)\n"
              << "  --no-highpass  tira o filtro da cadeia\n"
              << "  --no-drive     tira a distorcao da cadeia\n"
              << "  --no-delay     tira o eco da cadeia\n"
              << "\n"
              << "valores fora da faixa param na borda, como o batente de um knob.\n"
              << "\n"
              << "exemplos:\n"
              << "  " << program << " audio/guitar.wav audio/limpo.wav --gain 1 --no-drive\n"
              << "  " << program << " audio/guitar.wav audio/suave.wav --gain 2 --drive 1.5 --mix 0.2\n"
              << "  " << program << " audio/guitar.wav audio/crunch.wav --highpass 250 --gain 2 --drive 2 --no-delay\n"
              << "  " << program << " audio/guitar.wav audio/fuzz.wav --gain 8 --drive 40\n"
              << "\n"
              << "nota: --gain e --drive multiplicam antes da mesma curva, entao\n"
              << "so o PRODUTO deles importa. gain 2 drive 4 soa igual a gain 4 drive 2.\n";
}

// Toca ao vivo: entrada da placa de som -> efeitos -> saida.
//
// CUIDADO COM MICROFONIA: se a entrada for o microfone embutido e a saida for
// o alto-falante embutido, o som volta para a entrada e realimenta. Com ganho
// e distorcao no caminho, isso vira um apito alto muito rapido. Use fones.
int runLive(double seconds)
{
    LiveEngine engine;
    buildDefaultChain(engine.chain());

    std::cout << "AVISO: se a entrada e a saida forem os dispositivos embutidos,\n"
              << "       o som realimenta e vira microfonia. Use fones de ouvido.\n\n";

    if (!engine.start(AudioDevice::Mode::Duplex, 48000.0, 128))
    {
        std::cerr << "erro: " << engine.lastError() << "\n";
        return 1;
    }

    std::cout << "dispositivo: " << engine.deviceName() << "\n"
              << "  " << engine.sampleRate() << " Hz, "
              << engine.channelCount() << " canal(is) de saida\n\n";

    std::cout << "cadeia: ";
    for (std::size_t i = 0; i < engine.chain().size(); ++i)
    {
        if (i > 0)
            std::cout << " -> ";
        std::cout << engine.chain().moduleAt(i).name();
    }
    std::cout << "\n\n";

    std::cout << "tocando por " << seconds << " segundos...\n";

    // O bypass do drive e ligado e desligado durante a execucao, para
    // demonstrar que da para mudar a cadeia com o audio rodando. O comando
    // atravessa a fila sem bloquear a thread de audio.
    const auto inicio = std::chrono::steady_clock::now();
    bool driveDesligado = false;

    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - inicio).count() < seconds)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        driveDesligado = !driveDesligado;
        engine.setBypassed(1, driveDesligado);

        std::cout << "  drive " << (driveDesligado ? "em bypass" : "ligado")
                  << "  (" << engine.processedBlocks() << " blocos processados)\n";
    }

    engine.stop();

    std::cout << "\nparado. " << engine.processedBlocks() << " blocos no total.\n";
    return 0;
}

// Abre a pedaleira interativa.
int runInteractive(bool withHardware)
{
    LiveEngine engine;
    buildDefaultChain(engine.chain());

    if (withHardware)
    {
        std::cout << "AVISO: se a entrada e a saida forem os dispositivos embutidos,\n"
                  << "       o som realimenta e vira microfonia. Use fones.\n\n"
                  << "abrindo em 2 segundos...\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    PedalboardUI ui(engine);

    return ui.run(withHardware ? AudioDevice::Mode::Duplex : AudioDevice::Mode::Null,
                  48000.0, 128);
}

// Abre o dispositivo com o backend nulo, sem hardware.
//
// Serve para confirmar que a camada de plataforma funciona mesmo em maquina
// sem placa de som, sem permissao de microfone ou rodando em servidor.
int runDeviceCheck()
{
    LiveEngine engine;
    buildDefaultChain(engine.chain());

    if (!engine.start(AudioDevice::Mode::Null, 48000.0, 128))
    {
        std::cerr << "erro: " << engine.lastError() << "\n";
        return 1;
    }

    std::cout << "backend nulo aberto (sem hardware)\n"
              << "  " << engine.sampleRate() << " Hz, "
              << engine.channelCount() << " canal(is)\n\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    const std::size_t blocos = engine.processedBlocks();
    engine.stop();

    std::cout << blocos << " blocos processados em 0.5 s\n";

    if (blocos == 0)
    {
        std::cerr << "erro: o dispositivo abriu mas nao chamou o callback\n";
        return 1;
    }

    std::cout << "a camada de plataforma esta funcionando.\n";
    return 0;
}

int main(int argc, char** argv)
{
    std::cout << "IbiFX starting...\n\n";

    try
    {
        if (argc == 1)
        {
            runTerminalDemo();
            std::cout << "\n\n";
            printUsage(argv[0]);
            return 0;
        }

        if (argc == 2 && std::string(argv[1]) == "--ui")
        {
            return runInteractive(true);
        }

        if (argc == 2 && std::string(argv[1]) == "--ui-demo")
        {
            return runInteractive(false);
        }

        if (argc >= 2 && std::string(argv[1]) == "--live")
        {
            const double seconds = (argc == 3) ? std::stod(argv[2]) : 10.0;
            return runLive(seconds);
        }

        if (argc == 2 && std::string(argv[1]) == "--devices")
        {
            return runDeviceCheck();
        }

        if (argc == 3 && std::string(argv[1]) == "--generate")
        {
            const WavFile signal = offline::generateTestSignal();
            wav::write(argv[2], signal);

            std::cout << "sinal de teste gravado em " << argv[2] << "\n"
                      << "  " << signal.durationSeconds() << " s, "
                      << signal.sampleRate << " Hz, "
                      << signal.channelCount() << " canal\n";
            return 0;
        }

        if (argc >= 3)
        {
            const ChainSettings settings = parseSettings(argc, argv, 3);

            const WavFile input = wav::read(argv[1]);

            std::cout << "lido " << argv[1] << "\n"
                      << "  " << input.durationSeconds() << " s, "
                      << input.sampleRate << " Hz, "
                      << input.channelCount() << " canal(is), "
                      << input.frameCount() << " frames\n\n";

            ModuleChain chain;
            buildChain(chain, settings);

            std::cout << "cadeia: ";
            for (std::size_t i = 0; i < chain.size(); ++i)
            {
                if (i > 0)
                    std::cout << " -> ";
                std::cout << chain.moduleAt(i).name();
            }
            std::cout << "\n\n";

            printChainSettings(chain);

            const WavFile output = offline::processFile(input, chain);
            wav::write(argv[2], output);

            std::cout << "gravado " << argv[2] << "\n";
            return 0;
        }

        printUsage(argv[0]);
        return 1;
    }
    catch (const std::exception& error)
    {
        // Erro de arquivo é do domínio de controle: aqui exceção é o
        // mecanismo certo, e a mensagem precisa dizer o que houve.
        std::cerr << "erro: " << error.what() << "\n";
        return 1;
    }
}
