#pragma once

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
// O QUE FICOU DE FORA: SMOOTHING
// Mudar um parâmetro de forma abrupta no meio de um bloco produz um degrau na
// forma de onda, e degrau é um clique audível — o "zipper noise" que se ouve
// ao girar um knob rápido demais em software mal feito. A solução é
// interpolar o valor ao longo das amostras.
//
// Não está aqui de propósito. Smoothing precisa saber o sample rate e agir
// amostra a amostra, o que exige um prepare() no AudioModule que ainda não
// existe. O §29 do AI_GUIDELINES pede explicitamente que o problema seja
// estudado antes de ser abstraído — e para estudá-lo é preciso primeiro
// conseguir ouvir o clique.
class Parameter
{
public:
    Parameter(std::string id,
              std::string label,
              float minValue,
              float maxValue,
              float defaultValue);

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
    float m_value;
};
