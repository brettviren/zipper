#include "zipper.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <iterator>
#include <chrono>
#include <string>
#include <sstream>

using node_t = zipper::Node<std::string>;
using merge_t = zipper::merge<node_t>;

merge_t::timepoint_t us(int micros)
{
    merge_t::timepoint_t ret;
    ret += std::chrono::microseconds(micros);
    return ret;
}



void fill_some(merge_t& mq)
{
    bool ok = false;

    //          pay, ord, sid, time
    ok = mq.feed("a0", 0, 0, us(0));
    assert(ok);

    ok = mq.feed("b0", 1, 1, us(0));
    assert(ok);

    ok = mq.feed("c0", 2, 2, us(0));
    assert(ok);

    ok = mq.feed("a1", 3, 0, us(1));
    assert(ok);

    ok = mq.feed("b1", 4, 1, us(2));
    assert(ok);
}

void five_then_delay(int delay, int want, int latency)
{
    merge_t mq(3, std::chrono::microseconds(latency));

    fill_some(mq);

    std::vector<node_t> got;
    mq.drain_prompt(std::back_inserter(got), us(delay));

    std::cerr << "got: " << got.size()
              << " left: " << mq.size()
              << " after: " << delay
              << std::endl;
    assert(want == got.size());

    bool ok = mq.feed("c1", 3, 2, us(delay+1));
    if (!ok) {
        std::cerr << "lost c1\n";
    }
    else {
        std::cerr << "added c1\n";
    }
    assert(ok == latency > delay);
    
    mq.drain_prompt(std::back_inserter(got), us(delay+2));
    std::cerr << "got: " << got.size()
              << " left: " << mq.size()
              << std::endl;
}

void test_drain_prompt()
{
    five_then_delay(5, 2, 10);
    five_then_delay(20, 5, 10);
}
int main()
{
    std::cerr << "test_lossy\n";
    test_drain_prompt();
    return 0;
}
