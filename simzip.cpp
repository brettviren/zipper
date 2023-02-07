// A simcpp20-based simulation of a hiearchy of zippers and other messages processors.
//
// This program communicates via stdin/stdout via JSON text.
// See simzip/examples.jsonnet for example how to generate input.
// See xxxx.yyy for how to consume output.

#include "zipper.hpp"
#include "simzip/util.hpp"
#include "nlohmann/json.hpp"

#include <iostream>
#include <random>
#include <chrono>
#include <iomanip>
#include <unordered_map>
#include <fstream>

void usage()
{
    std::cerr << "simzip [-|input.json] [-|output.json]\n";
    exit (1);
}

// Mock up a trigger xxx message.  
struct Payload {
    int a{0}, b{0}, c{0}, d{0}, e{0}, f{0};
};
using message_t = zipper::Node<Payload>; // NOT a pgraph "node"
using merge_t = zipper::merge<message_t>;

using event_t = simcpp20::event<>;
using sim_t = simcpp20::simulation<>;
using mqueue_t = simzip::bufque<message_t>;

using cfg_t = nlohmann::json;


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

message_t::timepoint_t timepoint(double now)
{
    auto ddur = std::chrono::duration<double>(now);
    auto tdur = std::chrono::duration_cast<message_t::timepoint_t::duration>(ddur);
    return message_t::timepoint_t(tdur);
}


std::string make_key(const std::string& type, const std::string& name)
{
    if (type.empty()) {
        throw std::invalid_argument("node type must be specified");
    }
    if (name.empty()) {
        return type;
    }
    return type + ":" + name;
}
std::string make_key(const cfg_t& cfg)
{
    // std::cerr << "make_key: " << cfg << std::endl;
    std::string type = cfg["type"];
    std::string name = cfg.value("name","");
    return make_key(type, name);
}

std::string make_edge_key(const cfg_t& edge)
{
    const std::string tkey = edge["tail"]["node"];
    const std::string hkey = edge["head"]["node"];

    const size_t tind = edge["tail"]["port"];
    const size_t hind = edge["head"]["port"];

    std::stringstream ss;
    ss << tkey << ":" << tind << "->" << hkey << ":" << hind;
    return ss.str();
}


bool is_random(const cfg_t& cfg)
{
    if (cfg.value("type","") == "random") {
        return true;
    }
    return false;
}
bool is_service(const cfg_t& cfg)
{
    return is_random(cfg);
}
bool is_edge(const cfg_t& cfg)
{
    if (cfg.contains("head") and cfg.contains("tail")) return true;
    return false;
}

// A shareable ports.  A node must only pop on input ports and only
// push on outputports.
using port_t = std::shared_ptr<mqueue_t>;
using ports_t = std::vector<port_t>;


// Collect rand dists keep node functions brief, reuse engine.
struct rando_t {
    virtual double operator()() = 0;
};
struct rando_expo_t : public rando_t {
    std::default_random_engine& rangen;
    std::exponential_distribution<> dist;
    rando_expo_t(std::default_random_engine& r, double lifetime)
        : rangen(r)
        , dist(1.0/lifetime) {}

    virtual double operator()() { return dist(rangen); }
};
struct rando_fixed_t : public rando_t {
    double value;
    rando_fixed_t(double v) : value(v) {}
    virtual double operator()() { return value; }
};
struct rnd_t {
    using rando_ptr = std::shared_ptr<rando_t>;
    std::unordered_map<std::string, rando_ptr> dists;
    std::default_random_engine rangen;

    rnd_t(size_t seed=123456) : rangen(seed)
    {
        dists["random:zeros"] = std::make_shared<rando_fixed_t>(0.0);
    }

    rando_t& declare(const cfg_t& cfg) {
        std::string type = cfg["type"];
        if (type != "random")
        {
            throw std::invalid_argument("not a rando: " + type);
        }

        std::string name = cfg["name"];
        std::string dtype = cfg["data"]["distribution"];

        auto key = make_key(type, name);

        if (dtype == "exponential") {
            double lifetime = cfg["data"]["lifetime"];
            dists[key] = std::make_shared<rando_expo_t>(rangen, lifetime);
            return *dists[key];
        }
        if (dtype == "fixed") {
            double value = cfg["data"].value("value", 0.0);
            dists[key] = std::make_shared<rando_fixed_t>(value);
            return *dists[key];
        }

        throw std::invalid_argument("unsupported distribution: " + key);
    }
    rando_t& operator()(const std::string& dtype, const std::string& name) {
        std::string key = make_key(dtype, name);
        return (*this)(key);
    }
    rando_t& operator()(const std::string& key) {
        auto got = dists.find(key);
        if (got == dists.end()) {
            throw std::invalid_argument("no rando at " + key);
        }
        return *(got->second);
    }
};




ports_t make_ports(sim_t& sim, const cfg_t& node, const std::string which)
{
    ports_t ret;

    if (!node.contains("data")) return ret;
    const auto& data = node["data"];

    if (!data.contains(which)) return ret;
    auto got = data[which];

    if (got.is_null()) {
        return ret;
    }
    if (got.is_number()) {
        size_t capacity = got;
        got = cfg_t::array({capacity});
    }
    for (const auto one : got) {
        size_t capacity = one;
        ret.push_back(std::make_shared<mqueue_t>(sim, capacity));
    }
    return ret;

}

// Model a pgraph node.
struct pnode_t {

    // The type/name/data object
    cfg_t cfg;

    // Vector of input and output ports.
    ports_t iports, oports;

    pnode_t() = default;

    // Pnode owning ports
    pnode_t(sim_t& sim, const cfg_t& cfg)
        : cfg(cfg)
        , iports(make_ports(sim, cfg, "ibox"))
        , oports(make_ports(sim, cfg, "obox"))
    {
    }

    // Pnode using other ports
    pnode_t(const cfg_t& cfg,
            ports_t iports, ports_t oports)
        : cfg(cfg), iports(iports), oports(oports) {}
            
};

// Every pgraph node is a coroutine called like this.  The function
// may modify pnode.cfg.
using node_t = std::function<event_t(sim_t& sim, pnode_t& pnode, rnd_t& rnd)>;


// A source that produces sequence of messages out all its ports.
// Expect this in pnode.cfg["data"]:
// - delay :: a random <distribution>:<name> for a delay 
event_t source(sim_t& sim, pnode_t& pnode, rnd_t rnd) 
{
    static size_t ident = 0;
    ++ident;

    // std::cerr << sim << "source: " << pnode.cfg << "\n";
    auto delay = pnode.cfg["data"].value("delay", "fixed");
    auto& del = rnd(delay);
    // std::cerr << sim << "source: " << delay << "\n";

    size_t count = 0;
    while (true) {

        const double delay = del();
        co_await sim.timeout(delay);

        const double now = sim.now();
        const auto debut = timepoint(now);

        size_t ticks = now / tick_period;
        message_t msg{{}, ticks, ident, debut};
        // note, debut should be rewritten just prior to zipper to
        // reflect delays from here to there.

        // This will block when outbox gets stuffed.  We could add
        // here a timeout + event abort to simulate loss.
        for (auto& port : pnode.oports) {
            co_await port->push(msg);
        }

        // std::cerr << sim << ident << " -> " << msg << "\n";
        ++count;
        pnode.cfg["data"]["count"] = count;
    }
}
    
// A sink with a single input port
event_t sink(sim_t& sim, pnode_t& pnode, rnd_t rnd)
{
    size_t count = 0;
    while (true) {
        auto msg = co_await pnode.iports[0]->pop();
        ++count;
        pnode.cfg["data"]["count"] = count;
        pnode.cfg["data"]["last_seen"] = msg.identity;
    }
}

// A transfer is a special node type that connects a tail port to a
// head port.  It crudely models transport such as across TCP.
// It's configuration is that of an EDGE
event_t transfer(sim_t& sim, pnode_t& pnode, rnd_t rnd)
{
    // std::cerr << sim << "transfer: " << pnode.cfg << "\n";

    std::string rando = pnode.cfg.value("delay","random:zeros");
    auto& del = rnd(rando);

    // std::cerr << sim << "transfer: trial delay: " << del() << "\n";

    size_t count = 0;

    while (true) {

        const double delay = del();

        auto msg = (co_await pnode.iports[0]->pop());
        co_await sim.timeout(delay);
        co_await pnode.oports[0]->push(msg);
        ++count;
        pnode.cfg["data"]["count"] = count;
    }
}

// A node running a zipper.  This has a single input and output port.
event_t zipit(sim_t& sim, pnode_t& pnode, rnd_t rnd)
{
    static size_t ident = 0;
    ++ident;

    size_t cardinality = pnode.cfg["data"].value("cardinality", 10);
    merge_t::duration_t max_latency = timepoint(pnode.cfg["data"].value("max_latency", 0.0)).time_since_epoch();
    merge_t zip(cardinality, max_latency);

    size_t in_count{0}, out_count{0};

    while (true) {
        auto msg = (co_await pnode.iports[0]->pop());

        ++in_count;
        const double now = sim.now();
        const auto debut = timepoint(now);
        msg.debut = debut;
        bool ok = zip.feed(msg);
        if (!ok) {
            std::cerr << "our ordering is broken\n";
        }

        std::vector<message_t> got;
        zip.drain_prompt(std::back_inserter(got), debut);
        for (auto one : got) {
            one.identity = ident;
            // fixme: reset debug
            co_await pnode.oports[0]->push(one);
            ++out_count;
        }

        pnode.cfg["in_count"] = in_count;
        pnode.cfg["out_count"] = out_count;
    }        
}

struct pnode_store_t {
    sim_t& sim;

    using pnodes_t = std::unordered_map<std::string, pnode_t>;
    pnodes_t store;

    pnode_t& get(const std::string& key) 
    {
        return const_cast<pnode_t&>(const_cast<const pnode_store_t*>(this)->get(key));
    }
    const pnode_t& get(const std::string& key) const
    {
        auto got = store.find(key);
        if (got == store.end()) {
            throw std::invalid_argument("no such pnode: " + key);
        }
        return got->second;
    }

    // set a "regular" node, return its key
    std::string set(const cfg_t& node)
    {
        auto key = make_key(node);
        store.emplace(key, pnode_t(sim, node));
        return key;
    }

    // set a special node representing an edge between regular nodes
    std::string set_edge(const cfg_t& edge)
    {
        const std::string tkey = edge["tail"]["node"];
        const std::string hkey = edge["head"]["node"];

        auto tnode = get(tkey);
        auto hnode = get(hkey);

        const size_t tind = edge["tail"]["port"];
        const size_t hind = edge["head"]["port"];

        auto tport = tnode.oports.at(tind);
        auto hport = hnode.iports.at(hind);

        auto ekey = make_edge_key(edge);
        
        store.emplace(ekey, pnode_t(edge, {tport}, {hport}));

        return ekey;
    }
};

struct context_t
{
    simcpp20::simulation<> sim;

    using node_types_t = std::unordered_map<std::string, node_t>;
    node_types_t node_types = {
        {"source",source}, {"sink",sink}, {"transfer",transfer}, {"zipper",zipit}
    };

    cfg_t cfg;
    rnd_t rnd;

    pnode_store_t pnodes;


    context_t(const cfg_t& c)
        : cfg(c)
        , rnd(c["main"].value("seed", 123456))
        , pnodes{sim}
    {
        // Loop once to dispatch non-node configurations or build
        // ports and params for nodes.
        for (auto& node : cfg["nodes"]) {
            if (is_random(node)) {
                rnd.declare(node);
                continue;
            }
            pnodes.set(node);
        }

        // Link up edges
        for (auto& edge : cfg["edges"])
        {
            auto ekey = pnodes.set_edge(edge);
            auto& pnode = pnodes.get(ekey);

            transfer(sim, pnode, rnd);
                
        }

        // Loop again to execute nodes
        for (auto& node : cfg["nodes"]) {
            if (is_service(node)) {
                continue;
            }

            auto& pnode = pnodes.get(make_key(node));
            std::string type = pnode.cfg["type"];
            auto func = node_types[type];

            // Call coroutine
            func(sim, pnode, rnd);
        }
    }

    void run()
    {
        double run_time = cfg["main"].value("run_time", 1.0);
        sim.run_until(run_time);
    }

    cfg_t state() const
    {
        cfg_t nodes = cfg_t::array();
        for (auto node : cfg["nodes"]) {
            if (is_service(node)) {
                // "services" don't mutate their state
                nodes.push_back(node);
                continue;
            } 
            auto key = make_key(node);
            const auto& pnode = pnodes.get(key);
            node.update(pnode.cfg, true);
            nodes.push_back(node);
        }

        cfg_t edges = cfg_t::array();
        for (auto edge : cfg["edges"]) {
            auto ekey = make_edge_key(edge);
            const auto& enode = pnodes.get(ekey);
            edge.update(enode.cfg, true);
            edges.push_back(edge);
        }

        cfg_t ret = cfg;
        ret["nodes"] = nodes;
        ret["edges"] = edges;
        return ret;
    }

};


int main(int argc, char* argv[])
{
    std::string ifname = "/dev/stdin";
    if (argc > 1 and argv[1][0] != '-') {
        ifname = argv[1];
    }
    cfg_t cfg = cfg_t::parse(std::ifstream(ifname));

    std::string ofname = "/dev/stdout";
    if (argc > 2 and argv[2][0] != '-') {
        ofname = argv[2];
    }

    context_t ctx(cfg);
    ctx.run();


    std::ofstream out(ofname);
    out << std::setw(4) << ctx.state() << std::endl;

    return 0;
}
