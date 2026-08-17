// Testes do Parameter.
//
// Cobrem identidade, faixa, clamp, normalização e reset.
//
// A infra de verificação vive em test_helpers.h.

#include <iostream>
#include <stdexcept>
#include <string>

#include "Parameter.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// O parâmetro guarda o que recebeu e nasce no valor padrão.
void testStoresIdentityAndRange()
{
    std::cout << "identidade e faixa\n";

    Parameter drive("drive", "Drive", 0.0f, 100.0f, 1.0f);

    check(drive.id() == "drive", "id e drive");
    check(drive.label() == "Drive", "label e Drive");
    checkClose(drive.minValue(), 0.0f, "minimo e 0.0");
    checkClose(drive.maxValue(), 100.0f, "maximo e 100.0");
    checkClose(drive.defaultValue(), 1.0f, "padrao e 1.0");
    checkClose(drive.value(), 1.0f, "nasce valendo o padrao");
}

// Valor acima do máximo para no máximo.
//
// É o comportamento de um knob físico, que não gira além do batente. Não é
// erro de programação, então não lança — apenas para na borda.
void testClampsAboveMaximum()
{
    std::cout << "clamp acima do maximo\n";

    Parameter threshold("threshold", "Threshold", 0.0f, 2.0f, 1.0f);

    threshold.setValue(50.0f);

    checkClose(threshold.value(), 2.0f, "50.0 vira 2.0");
}

// Valor abaixo do mínimo para no mínimo.
//
// É o que torna impossível um Clipper com teto negativo: a faixa começa em
// 0, então o problema deixa de existir na origem em vez de ser documentado.
void testClampsBelowMinimum()
{
    std::cout << "clamp abaixo do minimo\n";

    Parameter threshold("threshold", "Threshold", 0.0f, 2.0f, 1.0f);

    threshold.setValue(-5.0f);

    checkClose(threshold.value(), 0.0f, "-5.0 vira 0.0");
}

// Valor dentro da faixa passa intacto.
void testAcceptsValueInsideRange()
{
    std::cout << "valor dentro da faixa\n";

    Parameter gain("gain", "Gain", -8.0f, 8.0f, 1.0f);

    gain.setValue(2.5f);
    checkClose(gain.value(), 2.5f, "2.5 e aceito");

    gain.setValue(-3.0f);
    checkClose(gain.value(), -3.0f, "-3.0 e aceito (faixa inclui negativos)");
}

// As bordas são valores válidos, não excluídos.
void testBoundsAreValid()
{
    std::cout << "as bordas sao validas\n";

    Parameter drive("drive", "Drive", 0.0f, 100.0f, 1.0f);

    drive.setValue(0.0f);
    checkClose(drive.value(), 0.0f, "o minimo e aceito");

    drive.setValue(100.0f);
    checkClose(drive.value(), 100.0f, "o maximo e aceito");
}

// A forma normalizada expressa a posição dentro da faixa, de 0 a 1.
void testNormalizedReading()
{
    std::cout << "leitura normalizada\n";

    Parameter drive("drive", "Drive", 0.0f, 100.0f, 1.0f);

    drive.setValue(0.0f);
    checkClose(drive.normalized(), 0.0f, "no minimo, normalizado e 0.0");

    drive.setValue(100.0f);
    checkClose(drive.normalized(), 1.0f, "no maximo, normalizado e 1.0");

    drive.setValue(25.0f);
    checkClose(drive.normalized(), 0.25f, "em 25 de 100, normalizado e 0.25");
}

// Faixa com negativos também normaliza corretamente.
//
// O ganho vai de -8 a 8, então o zero fica no meio: 0.5 normalizado.
void testNormalizedWithNegativeRange()
{
    std::cout << "normalizacao com faixa negativa\n";

    Parameter gain("gain", "Gain", -8.0f, 8.0f, 1.0f);

    gain.setValue(-8.0f);
    checkClose(gain.normalized(), 0.0f, "-8.0 normalizado e 0.0");

    gain.setValue(0.0f);
    checkClose(gain.normalized(), 0.5f, "0.0 normalizado e 0.5 (meio da faixa)");

    gain.setValue(8.0f);
    checkClose(gain.normalized(), 1.0f, "8.0 normalizado e 1.0");
}

// Escrever em forma normalizada converte para a faixa real.
//
// É como um knob de tela ou um MIDI CC vão falar com o parâmetro: sempre em
// 0..1, sem precisar saber o que significa "8" para o ganho.
void testNormalizedWriting()
{
    std::cout << "escrita normalizada\n";

    Parameter gain("gain", "Gain", -8.0f, 8.0f, 1.0f);

    gain.setNormalized(0.0f);
    checkClose(gain.value(), -8.0f, "0.0 normalizado vira -8.0");

    gain.setNormalized(0.5f);
    checkClose(gain.value(), 0.0f, "0.5 normalizado vira 0.0");

    gain.setNormalized(1.0f);
    checkClose(gain.value(), 8.0f, "1.0 normalizado vira 8.0");
}

// Normalizado fora de 0..1 também sofre clamp.
void testNormalizedIsClamped()
{
    std::cout << "normalizado fora de 0..1\n";

    Parameter drive("drive", "Drive", 0.0f, 100.0f, 1.0f);

    drive.setNormalized(5.0f);
    checkClose(drive.value(), 100.0f, "5.0 normalizado para no maximo");

    drive.setNormalized(-2.0f);
    checkClose(drive.value(), 0.0f, "-2.0 normalizado para no minimo");
}

// Ler e escrever normalizado devem ser operações inversas.
void testNormalizedRoundTrip()
{
    std::cout << "ida e volta normalizada\n";

    Parameter gain("gain", "Gain", -8.0f, 8.0f, 1.0f);

    gain.setValue(3.5f);
    const float saved = gain.normalized();

    gain.setValue(-7.0f);
    gain.setNormalized(saved);

    checkClose(gain.value(), 3.5f, "3.5 sobrevive a ida e volta");
}

// reset() devolve o parâmetro ao valor padrão.
void testReset()
{
    std::cout << "reset volta ao padrao\n";

    Parameter drive("drive", "Drive", 0.0f, 100.0f, 1.0f);

    drive.setValue(42.0f);
    drive.reset();

    checkClose(drive.value(), 1.0f, "voltou para 1.0");
}

// Padrão fora da faixa é ajustado, não aceito às cegas.
void testDefaultIsClampedToRange()
{
    std::cout << "padrao fora da faixa\n";

    Parameter odd("odd", "Odd", 0.0f, 1.0f, 5.0f);

    checkClose(odd.defaultValue(), 1.0f, "padrao 5.0 virou 1.0");
    checkClose(odd.value(), 1.0f, "o valor inicial acompanha");
}

// Faixa de largura zero não quebra a normalização.
void testZeroWidthRange()
{
    std::cout << "faixa de largura zero\n";

    Parameter fixed("fixed", "Fixed", 1.0f, 1.0f, 1.0f);

    checkClose(fixed.value(), 1.0f, "o valor e o unico possivel");
    checkClose(fixed.normalized(), 0.0f, "normalizado devolve 0.0 sem dividir por zero");
}

// Faixa invertida é erro de programação e falha alto.
void testInvertedRangeThrows()
{
    std::cout << "faixa invertida lanca\n";

    bool lancou = false;
    try
    {
        Parameter bad("bad", "Bad", 10.0f, 1.0f, 5.0f);
    }
    catch (const std::invalid_argument&)
    {
        lancou = true;
    }

    check(lancou, "minimo maior que maximo lanca invalid_argument");
}

// Um parâmetro sem id não serviria para preset nem para mapeamento.
void testEmptyIdThrows()
{
    std::cout << "id vazio lanca\n";

    bool lancou = false;
    try
    {
        Parameter bad("", "Sem id", 0.0f, 1.0f, 0.5f);
    }
    catch (const std::invalid_argument&)
    {
        lancou = true;
    }

    check(lancou, "id vazio lanca invalid_argument");
}

int main()
{
    std::cout << "\n=== testes do Parameter ===\n\n";

    testStoresIdentityAndRange();
    testClampsAboveMaximum();
    testClampsBelowMinimum();
    testAcceptsValueInsideRange();
    testBoundsAreValid();
    testNormalizedReading();
    testNormalizedWithNegativeRange();
    testNormalizedWriting();
    testNormalizedIsClamped();
    testNormalizedRoundTrip();
    testReset();
    testDefaultIsClampedToRange();
    testZeroWidthRange();
    testInvertedRangeThrows();
    testEmptyIdThrows();

    return reportResults();
}
