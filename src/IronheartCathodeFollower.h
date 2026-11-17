#pragma once

// V3A/V3B: placa em 410 V, resistor de cátodo de 100k, sem bypass.
// A carga passiva ligada ao cátodo participa da mesma solução não linear.
class IronheartCathodeFollower
{
public:
    struct Load { double conductance = 0.0, historyAmps = 0.0; };
    struct OperatingPoint
    {
        double cathodeVolts = 0.0, plateCurrentAmps = 0.0;
        bool converged = false;
    };
    // A rede de grade anterior define esta polarização DC, em volts.
    void prepare(double gridBiasVolts);
    void reset() noexcept;
    double processSample(double gridACVolts, Load load) noexcept;
    OperatingPoint biasPoint() const noexcept { return m_bias; }
    OperatingPoint operatingPoint() const noexcept { return m_point; }
    double gridBias() const noexcept { return m_gridBias; }

private:
    OperatingPoint solve(double grid, Load load, double initialCathode) const noexcept;
    OperatingPoint m_bias, m_point;
    double m_gridBias = 0.0;
    bool m_prepared = false, m_atRest = true;
};
