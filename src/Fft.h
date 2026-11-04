#pragma once

#include <complex>
#include <cstddef>
#include <vector>

// Fft — transformada rápida de Fourier, radix-2, iterativa, in-place.
//
// O QUE ELA FAZ
// Converte N amostras no tempo em N "bins" de frequência (e de volta). A
// DFT direta custa N² multiplicações; a FFT reorganiza a mesma conta para
// custar N·log2(N) — para N = 256, 2048 em vez de 65536. É o que torna a
// convolução por blocos do ConvolutionEngine barata.
//
// COMO (COOLEY-TUKEY, RADIX-2)
// Uma DFT de tamanho N é duas DFTs de tamanho N/2 (amostras pares e
// ímpares) combinadas por "borboletas":
//
//     X[k]       = P[k] + W^k · I[k]
//     X[k + N/2] = P[k] - W^k · I[k]        W = e^(-2πj/N)
//
// Aplicado recursivamente até tamanho 1. A versão iterativa faz o mesmo
// sem recursão: primeiro embaralha a entrada na ordem de "bits invertidos"
// (o índice 1 = 001 vai para 100 = 4, quando N = 8), depois faz log2(N)
// passadas de borboletas, com blocos de tamanho 2, 4, 8... até N.
//
// POR QUE prepare() SEPARADO
// Os fatores W^k (twiddles) e a tabela de bits invertidos dependem só de N.
// Calculá-los aloca e usa sin/cos — trabalho do domínio de controle.
// forward()/inverse() só leem essas tabelas: podem rodar na thread de áudio.
//
// N precisa ser potência de 2.
class Fft
{
public:
    using Complex = std::complex<float>;

    // Prepara as tabelas para tamanho size (potência de 2). Aloca.
    void prepare(std::size_t size);

    std::size_t size() const;

    // Transformada direta, no lugar. data.size() precisa ser size().
    void forward(std::vector<Complex>& data) const;

    // Transformada inversa, no lugar, já dividida por N: inverse(forward(x))
    // devolve x.
    void inverse(std::vector<Complex>& data) const;

private:
    void transform(std::vector<Complex>& data, bool inverse) const;

    std::size_t m_size = 0;

    // W^k = e^(-2πjk/N), para k = 0 .. N/2-1.
    std::vector<Complex> m_twiddles;

    // m_bitReversed[i] = i com os log2(N) bits na ordem inversa.
    std::vector<std::size_t> m_bitReversed;
};
