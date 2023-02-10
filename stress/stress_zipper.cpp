// Run zipper hard with semi realistic payload to stress and profile.

#include "zipper.hpp"

#include <numeric>              // iota
#include <algorithm>            // transform, sort
#include <vector>
#include <iostream>

#include <cassert>

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

const int nchunks=100;

node_t init_node(size_t identity)
{
    static size_t ordering = 0;
    ++ordering;
    Payload payload;
    payload.chunks.resize(nchunks);
    return node_t{payload, ordering, identity, merge_t::clock_t::now()};
}


node_t make_node(size_t identity, size_t last_ordering = 0);
node_t make_node(size_t identity, size_t last_ordering)
{
    size_t ordering = last_ordering + 1; // make more variable.
    Payload payload;
    payload.chunks.resize(nchunks);
    return node_t{payload, ordering, identity, merge_t::clock_t::now()};
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

    std::vector<size_t> streamid(nstreams);
    std::iota(streamid.begin(), streamid.end(), 0);

    std::vector<node_t> stream;;
    std::transform(streamid.begin(), streamid.end(),
                   std::back_inserter(stream), init_node);

    merge_t zm(nstreams);

    auto t0 = std::chrono::steady_clock::now();
    std::chrono::nanoseconds zmdt;
    for (int count = 0 ; count < nsend; ++count) {

        // find next "active" stream
        std::sort(stream.begin(), stream.end(), sorder);
        node_t node = stream.back();

        // make next "produced" node in the stream
        stream.back() = make_node(node.identity, node.ordering);
        
        auto ta = std::chrono::steady_clock::now();
        // feed the merge
        bool accepted = zm.feed(node);

        // lossless mode for now
        assert(accepted);       

        // drain the merge
        std::vector<node_t> got;
        zm.drain_waiting(std::back_inserter(got));
        auto tb = std::chrono::steady_clock::now();
        zmdt += tb-ta;
    }

    auto t1 = std::chrono::steady_clock::now();
    double dt = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    double rate = nsend / dt;
    std::cerr << "Nstream=" << nstreams << ", Nsend="<< nsend*1e-6 << " M" << ", Nchunks=" << nchunks << std::endl;
    std::cerr << "Tot: " << dt*1e-6 << " s, " << rate << " MHz" << std::endl;

    double zmus =  std::chrono::duration_cast<std::chrono::microseconds>(zmdt).count();
    double zmrate = nsend / zmus;
    std::cerr << "Zip: " << zmus*1e-6 << " s, " << zmrate << " MHz" << std::endl;

    return 0;
}
