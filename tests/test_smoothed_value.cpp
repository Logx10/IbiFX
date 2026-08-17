// Testes do SmoothedValue.
//
// O sample rate usado é 500 Hz porque a rampa padrão de 20 ms dá exatamente
// 10 amostras — número redondo, fácil de conferir passo a passo.
//
// A infra de verificação vive em test_helpers.h.

#include <iostream>

#include "SmoothedValue.h"
#include "test_helpers.h"

namespace
{
// 0.02 s * 500 Hz = 10 amostras de rampa.
constexpr double kTestSampleRate = 500.0;
constexpr int kRampSamples = 10;
}

// ---------------------------------------------------------------------

// Sem prepare(), não há rampa: todo alvo é alcançado de imediato.
//
// É o que mantém o processamento offline previsível e o que permitiu ligar a
// suavização nos módulos sem alterar um único teste existente.
void testWithoutPrepareThereIsNoRamp()
{
    std::cout << "sem prepare nao ha rampa\n";

    SmoothedValue value;
    value.setTarget(5.0f);

    checkClose(value.current(), 5.0f, "o valor saltou direto para 5.0");
    check(!value.isSmoothing(), "nao ha rampa em andamento");
}

// prepare() assume o valor inicial sem rampa.
void testPrepareSnapsToInitialValue()
{
    std::cout << "prepare assume o valor inicial\n";

    SmoothedValue value;
    value.prepare(kTestSampleRate, kDefaultRampSeconds, 2.0f);

    checkClose(value.current(), 2.0f, "comeca em 2.0");
    checkClose(value.target(), 2.0f, "o alvo tambem e 2.0");
    check(!value.isSmoothing(), "nao comeca suavizando");
}

// A rampa caminha em passos iguais até o destino.
//
// O teste central: de 1.0 para 0.0 em 10 amostras dá passos de 0.1.
void testRampWalksInEqualSteps()
{
    std::cout << "a rampa caminha em passos iguais\n";

    SmoothedValue value;
    value.prepare(kTestSampleRate, kDefaultRampSeconds, 1.0f);
    value.setTarget(0.0f);

    checkClose(value.nextValue(), 0.9f, "1o passo: 0.9");
    checkClose(value.nextValue(), 0.8f, "2o passo: 0.8");
    checkClose(value.nextValue(), 0.7f, "3o passo: 0.7");

    for (int i = 0; i < 6; ++i)
    {
        value.nextValue();
    }

    checkClose(value.nextValue(), 0.0f, "10o passo chega exatamente em 0.0");
}

// A rampa termina exatamente no alvo, sem resíduo.
//
// Somar o passo dez vezes acumula erro de arredondamento; o último passo
// atribui o alvo direto para que o valor não fique preso perto dele.
void testRampLandsExactlyOnTarget()
{
    std::cout << "a rampa termina exatamente no alvo\n";

    SmoothedValue value;
    value.prepare(kTestSampleRate, kDefaultRampSeconds, 0.0f);
    value.setTarget(0.3f);

    for (int i = 0; i < kRampSamples; ++i)
    {
        value.nextValue();
    }

    check(value.current() == 0.3f, "o valor e exatamente 0.3, sem residuo");
    check(!value.isSmoothing(), "a rampa terminou");
}

// Depois de chegar, o valor fica parado.
void testValueHoldsAfterRamp()
{
    std::cout << "o valor fica parado apos a rampa\n";

    SmoothedValue value;
    value.prepare(kTestSampleRate, kDefaultRampSeconds, 0.0f);
    value.setTarget(1.0f);

    for (int i = 0; i < kRampSamples + 50; ++i)
    {
        value.nextValue();
    }

    checkClose(value.current(), 1.0f, "continua em 1.0 muito depois do fim");
}

// Pedir o mesmo alvo não reinicia a rampa.
//
// Sem esta guarda, chamar setTarget a cada bloco com o valor inalterado
// congelaria o valor a um passo do destino, para sempre.
void testSameTargetDoesNotRestartRamp()
{
    std::cout << "o mesmo alvo nao reinicia a rampa\n";

    SmoothedValue value;
    value.prepare(kTestSampleRate, kDefaultRampSeconds, 0.0f);
    value.setTarget(1.0f);

    value.nextValue();
    value.nextValue();

    const float meioDoCaminho = value.current();

    value.setTarget(1.0f);

    checkClose(value.current(), meioDoCaminho, "o valor nao voltou atras");

    for (int i = 0; i < kRampSamples; ++i)
    {
        value.nextValue();
    }

    checkClose(value.current(), 1.0f, "a rampa original terminou normalmente");
}

// Mudar de alvo no meio da rampa parte de onde o valor está.
//
// É o que evita um degrau quando o operador gira o knob de volta antes de a
// rampa anterior terminar.
void testChangingTargetMidRampStartsFromCurrent()
{
    std::cout << "trocar de alvo no meio da rampa\n";

    SmoothedValue value;
    value.prepare(kTestSampleRate, kDefaultRampSeconds, 0.0f);
    value.setTarget(1.0f);

    value.nextValue();
    value.nextValue();

    const float antes = value.current();
    checkClose(antes, 0.2f, "o valor estava em 0.2");

    value.setTarget(0.0f);

    checkClose(value.current(), 0.2f, "trocar o alvo nao mexeu no valor atual");

    const float primeiroPasso = value.nextValue();
    check(primeiroPasso < antes, "o valor passou a descer");
    check(primeiroPasso > 0.0f, "sem saltar direto para o novo alvo");
}

// snapTo descarta a rampa e vai direto.
void testSnapToSkipsTheRamp()
{
    std::cout << "snapTo pula a rampa\n";

    SmoothedValue value;
    value.prepare(kTestSampleRate, kDefaultRampSeconds, 0.0f);
    value.setTarget(1.0f);

    value.nextValue();
    value.snapTo(0.5f);

    checkClose(value.current(), 0.5f, "o valor foi direto para 0.5");
    checkClose(value.target(), 0.5f, "o alvo acompanhou");
    check(!value.isSmoothing(), "nao restou rampa em andamento");
}

// isSmoothing informa se a rampa está em andamento.
void testIsSmoothingReportsRampState()
{
    std::cout << "isSmoothing acompanha o estado\n";

    SmoothedValue value;
    value.prepare(kTestSampleRate, kDefaultRampSeconds, 0.0f);

    check(!value.isSmoothing(), "parado antes de mudar o alvo");

    value.setTarget(1.0f);
    check(value.isSmoothing(), "suavizando logo apos setTarget");

    for (int i = 0; i < kRampSamples; ++i)
    {
        value.nextValue();
    }

    check(!value.isSmoothing(), "parado depois de completar a rampa");
}

// Sample rate baixo demais faz a rampa arredondar para zero.
//
// A 10 Hz, uma rampa de 20 ms daria 0.2 amostra — menos que uma. Não existe
// meio passo, então o valor salta. Não é defeito: é o limite da resolução
// temporal daquele sample rate.
void testVeryLowSampleRateDisablesRamp()
{
    std::cout << "sample rate baixo demais desliga a rampa\n";

    SmoothedValue value;
    value.prepare(10.0, kDefaultRampSeconds, 0.0f);
    value.setTarget(1.0f);

    checkClose(value.current(), 1.0f, "o valor saltou, sem rampa possivel");
}

// Sample rate inválido não quebra.
void testInvalidSampleRateIsHarmless()
{
    std::cout << "sample rate invalido\n";

    SmoothedValue value;
    value.prepare(0.0, kDefaultRampSeconds, 1.0f);
    value.setTarget(2.0f);

    checkClose(value.current(), 2.0f, "sem rampa, o valor salta");
}

// A rampa funciona nos dois sentidos.
void testRampWorksDownwardAndUpward()
{
    std::cout << "a rampa sobe e desce\n";

    SmoothedValue value;
    value.prepare(kTestSampleRate, kDefaultRampSeconds, 0.0f);

    value.setTarget(1.0f);
    check(value.nextValue() > 0.0f, "subindo, o valor cresce");

    value.snapTo(1.0f);
    value.setTarget(0.0f);
    check(value.nextValue() < 1.0f, "descendo, o valor diminui");
}

int main()
{
    std::cout << "\n=== testes do SmoothedValue ===\n\n";

    testWithoutPrepareThereIsNoRamp();
    testPrepareSnapsToInitialValue();
    testRampWalksInEqualSteps();
    testRampLandsExactlyOnTarget();
    testValueHoldsAfterRamp();
    testSameTargetDoesNotRestartRamp();
    testChangingTargetMidRampStartsFromCurrent();
    testSnapToSkipsTheRamp();
    testIsSmoothingReportsRampState();
    testVeryLowSampleRateDisablesRamp();
    testInvalidSampleRateIsHarmless();
    testRampWorksDownwardAndUpward();

    return reportResults();
}
