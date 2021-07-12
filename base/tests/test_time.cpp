#define TEST_TIME_MONITOR
#include "base/time.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <cmath>

using namespace std::chrono_literals;

inline void do_some_work(std::chrono::duration<double> delay)
{
#ifdef IDLE_WORK
    std::this_thread::sleep_for(delta);
#else
    const auto start = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    while (now - start < delay)
        now = std::chrono::high_resolution_clock::now();
#endif
}

static void test_monitor(base::time_monitor& t, std::chrono::duration<double> delta)
{
    // expected characteristics:
    const double exp_mind = 0.98;                                 // minimum delay
    const double exp_maxd = 1.0 + std::max(delta.count(), 0.09);  // maximum delay
    const size_t exp_minr = 1.0 / (delta.count() + 1e-4);         // minimum event rate
    const size_t exp_maxr = std::ceil(1.0 / delta.count());       // maximum event rate
    size_t exp_events = 0;
    size_t sec = 0;
    std::cout << "Start delta=" << delta.count() << std::endl;
    t.reset();
    auto last = std::chrono::high_resolution_clock::now();
    while (sec < 5) {
        do_some_work(delta);
        ++exp_events;
        if (t.has_passed()) {
            const auto now = std::chrono::high_resolution_clock::now();
            const auto delay = std::chrono::duration<double>(now - last).count();
            const auto r = t.event_rate();
            const auto dr = t.get_delay_rate();
            std::cout << delay << " rate: " << r << " dr: " << dr;
            if (delay < exp_mind)
                std::cout << " [d too low]";
            if (exp_maxd < delay)
                std::cout << " [d too high]";
            if (r < exp_minr)
                std::cout << " [rate too low]";
            if (exp_maxr < r)
                std::cout << " [rate too high]";
            if (exp_events != t.get_events())
                std::cout << " [wrong event count!!!]";
            if (sec == 0) {
                if (dr > 5)
                    std::cout << " [dr too high]";
            } else {
                if (dr > 3)
                    std::cout << " [dr too high]";
            }
            std::cout << std::endl;
            last = now;
            t.next();
            exp_events = 0;
            ++sec;
        }
    }
}

int main()
{
    base::time_monitor timing{};
    auto ds = std::vector<std::chrono::duration<double>>{
        // 1ns, 3ns, 10ns, 30ns, 100ns, 300ns, // this is beyond precision
        1us, 3us, 10us, 30us, 100us, 300us, 1ms, 3ms, 10ms, 30ms, 100ms, 300ms, 1s, 2s};
    for (const auto& d : ds)
        test_monitor(timing, d);
}
