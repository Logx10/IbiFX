#pragma once

#include <array>
#include <vector>

#include "AudioModule.h"

// Reverb — simula o som de um espaço, não de um eco repetido.
//
// A DIFERENÇA PARA O Delay
// O Delay produz repetições DISCRETAS e espaçadas — dá pra contar "um, dois,
// três" ecos. Um espaço real (uma sala, uma igreja) devolve MILHARES de
// reflexões por segundo, tão próximas umas das outras que o ouvido não separa
// eco de eco: ouve uma cauda contínua, cada vez mais densa e mais fraca. Um
// delay com tempo curtíssimo e muito feedback começa a se aproximar disso,
// mas produz uma cor metálica característica (ressonância numa frequência
// só, a que corresponde ao tempo do delay) que não soa como sala nenhuma.
//
// A ESTRUTURA: SCHRODER, 1962
// A solução clássica — e ainda a base de reverbs digitais modernos, incluindo
// o Freeverb de código aberto que inspira os números usados aqui — é somar
// VÁRIOS filtros comb com tempos diferentes (para que as ressonâncias de cada
// um caiam em frequências diferentes, e a soma pareça densa em vez de
// metálica), seguido de filtros allpass em série (que espalham no tempo sem
// colorir o timbre, aumentando a densidade sem acrescentar ressonância nova).
//
//     entrada -> [4 combs em PARALELO, somados] -> [2 allpass em SÉRIE] -> saída
//
// FILTRO COMB COM AMORTECIMENTO (cada um dos 4)
// Um comb é um Delay com feedback, exatamente como o módulo Delay — mas aqui
// o feedback passa por um filtro passa-baixas antes de voltar ao buffer:
//
//     filterStore = saida * (1 - damping) + filterStore * damping
//     buffer[escrita] = entrada + filterStore * decay
//
// Isso simula um efeito real de salas: o agudo é absorvido mais rápido que o
// grave a cada reflexão contra uma parede, então a cauda escurece com o
// tempo. Sem o filtro, a cauda manteria o mesmo timbre do início ao fim — o
// que soa artificial.
//
// FILTRO ALLPASS (cada um dos 2, em série depois dos combs)
// Estrutura clássica de Schroeder: devolve a mesma energia que recebeu (daí
// "allpass" — deixa passar todas as frequências igual, sem colorir), mas
// espalhada no tempo. Serve só para aumentar a densidade de reflexões sem
// introduzir uma ressonância nova visível.
//
// OS NÚMEROS (1116, 1188, 1277, 1356, 556, 441 AMOSTRAS)
// Vêm do Freeverb (Jezar at Dreampoint, domínio público, ano 2000) — a
// referência mais estudada de reverb algorítmico em software. Não são
// arbitrários: foram escolhidos por tentativa e erro para que os tempos dos
// diferentes combs não tenham fatores em comum, evitando que as ressonâncias
// de dois filtros caiam na mesma frequência e reforcem uma nota só. Os
// valores são para 44100 Hz e são escalados proporcionalmente ao sample rate
// real em prepare().
//
// POR QUE SÓ 4 COMBS E 2 ALLPASS, NÃO OS 8 E 4 DO Freeverb
// É o tamanho do desenho ORIGINAL de Schroeder (1962), antes do Freeverb
// dobrar para estéreo denso. Metade do tamanho, mesma ideia, mais fácil de
// acompanhar — e dá pra crescer depois se a densidade não for suficiente.
class Reverb : public AudioModule
{
public:
    Reverb();

    // Quanto tempo a cauda leva para sumir. Não é segundos diretamente — é o
    // ganho de feedback de cada comb, e por isso o teto é 0.98, não 1.0:
    // feedback >= 1 nunca decai, é reverb infinito e instável.
    void setDecay(float amount);
    float decay() const;

    // Quanto a cauda escurece com o tempo. 0 = sem amortecimento (cauda
    // metálica, mesmo timbre do início ao fim). 1 = amortecimento máximo.
    void setDamping(float amount);
    float damping() const;

    // Quanto do sinal processado (molhado) entra na saída.
    void setMix(float amount);
    float mix() const;

    const char* name() const override;

    void prepare(double sampleRate, int blockSize) override;
    void reset() override;

    void process(std::vector<float>& buffer) override;

private:
    static constexpr int kCombCount = 4;
    static constexpr int kAllpassCount = 2;

    // Comprimentos de referência a 44100 Hz — ver o comentário da classe.
    static constexpr std::array<int, kCombCount> kCombLengthsAt44100 = {1116, 1188, 1277, 1356};
    static constexpr std::array<int, kAllpassCount> kAllpassLengthsAt44100 = {556, 441};

    // O feedback do allpass é fixo, não um parâmetro: é uma constante
    // estrutural do desenho de Schroeder (0.5 é o valor do artigo original),
    // não um controle que o músico gira. Os combs têm o "decay" ajustável
    // porque É esse o controle que faz sentido pro ouvido; o allpass não
    // tem equivalente perceptível de ajustar sozinho.
    static constexpr float kAllpassFeedback = 0.5f;

    struct Comb
    {
        std::vector<float> buffer;
        std::size_t writePosition = 0;

        // Estado do filtro passa-baixas dentro do loop de feedback — não é
        // uma amostra do buffer, é memória do PRÓPRIO filtro.
        float filterStore = 0.0f;
    };

    struct Allpass
    {
        std::vector<float> buffer;
        std::size_t writePosition = 0;
    };

    static float processComb(Comb& comb, float input, float feedback, float damping);
    static float processAllpass(Allpass& allpass, float input, float feedback);

    std::array<Comb, kCombCount> m_combs;
    std::array<Allpass, kAllpassCount> m_allpasses;
};
