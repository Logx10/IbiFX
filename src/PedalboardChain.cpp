#include "PedalboardChain.h"

#include <memory>

#include "AsymmetricClipper.h"
#include "Cabinet.h"
#include "Compressor.h"
#include "Delay.h"
#include "GainProcessor.h"
#include "HighPassFilter.h"
#include "Limiter.h"
#include "NoiseGate.h"
#include "Preamp.h"
#include "PowerAmp.h"
#include "Reverb.h"
#include "SoftClipper.h"
#include "ToneStack.h"

// Monta a cadeia — um pedal de drive seguido de eco.
//
// A ordem é a clássica de pedaleira: a distorção vem ANTES do delay, para que
// os ecos repitam o som já distorcido. Invertida, o delay produziria ecos
// limpos que depois seriam distorcidos juntos, e o resultado vira uma pasta.
void buildChain(ModuleChain& chain, const ChainSettings& settings)
{
    // O gate vem ANTES de tudo, inclusive do filtro. Ruído de fundo entra
    // junto com o sinal, e precisa ser cortado antes de qualquer estágio que
    // amplifique (Gain, SoftClipper) — depois deles, o próprio ruído já
    // amplificado pode passar do threshold e o gate deixa de enxergá-lo como
    // ruído.
    if (settings.useGate)
    {
        auto gate = std::make_unique<NoiseGate>();
        gate->setThreshold(settings.gateThreshold);
        gate->setRelease(settings.gateRelease);
        chain.add(std::move(gate));
    }

    // O compressor vem logo depois do gate, e ANTES de qualquer distorção —
    // é a ordem clássica de pedaleira: nivelar a dinâmica da corda primeiro,
    // pra depois a distorção reagir de forma mais previsível a ela. Feito ao
    // contrário, o compressor reagiria ao sinal já distorcido, que tem uma
    // dinâmica bem mais achatada — e comprimir algo já achatado faz pouco.
    if (settings.useCompressor)
    {
        auto compressor = std::make_unique<Compressor>();
        compressor->setThreshold(settings.compThreshold);
        compressor->setRatio(settings.compRatio);
        compressor->setAttack(settings.compAttack);
        compressor->setRelease(settings.compRelease);
        chain.add(std::move(compressor));
    }

    // O filtro vem em seguida, antes de qualquer ganho ou distorção.
    //
    // Saturação mistura as frequências que entram, e grave forte ocupa a
    // curva inteira do saturador, empastando tudo que vem junto. Cortar o
    // grave depois não conserta: a mistura já aconteceu. É a mesma ordem que
    // todo amplificador de guitarra usa.
    if (settings.useHighPass)
    {
        auto filter = std::make_unique<HighPassFilter>();
        filter->setFrequency(settings.highPass);
        chain.add(std::move(filter));
    }

    auto gain = std::make_unique<GainProcessor>();
    gain->setGain(settings.gain);
    chain.add(std::move(gain));

    if (settings.useDrive)
    {
        // --preamp e --asymmetric trocam a curva, não acrescentam uma
        // segunda distorção: os três fazem o mesmo papel na cadeia (um
        // estágio de saturação), e empilhar mais de um por padrão mudaria o
        // tom sem ninguém ter pedido.
        if (settings.usePreamp)
        {
            auto drive = std::make_unique<Preamp>();
            drive->setDrive(settings.preampDrive);
            chain.add(std::move(drive));
        }
        else if (settings.useAsymmetric)
        {
            auto drive = std::make_unique<AsymmetricClipper>();
            drive->setDrive(settings.drive);
            drive->setBias(settings.bias);
            chain.add(std::move(drive));
        }
        else
        {
            auto drive = std::make_unique<SoftClipper>();
            drive->setDrive(settings.drive);
            chain.add(std::move(drive));
        }
    }

    // Preamp -> Tone Stack -> Power Amp e a ordem classica de um ampli de
    // guitarra (AI_GUIDELINES §33) — o tone stack vem DEPOIS da distorcao,
    // porque no circuito real ele fica entre os estagios de preamplificacao
    // e o phase splitter, nunca antes.
    if (settings.useToneStack)
    {
        auto toneStack = std::make_unique<ToneStack>();
        toneStack->setBass(settings.toneBass);
        toneStack->setMid(settings.toneMid);
        toneStack->setTreble(settings.toneTreble);
        chain.add(std::move(toneStack));
    }

    if (settings.useDelay)
    {
        auto echo = std::make_unique<Delay>();
        echo->setTime(settings.delayTime);
        echo->setFeedback(settings.feedback);
        echo->setMix(settings.mix);
        chain.add(std::move(echo));
    }

    // O reverb vem por último entre os efeitos, depois do delay — molha o
    // eco discreto do delay numa cauda contínua, em vez do contrário (o que
    // faria o delay soar como repetições dentro de uma sala, menos definido).
    if (settings.useReverb)
    {
        auto reverb = std::make_unique<Reverb>();
        reverb->setDecay(settings.reverbDecay);
        reverb->setDamping(settings.reverbDamping);
        reverb->setMix(settings.reverbMix);
        chain.add(std::move(reverb));
    }

    // Power amp vem por ultimo entre os estagios de amplificacao, depois de
    // tudo o mais ja ter formado o sinal — e o ultimo estagio de verdade
    // antes da saida, tanto na cadeia quanto num ampli real.
    if (settings.usePowerAmp)
    {
        auto powerAmp = std::make_unique<PowerAmp>();
        powerAmp->setDrive(settings.powerAmpDrive);
        powerAmp->setSag(settings.powerAmpSag);
        chain.add(std::move(powerAmp));
    }

    // Cabinet vem depois do power amp: e o que aconteceria fisicamente
    // DEPOIS do sinal eletrico sair do estagio de saida — o alto-falante,
    // o ar da sala, o microfone. Carrega a IR aqui, no dominio de
    // controle, nunca dentro de um process().
    if (!settings.cabinetIRPath.empty())
    {
        auto cabinet = std::make_unique<Cabinet>();
        cabinet->loadImpulseResponseFile(settings.cabinetIRPath);
        cabinet->setMix(settings.cabinetMix);
        chain.add(std::move(cabinet));
    }

    // Sempre por último, e sem "use": não é uma cor de pedal que se liga ou
    // desliga, é a garantia de que nada que sair daqui passa de 1.0 — vale
    // tanto para a cadeia cheia quanto para qualquer subconjunto dela.
    chain.add(std::make_unique<Limiter>());
}

void buildDefaultChain(ModuleChain& chain)
{
    buildChain(chain, ChainSettings{});
}
