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
