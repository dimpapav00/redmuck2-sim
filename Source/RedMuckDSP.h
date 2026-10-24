#pragma once
// RedMuck2 Sim - DSP core (Big Muff style fuzz with a front-end Boost and switchable clipping).
//
// Public information used: Muff-style fuzz inspired by the '71 "triangle" Big Muff and the later Sovtek
// "Civil War" Muff; controls Gain, Tone, Level; a Boost footswitch driving a dedicated section "tuned to hit
// the front end" (internal trimmer sets its level); a 2-way clipping switch: symmetrical or asymmetrical "+".
// Component values are NOT published, so the numbers below are informed approximations taken from the
// well-known Muff topology. This is a behavioural model, not a schematic-exact copy.
//
//   in -> Boost section -> input booster -> Gain -> clipping stage 1 -> clipping stage 2
//      -> tone stack (low-pass / high-pass blend + mid fill) -> recovery + Level -> out
//
// Each clipping stage is Rf || Cf || diodes in the feedback path of a gain stage, solved per sample
// (backward Euler + safeguarded Newton). Asymmetrical mode puts one diode on one polarity and two in series
// on the other.

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace redmuck
{
constexpr float kTwoPi = 6.283185307179586f;

struct Params
{
    float inputDb    = 0.0f;   // host-side input trim, -24..+24
    float gain       = 0.6f;   // 0..1
    float tone       = 0.5f;   // 0..1  0 = bass (low-pass), 1 = treble (high-pass)
    float level      = 0.5f;   // 0..1  (0.5 ~ unity-ish)
    float boostLevel = 0.5f;   // 0..1  internal trimmer: 0 = 0 dB, 1 = +24 dB
    float outputDb   = 0.0f;   // host-side trim
    bool  on         = true;
    bool  boost      = false;
    int   clip       = 0;      // 0 symmetrical, 1 asymmetrical "+"
};

struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float fc, float fs) { a = 1.0f - std::exp (-kTwoPi * fc / fs); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
    void reset() { z = 0.0f; }
};

struct Coeffs
{
    float inGain = 1, boostGain = 1, sustainAtt = 1, volGain = 1, outGain = 1, onMix = 1, tone = 0.5f, norm = 1;
    bool asym = false;
};

// One Muff clipping stage. Feedback network: Rf (100k) || Cf (~500 pF) || diodes (silicon).
class ClipStage
{
public:
    void prepare (float fsOS, float hpHz, float gain)
    {
        hp.setCutoff (hpHz, fsOS);
        K = (double) gain;
        dt = 1.0 / (double) fsOS;
        reset();
    }
    void reset() { hp.reset(); vPrev = 0.0; }

    float process (float x, bool asym)
    {
        const double I = (double) hp.hp (x) * K / Rf;      // current into the virtual summing node
        const double Cdt = Cf / dt;
        const double a = nVt;                              // positive polarity: one diode
        const double b = asym ? 2.0 * nVt : nVt;           // negative polarity: two in series when asymmetrical

        // Implicit equation: g(V) = (V - vPrev)/dt*Cf + V/Rf + Is*(exp(V/a) - exp(-V/b)) - I = 0.
        // g is monotone and g(0) = -Jp, so the root has the sign of Jp = I + vPrev*Cf/dt. Solve for its
        // magnitude W on the conducting polarity: there the diode term is convex, and starting from an upper
        // bound of the root makes Newton converge monotonically (no bisection, no overshoot).
        const double Jp = I + vPrev * Cdt;
        const bool positive = Jp >= 0.0;
        const double aJ = std::fabs (Jp);
        const double c1 = positive ? a : b;   // diode(s) conducting in this direction
        const double c2 = positive ? b : a;   // opposite direction
        const double lin = Cdt + 1.0 / Rf;

        double W = c1 * std::log1p (aJ / Is);  // upper bound: ignores the linear terms
        for (int i = 0; i < 10; ++i)
        {
            const double e1 = std::exp (std::min (W / c1, 40.0));
            const double e2 = std::exp (-W / c2);
            const double g = W * lin + Is * (e1 - e2) - aJ;
            const double gp = lin + Is * (e1 / c1 + e2 / c2);
            const double dW = g / gp;
            W = std::max (0.0, W - dW);
            if (std::fabs (dW) < 1.0e-10) break;
        }
        vPrev = positive ? W : -W;
        return (float) vPrev;
    }

private:
    static constexpr double Rf = 100000.0;
    static constexpr double Cf = 500.0e-12;   // 470 pF + 27 pF class values
    static constexpr double Is = 2.5e-9, nVt = 0.0452;
    OnePole hp;
    double K = 10.0, dt = 1.0 / 176400.0, vPrev = 0.0;
};

class Channel
{
public:
    void prepare (float fsOS)
    {
        dcIn.setCutoff (10.0f, fsOS);
        stage1.prepare (fsOS, 50.0f, 15.0f);   // slightly lower coupling corners: "fatter low end"
        stage2.prepare (fsOS, 80.0f, 8.0f);
        toneLP.setCutoff (408.0f, fsOS);       // 39k + 10 nF
        toneHP.setCutoff (1855.0f, fsOS);      // 3.9 nF + 22k
        midA.setCutoff (1500.0f, fsOS);
        midB.setCutoff (350.0f, fsOS);
        outHP.setCutoff (15.0f, fsOS);
        reset();
    }

    void reset()
    {
        for (auto* f : { &dcIn, &toneLP, &toneHP, &midA, &midB, &outHP })
            f->reset();
        stage1.reset();
        stage2.reset();
    }

    float process (float x, const Coeffs& c)
    {
        const float dry = dcIn.hp (x);
        float v = dry * c.inGain;

        // Boost section: its own clean-ish gain stage in front of the fuzz
        v = 8.0f * std::tanh (v * c.boostGain * (1.0f / 8.0f));

        v = 4.0f * v;                                           // input booster
        v = 4.0f * std::tanh (v * 0.25f);                       // supply-limited
        v *= c.sustainAtt;                                      // Gain
        v = stage1.process (v, c.asym);
        v = stage2.process (v, c.asym) * c.norm;

        // tone stack: low-pass / high-pass blend, plus a fixed mid fill ("pronounced midrange")
        const float lo = toneLP.lp (v);
        const float hi = toneHP.hp (v);
        const float mid = midA.lp (v) - midB.lp (v);
        float y = (1.0f - c.tone) * lo + c.tone * hi + 0.2f * mid;

        y *= 3.0f;                                              // recovery stage
        y = outHP.hp (y) * c.volGain * c.outGain;

        const float out = dry + c.onMix * (y - dry);            // footswitch crossfade
        if (! std::isfinite (out))
        {
            reset();
            return 0.0f;
        }
        return std::clamp (out, -8.0f, 8.0f);
    }

private:
    OnePole dcIn, toneLP, toneHP, midA, midB, outHP;
    ClipStage stage1, stage2;
};

class Engine
{
public:
    static constexpr int kChunk = 32;

    void prepare (double sampleRate)
    {
        fsOS = (float) sampleRate * 4.0f;
        os.reset();
        os.initProcessing ((size_t) kChunk);
        for (auto& c : channels)
            c.prepare (fsOS);
        for (auto* s : { &smIn, &smGain, &smTone, &smLevel, &smBoostLvl, &smBoostOn, &smOut, &smOn })
            s->reset (sampleRate, 0.03);
        snap = true;
    }

    void reset()
    {
        os.reset();
        for (auto& c : channels) c.reset();
        snap = true;
    }

    int getLatencySamples() const { return (int) std::lround (os.getLatencyInSamples()); }

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        const int numCh = std::min (buffer.getNumChannels(), 2);
        const int n = buffer.getNumSamples();
        if (numCh == 0 || n == 0) return;

        auto set = [this] (juce::SmoothedValue<float>& s, float v)
        {
            if (snap) s.setCurrentAndTargetValue (v); else s.setTargetValue (v);
        };
        set (smIn, p.inputDb);   set (smGain, p.gain);   set (smTone, p.tone);
        set (smLevel, p.level);  set (smBoostLvl, p.boostLevel);
        set (smBoostOn, p.boost ? 1.0f : 0.0f);
        set (smOut, p.outputDb); set (smOn, p.on ? 1.0f : 0.0f);
        snap = false;

        for (int pos = 0; pos < n; pos += kChunk)
        {
            const int len = std::min (kChunk, n - pos);

            Coeffs c;
            c.inGain = std::pow (10.0f, smIn.skip (len) / 20.0f);
            c.sustainAtt = 0.03f * std::pow (10.0f, 1.52f * smGain.skip (len));   // log pot: -30 dB .. 0 dB
            c.tone = smTone.skip (len);
            const float lv = smLevel.skip (len) * 2.0f;
            c.volGain = lv * lv;
            const float bAmt = smBoostOn.skip (len), bLvl = smBoostLvl.skip (len);
            c.boostGain = std::pow (10.0f, bAmt * bLvl * 24.0f / 20.0f);
            c.outGain = std::pow (10.0f, smOut.skip (len) / 20.0f);
            c.onMix = smOn.skip (len);
            c.asym = p.clip == 1;
            c.norm = c.asym ? 0.8f : 1.0f;                                       // keep the two modes level matched

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) pos, (size_t) len);
            auto up = os.processSamplesUp (block);
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = up.getChannelPointer ((size_t) ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = channels[(size_t) ch].process (d[i], c);
            }
            os.processSamplesDown (block);
        }
    }

private:
    float fsOS = 176400.0f;
    bool snap = true;
    std::array<Channel, 2> channels;
    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::SmoothedValue<float> smIn, smGain, smTone, smLevel, smBoostLvl, smBoostOn, smOut, smOn;
};
} // namespace redmuck
