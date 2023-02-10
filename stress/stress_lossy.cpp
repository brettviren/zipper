// Run zipper hard with semi realistic payload to stress and profile.
// This version runs in lossy, latency bounding mode.

#include "zipper.hpp"

#include <numeric>              // iota
#include <algorithm>            // transform, sort
#include <vector>
#include <iostream>
#include <thread> 
#include <cassert>
#include <sstream>

// mock of trigger primitive and its set
struct Chunk {
    int u, v, w, x, y, z;
};
struct Payload {
    int a{0}, b{0}, c{0}, d{0}, e{0}, f{0};
    std::vector<Chunk> chunks{};
};


using node_t = zipper::Node<Payload>;
using merge_t = zipper::merge<node_t>;

std::string tostr(const node_t& node) {
    std::stringstream ss;
    ss << "node: #" << node.identity << " @" << node.ordering;
    return ss.str();
}

const int nchunks=100;

node_t init_node(size_t identity)
{
    static size_t ordering = 0;
    ++ordering;
    Payload payload;
    payload.chunks.resize(nchunks);
    return node_t{payload, ordering, identity, merge_t::clock_t::now()};
}


node_t make_node(size_t identity, size_t last_ordering,
                 node_t::timepoint_t tnow)
{
    size_t ordering = last_ordering + 1; // make more variable.
    Payload payload;
    payload.chunks.resize(nchunks);
    return node_t{payload, ordering, identity, tnow};
}

// sort nodes in descending order value
bool sorder(const node_t& a, const node_t& b)
{
    return a.ordering > b.ordering;
}

int main()
{
    const int nstreams=10;
    const int nsend=10000000;
    size_t nlost = 0;

    std::vector<size_t> streamid(nstreams);
    std::iota(streamid.begin(), streamid.end(), 0);

    std::vector<node_t> stream;;
    std::transform(streamid.begin(), streamid.end(),
                   std::back_inserter(stream), init_node);

    merge_t zm(nstreams, std::chrono::microseconds(1000));

    auto t0 = merge_t::clock_t::now();
    std::chrono::nanoseconds zmdt{0};
    for (int count = 0 ; count < nsend; ++count) {

        // find next "active" stream
        std::sort(stream.begin(), stream.end(), sorder);
        node_t node = stream.back();

        auto tnow = merge_t::clock_t::now();

        // are you from the future?
        while (node.debut > tnow) {
            std::this_thread::sleep_for (std::chrono::microseconds(1));
            tnow = merge_t::clock_t::now();
            
            auto diff = tnow.time_since_epoch().count() -
                node.debut.time_since_epoch().count();
            std::cerr << "waiting: " << diff << std::endl;
        }

        // Given sad Mr. 1 a slow down
        auto delay = std::chrono::microseconds(1);
        // if (node.identity == 1 and count %3 == 1) {
        //     delay = std::chrono::microseconds(10);
        // }

        stream.back() = make_node(node.identity, node.ordering,
                                  tnow + delay);

        // feed the merge
        bool accepted = zm.feed(node);

        if (!accepted) {
            ++nlost;
        }

        // drain the merge
        std::vector<node_t> got;
        zm.drain_prompt(std::back_inserter(got));
        auto tlater = merge_t::clock_t::now();
        zmdt += tlater-tnow;
        // std::cerr << "drained: " << got.size() << " " << zm.size() << std::endl;
    }

    auto t1 = merge_t::clock_t::now();
    double dt = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    double rate = nsend / dt;
    std::cerr << "Nstream=" << nstreams
              << ", Nsend="<< nsend*1e-6 << " M"
              << ", Nchunks=" << nchunks
              << ", Nlost="<< nlost << ", Nleft=" << zm.size()
              << std::endl;
    std::cerr << "Tot: " << dt*1e-6 << " s, " << rate << " MHz" << std::endl;

    double zmus =  std::chrono::duration_cast<std::chrono::microseconds>(zmdt).count();
    double zmrate = nsend / zmus;
    std::cerr << "Zip: " << zmus*1e-6 << " s, " << zmrate << " MHz" << std::endl;

    return 0;
}
