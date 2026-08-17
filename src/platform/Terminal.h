#pragma once

#include <string>

// Terminal — controle bruto do terminal, para desenhar e ler teclas.
//
// O PROBLEMA
// Um terminal normal trabalha em "modo linha": ele guarda o que você digita e
// só entrega ao programa quando você aperta Enter. Também ecoa cada tecla na
// tela sozinho. Ótimo para comandos, inútil para uma interface — não dá para
// reagir a uma seta se o programa só a recebe depois do Enter.
//
// O modo BRUTO desliga as duas coisas: cada tecla chega na hora, e nada é
// impresso sem o programa mandar.
//
// PRECISA SER DESFEITO
// Um terminal deixado em modo bruto continua assim depois que o programa
// termina — sem eco, sem processar Enter, aparentemente quebrado. Por isso a
// classe é RAII: o destrutor restaura a configuração original, e isso vale
// inclusive quando uma exceção desenrola a pilha.
//
// COMO SE DESENHA
// Por sequências de escape ANSI: pequenos códigos que o terminal interpreta
// como comando em vez de texto. "\033[2J" limpa a tela, "\033[10;5H" move o
// cursor para a linha 10, coluna 5, "\033[31m" pinta de vermelho. É o mesmo
// mecanismo que qualquer programa de terminal colorido usa.
//
// Para não piscar, a tela NÃO é limpa a cada quadro. O cursor volta ao topo e
// o conteúdo é reescrito por cima — o terminal só atualiza o que mudou.
class Terminal
{
public:
    // Códigos devolvidos por readKey() além dos caracteres comuns.
    //
    // As setas do teclado não são um caractere: chegam como três bytes
    // (ESC, '[', 'A'). readKey() reconhece a sequência e devolve um destes
    // valores, escolhidos fora da faixa ASCII para não colidir com letras.
    enum Key
    {
        None = 0,
        Up = 1000,
        Down,
        Right,
        Left,
        Escape
    };

    // Entra em modo bruto e esconde o cursor.
    Terminal();

    // Restaura o terminal ao estado anterior.
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    Terminal(Terminal&&) = delete;
    Terminal& operator=(Terminal&&) = delete;

    // Lê uma tecla sem esperar. Devolve None se nada foi digitado.
    int readKey();

    // Verdadeiro se o modo bruto foi ativado. Falso quando a saída não é um
    // terminal de verdade — num pipe ou redirecionamento, por exemplo.
    bool isInteractive() const;

    // Limpa a tela inteira. Usado uma vez, na entrada.
    static void clear();

    // Leva o cursor ao canto superior esquerdo, para redesenhar por cima.
    static void home();

    // Apaga do cursor até o fim da linha, evitando restos do quadro anterior.
    static std::string clearToEndOfLine();

    // Cores. Devolvem a sequência de escape, para compor na saída.
    static std::string color(int code);
    static std::string reset();
    static std::string bold();

private:
    bool m_interactive = false;

    // A configuração original, guardada para ser restaurada. O tipo real
    // depende do sistema, então fica escondido atrás de um ponteiro.
    struct State;
    State* m_state = nullptr;
};
