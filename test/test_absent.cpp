// issue #1
//
// Make a cardinality 2 zipper, feed it with only one stream,
// prompt-drain it (lossy, max latency guarantee).

#include "zipper.hpp"

#include <chrono>
#include <cassert>
#include <iostream>

using node_t = zipper::Node<int>;
using merge_t = zipper::merge<node_t>;

// Convert to time point
merge_t::timepoint_t ms(int num)
{
    merge_t::timepoint_t ret;
    ret += std::chrono::milliseconds(num);
    return ret;
}

template<typename Timepoint>
size_t nms(Timepoint timepoint)
{
    const Timepoint origin{};
    return std::chrono::duration_cast<std::chrono::milliseconds>(timepoint-origin).count();
}

const node_t::timepoint_t do_feed(int num, merge_t& mq, node_t::identity_t identity)
{
    const node_t::payload_t payload = num;
    const node_t::ordering_t ordering = num;
    const node_t::timepoint_t timepoint = ms(100*num);
    
    auto dt = nms(timepoint);
    bool ok = mq.feed(payload, ordering, identity, timepoint);
    if (!ok) {
        std::cerr << "lost: " << num << " @ " << dt << std::endl;
    }
    else {
        std::cerr << "feed: " << num << " @ " << dt << std::endl;
    }
    return timepoint;
}

template<typename Drainer>
size_t do_drain(int num, Drainer drainer)
{
    std::vector<node_t> got;

    drainer(std::back_inserter(got));
    if (got.empty()) {
        return 0;
    }
    
    std::cerr << num << "[" << got.size() << "]:";
    for (const auto& one : got) {
        std::cerr << " " << one.payload;
    }
    std::cerr << std::endl;
    return got.size();
}

void do_test(int cardinality)
{
    // A zipper with "1s" max latency
    const auto latency = std::chrono::seconds(1);
    std::cerr << "cardinality=" << cardinality << " latency=" << nms(latency) << std::endl;
    merge_t mq(cardinality, latency);

    // Feed one stream at "0.1s" period
    const int total = 100;
    for (int num=0; num<total; ++num) {

        // if (num == 50) {
        //      do_feed(num, mq, 1);
        // }
        const auto timepoint = do_feed(num, mq, 0);

        bool ok = mq.complete(timepoint);
        if (!ok) {
            std::cerr << "incomplete\n";
        }

        do_drain(num, [&](auto res) { return mq.drain_prompt(res, timepoint); });
        // do_drain(num, [&](auto res) { return mq.drain_waiting(res); });

    }
    auto ngot = do_drain(total, [&](auto res) { return mq.drain_full(res); });
    assert (ngot < total);
    std::cerr << "pass test with cardinality " << cardinality << std::endl;
}

int main() {
    do_test(1);
    do_test(2);
    return 0;
}
