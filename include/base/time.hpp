// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: nil; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of UPPAAL.
// Copyright (c) 2020, Aalborg University.
// All right reserved.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_BASE_TIME_HPP
#define INCLUDE_BASE_TIME_HPP

#include <chrono>

namespace base
{
    /**
     * Monitors the time periods based on event counts.
     * The precision is sacrificed in favor of minimizing
     * the number of "are we there yet?" queries to the system clock.
     * The monitor queries the system clock at most 5x/s during the first second
     * and later at most 3x/s (mostly 2x/s), as measured for event periods >=1us.
     * For events faster than 1us it can be as bad as 12x/s.
     */
    class time_monitor
    {
        const std::chrono::high_resolution_clock::time_point start;
        const double period;
        double delay{0};
        std::chrono::duration<double> last_delay{};   // time since last delay lookup
        std::chrono::duration<double> last_period{};  // time since last period end
        size_t events{0}, threshold{1};
#ifdef TEST_TIME_MONITOR
        size_t delay_rate{0};
#endif
        double get_delay()
        {                                                                // ~26.3ns
            const auto now = std::chrono::high_resolution_clock::now();  // ~22.0ns
            last_delay = now - start;
#ifdef TEST_TIME_MONITOR
            ++delay_rate;
#endif
            return last_delay.count() - last_period.count();
        }

    public:
        explicit time_monitor(double period_in_seconds = 1.0):
            start{std::chrono::high_resolution_clock::now()}, period{period_in_seconds}
        {}
        /** returns the number of events registered for the current period */
        size_t get_events() const { return events; }
        /** computes the number of events per second */
        size_t event_rate() const { return events / delay; }
        /** accounts the event and returns true if the specified time period has passed */
        bool has_passed()
        {
            if (++events >= threshold) {
                delay = get_delay();
                if (delay >= period)
                    return true;
                if (delay >= 1e-6) {
                    threshold = period * events / delay;
                    if (threshold == 0)
                        threshold = 1;
                } else
                    threshold *= 2;
            }
            return false;
        }
        /** prepares for the next period (to be called after has_passed()==true) */
        void next()
        {
            threshold = period * events / delay;
            if (threshold == 0)
                threshold = 1;
            events = 0;
            last_period = last_delay;
#ifdef TEST_TIME_MONITOR
            delay_rate = 0;
#endif
        }
        /** resets the monitor for another/unrelated performance measure */
        void reset()
        {
            events = 0;
            threshold = 1;
            delay = get_delay();
            last_period = last_delay;
#ifdef TEST_TIME_MONITOR
            delay_rate = 0;
#endif
        }
#ifdef TEST_TIME_MONITOR
        size_t get_delay_rate() const { return delay_rate; }
#endif
    };

}  // namespace base

#endif /* INCLUDE_BASE_TIME_HPP */
