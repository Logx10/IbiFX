// Testes dos caminhos paralelos do ModuleChain (Split / Branch / Merge).
//
// Cada caminho é só um GainProcessor, para a saída esperada ser conta de
// cabeça: entrada 0.1 por um caminho x2 e outro x3 tem que dar 0.5.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "GainProcessor.h"
#include "ModuleChain.h"
#include "ParallelMarker.h"
#include "Preset.h"
#include "test_helpers.h"

namespace
{
std::unique_ptr<GainProcessor> gain(float value)
{
    auto module = std::make_unique<GainProcessor>();
    module->setGain(value);
    return module;
}

std::unique_ptr<ParallelMarker> marker(ParallelMarker::Kind kind)
{
    return std::make_unique<ParallelMarker>(kind);
}

// Passa um bloco constante de 0.1 pela cadeia e devolve a última amostra
// (depois de a suavização de volume dos marcadores assentar).
float run(ModuleChain& chain)
{
    chain.prepare(1000.0, 64);
    std::vector<float> buffer(64, 0.1f);
    chain.process(buffer);
    return buffer.back();
}
}

// Dois caminhos somados: 0.1·2 + 0.1·3 = 0.5.
void testTwoLanesAreSummed()
{
    std::cout << "dois caminhos sao somados\n";

    ModuleChain chain;
    chain.add(marker(ParallelMarker::Kind::Split));
    chain.add(gain(2.0f));
    chain.add(marker(ParallelMarker::Kind::Branch));
    chain.add(gain(3.0f));
    chain.add(marker(ParallelMarker::Kind::Merge));

    checkClose(run(chain), 0.5f, "0.1*2 + 0.1*3 = 0.5");
}

// Cada caminho recebe o sinal ORIGINAL, não a saída do anterior: com três
// caminhos x2, a soma é 0.6, e não 0.1·2·2·2.
void testEveryLaneStartsFromTheSplitInput()
{
    std::cout << "cada caminho comeca do sinal do Split\n";

    ModuleChain chain;
    chain.add(marker(ParallelMarker::Kind::Split));
    chain.add(gain(2.0f));
    chain.add(marker(ParallelMarker::Kind::Branch));
    chain.add(gain(2.0f));
    chain.add(marker(ParallelMarker::Kind::Branch));
    chain.add(gain(2.0f));
    chain.add(marker(ParallelMarker::Kind::Merge));

    checkClose(run(chain), 0.6f, "3 caminhos x2 = 0.6");
}

// O que vem antes do Split alimenta todos os caminhos, e o que vem depois
// do Merge recebe a soma.
void testModulesAroundTheSectionSeeInputAndSum()
{
    std::cout << "antes do Split alimenta todos, depois do Merge recebe a soma\n";

    ModuleChain chain;
    chain.add(gain(2.0f));
    chain.add(marker(ParallelMarker::Kind::Split));
    chain.add(gain(1.0f));
    chain.add(marker(ParallelMarker::Kind::Branch));
    chain.add(gain(1.0f));
    chain.add(marker(ParallelMarker::Kind::Merge));
    chain.add(gain(0.5f));

    checkClose(run(chain), 0.2f, "(0.1*2)*(1+1)*0.5 = 0.2");
}

// Level e fase vêm do marcador que ABRE o caminho. Dois caminhos iguais,
// um com a fase invertida, se cancelam — o problema real que o botão de
// fase resolve.
void testLevelAndInvertBelongToTheOpeningMarker()
{
    std::cout << "level e fase do marcador que abre o caminho\n";

    {
        ModuleChain chain;
        auto split = marker(ParallelMarker::Kind::Split);
        split->setLevel(0.5f);
        chain.add(std::move(split));
        chain.add(gain(2.0f));
        chain.add(marker(ParallelMarker::Kind::Merge));

        checkClose(run(chain), 0.1f, "level 0.5 num caminho x2: 0.1");
    }
    {
        ModuleChain chain;
        chain.add(marker(ParallelMarker::Kind::Split));
        chain.add(gain(1.0f));
        auto branch = marker(ParallelMarker::Kind::Branch);
        branch->setInvert(true);
        chain.add(std::move(branch));
        chain.add(gain(1.0f));
        chain.add(marker(ParallelMarker::Kind::Merge));

        checkClose(run(chain), 0.0f, "caminho com fase invertida cancela o igual");
    }
}

// Bypass no marcador silencia o caminho inteiro.
void testBypassedMarkerMutesItsLane()
{
    std::cout << "bypass no marcador silencia o caminho\n";

    ModuleChain chain;
    chain.add(marker(ParallelMarker::Kind::Split));
    chain.add(gain(2.0f));
    chain.add(marker(ParallelMarker::Kind::Branch));
    chain.add(gain(3.0f));
    chain.add(marker(ParallelMarker::Kind::Merge));
    chain.setBypassed(2, true);

    checkClose(run(chain), 0.2f, "so o caminho A (x2) sobra");
}

// Estrutura malformada não quebra: Merge e Branch sem Split passam o sinal
// direto; Split sem Merge fecha no fim da fila.
void testMalformedStructureIsHarmless()
{
    std::cout << "estrutura malformada nao quebra o audio\n";

    {
        ModuleChain chain;
        chain.add(marker(ParallelMarker::Kind::Merge));
        chain.add(marker(ParallelMarker::Kind::Branch));
        chain.add(gain(2.0f));

        checkClose(run(chain), 0.2f, "Merge e Branch soltos sao ignorados");
    }
    {
        ModuleChain chain;
        chain.add(marker(ParallelMarker::Kind::Split));
        chain.add(gain(2.0f));
        chain.add(marker(ParallelMarker::Kind::Branch));
        chain.add(gain(3.0f));

        checkClose(run(chain), 0.5f, "Split sem Merge fecha no fim");
    }
}

// Os marcadores salvam e voltam por preset como qualquer módulo.
void testMarkersRoundTripThroughPresets()
{
    std::cout << "marcadores voltam por preset\n";

    ModuleChain original;
    original.add(marker(ParallelMarker::Kind::Split));
    original.add(gain(2.0f));
    auto branch = marker(ParallelMarker::Kind::Branch);
    branch->setLevel(1.5f);
    original.add(std::move(branch));
    original.add(gain(3.0f));
    original.add(marker(ParallelMarker::Kind::Merge));

    ModuleChain restored;
    preset::apply(preset::capture("teste", original), restored);

    check(restored.size() == 5, "cinco modulos");
    check(restored.moduleAt(2).name() == std::string("Branch"), "Branch restaurado");
    checkClose(run(restored), 0.1f * 2.0f + 0.1f * 3.0f * 1.5f, "mesma soma: 0.65");
}

int main()
{
    std::cout << "\n=== testes dos caminhos paralelos ===\n\n";

    testTwoLanesAreSummed();
    testEveryLaneStartsFromTheSplitInput();
    testModulesAroundTheSectionSeeInputAndSum();
    testLevelAndInvertBelongToTheOpeningMarker();
    testBypassedMarkerMutesItsLane();
    testMalformedStructureIsHarmless();
    testMarkersRoundTripThroughPresets();

    return reportResults();
}
