// Setup a layered hierarchy of zippers to explore buffer capacity and latency

#include "zipper.hpp"

#include <numeric>              // iota
#include <algorithm>            // transform, sort
#include <vector>
#include <iostream>
#include <random>
#include <cassert>

void usage()
{
    std::cerr << "Run layers of zippers forming a hiearchy.\n"
              << "\n"
              << "    hier_zipper C_1 C_2 ... C_i ... C_{n-1} C_n\n"
              << "\n"
              << "The C_i is cardinality of layer 'i'\n"
              << "Layer 1 has L_1 = 1 zipper node, \n"
              << "layer i+1 has L_{i+1} = L_i*C_i zipper nodes\n"
              << "Each zipper node of layer n is fed by C_n sources\n";
    exit(1);
}

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
using layer_t = std::vector<merge_t>;
using hier_t = std::vector<layer_t>;

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

template<typename Distribution, typename RandomEngine>
struct Source {
    Distribution& dist;
    RandomEngine& rand;
    node_t node;
 
    Source(Distribution& d, RandomEngine& re) : dist(d), rand(re) {}
    const node_t& operator()() {
        node.debut += dist(rand);
        ++node.ordering;
        return node;
    }
};
using source_t = Source<std::default_random_engine, std::exponential_distribution<> >;
using sources_t = std::vector<source_t>;

int main(int argc, char* argv[])
{
    if (argc < 2) {
        usage();
    }
    const size_t nlayers = argc - 1;
    std::vector<size_t> cards(nlayers);
    for (int iarg=1; iarg < argc; ++iarg) {
        const size_t layer = iarg-1;
        cards[layer] = atoi(argv[iarg]);
    }

    hier_t hier(nlayers);
    
    const merge_t::duration_t max_latency = std::chrono::microseconds(1000);

    size_t ninlayer = 1;
    for (size_t layer=0; layer<nlayers; ++layer) {
        const auto card = cards[layer];
        for (size_t ind=0; ind<ninlayer; ++ind) {
            hier[layer].emplace_back(card, max_latency);
        }
        std::cerr << "L=" << layer+1 << " N=" << ninlayer << " C=" << card << "\n";
        ninlayer *= card;
    }

    // mean lifetime between source producing
    const double lifetime = 100;

    std::default_random_engine rand(123456);
    std::exponential_distribution<> expo(lifetime);
    
    const size_t nsources = hier.back().size() * cards.back();
    sources_t sources;
    for (size_t ind=0; ind<nsources; ++ind) {
        sources.emplace_back(rand, expo);
    }
    std::cerr << "N_sources = " << nsources <<"\n";


    // excercise the hierarchy

    // We keep track of a "now" time


    return 0;
}
