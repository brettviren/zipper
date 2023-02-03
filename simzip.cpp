// A simcpp20-based simulation of a hiearchy of zippers and other messages processors.
//
// Eg, in source directory:
// $ git clone https://github.com/fschuetz04/simcpp20.git
// $ waf configure --simulation=simcpp20
// $ waf build --target=simzip
// $ ./build/simzip

#include "zipper.hpp"
#include "simzip/util.hpp"

#include <iostream>
#include <random>
#include <chrono>
#include <iomanip>


void usage()
{
    exit (1);
}

// Mock up a trigger xxx message
struct Payload {
    int a{0}, b{0}, c{0}, d{0}, e{0}, f{0};
};
using message_t = zipper::Node<Payload>;
using merge_t = zipper::merge<message_t>;

using event_t = simcpp20::event<>;
using sim_t = simcpp20::simulation<>;
using mqueue_t = simzip::bufque<message_t>;

std::ostream& operator<<(std::ostream& o, const message_t& m)
{
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(m.debut.time_since_epoch()).count();
    o << m.identity << " #" << m.ordering << " @" << ms << "us";
    return o;
}
std::ostream& operator<<(std::ostream& o, const sim_t& sim)
{
    const auto defprec {o.precision()};
    o << "[" << std::setw(4) << std::setprecision(2) << sim.now() << std::setprecision(defprec) << "] ";
    return o;
}

// There are three times involved.
//
// 1. The simcpp20 simulation time is represented by a double.  We
// will take its clock to be in units of seconds.
const double sim_time_second = 1.0;

// 2. The zipper "ordering" parameter is represented by size_t and is
// a hardware clock count we will call "tick".  We assume one tick is 
const double tick_period = 1e-6*sim_time_second;

// 3. The zipper "debut" and "now" represent "real" (system or here
// sim) time and is represented by std::chrono timepoint. 


struct inbox_t {
    std::shared_ptr<mqueue_t> inbox;
    mqueue_t::pop_event_t operator()() { return inbox->pop(); }
};
struct outbox_t {
    std::shared_ptr<mqueue_t> outbox;
    mqueue_t::push_event_t operator()(message_t msg) { return outbox->push(msg); }
};
struct delay_t {
    std::default_random_engine& rangen;
    // in sim time
    std::exponential_distribution<> & dist;
    double operator()() { return dist(rangen); }
};
message_t::timepoint_t timepoint(double now)
{
    auto ddur = std::chrono::duration<double>(now);
    auto tdur = std::chrono::duration_cast<message_t::timepoint_t::duration>(ddur);
    return message_t::timepoint_t(tdur);
}


struct source_stat_t {
    size_t count{0};
};
event_t source(sim_t& sim, source_stat_t& stat, size_t ident, outbox_t out, delay_t del)
{
    while (true) {

        // simulate random wait for some TP activity
        const double delay = del();
        // std::cerr << sim << ident << " source delay " << delay << "\n";
        co_await sim.timeout(delay);

        const double now = sim.now();
        const auto debut = timepoint(now);

        size_t ticks = now / tick_period;
        message_t msg{{}, ticks, ident, debut};
        // note, debut should be rewritten just prior to zipper to
        // reflect delays from here to there.

        // This will block when outbox gets stuffed.  We could add
        // here a timeout + event abort to simulate loss.
        co_await out(msg);
        // std::cerr << sim << ident << " -> " << msg << "\n";
        ++stat.count;
    }
}
    
struct sink_stat_t {
    size_t count{0};
};
event_t sink(sim_t& sim, sink_stat_t& stat, size_t ident, inbox_t in)
{
    while (true) {
        // std::cerr << sim << ident << " sink wait\n";
        co_await in();
        ++stat.count;
        // std::cerr << sim << ident << " <- " << got << "\n";
    }
}

struct transfer_stat_t {
    size_t count{0};
};
event_t transfer(sim_t& sim, transfer_stat_t& stat, size_t ident, inbox_t in, outbox_t out, delay_t del)
{
    while (true) {
        auto msg = (co_await in());
        co_await sim.timeout(del());
        co_await out(msg);
        ++stat.count;
    }
}

struct zipit_stat_t {
    size_t in_count{0}, out_count{0};
};
event_t zipit(sim_t& sim, zipit_stat_t& stat, size_t ident, inbox_t in, outbox_t out)
{
    merge_t zip(10);            // fixme latency

    while (true) {
        auto msg = (co_await in());

        ++stat.in_count;
        const double now = sim.now();
        const auto debut = timepoint(now);
        msg.debut = debut;
        bool ok = zip.feed(msg);
        if (!ok) {
            std::cerr << "our ordering is broken\n";
        }

        std::vector<message_t> got;
        zip.drain_prompt(std::back_inserter(got), debut);
        for (const auto& one : got) {
            co_await out(one);
            ++stat.out_count;
        }
    }
        
}
int main(int argc, char* argv[])
{
    const merge_t::duration_t max_latency = std::chrono::microseconds(1000);
    const size_t buffer_size = 1000;

    simcpp20::simulation<> sim;
    auto s1 = std::make_shared<mqueue_t>(sim, buffer_size);
    auto s2 = std::make_shared<mqueue_t>(sim, buffer_size);

    // mean period of TP generation in sim time (seconds);
    const double lifetime = 0.0001;
    std::default_random_engine rangen(123456);
    std::exponential_distribution<> expo(1.0/lifetime);

    delay_t sdel{rangen, expo};
    // delay_t tdel{rangen, expo};
    
    source_stat_t source_stat;
    source(sim, source_stat, 1, {s1}, sdel);
    // transfer_stat_t transfer_stat;
    // transfer(sim, transfer_stat, 1, {s1}, {s2}, tdel);
    zipit_stat_t zipit_stat;
    zipit(sim, zipit_stat, 1, {s1}, {s2});
    sink_stat_t sink_stat;
    sink(sim, sink_stat, 1, {s2});

    sim.run_until(10);

    std::cerr << source_stat.count << " -> "
              // << transfer_stat.count << " -> "
              << zipit_stat.in_count << "/"
              << zipit_stat.out_count << " -> "
              << sink_stat.count << "\n";
    return 0;
}
