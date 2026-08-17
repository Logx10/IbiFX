#pragma once

#include <cmath>
#include <iostream>
#include <string>

// Infra mínima de testes, compartilhada pelos executáveis de teste.
//
// POR QUE ESTE ARQUIVO EXISTE AGORA, E NÃO ANTES
// Ele nasceu do terceiro teste, não do primeiro. Pela regra do §55 do
// AI_GUIDELINES — primeiro problema: resolver; segundo parecido: observar;
// terceiro: considerar abstração —, esperar até aqui foi deliberado.
//
// O ganho de esperar não é filosófico, é prático: com três cópias na mão dava
// para VER o que era realmente comum (check, checkClose, o relatório final) e
// o que era específico de cada módulo. Extraído no primeiro teste, este
// arquivo teria sido um chute sobre o futuro — e chutes viram abstrações
// tortas que depois ninguém quer mexer.
//
// POR QUE NÃO UMA BIBLIOTECA DE TESTES (Catch2, GoogleTest)
// Continua valendo o que estava escrito no primeiro teste: nesta fase o
// mecanismo é mais valioso que a conveniência. Um teste é "rode, compare,
// devolva erro se divergir", e isso cabe nas poucas linhas abaixo — sem
// dependência externa, sem macro mágica, sem nada acontecendo escondido.
// Quando o número de testes tornar a chamada manual no main() incômoda, aí
// sim vale a conversa.

// Quantas verificações falharam até agora.
//
// POR QUE inline
// Sem o inline, esta linha seria uma DEFINIÇÃO de variável, e todo .cpp que
// incluísse este header criaria a sua própria. Se dois arquivos do mesmo
// executável fizessem isso, o linker acharia o símbolo duas vezes e recusaria
// — é a One Definition Rule (ODR), a mesma regra que te mordeu quando os dois
// testes tentaram virar um executável só.
//
// O inline diz ao linker: "várias unidades podem definir isto; são todas a
// mesma coisa, fique com uma". Hoje cada executável de teste tem um .cpp só e
// funcionaria sem ele — mas o dia em que um teste for dividido em dois
// arquivos não deveria ser o dia de descobrir isso.
//
// Uma variável global costuma ser má ideia. Aqui o programa é pequeno, tem
// uma thread só e uma responsabilidade só; carregar um objeto de contexto por
// todas as funções não se pagaria.
inline int failures = 0;

// Tolerância padrão para comparação de floats.
//
// 1e-6 é folgado para float (que tem ~7 dígitos decimais de precisão) e
// apertado o bastante para pegar erro de verdade.
inline constexpr float kFloatTolerance = 1e-6f;

// Verifica uma condição booleana.
//
// inline pelo mesmo motivo da variável: funções definidas em header precisam
// dele para não violarem a ODR quando incluídas em mais de um .cpp.
inline void check(bool condition, const std::string& description)
{
    if (condition)
    {
        std::cout << "  ok      " << description << "\n";
    }
    else
    {
        std::cout << "  FALHOU  " << description << "\n";
        ++failures;
    }
}

// Verifica se dois floats são "iguais o suficiente".
//
// POR QUE NÃO USAR == COM FLOAT
// 0.25 * 0.5 dá exatamente 0.125, e nesse caso o == funcionaria. Mas isso é
// sorte: 0.125 é potência de dois e cai redondo em binário. Já 0.1 não é
// representável exatamente, e uma conta como 0.1 * 3 produz 0.30000001... O
// == falharia, mesmo o resultado estando certo para qualquer efeito prático.
//
// Por isso comparamos com TOLERÂNCIA: "a diferença é menor que um fio de
// cabelo?".
inline void checkClose(float actual, float expected, const std::string& description)
{
    if (std::fabs(actual - expected) <= kFloatTolerance)
    {
        std::cout << "  ok      " << description << "\n";
    }
    else
    {
        std::cout << "  FALHOU  " << description
                  << "  (esperado " << expected
                  << ", obtido " << actual << ")\n";
        ++failures;
    }
}

// Imprime o resultado final e devolve o código de saída do processo.
//
// Uso no fim de cada main() de teste:
//
//     return reportResults();
//
// COMO O CTEST SABE SE PASSOU
// Ele não lê a saída do programa, olha o CÓDIGO DE SAÍDA: 0 passou, qualquer
// outro valor falhou. É a mesma convenção que o shell usa desde sempre.
inline int reportResults()
{
    std::cout << "\n";

    if (failures == 0)
    {
        std::cout << "todos os testes passaram\n\n";
        return 0;
    }

    std::cout << failures << " verificacao(oes) falharam\n\n";
    return 1;
}
