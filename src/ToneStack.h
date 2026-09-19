#pragma once

#include <vector>

#include "AudioModule.h"

// ToneStack — o circuito passivo bass/mid/treble de um amplificador real,
// não um EQ de 3 bandas.
//
// FONTE
// Yeh, D. T. e Smith, J. O., "Discretization of the '59 Fender Bassman Tone
// Stack", DAFx-06 (Stanford CCRMA). Analisaram o circuito simbolicamente
// (nodal analysis) com Mathematica, verificaram contra simulação SPICE, e
// publicaram a função de transferência em forma fechada. Este módulo
// implementa exatamente essa função — não é uma aproximação nossa.
//
// POR QUE NÃO É SÓ TRÊS FILTROS (E O QUE SE PERDE FAZENDO ASSIM)
// Um EQ de 3 bandas comum usa três filtros SEPARADOS: um pra grave, um pra
// médio, um pra agudo, cada um surdo aos outros dois. O tone stack de um
// amplificador de guitarra é uma coisa fisicamente diferente: os três
// potenciômetros (bass, mid, treble) são nós do MESMO circuito RC passivo,
// então mexer em um necessariamente afeta a resposta dos outros — os
// controles "não são ortogonais", como o artigo descreve. É essa interação
// — característica do som de amplificador valvulado — que um EQ comum não
// reproduz e este módulo reproduz, porque resolve o circuito de verdade em
// vez de simular o efeito sonoro dele.
//
// O CIRCUITO E POR QUE É DE 3ª ORDEM
// O circuito (Fig. 1 do artigo) tem 3 capacitores e uma rede de resistores
// fixos e potenciômetros. Cada capacitor com os resistores ao redor forma
// uma constante de tempo RC — um polo. 3 capacitores independentes, 3 polos:
// por isso a função de transferência tem grau 3 no numerador e no
// denominador, e por isso o filtro digital resultante usa 3 amostras de
// histórico de entrada e 3 de saída, não 1 como o HighPassFilter.
//
// UM DETALHE QUE SÓ APARECE PORQUE O CIRCUITO FOI RESOLVIDO DE VERDADE: O
// TREBLE NÃO CONTROLA OS POLOS
// O artigo mostra algo que não é óbvio de adivinhar: nenhum coeficiente do
// DENOMINADOR depende do controle de treble — só bass e mid controlam onde
// ficam os polos (as ressonâncias/decaimentos do sistema). Treble só move
// os ZEROS (os pontos de cancelamento). Na prática: bass e mid mudam o
// "corpo" da resposta, treble só pesa o quanto do agudo passa por cima
// desse corpo. Um EQ de 3 filtros separados não tem como reproduzir essa
// assimetria entre os três controles, porque trata os três da mesma forma.
//
// A FUNÇÃO DE TRANSFERÊNCIA (§2.1 do artigo)
//
//     H(s) = (b1 s + b2 s² + b3 s³) / (a0 + a1 s + a2 s² + a3 s³),  a0 = 1
//
// b1, b2, b3, a1, a2, a3 são funções de R1..R4, C1..C3 (os componentes do
// circuito) e de t, m, l — as posições de treble, mid e bass, cada uma de
// 0 a 1. computeCoefficients() no .cpp reproduz essas fórmulas linha por
// linha, comentadas com a equação correspondente do artigo.
//
// DISCRETIZAÇÃO: TRANSFORMADA BILINEAR
// H(s) descreve o circuito analógico, contínuo no tempo. Pra rodar em
// amostras discretas, o artigo substitui s = c(1 - z⁻¹)/(1 + z⁻¹), com
// c = 2/T (T = 1/sampleRate) — a transformada bilinear, o mesmo mapeamento
// padrão usado para digitalizar qualquer filtro analógico. O resultado é
// H(z) = (B0 + B1 z⁻¹ + B2 z⁻² + B3 z⁻³) / (A0 + A1 z⁻¹ + A2 z⁻² + A3 z⁻³),
// com B0..B3 e A0..A3 dados em função de b1..b3, a1..a3 e c (§2.3).
//
// A EQUAÇÃO DE DIFERENÇAS
// A0 y[n] + A1 y[n-1] + A2 y[n-2] + A3 y[n-3]
//     = B0 x[n] + B1 x[n-1] + B2 x[n-2] + B3 x[n-3]
//
//     y[n] = (B0 x[n] + B1 x[n-1] + B2 x[n-2] + B3 x[n-3]
//             - A1 y[n-1] - A2 y[n-2] - A3 y[n-3]) / A0
//
// É por isso que o módulo guarda 3 amostras de entrada e 3 de saída — a
// mesma ideia do buffer circular do Delay, só que aqui o "atraso" é de
// poucas amostras, não segundos, e existe pra fazer aritmética de filtro,
// não pra produzir eco audível.
//
// POR QUE OS COEFICIENTES SÃO double, NÃO float, COMO O RESTO DO PROJETO
// Os valores dos componentes vão de 0,25 nF (2,5 × 10⁻¹⁰) a 1 MΩ (10⁶) — uma
// faixa de doze ordens de grandeza. Multiplicações intermediárias nas somas
// de b2, b3, a2, a3 produzem números ainda mais extremos antes de tudo se
// cancelar. float (~7 dígitos decimais de precisão) perderia precisão real
// nessas contas; double (~15 dígitos) é o que sobra depois dos
// cancelamentos ainda ser confiável. Só a amostra de áudio em si continua
// float, como em todo o resto do projeto.
//
// COMPONENTES USADOS: OS DO FENDER '59 BASSMAN
// C1 = 0,25 nF, C2 = C3 = 20 nF, R1 = 250 kΩ (treble), R2 = 1 MΩ (bass),
// R3 = 25 kΩ (mid), R4 = 56 kΩ — os valores publicados no artigo pra esse
// amplificador específico. Um Marshall ou um Vox usam a MESMA topologia de
// circuito com valores de componente diferentes, e soariam diferente por
// causa disso — não é um parâmetro deste módulo ainda, é uma constante fixa
// dentro dele.
//
// LIMITAÇÃO DESTA PRIMEIRA VERSÃO
// Os coeficientes são recalculados uma vez por bloco, a partir dos valores
// atuais de bass/mid/treble — não por amostra, e sem suavização entre um
// jogo de coeficientes e o próximo. Um potenciômetro físico de verdade varia
// continuamente; girar os parâmetros digitais MUITO rápido pode, em teoria,
// produzir uma transição perceptível entre blocos. Não é diferente do que
// Delay já faz com o parâmetro time, e pela mesma razão: resolver isso
// direito é assunto próprio (interpolar coeficientes de filtro é mais
// delicado que interpolar um ganho — pode gerar instabilidade transitória
// se feito ingenuamente), documentado aqui para não ser esquecido.
class ToneStack : public AudioModule
{
public:
    ToneStack();

    void setBass(float amount);
    float bass() const;

    void setMid(float amount);
    float mid() const;

    void setTreble(float amount);
    float treble() const;

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    // Recalcula A0..A3 e B0..B3 a partir de bass/mid/treble atuais e do
    // sample rate. Chamado uma vez por bloco, não por amostra — ver a
    // LIMITAÇÃO no comentário da classe.
    void updateCoefficients();

    double m_sampleRate = 44100.0;

    double m_b0 = 0.0, m_b1 = 0.0, m_b2 = 0.0, m_b3 = 0.0;
    double m_a0 = 1.0, m_a1 = 0.0, m_a2 = 0.0, m_a3 = 0.0;

    // Histórico de entrada e saída — a "memória" de um filtro de 3ª ordem.
    double m_x1 = 0.0, m_x2 = 0.0, m_x3 = 0.0;
    double m_y1 = 0.0, m_y2 = 0.0, m_y3 = 0.0;
};
