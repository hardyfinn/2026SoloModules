// Cymbolism Fable: runs the engine for the Rack module.
//   - the engine works in blocks of 64 samples, on its own thread (default) or inline,
//     with a fixed latency (3 blocks threaded, 1 inline);
//   - geometry changes are built on a background thread (debounced, LRU cache of 8),
//     and the new cymbal is swapped in at the next strike with a short crossfade.
// Pure C++ (no Rack), so test/runner_check.cpp can drive it offline.
#pragma once
#include "CyEngine.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

namespace cy {

class Runner {
public:
	static const int B = 64;       // block size
	static const int NB = 72;      // blocks in the ring (enough for a 4096-frame host buffer)

	struct Block {
		Controls ctl;
		float exc[B];
		float strike[B];           // strike velocity (m/s), 0 = none
		float out[B];
	};

	// ---- audio-thread side ----
	Controls ctl;                  // set by the module before each sample
	GeomParams geom;               // set by the module (requests a rebuild when it changes)
	std::atomic<bool> threaded{true};

	Runner() {
		for (auto& b : ring) { std::fill(b.exc, b.exc + B, 0.f); std::fill(b.strike, b.strike + B, 0.f); std::fill(b.out, b.out + B, 0.f); }
		builder = std::thread([this] { builderLoop(); });
		worker = std::thread([this] { workerLoop(); });
	}
	~Runner() {
		quit = true;
		buildCv.notify_all();
		if (worker.joinable()) worker.join();
		if (builder.joinable()) builder.join();
		delete active;
		delete fading;
		delete pending.exchange(nullptr);
		for (auto& t : trash) delete t.exchange(nullptr);
	}

	// one sample: returns the output (volts), delayed by the block latency
	float tick(float excite, float strikeVel) {
		Block& w = ring[wIdx % NB];
		if (wPos == 0) w.ctl = ctl;
		w.exc[wPos] = excite;
		w.strike[wPos] = strikeVel;
		// output from an older block
		float y = 0.f;
		int64_t r = wIdx - latency();
		if (r >= 0) {
			if (done.load(std::memory_order_acquire) > r) y = ring[r % NB].out[wPos];
			else if (wPos == 0) {
				// Rack runs modules in bursts of one audio buffer, so the latency has to
				// cover the buffer: grow it until the engine thread keeps up
				xruns++;
				if (threaded && latBlocks < NB - 6) latBlocks++;
			}
		}
		if (++wPos == B) {
			wPos = 0;
			if (threaded) {
				if (wIdx + 1 - done.load(std::memory_order_acquire) < NB - 1) {
					filled.store(wIdx + 1, std::memory_order_release);
					wIdx++;
				}
				// else: the engine thread is far behind; this block's input is dropped
			}
			else {
				// inline: catch up anything the thread left, then this block
				while (done.load() <= wIdx) processBlock(done.load());
				filled.store(wIdx + 1);
				wIdx++;
			}
		}
		return y;
	}
	int latency() const { return threaded ? latBlocks : 1; }
	int latBlocks = 3;

	// request a model for these geometry settings (call when they change)
	// returns false if it should be called again (lock busy)
	// typeB >= 0 also builds TYPE B for MORPH (Cymbolism Extended)
	bool requestBuild(const GeomParams& g, const Controls& c, int typeB = -1) {
		std::unique_lock<std::mutex> lk(buildMx, std::try_to_lock);
		if (!lk.owns_lock()) return false;
		reqTypeB = typeB;
		reqGeom = g;
		reqCtl = c;
		reqSeq++;
		reqTime = std::chrono::steady_clock::now();
		lk.unlock();
		buildCv.notify_one();
		return true;
	}

	// display / status (any thread)
	std::atomic<bool> building{false};
	std::atomic<bool> hasModel{false};
	std::atomic<float> cpu{0.f};           // engine time / real time
	std::atomic<float> eps{0.f};
	std::atomic<int> nModes{0}, nCore{0}, rivetCount{0};
	std::atomic<float> fTop{0.f};
	std::atomic<int> xruns{0};
	std::atomic<float> swing{0.f};
	std::string modelKey() { std::lock_guard<std::mutex> lk(infoMx); return infoKey; }

private:
	Block ring[NB];
	int64_t wIdx = 0;
	int wPos = 0;
	std::atomic<int64_t> filled{0}, done{0};
	std::atomic<bool> quit{false};
	std::thread worker, builder;

	// engines (worker side)
	Engine* active = nullptr;
	Engine* fading = nullptr;
	double fadeGain = 0, fadeStep = 0;
	double pendingWait = 0;
	std::atomic<Engine*> pending{nullptr};
	std::atomic<Engine*> trash[4] = {};
	std::mutex procMx;             // inline vs thread hand-over

	void dispose(Engine* e) {
		if (!e) return;
		for (auto& t : trash) {
			Engine* exp = nullptr;
			if (t.compare_exchange_strong(exp, e)) return;
		}
		delete e;   // all slots busy (rare): free here
	}

	void swapIn(Engine* e, double fadeSec) {
		dispose(fading);
		fading = active;
		active = e;
		fadeGain = 1.0;
		fadeStep = fading ? 1.0 / std::max(1.0, fadeSec * active->fsHost) : 0.0;
		hasModel = true;
		nModes = active->N;
		nCore = active->NC;
		fTop = (float)(active->M->fTopCore);
	}

	void processBlock(int64_t idx) {
		std::lock_guard<std::mutex> lk(procMx);
		if (done.load() > idx) return;
		auto t0 = std::chrono::steady_clock::now();
		Block& b = ring[idx % NB];
		// a new model waits for the next strike (or 2 s of no strikes)
		Engine* p = pending.load();
		bool anyStrike = false;
		for (int i = 0; i < B; i++) anyStrike |= b.strike[i] > 0;
		if (p) {
			pendingWait += (double)B / (active ? active->fsHost : 48000.0);
			if (!active || anyStrike || pendingWait > 2.0) p = pending.exchange(nullptr);
			else p = nullptr;
			if (p) {
				p->ctl = b.ctl;
				p->updateCoefficients(true);
				swapIn(p, active && !anyStrike ? 0.3 : 0.03);
				pendingWait = 0;
			}
		}
		if (!active) {
			std::fill(b.out, b.out + B, 0.f);
			done.store(idx + 1, std::memory_order_release);
			return;
		}
		active->ctl = b.ctl;
		if (fading) {
			fading->ctl = b.ctl;
			fading->ctl.roll = false;
		}
		float tmp[B];
		int s = 0;
		while (s < B) {
			int e = s;
			if (b.strike[s] > 0) { active->strike(b.strike[s]); }
			e = s + 1;
			while (e < B && !(b.strike[e] > 0)) e++;
			active->process(b.exc + s, b.out + s, e - s);
			if (fading) {
				fading->process(nullptr, tmp, e - s);
				for (int i = 0; i < e - s; i++) {
					b.out[s + i] += (float)(tmp[i] * fadeGain);
					fadeGain -= fadeStep;
					if (fadeGain < 0) fadeGain = 0;
				}
			}
			s = e;
		}
		if (fading && fadeGain <= 0) { dispose(fading); fading = nullptr; }
		auto t1 = std::chrono::steady_clock::now();
		double secs = std::chrono::duration<double>(t1 - t0).count();
		double blockSecs = (double)B / active->fsHost;
		cpu = (float)(0.95 * cpu.load() + 0.05 * secs / blockSecs);
		eps = (float)active->lastEps;
		rivetCount = (int)active->rivets.size();
		swing = (float)active->swPsi;
		done.store(idx + 1, std::memory_order_release);
	}

	void workerLoop() {
		while (!quit) {
			int64_t d = done.load(std::memory_order_acquire);
			if (threaded && d < filled.load(std::memory_order_acquire)) processBlock(d);
			else std::this_thread::sleep_for(std::chrono::microseconds(150));
		}
	}

	// ---- background builds ----
	std::mutex buildMx, infoMx;
	std::condition_variable buildCv;
	GeomParams reqGeom;
	Controls reqCtl;
	int reqTypeB = -1;
	uint64_t reqSeq = 0, builtSeq = 0;
	std::chrono::steady_clock::time_point reqTime;
	std::string infoKey;
	std::list<std::pair<std::string, std::shared_ptr<Model>>> cache;

	void builderLoop() {
		while (!quit) {
			GeomParams g;
			Controls c;
			uint64_t seq;
			int typeB = -1;
			{
				std::unique_lock<std::mutex> lk(buildMx);
				buildCv.wait_for(lk, std::chrono::milliseconds(20), [this] { return quit || reqSeq != builtSeq; });
				for (auto& t : trash) delete t.exchange(nullptr);
				if (quit) return;
				if (reqSeq == builtSeq) continue;
				// debounce: 80 ms after the last change
				if (std::chrono::steady_clock::now() - reqTime < std::chrono::milliseconds(80)) {
					lk.unlock();
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					continue;
				}
				g = reqGeom;
				c = reqCtl;
				seq = reqSeq;
				typeB = reqTypeB;
			}
			GeomParams gb = g;
			if (typeB >= 0) {
				gb.fromType(typeB);
				gb.weight = g.weight;
				gb.alloy = g.alloy;
				gb.tension = g.tension;
			}
			std::string key = g.key() + (typeB >= 0 ? "|B" + gb.key() : std::string());
			{
				std::lock_guard<std::mutex> lk(infoMx);
				if (key == infoKey && hasModel) { builtSeq = seq; continue; }
			}
			building = true;
			auto fetch = [&](const GeomParams& gg) {
				std::string kk = gg.key();
				for (auto it = cache.begin(); it != cache.end(); ++it)
					if (it->first == kk) { auto m = it->second; cache.splice(cache.begin(), cache, it); return m; }
				auto m = buildModel(gg);
				cache.push_front({kk, m});
				while (cache.size() > 8) cache.pop_back();
				return m;
			};
			std::shared_ptr<Model> M = fetch(g);
			std::shared_ptr<Model> MB = typeB >= 0 ? fetch(gb) : nullptr;
			Engine* e = new Engine();
			e->ctl = c;   // the worker sets the live controls again at the swap
			e->init(M, g.sampleRate);
			if (MB) e->setMorphModel(*MB);
			delete pending.exchange(e);   // a newer build replaces one still waiting
			{
				std::lock_guard<std::mutex> lk(infoMx);
				infoKey = key;
			}
			{
				std::lock_guard<std::mutex> lk(buildMx);
				builtSeq = seq;
				building = reqSeq != builtSeq;
			}
		}
	}
};

} // namespace cy
