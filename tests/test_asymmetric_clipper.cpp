// Testes do AsymmetricClipper.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.

#include <cmath>
#include <iostream>
#include <vector>

#include "AsymmetricClipper.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// Silêncio continua silêncio, mesmo com bias diferente de zero — é a razão
// de existir o termo subtraído na fórmula (ver o comentário no .h). Sem
// ele, x=0 produziria um offset DC toda vez que bias != 0.
void testSilenceStaysSilentEvenWithBias()
{
    std::cout << "silencio continua silencio, mesmo com bias\n";

    std::vector<float> buffer = {0.0f, 0.0f, 0.0f};

    AsymmetricClipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipper.setDrive(5.0f);
    clipper.setBias(0.7f);
    clipper.process(buffer);

    checkClose(buffer[0], 0.0f, "0.0 continua 0.0 com bias 0.7");
    checkClose(buffer[1], 0.0f, "0.0 continua 0.0 (2a amostra)");
    checkClose(buffer[2], 0.0f, "0.0 continua 0.0 (3a amostra)");
}

// Com bias 0.0, a fórmula colapsa exatamente na do SoftClipper: tanh(drive
// * x). Os dois módulos são a mesma curva, bias é só o grau de liberdade
// extra.
void testZeroBiasMatchesPlainTanh()
{
    std::cout << "bias 0.0 colapsa na curva do SoftClipper\n";

    std::vector<float> buffer = {0.5f, 1.0f, -0.7f};

    AsymmetricClipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipper.setDrive(2.0f);
    clipper.setBias(0.0f);
    clipper.process(buffer);

    checkClose(buffer[0], std::tanh(2.0f * 0.5f), "0.5 bate com tanh(drive*x) puro");
    checkClose(buffer[1], std::tanh(2.0f * 1.0f), "1.0 bate com tanh(drive*x) puro");
    checkClose(buffer[2], std::tanh(2.0f * -0.7f), "-0.7 bate com tanh(drive*x) puro");
}

// O valor exato da curva, com drive 1.0 e bias 0.3 — calculado com a mesma
// fórmula do código (tanh deslocada, menos o centro, dividida por 1+|centro|)
// em vez de decorado, porque a divisão de normalização torna o número final
// pouco memorável à mão.
void testCurveValuesWithBias()
{
    std::cout << "valores da curva (drive 1.0, bias 0.3)\n";

    const float drive = 1.0f;
    const float bias = 0.3f;
    const float centro = std::tanh(drive * bias);

    auto esperado = [&](float x)
    {
        return (std::tanh(drive * (x + bias)) - centro) / (1.0f + std::fabs(centro));
    };

    std::vector<float> buffer = {0.5f, -0.5f};

    AsymmetricClipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipper.setDrive(drive);
    clipper.setBias(bias);
    clipper.process(buffer);

    checkClose(buffer[0], esperado(0.5f), "0.5 bate com a formula calculada separadamente");
    checkClose(buffer[1], esperado(-0.5f), "-0.5 bate com a formula calculada separadamente");
}

// A prova da assimetria: com bias != 0, f(x) != -f(-x). Se fosse igual, a
// curva continuaria sendo uma função ímpar, e o módulo não faria nada de
// diferente do SoftClipper.
void testCurveIsAsymmetricWithNonZeroBias()
{
    std::cout << "curva e assimetrica com bias diferente de zero\n";

    std::vector<float> positivo = {0.5f};
    std::vector<float> negativo = {-0.5f};

    AsymmetricClipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipper.setDrive(1.0f);
    clipper.setBias(0.3f);

    clipper.process(positivo);
    clipper.process(negativo);

    check(std::fabs(positivo[0] - (-negativo[0])) > 0.05f,
          "f(0.5) e -f(-0.5) diferem bastante — a curva nao e mais impar");
}

// A saída nunca escapa de [-1, +1], por mais absurda que seja a entrada —
// mesma garantia matemática da tanh que o SoftClipper tem.
void testOutputNeverLeavesRange()
{
    std::cout << "saida nunca sai de [-1, +1]\n";

    std::vector<float> buffer = {50.0f, -50.0f, 1000.0f, -1000.0f};

    AsymmetricClipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipper.setDrive(10.0f);
    clipper.setBias(0.5f);
    clipper.process(buffer);

    for (float sample : buffer)
    {
        check(sample <= 1.0f && sample >= -1.0f, "amostra extrema ficou dentro da faixa");
    }
}

// Drive maior distorce mais — testado com bias 0.0 de propósito, isolando
// só o parâmetro drive.
//
// COM BIAS != 0, ISSO NÃO É SEMPRE VERDADE, E NÃO É BUG
// No lado do sinal que tem o MESMO sinal do bias, aumentar o drive pode
// ENCOLHER a diferença entre a curva e o centro subtraído — os dois
// convergem pra mesma assíntota (ver o comentário "UMA PEGADINHA DO PRÓPRIO
// DESENHO" no .h). Testar essa direção aqui daria um resultado que parece
// errado mas não é; bias 0.0 evita a confusão e testa exatamente o que este
// teste se propõe a testar.
void testHigherDriveSaturatesMore()
{
    std::cout << "drive maior satura mais (bias 0.0, isolando o parametro)\n";

    std::vector<float> suave = {0.5f};
    std::vector<float> forte = {0.5f};

    AsymmetricClipper clipperSuave;
    clipperSuave.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipperSuave.setDrive(1.0f);
    clipperSuave.setBias(0.0f);  // o construtor parte de 0.3, precisa zerar
    clipperSuave.process(suave);

    AsymmetricClipper clipperForte;
    clipperForte.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipperForte.setDrive(5.0f);
    clipperForte.setBias(0.0f);
    clipperForte.process(forte);

    check(forte[0] > suave[0], "drive 5.0 leva a amostra mais perto do teto que drive 1.0");
}

// O lado OPOSTO ao bias, ao contrário, sempre satura mais com mais drive —
// é o outro lado da mesma moeda da pegadinha documentada no .h.
void testHigherDriveSaturatesMoreOnOppositeSideOfBias()
{
    std::cout << "no lado oposto ao bias, drive maior tambem satura mais\n";

    std::vector<float> suave = {-0.5f};
    std::vector<float> forte = {-0.5f};

    AsymmetricClipper clipperSuave;
    clipperSuave.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipperSuave.setDrive(1.0f);
    clipperSuave.setBias(0.2f);  // bias positivo, amostra negativa: lado oposto
    clipperSuave.process(suave);

    AsymmetricClipper clipperForte;
    clipperForte.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipperForte.setDrive(5.0f);
    clipperForte.setBias(0.2f);
    clipperForte.process(forte);

    check(std::fabs(forte[0]) > std::fabs(suave[0]),
          "drive 5.0 satura mais que drive 1.0, no lado oposto ao bias");
}

// Os getters devolvem o que os setters guardaram.
void testParameterRoundTrip()
{
    std::cout << "setters e getters\n";

    AsymmetricClipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro

    checkClose(clipper.drive(), 1.0f, "drive padrao e 1.0");
    checkClose(clipper.bias(), 0.3f, "bias padrao e 0.3");

    clipper.setDrive(7.5f);
    clipper.setBias(-0.6f);

    checkClose(clipper.drive(), 7.5f, "drive vira 7.5");
    checkClose(clipper.bias(), -0.6f, "bias vira -0.6");
}

// Buffer vazio não pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    AsymmetricClipper clipper;
    clipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    clipper.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

int main()
{
    std::cout << "\n=== testes do AsymmetricClipper ===\n\n";

    testSilenceStaysSilentEvenWithBias();
    testZeroBiasMatchesPlainTanh();
    testCurveValuesWithBias();
    testCurveIsAsymmetricWithNonZeroBias();
    testOutputNeverLeavesRange();
    testHigherDriveSaturatesMore();
    testHigherDriveSaturatesMoreOnOppositeSideOfBias();
    testParameterRoundTrip();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
