// Testes do WavFile.
//
// Gravam e leem arquivos de verdade num diretório temporário, porque é
// exatamente a ida ao disco que precisa ser verificada: um parser que só
// funciona com bytes montados na memória não prova nada sobre arquivos reais.
//
// A infra de verificação vive em test_helpers.h.

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "WavFile.h"
#include "test_helpers.h"

namespace
{
// PCM de 16 bits tem passo de 1/32768, cerca de 3e-5. A tolerância padrão de
// 1e-6 é apertada demais para uma ida e volta pelo disco — o erro esperado
// não é bug, é a resolução do formato.
constexpr float kPcm16Tolerance = 1e-4f;

void checkCloseWav(float actual, float expected, const std::string& description)
{
    if (std::fabs(actual - expected) <= kPcm16Tolerance)
    {
        std::cout << "  ok      " << description << "\n";
    }
    else
    {
        std::cout << "  FALHOU  " << description
                  << "  (esperado " << expected << ", obtido " << actual << ")\n";
        ++failures;
    }
}

std::filesystem::path tempPath(const std::string& name)
{
    return std::filesystem::temp_directory_path() / ("ibifx_test_" + name);
}

void removeIfExists(const std::filesystem::path& path)
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}
}

// ---------------------------------------------------------------------

// O que entra é o que sai, dentro da resolução de 16 bits.
void testMonoRoundTrip()
{
    std::cout << "ida e volta mono\n";

    const std::filesystem::path path = tempPath("mono.wav");

    WavFile original;
    original.sampleRate = 44100.0;
    original.channels = {{0.0f, 0.5f, -0.5f, 0.25f, -1.0f, 1.0f}};

    wav::write(path.string(), original);
    const WavFile loaded = wav::read(path.string());

    check(loaded.channelCount() == 1, "voltou com 1 canal");
    check(loaded.frameCount() == 6, "voltou com 6 frames");
    checkClose(static_cast<float>(loaded.sampleRate), 44100.0f, "sample rate preservado");

    checkCloseWav(loaded.channels[0][0], 0.0f, "0.0 preservado");
    checkCloseWav(loaded.channels[0][1], 0.5f, "0.5 preservado");
    checkCloseWav(loaded.channels[0][2], -0.5f, "-0.5 preservado");
    checkCloseWav(loaded.channels[0][3], 0.25f, "0.25 preservado");
    checkCloseWav(loaded.channels[0][5], 1.0f, "1.0 preservado");

    removeIfExists(path);
}

// Estéreo: os canais não podem se misturar na ida nem na volta.
//
// No arquivo as amostras ficam intercaladas (L R L R), e trocar a ordem da
// desintercalação é um erro fácil de cometer e difícil de notar sem teste.
void testStereoRoundTripKeepsChannelsSeparate()
{
    std::cout << "ida e volta estereo\n";

    const std::filesystem::path path = tempPath("stereo.wav");

    WavFile original;
    original.sampleRate = 48000.0;
    original.channels = {{0.1f, 0.2f, 0.3f},
                         {-0.1f, -0.2f, -0.3f}};

    wav::write(path.string(), original);
    const WavFile loaded = wav::read(path.string());

    check(loaded.channelCount() == 2, "voltou com 2 canais");
    check(loaded.frameCount() == 3, "voltou com 3 frames");

    checkCloseWav(loaded.channels[0][0], 0.1f, "esquerdo[0] e 0.1");
    checkCloseWav(loaded.channels[0][2], 0.3f, "esquerdo[2] e 0.3");
    checkCloseWav(loaded.channels[1][0], -0.1f, "direito[0] e -0.1");
    checkCloseWav(loaded.channels[1][2], -0.3f, "direito[2] e -0.3");

    removeIfExists(path);
}

// Amostras fora da faixa são limitadas, e não deixadas transbordar.
//
// Sem o clamp, 2.0 daria a volta no complemento de dois e viraria um valor
// NEGATIVO grande — um estalo violento no lugar de saturação.
void testWriteClampsOutOfRangeSamples()
{
    std::cout << "escrita limita amostras fora da faixa\n";

    const std::filesystem::path path = tempPath("clamp.wav");

    WavFile original;
    original.sampleRate = 44100.0;
    original.channels = {{2.0f, -3.0f, 0.5f}};

    wav::write(path.string(), original);
    const WavFile loaded = wav::read(path.string());

    checkCloseWav(loaded.channels[0][0], 1.0f, "2.0 virou 1.0");
    checkCloseWav(loaded.channels[0][1], -1.0f, "-3.0 virou -1.0");
    checkCloseWav(loaded.channels[0][2], 0.5f, "0.5 passou intacto");

    removeIfExists(path);
}

// Um arquivo vazio de amostras continua sendo um wav válido.
void testEmptyFileIsValid()
{
    std::cout << "arquivo sem amostras\n";

    const std::filesystem::path path = tempPath("empty.wav");

    WavFile original;
    original.sampleRate = 44100.0;
    original.channels = {{}};

    wav::write(path.string(), original);
    const WavFile loaded = wav::read(path.string());

    check(loaded.channelCount() == 1, "tem 1 canal");
    check(loaded.frameCount() == 0, "com zero frames");

    removeIfExists(path);
}

// Duração vem de frames dividido por sample rate.
void testDurationCalculation()
{
    std::cout << "calculo de duracao\n";

    WavFile file;
    file.sampleRate = 100.0;
    file.channels = {std::vector<float>(250, 0.0f)};

    checkClose(static_cast<float>(file.durationSeconds()), 2.5f, "250 frames a 100 Hz sao 2.5 s");

    WavFile vazio;
    checkClose(static_cast<float>(vazio.durationSeconds()), 0.0f, "arquivo vazio dura 0 s");
}

// Arquivo inexistente lança com mensagem clara.
void testMissingFileThrows()
{
    std::cout << "arquivo inexistente\n";

    bool lancou = false;
    try
    {
        wav::read("/definitivamente/nao/existe/arquivo.wav");
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }

    check(lancou, "leitura de arquivo inexistente lanca");
}

// Arquivo que não é RIFF é rejeitado, e não interpretado como ruído.
void testNonRiffFileThrows()
{
    std::cout << "arquivo que nao e wav\n";

    const std::filesystem::path path = tempPath("naowav.bin");

    {
        std::ofstream stream(path, std::ios::binary);
        stream << "isto aqui nao e um arquivo de audio de forma alguma";
    }

    bool lancou = false;
    try
    {
        wav::read(path.string());
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }

    check(lancou, "arquivo sem cabecalho RIFF lanca");

    removeIfExists(path);
}

// Gravar sem canais é erro de programação.
void testWriteWithoutChannelsThrows()
{
    std::cout << "gravar sem canais\n";

    const std::filesystem::path path = tempPath("semcanais.wav");

    WavFile file;
    file.sampleRate = 44100.0;

    bool lancou = false;
    try
    {
        wav::write(path.string(), file);
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }

    check(lancou, "gravar sem canais lanca");

    removeIfExists(path);
}

// Canais de tamanhos diferentes não formam um arquivo coerente.
void testWriteWithMismatchedChannelsThrows()
{
    std::cout << "canais de tamanhos diferentes\n";

    const std::filesystem::path path = tempPath("desiguais.wav");

    WavFile file;
    file.sampleRate = 44100.0;
    file.channels = {{0.1f, 0.2f, 0.3f}, {0.1f}};

    bool lancou = false;
    try
    {
        wav::write(path.string(), file);
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }

    check(lancou, "canais desiguais lancam");

    removeIfExists(path);
}

// Sample rate inválido é recusado na gravação.
void testWriteWithInvalidSampleRateThrows()
{
    std::cout << "sample rate invalido na gravacao\n";

    const std::filesystem::path path = tempPath("taxaruim.wav");

    WavFile file;
    file.sampleRate = 0.0;
    file.channels = {{0.1f}};

    bool lancou = false;
    try
    {
        wav::write(path.string(), file);
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }

    check(lancou, "sample rate zero lanca");

    removeIfExists(path);
}

// O cabeçalho gravado é o RIFF/WAVE canônico de 44 bytes.
//
// Confere os bytes na mão para garantir que o arquivo é reconhecível por
// qualquer tocador, e não apenas pelo nosso próprio leitor.
void testHeaderBytesAreCanonical()
{
    std::cout << "bytes do cabecalho\n";

    const std::filesystem::path path = tempPath("cabecalho.wav");

    WavFile file;
    file.sampleRate = 44100.0;
    file.channels = {{0.0f, 0.0f}};

    wav::write(path.string(), file);

    std::ifstream stream(path, std::ios::binary);
    std::vector<char> header(44, 0);
    stream.read(header.data(), 44);

    check(std::string(header.data(), 4) == "RIFF", "comeca com RIFF");
    check(std::string(header.data() + 8, 4) == "WAVE", "seguido de WAVE");
    check(std::string(header.data() + 12, 4) == "fmt ", "tem o chunk fmt");
    check(std::string(header.data() + 36, 4) == "data", "tem o chunk data");

    // audioFormat = 1 (PCM), no byte 20, little-endian.
    check(header[20] == 1 && header[21] == 0, "formato declarado e PCM");

    // bitsPerSample = 16, no byte 34.
    check(header[34] == 16 && header[35] == 0, "profundidade declarada e 16 bits");

    removeIfExists(path);
}

// writeToMemory()/readFromMemory() fazem a mesma ida e volta que
// write()/read() fazem pelo disco, sem tocar em arquivo nenhum — a Fase 20
// (WebAssembly) depende disso existir, porque o navegador entrega um
// upload como bytes, nunca como um caminho.
void testMemoryRoundTripMatchesFileRoundTrip()
{
    std::cout << "ida e volta em memoria bate com a ida e volta por arquivo\n";

    WavFile original;
    original.sampleRate = 44100.0;
    original.channels = {{0.0f, 0.5f, -0.5f, 0.25f, -1.0f, 1.0f}};

    const std::vector<unsigned char> bytes = wav::writeToMemory(original);
    const WavFile loaded = wav::readFromMemory(bytes);

    check(loaded.channelCount() == 1, "voltou com 1 canal");
    check(loaded.frameCount() == 6, "voltou com 6 frames");
    checkClose(static_cast<float>(loaded.sampleRate), 44100.0f, "sample rate preservado");

    checkCloseWav(loaded.channels[0][1], 0.5f, "0.5 preservado");
    checkCloseWav(loaded.channels[0][4], -1.0f, "-1.0 preservado");
}

// writeToMemory() produz exatamente os mesmos bytes que write() grava em
// disco — não é um formato "de memória" diferente, é o mesmo .wav.
void testWriteToMemoryMatchesWriteToFile()
{
    std::cout << "writeToMemory produz os mesmos bytes que write() grava\n";

    const std::filesystem::path path = tempPath("comparacao.wav");

    WavFile file;
    file.sampleRate = 48000.0;
    file.channels = {{0.1f, -0.2f, 0.3f}};

    wav::write(path.string(), file);
    const std::vector<unsigned char> fromMemory = wav::writeToMemory(file);

    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    const std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);

    std::vector<unsigned char> fromDisk(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(fromDisk.data()), size);

    check(fromMemory == fromDisk, "bytes identicos, byte a byte");

    removeIfExists(path);
}

// readFromMemory() rejeita bytes que não formam um RIFF/WAVE, igual a
// read() rejeita um arquivo assim.
void testReadFromMemoryRejectsNonRiffBytes()
{
    std::cout << "readFromMemory rejeita bytes que nao sao um wav\n";

    const std::vector<unsigned char> lixo = {'n', 'a', 'o', ' ', 'e', ' ', 'w', 'a', 'v'};

    bool lancou = false;
    try
    {
        wav::readFromMemory(lixo);
    }
    catch (const std::runtime_error&)
    {
        lancou = true;
    }

    check(lancou, "bytes sem cabecalho RIFF lancam");
}

// read() por arquivo ainda nomeia o caminho na mensagem de erro de
// formato — não regrediu ao passar a delegar para readFromMemory().
void testReadStillNamesPathOnFormatError()
{
    std::cout << "read() ainda nomeia o caminho no erro de formato\n";

    const std::filesystem::path path = tempPath("naowav2.bin");

    {
        std::ofstream stream(path, std::ios::binary);
        stream << "isto nao e um wav";
    }

    std::string mensagem;
    try
    {
        wav::read(path.string());
    }
    catch (const std::runtime_error& error)
    {
        mensagem = error.what();
    }

    check(mensagem.find(path.string()) != std::string::npos,
          "a mensagem de erro inclui o caminho do arquivo");

    removeIfExists(path);
}

int main()
{
    std::cout << "\n=== testes do WavFile ===\n\n";

    testMonoRoundTrip();
    testStereoRoundTripKeepsChannelsSeparate();
    testWriteClampsOutOfRangeSamples();
    testEmptyFileIsValid();
    testDurationCalculation();
    testMissingFileThrows();
    testNonRiffFileThrows();
    testWriteWithoutChannelsThrows();
    testWriteWithMismatchedChannelsThrows();
    testWriteWithInvalidSampleRateThrows();
    testHeaderBytesAreCanonical();
    testMemoryRoundTripMatchesFileRoundTrip();
    testWriteToMemoryMatchesWriteToFile();
    testReadFromMemoryRejectsNonRiffBytes();
    testReadStillNamesPathOnFormatError();

    return reportResults();
}
