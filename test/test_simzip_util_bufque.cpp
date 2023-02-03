#include "simzip/util.hpp"

#include <iostream>
#include <iomanip>
using namespace simzip;

struct message_t {
    std::string s{""};
    int i{0};
};

std::ostream& operator<<(std::ostream& o, const message_t& m)
{
    o << m.s << " " << m.i;
    return o;
}

using mqueue_t = simzip::bufque<message_t>;
using sim_t = simcpp20::simulation<>;

std::ostream& operator<<(std::ostream& o, const sim_t& sim)
{
    const auto defprec {o.precision()};
    o << "[" << std::setw(4) << std::setprecision(2) << sim.now() << std::setprecision(defprec) << "] ";
    return o;
}

std::ostream& operator<<(std::ostream& o, const mqueue_t& mq)
{
    o << "(" << mq.inbox_size() << " -> " << mq.buffer_size()
    << "/" << mq.capacity() << " -> " << mq.outbox_size() << ") ";
    return o;
}

simcpp20::event<> test_push(sim_t& sim, mqueue_t& mq, const std::string& what="foo", int num=1) 
{
    for (int ind=0; ind<num; ++ind) {
        message_t m{what,ind};
        std::cerr << sim << mq << "pushing "<<m<<"\n";
        co_await sim.timeout(0.1);
        co_await mq.push(m);
        std::cerr << sim << mq << "pushed "<<m<<"\n";
    }
    //return sim.timeout(0);
}
simcpp20::event<> test_pop(sim_t& sim, mqueue_t& mq, int num=1)
{
    for (int ind=0; ind<num; ++ind) {
        auto e = mq.pop();
        auto m = co_await e;
        co_await sim.timeout(1);
        std::cerr << sim << mq << "popped " << m << "\n";
    }
//    sim.timeout(0);
}
int main()
{
    simcpp20::simulation<> sim;
    mqueue_t mq(sim, 2);
    test_push(sim, mq, "foo", 5);
    test_pop(sim, mq, 2);
    test_push(sim, mq, "bar", 3);
    test_pop(sim, mq, 4);
    std::cerr << sim << mq << "running\n";
    sim.run_until(10);
    std::cerr << sim << mq << "done\n";
    return 0;
}
