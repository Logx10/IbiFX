#pragma once

#include <string>
#include <vector>

// loadImpulseResponse — lê um .wav e devolve a resposta ao impulso como um
// sinal mono, pronto pra virar os "taps" de uma convolução.
//
// O QUE É UMA IMPULSE RESPONSE
// Se você excita um sistema linear (um alto-falante dentro de um gabinete,
// captado por um microfone, numa sala) com um impulso — um único pico
// infinitamente curto — o que sai do outro lado é a "impressão digital"
// completa daquele sistema: como ele reage a CADA frequência e o quanto
// demora pra cada uma decair. Gravar essa resposta uma vez, num arquivo
// .wav, e depois convolver o sinal seco com ela reproduz — matematicamente,
// não por aproximação — o efeito do gabinete/microfone/sala originais.
//
// POR QUE MONO
// O núcleo de DSP do projeto é mono — ver o comentário "MONO POR DENTRO" em
// LiveEngine.h. Uma IR estéreo (dois microfones, por exemplo) é reduzida a
// mono pela MÉDIA dos canais: preserva o que os dois têm em comum sem
// favorecer nenhum lado.
//
// Lança std::runtime_error com mensagem clara se o arquivo não existir,
// estiver vazio ou não puder ser lido como .wav — mesma política de erro do
// wav::read (WavFile.h): um carregamento que falha não deve virar silêncio
// sem explicação.
std::vector<float> loadImpulseResponse(const std::string& path);

// A IR junto com a taxa em que foi gravada — loadImpulseResponse() joga
// essa informação fora, e o Cabinet precisa dela para converter a IR para
// a taxa do motor (ver Cabinet::prepare()).
struct ImpulseResponse
{
    std::vector<float> samples;
    double sampleRate = 0.0;
};

ImpulseResponse readImpulseResponse(const std::string& path);

// normalizeImpulseResponseLevel — ajusta o volume de uma IR para que ela,
// em média, não aumente nem diminua o nível da guitarra.
//
// O PROBLEMA
// IRs comerciais vêm normalizadas pelo PICO da amostra (o maior tap perto
// de 1.0), não pelo volume que produzem. Como a convolução SOMA milhares
// de taps, uma IR assim amplifica 12 a 20 dB na faixa da guitarra — a
// Celestion V30 "Cenzo Mix" dá +15 dB entre 100 Hz e 1 kHz e +20 dB em
// 240 Hz. Os amplis do catálogo foram calibrados com as IRs Factory, que
// ficam perto de 0 dB, então uma IR dessas estourava a cadeia.
//
// A REGRA: ENERGIA MÉDIA DE 80 Hz A 5 kHz EM 0 dB
// Mede |H(f)| em pontos a cada 1/12 de oitava entre 80 Hz e 5 kHz (a faixa
// em que a guitarra tem energia) e escala a IR para que a média da
// potência, |H|², dê 1. Duas regras mais simples foram testadas nas IRs
// reais e descartadas:
// - 0 dB em 1 kHz (a regra de tools/generate_factory_irs.py): uma IR com
//   vale em 1 kHz sobe demais — os JCM900 ficavam com +23 dB em 4 kHz.
// - pico da resposta em 0 dB: nunca amplifica, mas uma IR com um pico
//   estreito de presença fica 20 dB mais baixa que as outras.
// Pela média, as IRs Factory mudam menos de 1 dB e as comerciais ficam
// todas no mesmo volume percebido.
//
// Chamada do domínio de controle (ao carregar a IR), nunca no process().
// Uma IR sem energia na faixa (silêncio) é deixada como está.
void normalizeImpulseResponseLevel(std::vector<float>& samples, double sampleRate);
