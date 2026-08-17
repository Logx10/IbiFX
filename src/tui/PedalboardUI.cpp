#include "PedalboardUI.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace
{
constexpr int kKnobWidth = 24;
constexpr int kMeterWidth = 32;

// Cerca de 30 quadros por segundo. Rápido o bastante para o medidor parecer
// contínuo, devagar o bastante para não gastar CPU à toa — e a CPU aqui
// disputa com a thread de áudio, que tem prazo para cumprir.
constexpr int kFrameMillis = 33;

constexpr int kGray = 90;
constexpr int kGreen = 32;
constexpr int kYellow = 33;
constexpr int kRed = 31;
constexpr int kCyan = 36;
constexpr int kWhite = 37;

// Conta COLUNAS, não bytes.
//
// Os caracteres de moldura e os símbolos como ● ocupam vários bytes em UTF-8
// mas uma coluna só na tela. Contar bytes desalinharia a moldura.
std::size_t displayWidth(const std::string& text)
{
    std::size_t columns = 0;

    for (unsigned char c : text)
    {
        // Em UTF-8, os bytes de continuação começam com os bits 10. Ignorá-los
        // faz a conta bater com o número de caracteres.
        if ((c & 0xC0) != 0x80)
        {
            ++columns;
        }
    }

    return columns;
}

std::string formatValue(const Parameter& parameter, float value)
{
    std::ostringstream out;
    out << std::fixed;

    // Frequência em hertz não precisa de casas decimais; ganho e mix
    // precisam. O id do parâmetro é o que diz qual é o caso.
    if (parameter.id() == "frequency")
    {
        out << std::setprecision(0) << value << " Hz";
    }
    else if (parameter.id() == "time")
    {
        out << std::setprecision(0) << value * 1000.0f << " ms";
    }
    else
    {
        out << std::setprecision(2) << value;
    }

    return out.str();
}
}

PedalboardUI::PedalboardUI(LiveEngine& engine)
    : m_engine(engine)
{
}

std::size_t PedalboardUI::totalParameters() const
{
    std::size_t total = 0;

    for (std::size_t i = 0; i < m_engine.chain().size(); ++i)
    {
        total += m_engine.chain().moduleAt(i).parameterCount();
    }

    return total;
}

std::size_t PedalboardUI::selectedModule() const
{
    std::size_t counter = 0;

    for (std::size_t i = 0; i < m_engine.chain().size(); ++i)
    {
        const std::size_t count = m_engine.chain().moduleAt(i).parameterCount();

        if (m_selection < counter + count)
        {
            return i;
        }

        counter += count;
    }

    return 0;
}

std::size_t PedalboardUI::selectedParameter() const
{
    std::size_t counter = 0;

    for (std::size_t i = 0; i < m_engine.chain().size(); ++i)
    {
        const std::size_t count = m_engine.chain().moduleAt(i).parameterCount();

        if (m_selection < counter + count)
        {
            return m_selection - counter;
        }

        counter += count;
    }

    return 0;
}

void PedalboardUI::moveSelection(int delta)
{
    const std::size_t total = totalParameters();

    if (total == 0)
    {
        return;
    }

    // Aritmética com sinal antes de voltar para o índice, e com módulo para
    // dar a volta nas pontas — descer do último leva ao primeiro.
    const int current = static_cast<int>(m_selection);
    const int count = static_cast<int>(total);
    const int next = ((current + delta) % count + count) % count;

    m_selection = static_cast<std::size_t>(next);
}

void PedalboardUI::captureTargets()
{
    m_targets.clear();

    for (std::size_t i = 0; i < m_engine.chain().size(); ++i)
    {
        const AudioModule& module = m_engine.chain().moduleAt(i);

        for (std::size_t p = 0; p < module.parameterCount(); ++p)
        {
            m_targets.push_back(module.parameterAt(p).value());
        }
    }
}

void PedalboardUI::adjustSelected(float direction)
{
    if (totalParameters() == 0 || m_selection >= m_targets.size())
    {
        return;
    }

    const std::size_t moduleIndex = selectedModule();
    const std::size_t parameterIndex = selectedParameter();

    const Parameter& parameter = m_engine.chain().moduleAt(moduleIndex).parameterAt(parameterIndex);

    // O passo é 1% da faixa, para que cada controle ande na sua própria
    // escala. Um passo fixo seria grosseiro no mix e lento no corte.
    const float step = (parameter.maxValue() - parameter.minValue()) * 0.01f;

    // Parte do ALVO local, não do valor lido do módulo. O clamp é feito aqui
    // também, senão o alvo se afastaria da borda sem que a tela mostrasse.
    float target = m_targets[m_selection] + step * direction;
    target = std::max(parameter.minValue(), std::min(parameter.maxValue(), target));

    m_targets[m_selection] = target;

    m_engine.setParameter(moduleIndex, parameterIndex, target);
}

std::string PedalboardUI::knob(float normalized, int width) const
{
    const int position = static_cast<int>(normalized * static_cast<float>(width - 1) + 0.5f);

    std::string bar;
    bar.reserve(static_cast<std::size_t>(width) * 3);

    for (int i = 0; i < width; ++i)
    {
        if (i == position)
        {
            bar += Terminal::color(kCyan) + "●" + Terminal::reset();
        }
        else if (i < position)
        {
            bar += Terminal::color(kGray) + "─" + Terminal::reset();
        }
        else
        {
            bar += Terminal::color(kGray) + "·" + Terminal::reset();
        }
    }

    return bar;
}

std::string PedalboardUI::meter(float peak, int width) const
{
    // Converte amplitude em decibéis. O piso de -60 dB é onde o sinal já é
    // inaudível na prática; abaixo disso a barra fica vazia em vez de tender
    // ao infinito negativo.
    const float floorDb = -60.0f;
    const float db = peak > 0.0f ? 20.0f * std::log10(peak) : floorDb;
    const float clamped = std::max(db, floorDb);
    const float fraction = (clamped - floorDb) / (0.0f - floorDb);

    const int filled = static_cast<int>(fraction * static_cast<float>(width) + 0.5f);

    std::string bar;

    for (int i = 0; i < width; ++i)
    {
        if (i >= filled)
        {
            bar += Terminal::color(kGray) + "░" + Terminal::reset();
            continue;
        }

        // As cores marcam as três regiões que importam ao operar: verde é
        // saudável, amarelo é perto do teto, vermelho é onde vai cortar.
        const float positionDb = floorDb + (static_cast<float>(i) / static_cast<float>(width)) * -floorDb;

        int c = kGreen;
        if (positionDb > -3.0f)      c = kRed;
        else if (positionDb > -12.0f) c = kYellow;

        bar += Terminal::color(c) + "█" + Terminal::reset();
    }

    std::ostringstream out;
    out << bar << " " << std::fixed << std::setprecision(1) << std::setw(6);

    if (peak > 0.0f)
        out << db;
    else
        out << floorDb;

    out << " dB";

    return out.str();
}

void PedalboardUI::draw()
{
    std::ostringstream screen;

    const auto line = [&screen](const std::string& text = "")
    {
        screen << text << Terminal::clearToEndOfLine() << "\r\n";
    };

    Terminal::home();

    const bool running = m_engine.isRunning();

    line(Terminal::bold() + Terminal::color(kCyan) +
         "  ╭──────────────────────────────────────────────────────────────╮" + Terminal::reset());

    const std::string estado = running ? "● tocando" : "○ parado";

    // O texto visível é montado à parte para poder ser MEDIDO. As sequências
    // de cor ocupam bytes que não aparecem na tela, então contar o tamanho da
    // string colorida daria um alinhamento errado.
    const std::string visivel = "  │  IbiFX   pedaleira   " + estado;
    const std::size_t largura = displayWidth(visivel);
    const std::size_t alvo = 67;   // a moldura tem 67 colunas até a borda direita

    std::ostringstream title;
    title << Terminal::bold() << Terminal::color(kCyan) << "  │  IbiFX" << Terminal::reset()
          << Terminal::color(kGray) << "   pedaleira" << Terminal::reset()
          << "   " << (running ? Terminal::color(kGreen) : Terminal::color(kGray))
          << estado << Terminal::reset()
          << std::string(alvo > largura ? alvo - largura : 1, ' ')
          << Terminal::bold() << Terminal::color(kCyan) << "│" << Terminal::reset();

    line(title.str());

    line(Terminal::bold() + Terminal::color(kCyan) +
         "  ╰──────────────────────────────────────────────────────────────╯" + Terminal::reset());
    line();

    std::ostringstream info;
    info << Terminal::color(kGray) << "   " << m_engine.deviceName()
         << "   " << static_cast<int>(m_engine.sampleRate()) << " Hz"
         << "   " << m_engine.processedBlocks() << " blocos" << Terminal::reset();
    line(info.str());
    line();

    line("   " + Terminal::color(kWhite) + "entrada" + Terminal::reset() + "  " +
         meter(m_engine.inputPeak(), kMeterWidth));
    line("   " + Terminal::color(kWhite) + "saida  " + Terminal::reset() + "  " +
         meter(m_engine.outputPeak(), kMeterWidth));
    line();

    const ModuleChain& chain = m_engine.chain();
    std::size_t flat = 0;

    for (std::size_t i = 0; i < chain.size(); ++i)
    {
        const AudioModule& module = chain.moduleAt(i);
        const bool bypassed = chain.isBypassed(i);

        std::ostringstream header;
        header << "   ";

        if (bypassed)
            header << Terminal::color(kGray) << "▪ " << module.name() << "  [bypass]" << Terminal::reset();
        else
            header << Terminal::bold() << Terminal::color(kGreen) << "▪ " << Terminal::reset()
                   << Terminal::bold() << module.name() << Terminal::reset();

        line(header.str());

        for (std::size_t p = 0; p < module.parameterCount(); ++p)
        {
            const Parameter& parameter = module.parameterAt(p);
            const bool selected = (flat == m_selection);

            // Desenha a posição do ALVO, e não a lida do módulo: é o que
            // torna o knob responsivo, sem esperar o próximo bloco de áudio.
            const float target = flat < m_targets.size() ? m_targets[flat] : parameter.value();
            const float range = parameter.maxValue() - parameter.minValue();
            const float normalized = range > 0.0f ? (target - parameter.minValue()) / range : 0.0f;

            std::ostringstream row;
            row << (selected ? Terminal::color(kCyan) + "   ▸ " + Terminal::reset() : "     ");

            row << (selected ? Terminal::bold() : Terminal::color(kGray));
            row << std::left << std::setw(10) << parameter.id() << Terminal::reset();

            row << knob(normalized, kKnobWidth);
            row << "  " << (selected ? Terminal::bold() : "") << std::right << std::setw(9)
                << formatValue(parameter, target) << Terminal::reset();

            line(row.str());
            ++flat;
        }

        line();
    }

    line(Terminal::color(kGray) +
         "   ↑↓ escolher    ←→ ajustar    b bypass    r reset    espaco liga/desliga    q sair" +
         Terminal::reset());
    line();

    if (!m_message.empty())
    {
        line("   " + Terminal::color(kYellow) + m_message + Terminal::reset());
    }
    else
    {
        line();
    }

    std::cout << screen.str() << std::flush;
}

bool PedalboardUI::handleKey(int key)
{
    switch (key)
    {
        case Terminal::None:
            return false;

        case 'q':
        case 'Q':
        case Terminal::Escape:
            m_quit = true;
            return true;

        case Terminal::Up:
        case 'k':
            moveSelection(-1);
            return true;

        case Terminal::Down:
        case 'j':
            moveSelection(1);
            return true;

        case Terminal::Left:
        case 'h':
            adjustSelected(-1.0f);
            return true;

        case Terminal::Right:
        case 'l':
            adjustSelected(1.0f);
            return true;

        case 'b':
        case 'B':
        {
            const std::size_t index = selectedModule();
            const bool wasBypassed = m_engine.chain().isBypassed(index);
            m_engine.setBypassed(index, !wasBypassed);
            return true;
        }

        case 'r':
        case 'R':
            m_engine.resetModule(selectedModule());
            m_message = "estado do modulo descartado";
            return true;

        case ' ':
            if (m_engine.isRunning())
            {
                m_engine.stop();
                m_message = "audio parado";
            }
            else if (m_engine.start(m_mode, m_sampleRate, m_blockSize))
            {
                // Os alvos são recapturados porque o start() refaz o prepare
                // dos módulos, e os valores podem ter voltado ao padrão.
                captureTargets();
                m_message.clear();
            }
            else
            {
                m_message = "falha ao reabrir: " + m_engine.lastError();
            }
            return true;

        default:
            return false;
    }
}

int PedalboardUI::run(AudioDevice::Mode mode, double sampleRate, int blockSize)
{
    // Guardados para que a tecla de espaço consiga reabrir o dispositivo com
    // a mesma configuração.
    m_mode = mode;
    m_sampleRate = sampleRate;
    m_blockSize = blockSize;

    if (!m_terminal.isInteractive())
    {
        std::cerr << "erro: a interface precisa de um terminal de verdade "
                     "(sem pipe nem redirecionamento)\n";
        return 1;
    }

    if (!m_engine.start(mode, sampleRate, blockSize))
    {
        std::cerr << "erro: " << m_engine.lastError() << "\n";
        return 1;
    }

    captureTargets();

    Terminal::clear();

    while (!m_quit)
    {
        // Consome TODAS as teclas pendentes antes de desenhar. Uma tecla por
        // quadro faria a interface parecer travada se alguém segurasse a
        // seta, porque as repetições se acumulariam na fila do sistema.
        int key = m_terminal.readKey();

        while (key != Terminal::None)
        {
            handleKey(key);
            key = m_terminal.readKey();
        }

        draw();

        std::this_thread::sleep_for(std::chrono::milliseconds(kFrameMillis));
    }

    m_engine.stop();

    Terminal::clear();
    std::cout << "ate mais.\n";

    return 0;
}
