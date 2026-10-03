#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// BackingTrackPlayer — toca um arquivo de apoio sincronizado ao relógio
// compartilhado (Fase 16: "Load, play, seek, loop A/B").
//
// POR QUE PLAY/SEEK/LOOP NÃO APARECEM AQUI
// Já existem no MasterTransport (Fase 14), e não por acaso: o motivo de
// construir o relógio compartilhado ANTES das backing tracks era
// justamente este — qualquer consumidor que leia a posição do transport
// ganha play/stop/seek/loop de graça, sem reimplementar nada. Esta classe
// não guarda nem referencia um MasterTransport: ela só faz duas coisas que
// ele não pode fazer por ela — carregar um arquivo e, dada uma posição
// (que quem chama já leu de algum transport), dizer qual amostra dele
// toca ali. Isso também a deixa testável sem precisar montar um transport
// de verdade.
//
// Se o transport de quem chama estiver com loop ligado entre os pontos A
// e B, quando a posição dele voltar para A (ver o comentário de
// MasterTransport::advance() sobre preservar o excesso por módulo), basta
// chamar process() de novo com essa posição — é o "loop A/B" do título da
// fase, resolvido por composição, não por código novo aqui.
//
// MONO, PELO MESMO MOTIVO DO LiveEngine
// A cadeia inteira do IbiFX é mono (ver o comentário em LiveEngine.h);
// uma backing track estéreo é reduzida à média dos canais no load(), para
// poder ser somada ao mesmo buffer mono de todo o resto.
//
// SEM RESAMPLE
// O arquivo precisa estar no MESMO sample rate em que o motor está
// rodando — load() lança se não estiver. Reamostrar é um projeto à parte
// (qualidade do filtro, custo de CPU), e nenhuma outra parte do IbiFX faz
// isso hoje (nem o processamento de arquivo, nem o Cabinet); seria
// inconsistente a backing track ser a exceção silenciosa que muda o pitch
// de quem esquecer de exportar na taxa certa.
class BackingTrackPlayer
{
public:
    BackingTrackPlayer() = default;

    // Define o sample rate do motor, usado para conferir contra o do
    // arquivo em load().
    void prepare(double sampleRate);

    // Lê um .wav e guarda como mono. Lança std::runtime_error se o arquivo
    // não puder ser lido (mensagem de wav::read) ou se o sample rate dele
    // não bater com o do motor (ver o comentário da classe sobre resample).
    void load(const std::string& path);

    bool isLoaded() const;
    std::uint64_t lengthSamples() const;
    double lengthSeconds() const;

    void setVolume(float volume);
    float volume() const;

    // Soma o trecho do arquivo correspondente à posição informada ao
    // buffer. startPositionSamples é a posição, no relógio de QUALQUER
    // transport que quem chama esteja usando, do PRIMEIRO frame deste
    // buffer — mesmo contrato do Metronome::process(). Depois do fim do
    // arquivo, sem loop que traga de volta, fica em silêncio — não trava,
    // não repete a última amostra, não lança.
    void process(std::vector<float>& buffer, std::uint64_t startPositionSamples);

private:
    double m_sampleRate = 48000.0;
    std::vector<float> m_samples;
    float m_volume = 1.0f;
};
