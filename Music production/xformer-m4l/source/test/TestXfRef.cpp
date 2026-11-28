#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <array>
#include <functional>
#include <memory>
#include <algorithm>
#include <cmath>
#include <utility>
#include "UnitTest.h"

#include "EngineTestFixture.h"

#include "apps/sequencer/model/Track.h"
#include "apps/sequencer/engine/StochasticTrackEngine.h"
#include "apps/sequencer/engine/NoteTrackEngine.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>

// Reference trace generator for the Max for Live port.
// Reads a scenario (XF_SCENARIO) of "key value..." lines, runs track 0 for
// the requested number of engine ticks and writes "tick gate cv" lines on
// every gate/cv change (XF_OUT).

UNIT_TEST("XfRef") {

CASE("trace") {
    const char *scen = getenv("XF_SCENARIO");
    const char *outPath = getenv("XF_OUT");
    if (!scen || !outPath) { expectTrue(true, "no scenario"); return; }

    EngineTestFixture fx;
    auto &project = fx.project();
    project.setTempo(30.f);
    project.setScale(0);
    project.setRootNote(0);

    std::ifstream in(scen);
    std::string line;
    std::string engine;
    int ticks = 2000;
    bool renew = false;

    std::vector<std::vector<std::string>> cmds;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::vector<std::string> tok; std::string t;
        while (ss >> t) tok.push_back(t);
        if (tok.empty()) continue;
        if (tok[0] == "engine") engine = tok[1];
        else if (tok[0] == "ticks") ticks = atoi(tok[1].c_str());
        else if (tok[0] == "renew") renew = true;
        else cmds.push_back(tok);
    }

    if (engine == "tuesday") project.setTrackMode(0, Track::TrackMode::Tuesday);
    if (engine == "stochastic") project.setTrackMode(0, Track::TrackMode::Stochastic);
    if (engine == "note") project.setTrackMode(0, Track::TrackMode::Note);
    if (engine == "indexed") project.setTrackMode(0, Track::TrackMode::Indexed);
    if (engine == "discrete") project.setTrackMode(0, Track::TrackMode::DiscreteMap);
    if (engine == "phaseflux") project.setTrackMode(0, Track::TrackMode::PhaseFlux);
    // Fractal records another track: track 0 is a Note track ("p ..." lines), track 1 the Fractal
    if (engine == "fractal") {
        project.setTrackMode(0, Track::TrackMode::Note);
        project.setTrackMode(1, Track::TrackMode::Fractal);
    }
    const int outTrack = engine == "fractal" ? 1 : 0;
    fx.engine().update();

    auto &track = project.track(0);
    // the simulator leaves a freshly switched Note track uninitialised; clear it like the UI does
    if (engine == "note" || engine == "fractal") {
        track.noteTrack().clear();
    }
    // slides are CV slews on the module; the port only marks them, so compare unslewed pitch
    if (engine == "indexed") track.indexedTrack().setSlideTime(0);
    if (engine == "discrete") track.discreteMapTrack().sequence(0).setSlewTime(0);
    if (engine == "phaseflux") track.phaseFluxTrack().setSlideTime(0);
    const std::string engine0 = engine;
    for (auto &c0 : cmds) {
        std::string engine = engine0;
        std::vector<std::string> c = c0;
        Track *trackp = &project.track(0);
        if (engine0 == "fractal") {
            if (c[0] == "p") { c.erase(c.begin()); engine = "note"; }
            else trackp = &project.track(1);
        }
        auto &track = *trackp;
        const std::string &k = c[0];
        int v = c.size() > 1 ? atoi(c[1].c_str()) : 0;
        if (engine == "tuesday") {
            auto &s = track.tuesdayTrack().sequence(0);
            if (k == "algorithm") s.setAlgorithm(v);
            else if (k == "flow") s.setFlow(v);
            else if (k == "ornament") s.setOrnament(v);
            else if (k == "power") s.setPower(v);
            else if (k == "start") s.setStart(v);
            else if (k == "loopLength") s.setLoopLength(v);
            else if (k == "glide") s.setGlide(v);
            else if (k == "trill") s.setTrill(v);
            else if (k == "stepTrill") s.setStepTrill(v);
            else if (k == "skew") s.setSkew(v);
            else if (k == "octave") s.setOctave(v);
            else if (k == "transpose") s.setTranspose(v);
            else if (k == "divisor") s.setDivisor(v);
            else if (k == "clockMult") s.setClockMultiplier(v);
            else if (k == "scale") s.setScale(v);
            else if (k == "rootNote") s.setRootNote(v);
            else if (k == "scaleRotate") s.setScaleRotate(v);
            else if (k == "gateLength") s.setGateLength(v);
            else if (k == "gateOffset") s.setGateOffset(v);
            else if (k == "maskParameter") s.setMaskParameter(v);
            else if (k == "timeMode") s.setTimeMode(v);
            else if (k == "maskProgression") s.setMaskProgression(v);
            else if (k == "rotate") s.setRotate(v);
            else printf("unknown %s\n", k.c_str());
        } else if (engine == "stochastic") {
            auto &t = track.stochasticTrack();
            auto &s = t.sequence(0);
            if (k == "rhythmMode") s.setRhythmMode(StochasticSourceMode(v));
            else if (k == "melodyMode") s.setMelodyMode(StochasticSourceMode(v));
            else if (k == "complexity") s.setComplexity(v);
            else if (k == "contour") s.setContour(v);
            else if (k == "repeatProb") s.setRepeatProb(v);
            else if (k == "noteDuration") s.setNoteDuration(v);
            else if (k == "variation") s.setVariation(v);
            else if (k == "rest") s.setRest(v);
            else if (k == "slide") s.setSlide(v);
            else if (k == "burstRate") s.setBurstRate(v);
            else if (k == "burstCount") s.setBurstCount(v);
            else if (k == "burstHold") s.setBurstHold(StochasticBurstHold(v));
            else if (k == "sleep") s.setSleep(v);
            else if (k == "mutate") s.setMutate(v);
            else if (k == "jump") s.setJump(v);
            else if (k == "range") s.setRange(v);
            else if (k == "degreeTicket") s.setDegreeTicket(v, atoi(c[2].c_str()));
            else if (k == "durationTicket") s.setDurationTicket(v, atoi(c[2].c_str()));
            else if (k == "degreeRotation") s.setDegreeRotation(v);
            else if (k == "maskRotation") s.setMaskRotation(v);
            else if (k == "patienceMelody") s.setPatienceMelody(v);
            else if (k == "patienceRhythm") s.setPatienceRhythm(v);
            else if (k == "legatoProb") s.setLegatoProb(v);
            else if (k == "marblesSpread") s.setMarblesSpread(v);
            else if (k == "marblesBias") s.setMarblesBias(v);
            else if (k == "maskMelody") s.setMaskMelody(v);
            else if (k == "tiltMelody") s.setTiltMelody(v);
            else if (k == "maskRhythm") s.setMaskRhythm(v);
            else if (k == "gateLength") s.setGateLength(v);
            else if (k == "tiltRhythm") s.setTiltRhythm(v);
            else if (k == "burst") s.setBurst(v);
            else if (k == "feel") s.setFeel(v);
            else if (k == "rotate") s.setRotate(v);
            else if (k == "size") s.setSize(v);
            else if (k == "first") s.setFirst(v);
            else if (k == "divisor") s.setDivisor(v);
            else if (k == "clockMult") s.setClockMultiplier(v);
            else if (k == "scale") s.setScale(v);
            else if (k == "rootNote") s.setRootNote(v);
            else if (k == "scaleRotate") s.setScaleRotate(v);
            else if (k == "octave") t.setOctave(v);
            else if (k == "transpose") t.setTranspose(v);
            else if (k == "cvUpdate") t.setCvUpdateMode(StochasticTrack::CvUpdateMode(v));
            else printf("unknown %s\n", k.c_str());
        } else if (engine == "indexed") {
            auto &t = track.indexedTrack();
            auto &s = t.sequence(0);
            if (k == "step") {
                auto &st = s.step(v);
                const std::string &f = c[2];
                int x = atoi(c[3].c_str());
                if (f == "duration") st.setDuration(x);
                else if (f == "gateLength") st.setGateLength(x);
                else if (f == "noteIndex") st.setNoteIndex(x);
                else if (f == "slide") st.setSlide(x);
                else if (f == "groupMask") st.setGroupMask(x);
                else printf("unknown step field %s\n", f.c_str());
            }
            else if (k == "divisor") s.setDivisor(v);
            else if (k == "clockMult") s.setClockMultiplier(v);
            else if (k == "loop") s.setLoop(v);
            else if (k == "runMode") s.setRunMode(Types::RunMode(v));
            else if (k == "activeLength") s.setActiveLength(v);
            else if (k == "scale") s.setScale(v);
            else if (k == "rootNote") s.setRootNote(v);
            else if (k == "scaleRotate") s.setScaleRotate(v);
            else if (k == "firstStep") s.setFirstStep(v);
            else if (k == "syncMode") s.setSyncMode(IndexedSequence::SyncMode(v));
            else if (k == "resetMeasure") s.setResetMeasure(v);
            else if (k == "octave") t.setOctave(v);
            else if (k == "transpose") t.setTranspose(v);
            else if (k == "playMode") t.setPlayMode(Types::PlayMode(v));
            else printf("unknown %s\n", k.c_str());
        } else if (engine == "discrete") {
            auto &t = track.discreteMapTrack();
            auto &s = t.sequence(0);
            if (k == "stage") {
                auto &st = s.stage(v);
                const std::string &f = c[2];
                int x = atoi(c[3].c_str());
                if (f == "threshold") st.setThreshold(x);
                else if (f == "direction") st.setDirection(DiscreteMapSequence::Stage::TriggerDir(x));
                else if (f == "noteIndex") st.setNoteIndex(x);
                else printf("unknown stage field %s\n", f.c_str());
            }
            else if (k == "clockSource") s.setClockSource(DiscreteMapSequence::ClockSource(v));
            else if (k == "syncMode") s.setSyncMode(DiscreteMapSequence::SyncMode(v));
            else if (k == "divisor") s.setDivisor(v);
            else if (k == "clockMult") s.setClockMultiplier(v);
            else if (k == "gateLength") s.setGateLength(v);
            else if (k == "loop") s.setLoop(v);
            else if (k == "resetMeasure") s.setResetMeasure(v);
            else if (k == "thresholdMode") s.setThresholdMode(DiscreteMapSequence::ThresholdMode(v));
            else if (k == "scale") s.setScale(v);
            else if (k == "rootNote") s.setRootNote(v);
            else if (k == "scaleRotate") s.setScaleRotate(v);
            else if (k == "octave") s.setOctave(v);
            else if (k == "transpose") s.setTranspose(v);
            else if (k == "rangeHigh") s.setRangeHigh(v / 100.f);
            else if (k == "rangeLow") s.setRangeLow(v / 100.f);
            else if (k == "input") t.setRoutedInput(v / 100.f);
            else if (k == "scanner") t.setRoutedScanner(v);
            else if (k == "playMode") t.setPlayMode(Types::PlayMode(v));
            else printf("unknown %s\n", k.c_str());
        } else if (engine == "phaseflux") {
            auto &t = track.phaseFluxTrack();
            auto &s = t.sequence(0);
            using PS = PhaseFluxSequence;
            if (k == "stage") {
                auto &st = s.stage(v);
                const std::string &f = c[2];
                int x = atoi(c[3].c_str());
                if (f == "skip") st.setSkip(x);
                else if (f == "pulseCount") st.setPulseCount(x);
                else if (f == "length") st.setLength(x);
                else if (f == "basePitch") st.setBasePitch(x);
                else if (f == "pitchRange") st.setPitchRange(PS::PitchRangeType(x));
                else if (f == "pitchDirection") st.setPitchDirection(PS::PitchDirectionType(x));
                else if (f == "pitchCurve") st.setPitchCurve(PS::PitchCurveType(x));
                else if (f == "pitchFlipV") st.setPitchFlipV(x);
                else if (f == "pitchFlipH") st.setPitchFlipH(x);
                else if (f == "pitchWarp") st.setPitchWarp(x);
                else if (f == "pitchResponse") st.setPitchResponse(x);
                else if (f == "temporalCurve") st.setTemporalCurve(PS::TemporalCurveType(x));
                else if (f == "temporalFlipV") st.setTemporalFlipV(x);
                else if (f == "temporalFlipH") st.setTemporalFlipH(x);
                else if (f == "temporalWarp") st.setTemporalWarp(x);
                else if (f == "temporalResponse") st.setTemporalResponse(x);
                else if (f == "maskMelody") st.setMaskMelody(x);
                else if (f == "tiltMelody") st.setTiltMelody(x);
                else if (f == "phaseShift") st.setPhaseShift(x);
                else if (f == "mask") st.setMask(PS::MaskType(x));
                else if (f == "maskShift") st.setMaskShift(x);
                else if (f == "accumulatorStep") st.setAccumulatorStep(x);
                else if (f == "pulseAccumStep") st.setPulseAccumStep(x);
                else if (f == "accumulatorTrigger") st.setAccumulatorTrigger(PS::AccumulatorTriggerType(x));
                else if (f == "pulseAccumTrigger") st.setPulseAccumTrigger(PS::AccumulatorTriggerType(x));
                else if (f == "gateLength") st.setGateLength(x);
                else if (f == "temporalRepeat") st.setTemporalRepeat(PS::RepeatType(x));
                else if (f == "pitchRepeat") st.setPitchRepeat(PS::RepeatType(x));
                else if (f == "temporalWindow") st.setTemporalWindow(PS::WindowType(x));
                else if (f == "pitchWindow") st.setPitchWindow(PS::WindowType(x));
                else printf("unknown stage field %s\n", f.c_str());
            }
            else if (k.rfind("noteAcc", 0) == 0 || k.rfind("pulseAcc", 0) == 0) {
                bool note = k.rfind("noteAcc", 0) == 0;
                auto &a = note ? s.noteAccumConfig() : s.pulseAccumConfig();
                std::string f = k.substr(note ? 7 : 8);
                if (f == "Scope") a.setScope(AccumulatorConfig::Scope(v));
                else if (f == "Order") a.setOrder(AccumulatorConfig::Order(v));
                else if (f == "Polarity") a.setPolarity(AccumulatorConfig::Polarity(v));
                else if (f == "Reset") a.setReset(v);
                else if (f == "Pos") a.setPosLim(v);
                else if (f == "Neg") a.setNegLim(v);
                else printf("unknown %s\n", k.c_str());
            }
            else if (k == "divisor") s.setDivisor(v);
            else if (k == "clockMult") s.setClockMultiplier(v);
            else if (k == "cycleLength") s.setCycleLength(PS::CycleLengthMode(v));
            else if (k == "traversal") s.setTraversalPattern(v);
            else if (k == "pitchMode") s.setPitchMode(PS::PitchMode(v));
            else if (k == "pitchRate") s.setPitchRate(v);
            else if (k == "globalPhase") s.setGlobalPhase(v / 100.f);
            else if (k == "warpNudge") s.setWarpNudge(v);
            else if (k == "responseNudge") s.setResponseNudge(v);
            else if (k == "cyclePhaseWarp") s.setCyclePhaseWarp(v);
            else if (k == "pulseNudge") s.setPulseNudge(v);
            else if (k == "lenNudge") s.setLenNudge(v);
            else if (k == "resetMeasure") s.setResetMeasure(v);
            else if (k == "firstStage") s.setFirstStage(v);
            else if (k == "lastStage") s.setLastStage(v);
            else if (k == "scale") s.setScale(v);
            else if (k == "rootNote") s.setRootNote(v);
            else if (k == "scaleRotate") s.setScaleRotate(v);
            else if (k == "octave") t.setOctave(v);
            else if (k == "transpose") t.setTranspose(v);
            else printf("unknown %s\n", k.c_str());
        } else if (engine == "fractal") {
            auto &t = track.fractalTrack();
            auto &s = t.sequence(0);
            using FS = FractalSequence;
            if (k == "divisor") s.setDivisor(v);
            else if (k == "clockMult") s.setClockMultiplier(v);
            else if (k == "resetMeasure") s.setResetMeasure(v);
            else if (k == "loopFirst") s.setLoopFirst(v);
            else if (k == "loopLast") s.setLoopLast(v);
            else if (k == "rotate") s.setRotate(v);
            else if (k == "orderMode") s.setOrderMode(FS::OrderMode(v));
            else if (k == "recordSkip") s.setRecordSkip(v);
            else if (k == "recordFirst") s.setRecordFirst(v);
            else if (k == "recordLast") s.setRecordLast(v);
            else if (k == "ornFirst") s.setOrnFirst(v);
            else if (k == "ornLast") s.setOrnLast(v);
            else if (k == "recordMode") s.setRecordMode(FS::RecordMode(v));
            else if (k == "cadence") s.setCaptureCadence(FS::CaptureCadence(v));
            else if (k == "fidelity") s.setCaptureFidelity(FS::CaptureFidelity(v));
            else if (k == "record") s.setRecordTrigger(v);
            else if (k == "branchCount") s.setBranchCount(v);
            else if (k == "path") s.setPath(v);
            else if (k == "branchSeed") s.setBranchSeed(v);
            else if (k == "branchPool") s.setBranchPool(v);
            else if (k == "ornRate") s.setOrnamentRate(v);
            else if (k == "ornIntensity") s.setOrnamentIntensity(v);
            else if (k == "scale") s.setScale(v);
            else if (k == "rootNote") s.setRootNote(v);
            else if (k == "scaleRotate") s.setScaleRotate(v);
            else if (k == "sourceA") t.setSourceA(v);
            else if (k == "lock") t.setLock(v);
            else if (k == "octave") t.setOctave(v);
            else if (k == "transpose") t.setTranspose(v);
            else if (k == "quantize") t.setQuantize(v);
            else if (k == "trackDelay") t.setTrackDelay(v);
            else printf("unknown %s\n", k.c_str());
        } else if (engine == "note") {
            auto &t = track.noteTrack();
            auto &s = t.sequence(0);
            auto &acc = s.accumulator();
            if (k == "step") {
                int i = v;
                const std::string &f = c[2];
                int x = atoi(c[3].c_str());
                auto &st = s.step(i);
                if (f == "gate") st.setGate(x);
                else if (f == "gateProbability") st.setGateProbability(x);
                else if (f == "gateOffset") st.setGateOffset(x);
                else if (f == "slide") st.setSlide(x);
                else if (f == "retrigger") st.setRetrigger(x);
                else if (f == "retriggerProbability") st.setRetriggerProbability(x);
                else if (f == "length") st.setLength(x);
                else if (f == "lengthVariationRange") st.setLengthVariationRange(x);
                else if (f == "lengthVariationProbability") st.setLengthVariationProbability(x);
                else if (f == "note") st.setNote(x);
                else if (f == "noteVariationRange") st.setNoteVariationRange(x);
                else if (f == "noteVariationProbability") st.setNoteVariationProbability(x);
                else if (f == "condition") st.setCondition(Types::Condition(x));
                else if (f == "accumulator") st.setAccumulatorStepValue(x);
                else if (f == "pulseCount") st.setPulseCount(x);
                else if (f == "gateMode") st.setGateMode(x);
                else printf("unknown step field %s\n", f.c_str());
            }
            else if (k == "firstStep") s.setFirstStep(v);
            else if (k == "lastStep") s.setLastStep(v);
            else if (k == "runMode") s.setRunMode(Types::RunMode(v));
            else if (k == "divisor") s.setDivisor(v);
            else if (k == "clockMult") s.setClockMultiplier(v);
            else if (k == "resetMeasure") s.setResetMeasure(v);
            else if (k == "scale") s.setScale(v);
            else if (k == "rootNote") s.setRootNote(v);
            else if (k == "scaleRotate") s.setScaleRotate(v);
            else if (k == "octave") t.setOctave(v);
            else if (k == "transpose") t.setTranspose(v);
            else if (k == "playMode") t.setPlayMode(Types::PlayMode(v));
            else if (k == "slideTime") t.setSlideTime(v);
            else if (k == "rotate") t.setRotate(v);
            else if (k == "gateProbabilityBias") t.setGateProbabilityBias(v);
            else if (k == "retriggerProbabilityBias") t.setRetriggerProbabilityBias(v);
            else if (k == "lengthBias") t.setLengthBias(v);
            else if (k == "noteProbabilityBias") t.setNoteProbabilityBias(v);
            else if (k == "cvUpdate") t.setCvUpdateMode(NoteTrack::CvUpdateMode(v));
            else if (k == "accEnabled") acc.setEnabled(v);
            else if (k == "accMode") acc.setMode(Accumulator::Mode(v));
            else if (k == "accDirection") acc.setDirection(Accumulator::Direction(v));
            else if (k == "accOrder") acc.setOrder(Accumulator::Order(v));
            else if (k == "accTrigger") acc.setTriggerMode(Accumulator::TriggerMode(v));
            else if (k == "accMin") { acc.setMinValue(v); acc.reset(); }
            else if (k == "accMax") acc.setMaxValue(v);
            else if (k == "accStep") acc.setStepValue(v);
            else printf("unknown %s\n", k.c_str());
        }
    }

    if (renew && engine == "stochastic") {
        auto &te = static_cast<StochasticTrackEngine &>(fx.engine().trackEngine(0));
        te.renewLoopSources();
    }

    FILE *out = fopen(outPath, "w");
    fx.start();
    int lastGate = -1;
    float lastCv = -1000.f;
    uint32_t lastTick = 0;
    for (int ms = 0; ms < 10000000; ++ms) {
        fx.advance(1);
        uint32_t tick = fx.engine().tick();
        if (tick >= uint32_t(ticks)) break;
        auto &te = fx.engine().trackEngine(outTrack);
        int g = te.gateOutput(0) ? 1 : 0;
        float cv = te.cvOutput(0);
        if (getenv("XF_TP")) fprintf(out, "tp %d %u %.5f\n", ms, tick, fx.engine().clock().tickPosition());
        else if (getenv("XF_ALL")) fprintf(out, "ms %d %u %d %.4f\n", ms, tick, g, cv);
        else if (g != lastGate || (g && cv != lastCv)) {
            fprintf(out, "%u %d %.4f\n", tick, g, cv);
            lastGate = g;
            lastCv = cv;
        }
        lastTick = tick;
    }
    (void)lastTick;
    fclose(out);
    expectTrue(true, "done");
}

}
