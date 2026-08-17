#include "Terminal.h"

#include <cstdio>
#include <iostream>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif

#ifdef _WIN32

struct Terminal::State
{
    DWORD inputMode = 0;
    DWORD outputMode = 0;
};

Terminal::Terminal()
    : m_state(new State())
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

    if (input == INVALID_HANDLE_VALUE || output == INVALID_HANDLE_VALUE)
    {
        return;
    }

    GetConsoleMode(input, &m_state->inputMode);
    GetConsoleMode(output, &m_state->outputMode);

    // Sem ENABLE_LINE_INPUT e ENABLE_ECHO_INPUT, cada tecla chega na hora e
    // nada é impresso sozinho — o equivalente ao modo bruto do POSIX.
    SetConsoleMode(input, m_state->inputMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));

    // O Windows só interpreta sequências ANSI se isto for ligado. Sem ele o
    // terminal imprimiria os códigos como texto.
    SetConsoleMode(output, m_state->outputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    m_interactive = true;
    std::cout << "\033[?25l" << std::flush;   // esconde o cursor
}

Terminal::~Terminal()
{
    if (m_interactive)
    {
        std::cout << "\033[?25h" << std::flush;   // mostra o cursor

        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), m_state->inputMode);
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), m_state->outputMode);
    }

    delete m_state;
}

int Terminal::readKey()
{
    if (!m_interactive || !_kbhit())
    {
        return None;
    }

    const int c = _getch();

    // No Windows as setas chegam como 0 ou 224 seguido de um código.
    if (c == 0 || c == 224)
    {
        switch (_getch())
        {
            case 72: return Up;
            case 80: return Down;
            case 77: return Right;
            case 75: return Left;
            default: return None;
        }
    }

    if (c == 27)
    {
        return Escape;
    }

    return c;
}

#else

struct Terminal::State
{
    termios original{};
};

Terminal::Terminal()
    : m_state(new State())
{
    // Se a entrada não for um terminal — num pipe, por exemplo — não há modo
    // bruto para ativar, e tentar configurá-lo falharia.
    if (!isatty(STDIN_FILENO))
    {
        return;
    }

    if (tcgetattr(STDIN_FILENO, &m_state->original) != 0)
    {
        return;
    }

    termios raw = m_state->original;

    // ICANON desliga o modo linha: as teclas param de esperar pelo Enter.
    // ECHO impede o terminal de imprimir sozinho o que foi digitado.
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));

    // VMIN 0 e VTIME 0 fazem a leitura devolver imediatamente, mesmo sem
    // nada digitado. É o que torna readKey() não bloqueante.
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
    {
        return;
    }

    m_interactive = true;
    std::cout << "\033[?25l" << std::flush;   // esconde o cursor
}

Terminal::~Terminal()
{
    if (m_interactive)
    {
        std::cout << "\033[?25h" << std::flush;   // mostra o cursor
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_state->original);
    }

    delete m_state;
}

int Terminal::readKey()
{
    if (!m_interactive)
    {
        return None;
    }

    char c = 0;

    if (read(STDIN_FILENO, &c, 1) != 1)
    {
        return None;
    }

    if (c != 27)
    {
        return static_cast<unsigned char>(c);
    }

    // Começou com ESC: pode ser a tecla Escape sozinha ou o início de uma
    // sequência de seta. Se os próximos bytes não vierem, era Escape.
    char sequence[2] = {0, 0};

    if (read(STDIN_FILENO, &sequence[0], 1) != 1) return Escape;
    if (read(STDIN_FILENO, &sequence[1], 1) != 1) return Escape;

    if (sequence[0] == '[')
    {
        switch (sequence[1])
        {
            case 'A': return Up;
            case 'B': return Down;
            case 'C': return Right;
            case 'D': return Left;
            default: break;
        }
    }

    return Escape;
}

#endif

bool Terminal::isInteractive() const
{
    return m_interactive;
}

void Terminal::clear()
{
    std::cout << "\033[2J\033[H" << std::flush;
}

void Terminal::home()
{
    std::cout << "\033[H";
}

std::string Terminal::clearToEndOfLine()
{
    return "\033[K";
}

std::string Terminal::color(int code)
{
    return "\033[" + std::to_string(code) + "m";
}

std::string Terminal::reset()
{
    return "\033[0m";
}

std::string Terminal::bold()
{
    return "\033[1m";
}
