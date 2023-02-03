#include "fschuetz04/simcpp20.hpp"
#include <iostream>
#include <string>

//using message_t = std::string;
struct message_t { std::string s; int i; };
const message_t m1{"hi",1};
const message_t m2{"bye",2};
std::ostream& operator<<(std::ostream& o, const message_t& msg)
{
    o << msg.s << " " << msg.i;
    return o;
}

using value_event_t = simcpp20::value_event<message_t>;


value_event_t producer(simcpp20::simulation<> &sim)
{
//    co_await sim.timeout<message_t>(1, m1);
//    co_return m2;
    // message_t msg{"foo",42};
//    co_return msg;
    auto e = sim.event<message_t>();
    // value_event_t e{sim};
    // e.set_value("foo", 42);
    // e.trigger("foo", 42);
    return e;
}

simcpp20::event<> delayed_trigger(simcpp20::simulation<> &sim, value_event_t e)
{
    std::cerr << "[" << sim.now() << "] delay...\n";
    co_await sim.timeout(2);
    std::cerr << "[" << sim.now() << "] trigger...\n";
    // e.trigger("foo", 42);
    e.trigger(m2);
}
simcpp20::event<> consumer(simcpp20::simulation<> &sim) {
    auto e = producer(sim);
    std::cerr << "[" << sim.now() << "] produced\n";
    delayed_trigger(sim, e);
    std::cerr << "[" << sim.now() << "] waiting...\n";
    auto val = co_await e;
    std::cerr << "[" << sim.now() << "] " << val << "\n";
}

int main() {
    simcpp20::simulation<> sim;
    consumer(sim);
    sim.run();
    return 0;
}
