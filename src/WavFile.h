#pragma once

#include <cstddef>
#include <string>
#include <vector>

// WavFile — leitura e escrita de arquivos .wav, sem dependência externa.
//
// POR QUE ISTO EXISTE
// Tudo construído até aqui foi verificado lendo números no terminal. Dá para
// provar que o soft clipping preserva a hierarquia entre picos e que o delay
// devolve o impulso na amostra certa — mas não dá para ouvir nenhum dos dois.
// O próprio AI_GUIDELINES lembra que testes não substituem audição.
//
// Ler e escrever .wav fecha essa lacuna: o áudio entra por um arquivo, passa
// pela cadeia de módulos e sai por outro, pronto para ser tocado.
//
// POR QUE ESCREVER O PARSER À MÃO
// O formato é simples o bastante para caber em algumas centenas de linhas, e
// o §11 do guia pede que dependências sejam discutidas antes de entrar. Uma
// biblioteca resolveria mais casos, mas esconderia justamente o que vale
// aprender aqui: como bytes viram amostras.
//
// O FORMATO RIFF, EM UM PARÁGRAFO
// Um .wav é uma sequência de blocos ("chunks"), cada um com uma etiqueta de
// 4 letras e um tamanho. O arquivo inteiro é um chunk "RIFF" cujo conteúdo
// começa com a palavra "WAVE", seguida dos blocos internos:
//
//     R I F F | tamanho | W A V E
//                         f m t ' ' | tamanho | formato, canais, taxa...
//                         d a t a   | tamanho | amostras, intercaladas
//
// Pode haver outros blocos (metadados, marcadores, lixo de editor) entre os
// dois que importam, então o parser precisa pular o que não reconhece em vez
// de assumir posições fixas.
//
// TUDO É LITTLE-ENDIAN
// Números no .wav são gravados com o byte menos significativo primeiro,
// independente da máquina. Os leitores aqui montam cada valor byte a byte,
// em vez de copiar a memória direto — assim o código funciona igual em
// qualquer arquitetura.
//
// CANAIS SEPARADOS, E NÃO INTERCALADOS
// No arquivo as amostras vêm alternadas (L R L R L R...). Aqui elas são
// separadas em um vetor por canal, porque é assim que os módulos processam:
// cada process() recebe um buffer de um canal só.
//
// O QUE É SUPORTADO
// Leitura:  PCM de 16, 24 e 32 bits, e float de 32 bits.
// Escrita:  PCM de 16 bits, o formato mais universal.
//
// Formatos fora dessa lista — comprimidos, 8 bits, float de 64 — geram erro
// explícito dizendo o que foi encontrado, em vez de devolver ruído.
struct WavFile
{
    // Uma entrada por canal; cada uma com uma amostra por frame.
    std::vector<std::vector<float>> channels;

    double sampleRate = 0.0;

    // Quantidade de canais.
    std::size_t channelCount() const;

    // Quantidade de frames, ou seja, de amostras por canal.
    std::size_t frameCount() const;

    // Duração em segundos.
    double durationSeconds() const;
};

namespace wav
{
// Interpreta um .wav que já está em memória — o parser de verdade.
//
// read() é só esta função mais a leitura do arquivo; existe separada
// porque nem todo frontend tem um caminho de arquivo pra oferecer. Uma
// página no navegador recebe um upload como bytes (um ArrayBuffer), nunca
// como um caminho de disco — é o passo 2 do plano de portabilidade do
// ADR 0001 (docs/adr/0001-portabilidade-desktop-e-web.md), que a Fase 20
// (WebAssembly) depende dele para existir.
//
// Lança std::runtime_error com mensagem descritiva se os bytes não
// formarem um RIFF/WAVE válido, estiverem truncados ou usarem um formato
// de amostra fora da lista suportada.
WavFile readFromMemory(const std::vector<unsigned char>& bytes);

// Lê um arquivo .wav do disco. Atalho para readFromMemory() mais a
// leitura do arquivo — lança com o caminho na mensagem tanto para a
// leitura em si (arquivo não existe, sem permissão) quanto para um
// formato inválido, para manter o diagnóstico completo de antes.
WavFile read(const std::string& path);

// Codifica em PCM de 16 bits, devolvendo os bytes prontos em vez de
// gravá-los em disco — mesmo motivo de readFromMemory(): um frontend sem
// caminho de arquivo (o navegador) ainda precisa do .wav pronto, só que
// como bytes para entregar a quem pediu (por exemplo, um link de
// download).
//
// Amostras fora de [-1, +1] são limitadas na borda: o formato inteiro não
// tem como representá-las, e deixar transbordar produziria estalo violento
// em vez de saturação. Lança se os canais tiverem tamanhos diferentes ou
// se o sample rate for inválido.
std::vector<unsigned char> writeToMemory(const WavFile& file);

// Escreve um .wav PCM de 16 bits no disco. Atalho para writeToMemory()
// mais a escrita do arquivo.
void write(const std::string& path, const WavFile& file);
}
