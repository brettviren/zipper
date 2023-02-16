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
using event_t = simcpp20::event<>;

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


// value_event_t any_of(simcpp20::simulation<>& sim, std::vector<value_event_t> evs)
// {
//     message_t dummy{"dummy",42};
//     if (evs.size() == 0) {
//         return sim.timeout<message_t>(0, dummy);
//     }

//     for (const auto &ev : evs) {
//         if (ev.processed()) {
//             return sim.timeout<message_t>(0, dummy);
//         }
//     }

//     auto any_of_ev = sim.event<message_t>();

//     for (const auto &ev : evs) {
//         ev.add_callback(
//             [any_of_ev, dummy](const auto & other) mutable { any_of_ev.trigger(other.value()); });
//     }

//     return any_of_ev;
// }

template<typename ValueType>
simcpp20::value_event<ValueType>
any_of(simcpp20::simulation<>& sim, std::vector<simcpp20::value_event<ValueType>> ves)
{
    ValueType dummy;

    if (ves.size() == 0) {
        return sim.timeout<ValueType>(0, dummy);
    }

    for (const auto &ve : ves) {
        if (ve.processed()) {
            return ve;
            // return sim.timeout<ValueType>(0, dummy);
        }
    }

    auto any_of_ve = sim.event<ValueType>();

    for (const auto &ve : ves) {
        ve.add_callback(
            [any_of_ve, ve](const auto & other) mutable {
                any_of_ve.trigger(ve.value()); });
    }

    return any_of_ve;
}

simcpp20::event<> consumer2(simcpp20::simulation<> &sim)
{
    message_t msg{"from consumer2", 1};
    message_t msg1{"msg1", 1};
    message_t msg2{"msg2", 2};
    // auto got = co_await any_of(sim, {ev, sim.timeout<message_t>(1, msg)});
    // auto got = co_await(sim.timeout<int>(1,1) | sim.timeout<int>(2,2));
    // std::vector<value_event_t> evs = {sim.timeout<message_t>(1,msg1),
    //                                   sim.timeout<message_t>(2,msg2)};
    // auto got = co_await any_of(sim, evs) ;
    // auto got = co_await either(sim, sim.timeout(1), sim.timeout(2,msg)); 
    std::vector<value_event_t> ve = {sim.timeout<message_t>(1,msg1),
                                     sim.timeout<message_t>(2,msg2)};
    auto got = co_await any_of<message_t>(sim,ve);
    std::cerr << "consumer2: " << got << std::endl;
}

int main() {
    simcpp20::simulation<> sim;
    consumer(sim);
    message_t msg{"from main", 42};

    consumer2(sim);
    sim.run();
    return 0;
}
