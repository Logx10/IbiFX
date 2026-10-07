// Testes do SoftClipper.
//
// A infra de verificação (check, checkClose, reportResults) vive em
// test_helpers.h, compartilhada com os outros testes.
//
// INCLUA O QUE VOCÊ USA
// Este arquivo usa std::cout, std::fabs e std::vector diretamente, então
// inclui <iostream>, <cmath> e <vector> — mesmo que test_helpers.h já traga
// os dois primeiros de carona. Depender de include transitivo é contar com
// sorte: no dia em que o helper parar de precisar de <cmath>, este arquivo
// quebraria sem ter mudado uma linha sequer.

#include <cmath>
#include <iostream>
#include <vector>

#include "SoftClipper.h"
#include "test_helpers.h"

// ---------------------------------------------------------------------

// Silêncio tem que continuar silêncio: tanh(0) == 0.
//
// Parece bobo, mas é a verificação mais importante de qualquer distorcedor.
// Uma curva que devolvesse algo diferente de zero para entrada zero
// produziria um offset DC — uma corrente contínua somada ao sinal, inaudível
// sozinha, mas que come headroom, faz o alto-falante trabalhar deslocado do
// repouso e gera estalos ao ligar e desligar o efeito.
void testSilenceStaysSilent()
{
    std::cout << "silencio continua silencio\n";

    std::vector<float> buffer = {0.0f, 0.0f, 0.0f};

    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    softClipper.setDrive(5.0f);
    softClipper.process(buffer);

    checkClose(buffer[0], 0.0f, "0.0 continua 0.0 com drive 5.0");
    checkClose(buffer[1], 0.0f, "0.0 continua 0.0 (2a amostra)");
    checkClose(buffer[2], 0.0f, "0.0 continua 0.0 (3a amostra)");
}

// Os valores exatos da curva, com drive 1.0.
//
// Estes números vieram de rodar std::tanh, não de memória. Eles travam o
// formato da curva: se alguém trocar a tanh por outra função, mesmo uma de
// aparência parecida, este teste acusa na hora.
void testTanhCurveValues()
{
    std::cout << "valores da curva (drive 1.0)\n";

    std::vector<float> buffer = {0.5f, 1.0f, 2.0f};

    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    softClipper.setDrive(1.0f);
    softClipper.process(buffer);

    checkClose(buffer[0], 0.462117165f, "0.5 vira 0.4621");
    checkClose(buffer[1], 0.761594176f, "1.0 vira 0.7616");
    checkClose(buffer[2], 0.964027584f, "2.0 vira 0.9640");
}

// Sinal fraco atravessa quase intacto — a parte reta da curva.
//
// É daqui que vem a dinâmica de toque: tocando leve, o sinal fica na região
// linear e sai limpo. O erro em 0.01 é de 0.00003 — três centésimos de por
// cento, inaudível.
void testSmallSignalsPassNearlyUnchanged()
{
    std::cout << "sinal fraco quase nao muda (regiao linear)\n";

    std::vector<float> buffer = {0.01f, 0.1f};

    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    softClipper.setDrive(1.0f);
    softClipper.process(buffer);

    checkClose(buffer[0], 0.009999666f, "0.01 vira 0.0099997");
    checkClose(buffer[1], 0.099667996f, "0.1 vira 0.0997");

    check(std::fabs(buffer[0] - 0.01f) < 0.0001f, "erro em 0.01 e menor que 0.0001");
}

// A saída nunca escapa da faixa, por mais absurda que seja a entrada.
//
// ATENÇÃO AO <= : em float, tanh(10.0) já arredonda para exatamente 1.0f.
// A matemática diz que a tanh nunca ALCANÇA 1, só se aproxima; a aritmética
// de 32 bits discorda, porque a diferença (4e-9) some na precisão do tipo.
// Um teste escrito com < falharia — e o erro estaria no teste, não no código.
void testOutputNeverLeavesRange()
{
    std::cout << "saida nunca sai de [-1, +1]\n";

    std::vector<float> buffer = {50.0f, -50.0f, 1000.0f, -1000.0f};

    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    softClipper.setDrive(10.0f);
    softClipper.process(buffer);

    for (float sample : buffer)
    {
        check(sample <= 1.0f && sample >= -1.0f, "amostra extrema ficou dentro da faixa");
    }
}

// tanh é uma função ímpar: f(-x) == -f(x).
//
// Consequência musical: a distorção é SIMÉTRICA, e simetria gera apenas
// harmônicos ímpares. É o caráter de som mais "duro", associado a válvulas
// push-pull e transistores. Distorção assimétrica gera harmônicos pares
// também e soa mais "doce" — é o caráter de válvula single-ended.
//
// Este teste registra a escolha: nosso distorcedor é simétrico. Se um dia
// alguém introduzir assimetria (um bias somado antes da curva, por exemplo),
// este teste falha e a decisão vira conversa em vez de acidente.
void testCurveIsSymmetric()
{
    std::cout << "curva simetrica (funcao impar)\n";

    std::vector<float> buffer = {0.3f, -0.3f, 0.8f, -0.8f};

    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    softClipper.setDrive(3.0f);
    softClipper.process(buffer);

    checkClose(buffer[0], -buffer[1], "f(0.3) == -f(-0.3)");
    checkClose(buffer[2], -buffer[3], "f(0.8) == -f(-0.8)");
}

// Drive maior distorce mais: a mesma amostra sai mais perto do teto.
//
// É a prova de que o parâmetro faz o que promete. Sem este teste, uma
// implementação que ignorasse m_drive passaria em quase todos os outros.
void testHigherDriveSaturatesMore()
{
    std::cout << "drive maior satura mais\n";

    std::vector<float> suave = {0.5f};
    std::vector<float> forte = {0.5f};

    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro

    softClipper.setDrive(1.0f);
    softClipper.process(suave);

    softClipper.setDrive(5.0f);
    softClipper.process(forte);

    checkClose(suave[0], 0.462117165f, "drive 1.0: 0.5 vira 0.4621");
    checkClose(forte[0], 0.986614287f, "drive 5.0: 0.5 vira 0.9866");

    check(forte[0] > suave[0], "drive 5.0 leva a amostra mais perto do teto que drive 1.0");
}

// Drive altíssimo faz o soft clipping virar hard clipping.
//
// Este é o teste mais bonito da bateria, porque revela que os dois módulos
// são pontos de uma mesma linha, e não coisas separadas. Conforme o drive
// cresce, a curva em S vai ficando cada vez mais vertical no meio e cada vez
// mais achatada nas pontas — até virar, no limite, exatamente o degrau do
// hard clipping.
//
// Ou seja: fuzz e overdrive não são efeitos diferentes. São a mesma curva com
// drive diferente.
void testExtremeDriveApproachesHardClipping()
{
    std::cout << "drive extremo vira hard clipping\n";

    std::vector<float> buffer = {0.5f, -0.5f, 0.05f, -0.05f};

    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    softClipper.setDrive(100.0f);
    softClipper.process(buffer);

    checkClose(buffer[0], 1.0f, "0.5 com drive 100 vira 1.0 (degrau)");
    checkClose(buffer[1], -1.0f, "-0.5 com drive 100 vira -1.0 (degrau)");

    // 0.05 é um sinal fraquíssimo — e mesmo ele é empurrado a 0.9999.
    // Não chega a 1.0 dentro da tolerância de 1e-6, e o número exato conta a
    // história melhor do que um arredondamento contaria: a curva ainda é uma
    // curva, só que espremida num intervalo estreitíssimo em torno do zero.
    checkClose(buffer[2], 0.999909222f, "ate 0.05 e empurrado a 0.9999");
    checkClose(buffer[3], -0.999909222f, "ate -0.05 e empurrado a -0.9999");
}

// Drive 0.0 zera tudo, porque tanh(0) == 0 para qualquer entrada.
void testZeroDriveSilences()
{
    std::cout << "drive 0.0 (mute)\n";

    std::vector<float> buffer = {0.5f, -0.9f, 1.0f};

    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    softClipper.setDrive(0.0f);
    softClipper.process(buffer);

    checkClose(buffer[0], 0.0f, "0.5 vira 0.0");
    checkClose(buffer[1], 0.0f, "-0.9 vira 0.0");
    checkClose(buffer[2], 0.0f, "1.0 vira 0.0");
}

// O getter devolve o que o setter guardou.
void testDriveRoundTrip()
{
    std::cout << "setter e getter\n";

    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro

    checkClose(softClipper.drive(), 1.0f, "drive padrao e 1.0");

    softClipper.setDrive(7.5f);
    checkClose(softClipper.drive(), 7.5f, "drive vira 7.5");
}

// Buffer vazio nao pode quebrar.
void testEmptyBufferDoesNotCrash()
{
    std::cout << "buffer vazio\n";

    std::vector<float> buffer;

    SoftClipper softClipper;
    softClipper.setOversampling(false);  // curva pura, sem o atraso do filtro
    softClipper.process(buffer);

    check(buffer.empty(), "buffer vazio continua vazio");
}

int main()
{
    std::cout << "\n=== testes do SoftClipper ===\n\n";

    testSilenceStaysSilent();
    testTanhCurveValues();
    testSmallSignalsPassNearlyUnchanged();
    testOutputNeverLeavesRange();
    testCurveIsSymmetric();
    testHigherDriveSaturatesMore();
    testExtremeDriveApproachesHardClipping();
    testZeroDriveSilences();
    testDriveRoundTrip();
    testEmptyBufferDoesNotCrash();

    return reportResults();
}
