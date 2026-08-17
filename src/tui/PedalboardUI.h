#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "LiveEngine.h"
#include "Terminal.h"

// PedalboardUI — a pedaleira desenhada em texto, controlada pelo teclado.
//
// PARA QUE SERVE
// Até aqui, mexer num parâmetro exigia editar a linha de comando e rodar de
// novo. Aqui os controles respondem enquanto o som toca, que é como uma
// pedaleira de verdade funciona — e é a diferença entre conferir números e
// procurar um som.
//
// ONDE ELA SE ENCAIXA
// É uma CAMADA sobre o engine, exatamente como o princípio 2 do ARCHITECTURE
// pede. Ela não calcula nada de áudio: lê o estado para desenhar e envia
// comandos para alterar. Apagar este arquivo não afetaria uma linha de DSP.
//
// A REGRA DAS DUAS THREADS
// O desenho roda na thread principal, num laço de cerca de 30 quadros por
// segundo. O áudio roda na thread do driver. Elas se falam por dois caminhos
// já construídos, e por nenhum outro:
//
//   interface -> áudio    fila de comandos (setParameter, setBypassed)
//   áudio -> interface    valores atômicos (picos, contagem de blocos)
//
// Nada aqui trava a thread de áudio, e nada aqui espera por ela.
//
// SOBRE O MEDIDOR EM dB
// O medidor mostra decibéis porque o ouvido percebe volume de forma
// logarítmica, não linear. Metade da amplitude não soa "metade do volume" —
// soa 6 dB mais baixo. Uma barra linear passaria quase toda a vida colada no
// canto esquerdo; em dB, ela se move como o ouvido espera.
class PedalboardUI
{
public:
    // O engine precisa ter a cadeia montada antes de entrar aqui.
    explicit PedalboardUI(LiveEngine& engine);

    // Roda o laço da interface até a pessoa sair. Devolve o código de saída.
    //
    // O modo do dispositivo é escolhido por quem chama: duplex para tocar
    // guitarra, nulo para experimentar a interface sem hardware.
    int run(AudioDevice::Mode mode, double sampleRate, int blockSize);

private:
    void draw();
    bool handleKey(int key);

    // Move a seleção entre os parâmetros de todos os módulos, como se fossem
    // uma lista só. É o que faz a navegação parecer natural.
    void moveSelection(int delta);

    // Soma ou subtrai um passo no parâmetro selecionado.
    //
    // O passo é proporcional à faixa do parâmetro: 1% dela. Um passo fixo
    // seria grosseiro demais para o mix, que vai de 0 a 1, e lento demais
    // para o corte do filtro, que vai até 2000.
    void adjustSelected(float direction);

    std::size_t selectedModule() const;
    std::size_t selectedParameter() const;
    std::size_t totalParameters() const;

    // Barra horizontal com marcador de posição, para um parâmetro.
    std::string knob(float normalized, int width) const;

    // Barra de nível em dB, com cor mudando conforme se aproxima do teto.
    std::string meter(float peak, int width) const;

    // A POSIÇÃO DOS KNOBS É ESTADO DA INTERFACE.
    //
    // Poderia parecer natural ler o valor do parâmetro, somar um passo e
    // enviar o resultado. Não funciona: os comandos passam por uma fila, e
    // vinte teclas apertadas depressa entram todas ANTES de a thread de áudio
    // aplicar a primeira. As vinte leriam o mesmo valor antigo e enviariam o
    // mesmo destino — o knob andaria um passo só.
    //
    // Guardando o alvo aqui, cada tecla parte de onde a anterior chegou, e a
    // fila deixa de importar. É também como uma interface de verdade se
    // comporta: o knob mostra onde a MÃO está, não onde o motor chegou.
    std::vector<float> m_targets;

    // Preenche m_targets a partir dos valores atuais dos parâmetros.
    void captureTargets();

    LiveEngine& m_engine;
    Terminal m_terminal;

    std::size_t m_selection = 0;
    bool m_quit = false;
    std::string m_message;

    // Configuração do dispositivo, guardada para a tecla de espaço reabrir.
    AudioDevice::Mode m_mode = AudioDevice::Mode::Null;
    double m_sampleRate = 48000.0;
    int m_blockSize = 128;
};
