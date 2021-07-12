// -*- mode: C++; c-file-style: "stroustrup"; c-basic-offset: 4; indent-tabs-mode: t; -*-
///////////////////////////////////////////////////////////////////////////////
//
// This file is a part of UPPAAL.
// Copyright (c) 2018, Aalborg University.
// All right reserved.
//
// Author: Marius Mikucionis <marius@cs.aau.dk>
//
///////////////////////////////////////////////////////////////////////////////

#ifndef DEBUG_TRACING_H
#define DEBUG_TRACING_H

#include <iostream>
#include <sstream>
#include <vector>
#include <chrono>
#include <string>
#include <set>

namespace tracing
{
    /**
     * trace implements tracing by logging debug messages and displaying only the selected.
     * By default it outputs timestamps in fractions of a second.
     * See other time-unit specific aliases below.
     */
    template <typename time_units = std::chrono::duration<double>>
    class trace_t
    {
        std::ostream& os;                      // default output stream
        bool record{false}, show_time{false};  // whether the time stamp is printed
        struct actor_t
        {  // details about the actors
            std::string name;
            bool visible;
            actor_t(std::string name, bool visible): name{std::move(name)}, visible{visible} {};
        };
        std::vector<actor_t> actors;
        using time_point = std::chrono::time_point<std::chrono::high_resolution_clock>;
        time_point t0;  // the beginning of the trace
        struct entry_t
        {
            time_point ts;
            const actor_t& id;
            std::string msg;
            entry_t(time_point ts, const actor_t& id, std::string msg):
                ts{ts}, id{id}, msg{std::move(msg)}
            {}
        };
        std::vector<entry_t> entries;  // the log entries

    public:
        trace_t(std::ostream& os, bool record = false, bool show_time = false):
            os{os}, record{record}, show_time{show_time},
            t0{std::chrono::high_resolution_clock::now()}
        {
            auto id = add_("TRC");
            log(os, id, "started");
        }
        /** adds an actor and returns its identifier */
        size_t add(std::string actor)
        {
            auto id = actors.size();
            actors.emplace_back(std::move(actor), true);
            return id;
        }
        /** adds hidden actor (does not show up in output) */
        size_t add_(std::string actor)
        {
            auto res = actors.size();
            actors.emplace_back(std::move(actor), false);
            return res;
        }
        bool shown(size_t actor) { return actors[actor].visible; }
        void show(size_t actor, bool visible)
        {
            assert(actor < actors.size());
            actors[actor].visible = visible;
        }
        template <typename... Msg>
        trace_t& operator()(size_t actor, const Msg&... msg)
        {
            return log(os, actor, msg...);
        }
        template <typename units = time_units, typename... Msg>
        trace_t& log(std::ostream& os, size_t actor, const Msg&... msg)
        {
            auto ts = std::chrono::high_resolution_clock::now();
            assert(actor < actors.size());
            auto ss = std::ostringstream{};
            (ss << ... << msg);
            entries.emplace_back(ts, actors[actor], std::move(ss.str()));
            print<units>(os, entries.back());
            if (!record)
                entries.clear();
            return *this;
        }
        template <typename units = time_units>
        void print(std::ostream& os, const entry_t& entry) const
        {
            if (entry.id.visible) {
                if (show_time)
                    os << std::chrono::duration_cast<time_units>(entry.ts - t0).count() << ' ';
                os << '[' << entry.id.name << "] " << entry.msg << std::endl;
            }
        }
        template <typename units = time_units>
        void dump(std::ostream& os) const
        {
            for (auto& entry : entries)
                print<units>(os, entry);
        }
    };

    using trace_us = trace_t<std::chrono::microseconds>;
    using trace_ms = trace_t<std::chrono::milliseconds>;
    using trace_s = trace_t<std::chrono::seconds>;
}  // namespace tracing

#endif /* DEBUG_TRACING_H */
