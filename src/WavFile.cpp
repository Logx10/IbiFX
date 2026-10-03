#include "WavFile.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace
{
// Códigos de formato do campo audioFormat do chunk fmt.
constexpr std::uint16_t kFormatPcm = 0x0001;
constexpr std::uint16_t kFormatFloat = 0x0003;
constexpr std::uint16_t kFormatExtensible = 0xFFFE;

// Lê o arquivo inteiro para a memória.
//
// Um .wav de guitarra tem alguns megabytes; ler de uma vez simplifica todo o
// resto, porque o parser vira aritmética de índices em vez de uma sequência
// de seeks com estado.
std::vector<unsigned char> readWholeFile(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);

    if (!stream)
    {
        throw std::runtime_error("nao foi possivel abrir '" + path + "' para leitura");
    }

    const std::streamsize size = stream.tellg();

    if (size <= 0)
    {
        throw std::runtime_error("'" + path + "' esta vazio");
    }

    stream.seekg(0, std::ios::beg);

    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));

    if (!stream.read(reinterpret_cast<char*>(bytes.data()), size))
    {
        throw std::runtime_error("falha ao ler '" + path + "'");
    }

    return bytes;
}

// Montagem byte a byte, do menos significativo para o mais significativo.
// É o que torna a leitura independente da arquitetura da máquina.
std::uint16_t readU16(const std::vector<unsigned char>& bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

std::uint32_t readU32(const std::vector<unsigned char>& bytes, std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset])
         | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
         | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
         | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

bool tagEquals(const std::vector<unsigned char>& bytes, std::size_t offset, const char* tag)
{
    return bytes[offset] == static_cast<unsigned char>(tag[0])
        && bytes[offset + 1] == static_cast<unsigned char>(tag[1])
        && bytes[offset + 2] == static_cast<unsigned char>(tag[2])
        && bytes[offset + 3] == static_cast<unsigned char>(tag[3]);
}

// Converte uma amostra do formato do arquivo para float em [-1, +1].
//
// Cada profundidade tem seu divisor: o maior valor positivo representável.
// Para 16 bits inteiros isso é 32768, para 24 bits 8388608, e assim por
// diante — sempre 2^(bits-1).
float sampleToFloat(const std::vector<unsigned char>& bytes,
                    std::size_t offset,
                    std::uint16_t bitsPerSample,
                    bool isFloat)
{
    if (isFloat)
    {
        // Float de 32 bits já está na escala certa; só é preciso reinterpretar
        // os quatro bytes. memcpy é a forma correta de fazer isso em C++:
        // ler por ponteiro convertido violaria as regras de aliasing.
        const std::uint32_t raw = readU32(bytes, offset);
        float value = 0.0f;
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    }

    if (bitsPerSample == 16)
    {
        const std::int16_t raw = static_cast<std::int16_t>(readU16(bytes, offset));
        return static_cast<float>(raw) / 32768.0f;
    }

    if (bitsPerSample == 24)
    {
        // 24 bits não tem tipo próprio em C++. Montamos nos 24 bits altos de
        // um int32 e deslocamos de volta: o deslocamento aritmético replica o
        // bit de sinal, o que estende o negativo corretamente.
        const std::int32_t raw = static_cast<std::int32_t>(
            (static_cast<std::uint32_t>(bytes[offset]) << 8)
            | (static_cast<std::uint32_t>(bytes[offset + 1]) << 16)
            | (static_cast<std::uint32_t>(bytes[offset + 2]) << 24));

        return static_cast<float>(raw >> 8) / 8388608.0f;
    }

    if (bitsPerSample == 32)
    {
        const std::int32_t raw = static_cast<std::int32_t>(readU32(bytes, offset));
        return static_cast<float>(raw) / 2147483648.0f;
    }

    throw std::runtime_error("profundidade de amostra nao suportada: "
                             + std::to_string(bitsPerSample) + " bits");
}

void appendU16(std::vector<unsigned char>& out, std::uint16_t value)
{
    out.push_back(static_cast<unsigned char>(value & 0xFF));
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
}

void appendU32(std::vector<unsigned char>& out, std::uint32_t value)
{
    out.push_back(static_cast<unsigned char>(value & 0xFF));
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
    out.push_back(static_cast<unsigned char>((value >> 16) & 0xFF));
    out.push_back(static_cast<unsigned char>((value >> 24) & 0xFF));
}

void appendTag(std::vector<unsigned char>& out, const char* tag)
{
    for (int i = 0; i < 4; ++i)
    {
        out.push_back(static_cast<unsigned char>(tag[i]));
    }
}
}

std::size_t WavFile::channelCount() const
{
    return channels.size();
}

std::size_t WavFile::frameCount() const
{
    return channels.empty() ? 0 : channels.front().size();
}

double WavFile::durationSeconds() const
{
    if (sampleRate <= 0.0)
    {
        return 0.0;
    }

    return static_cast<double>(frameCount()) / sampleRate;
}

namespace wav
{
WavFile readFromMemory(const std::vector<unsigned char>& bytes)
{
    if (bytes.size() < 12)
    {
        throw std::runtime_error("wav curto demais (menos de 12 bytes)");
    }

    if (!tagEquals(bytes, 0, "RIFF") || !tagEquals(bytes, 8, "WAVE"))
    {
        throw std::runtime_error("nao e um arquivo RIFF/WAVE valido");
    }

    std::uint16_t audioFormat = 0;
    std::uint16_t channelCount = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;

    bool sawFormat = false;
    std::size_t dataOffset = 0;
    std::size_t dataSize = 0;
    bool sawData = false;

    // Percorre os chunks a partir do byte 12, pulando os que não interessam.
    // Editores costumam inserir metadados entre fmt e data, então assumir
    // posições fixas quebraria com arquivos perfeitamente válidos.
    std::size_t offset = 12;

    while (offset + 8 <= bytes.size())
    {
        const std::size_t chunkSize = readU32(bytes, offset + 4);
        const std::size_t contentOffset = offset + 8;

        if (tagEquals(bytes, offset, "fmt "))
        {
            if (contentOffset + 16 > bytes.size())
            {
                throw std::runtime_error("chunk fmt truncado");
            }

            audioFormat = readU16(bytes, contentOffset);
            channelCount = readU16(bytes, contentOffset + 2);
            sampleRate = readU32(bytes, contentOffset + 4);
            bitsPerSample = readU16(bytes, contentOffset + 14);

            // WAVE_FORMAT_EXTENSIBLE guarda o formato real num GUID ao fim do
            // chunk. Os dois primeiros bytes desse GUID repetem o código
            // clássico, então basta lê-los para saber se é PCM ou float.
            if (audioFormat == kFormatExtensible && contentOffset + 26 <= bytes.size())
            {
                audioFormat = readU16(bytes, contentOffset + 24);
            }

            sawFormat = true;
        }
        else if (tagEquals(bytes, offset, "data"))
        {
            dataOffset = contentOffset;
            dataSize = std::min(chunkSize, bytes.size() - contentOffset);
            sawData = true;
        }

        // Chunks têm tamanho par: um de tamanho ímpar leva um byte de
        // preenchimento que não é contado no campo de tamanho.
        offset = contentOffset + chunkSize + (chunkSize % 2);
    }

    if (!sawFormat)
    {
        throw std::runtime_error("wav sem chunk fmt");
    }

    if (!sawData)
    {
        throw std::runtime_error("wav sem chunk data");
    }

    if (audioFormat != kFormatPcm && audioFormat != kFormatFloat)
    {
        throw std::runtime_error("formato comprimido ou desconhecido (codigo "
                                 + std::to_string(audioFormat) + "); apenas PCM e float sao suportados");
    }

    if (channelCount == 0)
    {
        throw std::runtime_error("wav declara zero canais");
    }

    if (sampleRate == 0)
    {
        throw std::runtime_error("wav declara sample rate zero");
    }

    const bool isFloat = audioFormat == kFormatFloat;

    if (isFloat && bitsPerSample != 32)
    {
        throw std::runtime_error("float de " + std::to_string(bitsPerSample)
                                 + " bits nao suportado; apenas 32");
    }

    if (!isFloat && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32)
    {
        throw std::runtime_error("PCM de " + std::to_string(bitsPerSample)
                                 + " bits nao suportado; apenas 16, 24 e 32");
    }

    const std::size_t bytesPerSample = bitsPerSample / 8u;
    const std::size_t bytesPerFrame = bytesPerSample * channelCount;
    const std::size_t frameCount = bytesPerFrame == 0 ? 0 : dataSize / bytesPerFrame;

    WavFile file;
    file.sampleRate = static_cast<double>(sampleRate);
    file.channels.assign(channelCount, std::vector<float>(frameCount, 0.0f));

    // As amostras no arquivo vêm intercaladas (L R L R...); aqui elas são
    // separadas por canal, que é o formato com que os módulos trabalham.
    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        for (std::size_t channel = 0; channel < channelCount; ++channel)
        {
            const std::size_t position = dataOffset
                                       + frame * bytesPerFrame
                                       + channel * bytesPerSample;

            file.channels[channel][frame] = sampleToFloat(bytes, position, bitsPerSample, isFloat);
        }
    }

    return file;
}

WavFile read(const std::string& path)
{
    const std::vector<unsigned char> bytes = readWholeFile(path);

    // readWholeFile() já lança com o caminho na mensagem (abrir falhou,
    // arquivo vazio); aqui só falta dar esse mesmo contexto para os erros
    // de FORMATO, que readFromMemory() não tem como nomear sozinha — ela
    // nunca viu um caminho, só bytes.
    try
    {
        return readFromMemory(bytes);
    }
    catch (const std::runtime_error& error)
    {
        throw std::runtime_error("'" + path + "': " + error.what());
    }
}

std::vector<unsigned char> writeToMemory(const WavFile& file)
{
    if (file.channels.empty())
    {
        throw std::runtime_error("nao ha canais para gravar");
    }

    if (file.sampleRate <= 0.0)
    {
        throw std::runtime_error("sample rate invalido");
    }

    const std::size_t frameCount = file.frameCount();

    for (const std::vector<float>& channel : file.channels)
    {
        if (channel.size() != frameCount)
        {
            throw std::runtime_error("os canais tem tamanhos diferentes");
        }
    }

    const std::uint16_t channelCount = static_cast<std::uint16_t>(file.channels.size());
    const std::uint16_t bitsPerSample = 16;
    const std::uint16_t blockAlign = static_cast<std::uint16_t>(channelCount * bitsPerSample / 8);
    const std::uint32_t sampleRate = static_cast<std::uint32_t>(std::lround(file.sampleRate));
    const std::uint32_t byteRate = sampleRate * blockAlign;
    const std::uint32_t dataSize = static_cast<std::uint32_t>(frameCount * blockAlign);

    std::vector<unsigned char> out;
    out.reserve(44 + dataSize);

    appendTag(out, "RIFF");
    appendU32(out, 36 + dataSize);   // tamanho do resto do arquivo
    appendTag(out, "WAVE");

    appendTag(out, "fmt ");
    appendU32(out, 16);              // tamanho do chunk fmt para PCM simples
    appendU16(out, kFormatPcm);
    appendU16(out, channelCount);
    appendU32(out, sampleRate);
    appendU32(out, byteRate);
    appendU16(out, blockAlign);
    appendU16(out, bitsPerSample);

    appendTag(out, "data");
    appendU32(out, dataSize);

    for (std::size_t frame = 0; frame < frameCount; ++frame)
    {
        for (std::size_t channel = 0; channel < channelCount; ++channel)
        {
            // O formato inteiro não representa valores fora de [-1, +1].
            // Deixar transbordar daria a volta no complemento de dois e
            // produziria estalo violento, então limitamos na borda.
            const float clamped = std::clamp(file.channels[channel][frame], -1.0f, 1.0f);

            // 32767 e não 32768: é o maior positivo representável em 16 bits.
            const std::int16_t value = static_cast<std::int16_t>(std::lround(clamped * 32767.0f));

            appendU16(out, static_cast<std::uint16_t>(value));
        }
    }

    return out;
}

void write(const std::string& path, const WavFile& file)
{
    // writeToMemory() pode lançar por causa dos DADOS (canais vazios, de
    // tamanhos diferentes, sample rate inválido) — nada disso tem relação
    // com o caminho, então a mensagem dela já basta sem precisar
    // adicionar contexto aqui, diferente do que read() faz para os erros
    // de formato de readFromMemory().
    const std::vector<unsigned char> bytes = writeToMemory(file);

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);

    if (!stream)
    {
        throw std::runtime_error("nao foi possivel abrir '" + path + "' para escrita");
    }

    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

    if (!stream)
    {
        throw std::runtime_error("falha ao gravar '" + path + "'");
    }
}
}
