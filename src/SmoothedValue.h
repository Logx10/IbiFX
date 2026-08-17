#pragma once

// SmoothedValue — um valor que caminha até o destino em vez de saltar.
//
// O PROBLEMA: ZIPPER NOISE
// Mudar um parâmetro no meio de um bloco cria um degrau na forma de onda.
// Com ganho saltando de 1.0 para 0.0 entre duas amostras vizinhas:
//
//     ...  0.8   0.7   0.6  |  0.0   0.0   0.0  ...
//                            ↑
//                   descontinuidade
//
// Aquele salto vertical não estava no sinal original. Uma descontinuidade é,
// em termos de frequência, um estalo de banda larga — o ouvido escuta um
// clique. Girando um knob continuamente, os cliques viram um chiado
// característico, o "zipper noise" que denuncia software mal feito.
//
// O detalhe traiçoeiro é que o valor final está CERTO. O erro não está em
// para onde o parâmetro foi, e sim em quão rápido chegou lá.
//
// A SOLUÇÃO
// Em vez de saltar, caminhar. O valor pedido vira um ALVO, e a cada amostra
// o valor atual se aproxima um pouco dele. Em 20 ms de rampa a 48 kHz, são
// 960 passos minúsculos no lugar de um salto — e a onda continua contínua.
//
// RAMPA LINEAR, E NÃO EXPONENCIAL
// Esta implementação divide a distância em partes iguais e caminha em linha
// reta. A alternativa clássica é o filtro de um polo, que se aproxima do
// alvo por uma curva exponencial e nunca chega exatamente:
//
//     atual += (alvo - atual) * coeficiente;
//
// O exponencial é uma linha só e soa um pouco mais natural, mas tem duas
// propriedades ruins para aprender: nunca termina de verdade, e o tempo de
// rampa vira um coeficiente abstrato em vez de uma duração em segundos.
// A rampa linear tem começo, meio e fim visíveis, e é isso que importa agora.
//
// DOIS DOMÍNIOS
// prepare() e setTarget() vêm do domínio de controle. nextValue() roda uma
// vez por amostra na thread de áudio: uma soma e uma comparação, sem
// alocação e sem ramificação imprevisível.
//
// SEM PREPARE, SEM RAMPA
// Enquanto prepare() não for chamado, o comprimento da rampa é zero e todo
// setTarget() salta direto para o destino. É o comportamento correto para
// processamento offline e para testes que não simulam um dispositivo de
// áudio — e evita que um módulo não preparado fique preso num valor antigo.
// Duração padrão da rampa dos módulos.
//
// 20 ms é o compromisso usual: longo o bastante para eliminar o clique,
// curto o bastante para o controle continuar parecendo instantâneo ao
// operador. Rampas muito longas fazem o knob soar "borrachudo"; muito
// curtas voltam a estalar.
inline constexpr float kDefaultRampSeconds = 0.02f;

class SmoothedValue
{
public:
    // Define quantas amostras a rampa dura e assume o valor inicial de
    // imediato, sem rampa. Chamada do domínio de controle.
    void prepare(double sampleRate, float rampSeconds, float initialValue);

    // Define o destino. A rampa recomeça a partir do valor atual, o que
    // torna seguro mudar o alvo no meio de uma rampa em andamento.
    void setTarget(float newTarget);

    // Salta para o valor, descartando qualquer rampa em andamento. Usado no
    // reset(), onde não há som anterior para proteger de descontinuidade.
    void snapTo(float newValue);

    // Valor atual, sem avançar.
    float current() const;

    // Valor de destino.
    float target() const;

    // Verdadeiro enquanto a rampa não terminou.
    bool isSmoothing() const;

    // Avança uma amostra e devolve o novo valor atual.
    float nextValue();

private:
    float m_current = 0.0f;
    float m_target = 0.0f;
    float m_step = 0.0f;
    int m_stepsRemaining = 0;
    int m_rampLength = 0;
};
