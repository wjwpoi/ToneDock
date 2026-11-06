#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace tonedock {
// Measures round-trip latency through a physical loopback cable (output -> input).
// Each trial listens to the noise floor, emits a short click, then waits for the click
// to return. The median of three consistent trials is the result.
// next() is real-time safe: no allocation, locking or system calls.
class LatencyProbe {
public:
    static constexpr int trials=3;
    static constexpr float amplitude=.3f;

    void start(double sampleRate) noexcept {
        rate=sampleRate;quietLength=int(rate*.15);listenLength=int(rate*.5);
        trial=0;position=0;noise=0;found=-1;results.fill(-1);measured=-1;running=true;
    }
    void cancel() noexcept{running=false;}
    bool isRunning() const noexcept{return running;}
    // Round-trip latency in samples, or -1 when the last run failed or was inconsistent.
    int result() const noexcept{return measured;}

    // Feed one input sample; returns the test signal to send to the output for the same frame.
    float next(float input) noexcept {
        if(!running)return 0;
        auto level=std::abs(input);float out=0;
        if(position<quietLength)noise=std::max(noise,level);
        else {
            auto since=position-quietLength;
            if(since<clickLength)out=since<clickLength/2?amplitude:-amplitude;
            if(found<0 && level>std::max(minimumThreshold,noise*4))found=since;
            if(since+1>=listenLength)finishTrial();
        }
        ++position;return out;
    }

private:
    void finishTrial() noexcept {
        results[size_t(trial)]=found;position=-1;noise=0;found=-1;
        if(++trial<trials)return;
        running=false;
        auto sorted=results;std::sort(sorted.begin(),sorted.end());
        auto consistent=sorted.front()>=0 && sorted.back()-sorted.front()<=int(rate*.002)+1;
        measured=consistent?sorted[size_t(trials/2)]:-1;
    }
    static constexpr int clickLength=24;
    static constexpr float minimumThreshold=.02f;
    double rate=48000;
    int quietLength=0,listenLength=0,trial=0,position=0,found=-1,measured=-1;
    float noise=0;
    bool running=false;
    std::array<int,trials> results{};
};
}
