#pragma once

// Estágio ECC83 do IRT120H. Padrão: V1A, R18=100k/R40=1k5/C18=1uF.
// Resolve corrente de placa e polarização dinâmica, incluindo uma carga
// Norton na placa. Corrente de grade e Miller ainda não incluídos.
// O pré possui este objeto; somente sua thread de áudio acessa os estados.
class IronheartTriodeStage
{
public:
    struct Circuit
    {
        double supplyVolts = 383.0;
        double plateResistance = 100e3;
        double cathodeResistance = 1.5e3;
        double cathodeCapFarads = 1e-6;
        double plateDCConductance = 0.0;
    };
    struct OperatingPoint
    {
        double plateVolts = 0.0;
        double cathodeVolts = 0.0;
        double plateCurrentAmps = 0.0;
        bool converged = false;
    };
    // Corrente saindo da placa: G * (Va - VaRepouso) - historyAmps.
    struct PlateLoad
    {
        double conductance = 0.0;
        double historyAmps = 0.0;
    };

    // Fora do callback: valida taxa/componentes e resolve o repouso DC,
    // com o capacitor de cátodo aberto e incluindo a carga resistiva da placa.
    void prepare(double internalSampleRate);
    void prepare(double internalSampleRate, Circuit circuit);
    void reset() noexcept;
    // Entrada em volts da grade para terra; saída em volts AC da placa.
    double processSample(double gridVolts) noexcept;
    double processSample(double gridVolts, PlateLoad load) noexcept;

    OperatingPoint biasPoint() const noexcept { return m_bias; }
    OperatingPoint operatingPoint() const noexcept { return m_point; }
    // Magnitude do ganho linear com cátodo totalmente bypassado, para
    // diagnóstico. Não normaliza o sinal de áudio.
    double bypassedGain() const noexcept { return m_bypassedGain; }

    // Lei de corrente compartilhada pelos estágios de ganho e seguidores.
    // Função pura; entradas são tensões relativas ao cátodo.
    struct Current
    {
        double amps = 0.0;
        double plateDerivative = 0.0;
        double gridDerivative = 0.0;
    };
    static Current evaluate(double plateCathode, double gridCathode) noexcept;

private:
    static OperatingPoint solve(double grid, double conductance, double history,
                                double initialCurrent, double plateSource,
                                double plateResistance) noexcept;

    OperatingPoint m_bias;
    OperatingPoint m_point;
    double m_capConductance = 0.0;
    double m_history = 0.0;
    double m_bypassedGain = 1.0;
    bool m_atRest = true;
    Circuit m_circuit;
};
