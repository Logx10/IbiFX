#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Clipper.h"
#include "Delay.h"
#include "GainProcessor.h"
#include "ModuleChain.h"
#include "SoftClipper.h"

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

int main()
{
    std::cout << "IbiFX starting...\n\n";

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

    return 0;
}
