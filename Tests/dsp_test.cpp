// Offline sanity test for the RedMuck2 Sim DSP core (no GUI needed).
#include "RedMuckDSP.h"
#include <cstdio>

static double goertzel (const std::vector<float>& x, double f, double fs)
{
    const double w = 2.0 * M_PI * f / fs;
    double s1 = 0, s2 = 0, c = 2.0 * std::cos (w);
    for (float v : x) { double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
    const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
    return 2.0 * std::sqrt (std::max (0.0, p)) / (double) x.size();
}

struct Result { double rms, peak, dc, fund, h2; bool finite; };

static Result run (redmuck::Params p, double amp, double f0, double fs = 48000.0)
{
    redmuck::Engine eng;
    eng.prepare (fs);
    const int N = (int) fs, block = 512;
    juce::AudioBuffer<float> buf (2, block);
    std::vector<float> out;
    for (int pos = 0; pos < N; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            float v = (float) (amp * std::sin (2.0 * M_PI * f0 * (pos + i) / fs));
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        eng.process (buf, p);
        for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
    }
    std::vector<float> tail (out.begin() + N / 2, out.end());
    Result r { 0, 0, 0, 0, 0, true };
    for (float v : tail)
    {
        if (! std::isfinite (v)) r.finite = false;
        r.rms += v * v; r.peak = std::max (r.peak, (double) std::fabs (v)); r.dc += v;
    }
    r.rms = std::sqrt (r.rms / tail.size()); r.dc /= tail.size();
    r.fund = goertzel (tail, f0, fs);
    r.h2 = goertzel (tail, 2 * f0, fs);
    return r;
}

static void show (const char* name, const Result& r)
{
    std::printf ("%-38s rms=%.3f peak=%.3f dc=%+.4f fund=%.3f h2=%.3f %s\n", name, r.rms, r.peak, r.dc, r.fund, r.h2,
                 r.finite ? "ok" : "NaN/Inf!");
}

int main()
{
    int failures = 0;
    auto check = [&failures] (bool c, const char* m) { if (! c) { std::printf ("FAIL: %s\n", m); ++failures; } };

    redmuck::Params base;

    // --- bypass
    redmuck::Params off = base; off.on = false;
    auto r0 = run (off, 0.1, 330.0);
    show ("bypass (0.1)", r0);
    check (std::fabs (r0.fund - 0.1) < 0.01, "bypass is transparent");

    // --- stability, both clipping modes, several input levels
    for (int clip = 0; clip < 2; ++clip)
        for (double a : { 0.02, 0.1, 0.5, 1.0 })
        {
            redmuck::Params p = base; p.clip = clip;
            char n[64]; std::snprintf (n, sizeof n, "%s, 330 Hz, in=%.2f", clip ? "asym" : "sym ", a);
            auto r = run (p, a, 330.0); show (n, r);
            check (r.finite && r.peak < 4.0 && std::fabs (r.dc) < 0.03, "stable and DC free");
        }

    // --- Gain: more gain = more level for quiet notes; heavy compression
    redmuck::Params gl = base, gh = base; gl.gain = 0.1f; gh.gain = 1.0f;
    auto rgl = run (gl, 0.02, 330.0), rgh = run (gh, 0.02, 330.0);
    show ("gain 0.1, quiet in", rgl); show ("gain 1.0, quiet in", rgh);
    check (rgh.rms > rgl.rms * 1.5, "Gain raises the level of quiet notes");
    auto rs = run (gh, 0.05, 330.0), rl = run (gh, 0.5, 330.0);
    show ("gain 1.0, in=0.05", rs); show ("gain 1.0, in=0.5", rl);
    check (rl.rms < rs.rms * 3.0, "output is heavily compressed (10x input -> <3x output)");

    // --- Tone: mid scoop at noon, bass/treble tilt at the extremes
    redmuck::Params tn = gh;
    double lvl[3]; int k = 0;
    for (float t : { 0.0f, 0.5f, 1.0f })
    {
        tn.tone = t;
        auto r = run (tn, 0.2, 880.0);
        char n[64]; std::snprintf (n, sizeof n, "tone=%.1f, 880 Hz", t);
        show (n, r); lvl[k++] = r.rms;
    }
    check (lvl[1] < lvl[0] && lvl[1] < lvl[2], "tone at noon scoops the mids (880 Hz lowest)");
    tn.tone = 0.0f; auto bLow = run (tn, 0.2, 100.0), bHigh = run (tn, 0.2, 5000.0);
    show ("tone=0, 100 Hz", bLow); show ("tone=0, 5 kHz", bHigh);
    check (bLow.rms > bHigh.rms, "tone fully down favours bass");
    tn.tone = 1.0f; auto tLow = run (tn, 0.2, 100.0), tHigh = run (tn, 0.2, 5000.0);
    show ("tone=1, 100 Hz", tLow); show ("tone=1, 5 kHz", tHigh);
    check (tHigh.rms > tLow.rms, "tone fully up favours treble");

    // --- Clipping switch: asymmetrical adds even harmonics, stays level matched
    redmuck::Params sy = base, as = base; sy.clip = 0; as.clip = 1;
    auto rsy = run (sy, 0.1, 330.0), ras = run (as, 0.1, 330.0);
    show ("symmetrical, 330 Hz", rsy); show ("asymmetrical, 330 Hz", ras);
    check (ras.h2 / std::max (ras.fund, 1e-9) > 2.0 * rsy.h2 / std::max (rsy.fund, 1e-9), "asymmetrical clipping adds 2nd harmonic");
    check (ras.rms / rsy.rms > 0.7 && ras.rms / rsy.rms < 1.4, "clipping modes are level matched within about 3 dB");

    // --- Boost: raises a quiet signal; trimmer at 0 means no change
    redmuck::Params bo0 = base, bo1 = base, bo2 = base;
    bo0.gain = bo1.gain = bo2.gain = 0.2f;
    bo1.boost = true; bo1.boostLevel = 1.0f;
    bo2.boost = true; bo2.boostLevel = 0.0f;
    auto rb0 = run (bo0, 0.005, 330.0), rb1 = run (bo1, 0.005, 330.0), rb2 = run (bo2, 0.005, 330.0);
    show ("boost off", rb0); show ("boost on, trimmer 1.0", rb1); show ("boost on, trimmer 0.0", rb2);
    check (rb1.rms > rb0.rms * 1.5, "Boost raises the level into the fuzz");
    check (std::fabs (rb2.rms / rb0.rms - 1.0) < 0.15, "Boost with trimmer at 0 changes almost nothing");

    // --- extremes, both clipping modes, boost on
    for (int clip = 0; clip < 2; ++clip)
    {
        redmuck::Params mx = base; mx.clip = clip; mx.gain = 1.0f; mx.level = 1.0f; mx.boost = true;
        mx.boostLevel = 1.0f; mx.inputDb = 24.0f;
        for (double f : { 82.0, 440.0, 3000.0 })
        {
            char n[64]; std::snprintf (n, sizeof n, "max, %s @ %.0f Hz", clip ? "asym" : "sym ", f);
            auto r = run (mx, 1.0, f); show (n, r);
            check (r.finite && r.peak < 10.0 && std::fabs (r.dc) < 0.15, "max settings stable (bounded, no runaway)");
        }
    }

    for (double fs : { 44100.0, 96000.0 })
    {
        auto r = run (base, 0.3, 330.0, fs);
        char n[64]; std::snprintf (n, sizeof n, "default @ %.0f Hz", fs);
        show (n, r);
        check (r.finite, "finite at other sample rates");
    }

    std::printf (failures == 0 ? "\nALL CHECKS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures;
}
