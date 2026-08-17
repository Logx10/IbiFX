#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Parameter.h"

// AudioModule — o contrato comum a todo módulo processador do IbiFX.
//
// O PROBLEMA QUE ELE RESOLVE
// GainProcessor, Clipper e SoftClipper fazem a mesma coisa em essência:
// recebem um buffer e o modificam. Sem um contrato comum, quem quisesse
// encadeá-los precisaria conhecer os três tipos e chamar cada um pelo nome —
// o que torna impossível uma cadeia montada em tempo de execução.
//
// COMO ELE RESOLVE
// Uma classe base declara process() sem implementar. Cada módulo herda e
// fornece a sua versão. A partir daí é possível guardar módulos diferentes
// numa mesma lista e processá-los num laço só, sem o laço saber quem é quem.
// Isso se chama polimorfismo.
//
// = 0 (função virtual pura) significa "não tenho implementação; quem herdar é
// obrigado a fornecer". Uma classe com pelo menos uma dessas é ABSTRATA: não
// pode ser instanciada, só serve como contrato. `AudioModule m;` não compila.
//
// virtual faz a chamada ser resolvida em tempo de execução, pelo objeto real
// e não pelo tipo do ponteiro. Sem ele, chamar process() por um AudioModule*
// executaria a versão da base — que aqui nem existe.
//
// O destrutor virtual é obrigatório: sem ele, destruir um objeto derivado
// através de um ponteiro para a base é comportamento indefinido, e o que for
// exclusivo do derivado vaza.
//
// PARÂMETROS FICAM AQUI, E NÃO SÃO VIRTUAIS
// A base guarda a lista de parâmetros e oferece o acesso a ela. Cada módulo
// só precisa preencher m_parameters no seu construtor.
//
// A alternativa seria declarar parameterCount() e parameterAt() como virtuais
// puros e obrigar cada módulo a implementá-los. Daria exatamente o mesmo
// resultado, com três cópias do mesmo código e uma cópia nova a cada módulo
// futuro. Virtual serve para quando o COMPORTAMENTO varia entre os
// derivados; aqui só varia o CONTEÚDO da lista, e conteúdo se resolve com
// dado, não com herança.
//
// É o que permite a um preset, a um controlador MIDI ou a um knob de tela
// ajustarem qualquer módulo sem conhecer o tipo concreto dele.
//
// prepare() E reset() TÊM CORPO VAZIO, E NÃO SÃO PUROS
// O Delay finalmente justificou os dois: ele precisa saber o sample rate para
// converter segundos em amostras, precisa alocar seu buffer circular longe da
// thread de áudio, e precisa de uma forma de esquecer o eco antigo.
//
// Mas Gain, Clipper e SoftClipper genuinamente não têm o que preparar nem o
// que esquecer — são todos stateless. Torná-los virtuais puros obrigaria três
// corpos vazios escritos à mão, e um corpo vazio novo a cada módulo simples
// futuro. Com implementação padrão, só sobrescreve quem tem o que dizer.
//
// process() NÃO é const, de propósito: o contrato vale para todos os
// implementadores futuros, e o Delay precisa modificar o próprio estado a
// cada bloco.
//
// A REGRA QUE SEPARA prepare() DE process()
// prepare() pertence ao domínio de controle: pode alocar, redimensionar e
// demorar. process() roda na thread de áudio e não pode fazer nada disso.
// Sempre que um módulo precisar de memória, ela nasce no prepare().
//
// CUSTO EM TEMPO REAL
// Chamada virtual custa uma indireção e impede o compilador de embutir a
// função. É irrelevante uma vez por bloco de amostras, como aqui. Seria caro
// uma vez por amostra — e é por isso que process() recebe o buffer inteiro.
class AudioModule
{
public:
    virtual ~AudioModule() = default;

    // Nome curto do módulo, para exibição e depuração.
    virtual const char* name() const = 0;

    // Avisa o módulo do sample rate e do tamanho de bloco que virão, dando a
    // ele a chance de alocar o que precisar. Chamada do domínio de controle,
    // nunca da thread de áudio. Pode ser chamada mais de uma vez.
    virtual void prepare(double sampleRate, int blockSize);

    // Descarta o estado acumulado, sem mexer nos parâmetros. Usada ao trocar
    // de preset ou dar stop, para que o som anterior não continue ressoando.
    virtual void reset();

    // Processa o buffer no lugar.
    virtual void process(std::vector<float>& buffer) = 0;

    // Quantidade de parâmetros expostos pelo módulo.
    std::size_t parameterCount() const;

    // Acesso a um parâmetro pela posição. Lança se o índice não existir.
    Parameter& parameterAt(std::size_t index);
    const Parameter& parameterAt(std::size_t index) const;

    // Busca um parâmetro pelo id. Devolve nullptr se não houver.
    Parameter* findParameter(const std::string& id);
    const Parameter* findParameter(const std::string& id) const;

    // Devolve todos os parâmetros aos valores padrão.
    void resetParameters();

protected:
    // Preenchida pelo construtor de cada módulo concreto.
    std::vector<Parameter> m_parameters;
};
