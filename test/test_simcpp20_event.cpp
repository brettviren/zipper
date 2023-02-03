#include "fschuetz04/simcpp20.hpp"
#include <iostream>

#if 0
struct message_t {
    std::string s{"default"};
    int i{42};
//    message_t(const std::string s, int i) : s(s), i(i) {}
};
std::ostream& operator<<(std::ostream& o, const message_t& m)
{
    o << m.s << " " << m.i;
    return o;
}
#else
using message_t = int;


using event_t = simcpp20::event<>;
using value_t = simcpp20::value_event<message_t>;
using sim_t = simcpp20::simulation<>;

std::ostream& operator<<(std::ostream& o, const sim_t& sim)
{
    o << "[" << sim.now() << "]";
    return o;
}

value_t make_message(sim_t& sim)
{
    message_t m{9}; //{"hi",9};
    std::cerr << sim << " make message " << m << "\n";
    auto mv = sim.event<message_t>();
    // mv.trigger(m);
    std::cerr << sim << " made: "<< m <<"\n";
    // co_return mv;
}

event_t take_message(sim_t& sim)
{
    auto val = co_await make_message(sim);
//    std::cerr << sim << " got: " << val << "\n";
}

void test_triggered(sim_t& sim)
{
    take_message(sim);
    std::cout << sim << " done\n";
}

int main()
{
    simcpp20::simulation<> sim;
    test_triggered(sim);
    sim.run();
}
