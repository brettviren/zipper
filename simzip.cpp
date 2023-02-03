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

// Each source function's parametrization 
struct source_config_t {
    size_t ident;
    std::shared_ptr<mqueue_t> outbox;
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

event_t source(sim_t& sim, size_t ident, outbox_t out, delay_t del)
{
    for (size_t seqno=0; ; ++seqno) {

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
    }
}

struct inbox_t {
    std::shared_ptr<mqueue_t> inbox;
    mqueue_t::pop_event_t operator()() { return inbox->pop(); }
};
    
event_t sink(sim_t& sim, size_t ident, inbox_t in)
{
    while (true) {
        // std::cerr << sim << ident << " sink wait\n";
        auto got = (co_await in());
        // std::cerr << sim << ident << " <- " << got << "\n";
    }
}

int main(int argc, char* argv[])
{
    const merge_t::duration_t max_latency = std::chrono::microseconds(1000);
    const size_t buffer_size = 1000;

    simcpp20::simulation<> sim;
    auto s1 = std::make_shared<mqueue_t>(sim, buffer_size);

    // mean period of TP generation in sim time (seconds);
    const double lifetime = 0.0001;
    std::default_random_engine rangen(123456);
    std::exponential_distribution<> expo(1.0/lifetime);

    source(sim, 1, {s1}, {rangen, expo});

    sink(sim, 1, {s1});

    sim.run_until(10);
    return 0;
}
