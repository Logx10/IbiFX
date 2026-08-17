#pragma once

#include <atomic>
#include <string>

// Parameter — um valor ajustável de um módulo, com identidade e limites.
//
// O PROBLEMA
// Cada módulo hoje expõe setters próprios: setGain, setThreshold, setDrive.
// Funciona quando o chamador conhece o tipo concreto, e só nesse caso. Um
// preset lendo de arquivo, um controlador MIDI ou um knob de interface não
// conhecem — eles têm em mãos um nome e um número, não um GainProcessor.
//
// Sem uma representação comum, salvar um preset exigiria um trecho de código
// por tipo de módulo, e cada módulo novo obrigaria a mexer no preset, no MIDI
// e na UI. É esse acoplamento que este arquivo desfaz.
//
// AS TRÊS COISAS QUE UM PARÂMETRO PRECISA CARREGAR
//
//   id      identificador estável, usado por preset e mapeamento MIDI.
//           Nunca deve mudar depois de publicado: é ele que aparece gravado
//           no arquivo de preset do usuário.
//
//   label   texto para humanos, livre para mudar e para ser traduzido.
//
//   faixa   mínimo e máximo. Valor fora dela é ajustado para a borda, não
//           rejeitado — é o comportamento de um knob físico, que simplesmente
//           não gira além do batente. Isso não conflita com a regra de não
//           esconder erros: pedir 20 num knob que vai até 8 não é erro de
//           programação, é o limite do controle.
//
// NORMALIZAÇÃO
// Cada parâmetro tem sua própria faixa: ganho vai de -8 a 8, drive de 0 a 100.
// Quem controla, porém, fala outra língua: MIDI CC entrega 0 a 127, um knob
// de tela entrega 0 a 1, automação entrega 0 a 1.
//
// A forma normalizada (0 a 1) é a moeda comum entre esses mundos. O
// controlador trabalha sempre em 0..1 e nunca precisa saber o que significa
// "8" para o ganho; o parâmetro faz a conversão nos dois sentidos.
//
// O PARÂMETRO GUARDA O DESTINO, NÃO O VALOR INSTANTÂNEO
// Mudar um parâmetro de forma abrupta produz um degrau na onda, ouvido como
// clique. Quem resolve isso é o SmoothedValue, dentro de cada módulo: ele
// trata o valor daqui como ALVO e caminha até ele ao longo das amostras.
//
// A divisão é proposital. Este arquivo trata de identidade, limites e
// conversão — assuntos do domínio de controle. A interpolação amostra a
// amostra é assunto da thread de áudio, e vive lá.
class Parameter
{
public:
    Parameter(std::string id,
              std::string label,
              float minValue,
              float maxValue,
              float defaultValue);

    // std::atomic não é copiável nem movível, e sem estes construtores um
    // std::vector<Parameter> não compilaria — ele precisa realocar. Copiar
    // carrega o valor e o regrava; é seguro porque cópia de Parameter só
    // acontece no domínio de controle, nunca durante o processamento.
    Parameter(const Parameter& other);
    Parameter& operator=(const Parameter& other);

    // Identificador estável, usado por presets e mapeamentos.
    const std::string& id() const;

    // Texto legível para interface.
    const std::string& label() const;

    float minValue() const;
    float maxValue() const;

    // Valor com que o parâmetro nasce e para o qual reset() o devolve.
    float defaultValue() const;

    // Valor atual, sempre dentro da faixa.
    float value() const;

    // Define o valor, ajustando para a borda mais próxima se estiver fora.
    void setValue(float newValue);

    // Valor atual expresso de 0 a 1 dentro da faixa.
    float normalized() const;

    // Define o valor a partir de uma posição de 0 a 1 dentro da faixa.
    void setNormalized(float normalizedValue);

    // Devolve o parâmetro ao valor padrão.
    void reset();

private:
    std::string m_id;
    std::string m_label;
    float m_minValue;
    float m_maxValue;
    float m_defaultValue;

    // ATÔMICO PORQUE DUAS THREADS TOCAM NELE
    // O domínio de controle escreve (knob, MIDI, preset) enquanto a thread de
    // áudio lê, a cada bloco. Um float comum lido e escrito ao mesmo tempo é
    // corrida de dados — comportamento indefinido pelo padrão, mesmo que na
    // prática costume "funcionar" nas arquiteturas atuais.
    //
    // std::atomic<float> é lock-free em toda plataforma que nos interessa
    // (há verificação em teste), então a leitura na thread de áudio continua
    // sendo uma instrução comum, sem bloqueio e sem espera.
    //
    // memory_order_relaxed basta aqui: cada parâmetro é independente, e não
    // há outro dado cuja visibilidade precise ser ordenada junto com ele. O
    // pior caso é a thread de áudio usar o valor antigo por um bloco, o que
    // é imperceptível — e a suavização ainda o transforma numa rampa.
    std::atomic<float> m_value;
};
