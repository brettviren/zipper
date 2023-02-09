// A simcpp20-based simulation of a hiearchy of zippers and other messages processors.
//
// This program communicates via stdin/stdout via JSON text.
// See simzip/examples.jsonnet for example how to generate input.
// See xxxx.yyy for how to consume output.

#include "zipper.hpp"
#include "simzip/util.hpp"
#include "simzip/stats.hpp"

#define JSON_DIAGNOSTICS 1
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
using namespace nlohmann::literals;


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

double timepoint(const message_t::timepoint_t& now)
{
    auto ddur = std::chrono::duration<double>(now.time_since_epoch());
    return ddur.count();
}



// Keep track of message times in the context of a node.
struct time_tracker_t
{
    sim_t& sim;

    // The duration from when the message was created to when it was
    // received by a node.
    simzip::Stats wire;
    // The duration from when a message was held by the node.
    simzip::Stats held;

    // Call after a co_await on pop.  This will set message debut time
    // to simulation now().
    void recv(message_t& m)
    {
        const double then = m.ordering * tick_period;
        const double now = sim.now();        
        wire(now-then);
        m.debut = timepoint(now);
    }

    // Call just before a co_await on push.  This will set message
    // debut time to simulation now().
    void send(message_t& m)
    {
        const double then = timepoint(m.debut);
        const double now = sim.now();        
        held(now-then);
        m.debut = timepoint(now);
    }

    // Dump stats as cfg obj
    cfg_t stats(const simzip::Stats& stats) const
    {
        return cfg_t{
            {"n", stats.S0}, {"mu", stats.mean()}, {"rms", stats.rms()},
            {"str", stats.dump()}
        };
    }
    // Dump self as cfg obj
    cfg_t stats() const
    {
        return cfg_t{
            {"wn", wire.S0},
            {"wmu", wire.mean()},
            {"wrms", wire.rms()},
            {"wstr", wire.dump()},
            {"hn", held.S0},
            {"hmu", held.mean()},
            {"hrms", held.rms()},
            {"hstr", held.dump()},
        };
    }
};


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
        if (iports.empty() and oports.empty()) {
            throw std::invalid_argument("no ports in node:" + cfg.dump());
        }
    }

    // Pnode using other ports
    pnode_t(const cfg_t& cfg,
            ports_t iports, ports_t oports)
        : cfg(cfg), iports(iports), oports(oports) {}
            
};

// Every pgraph node is a coroutine called like this.  The function
// may modify pnode.cfg.
using node_t = std::function<event_t(sim_t& sim, pnode_t& pnode, rnd_t& rnd)>;


// A source that produces a sequence of messages its port.
// Expect this in pnode.cfg["data"]:
// - delay :: a random <distribution>:<name> for a delay 
event_t source(sim_t& sim, pnode_t& pnode, rnd_t rnd) 
{
    time_tracker_t tt{sim};
    const size_t ident = pnode.cfg["/data/ident"_json_pointer];

    // std::cerr << sim << "source: " << pnode.cfg << "\n";
    auto delay = pnode.cfg["data"].value("delay", "fixed");
    auto& del = rnd(delay);
    // std::cerr << sim << "source: " << delay << "\n";

    while (true) {

        const double now = sim.now();        
        const message_t::ordering_t ordering = now / tick_period;
            
        message_t msg{{}, ordering, ident, timepoint(now)};

        /// we are source, recv times are meaningless
        // tt.recv(msg);

        // Now spend some "time" making the message
        const double delay = del();
        co_await sim.timeout(delay);

        tt.send(msg);
        co_await pnode.oports[0]->push(msg);

        // Maybe do this every N calls if this ends up being slow.
        pnode.cfg["data"].update(tt.stats());
    }
}
    
// A sink with a single input port
event_t sink(sim_t& sim, pnode_t& pnode, rnd_t rnd)
{
    time_tracker_t tt{sim};
    while (true) {
        auto msg = co_await pnode.iports[0]->pop();
        tt.recv(msg);
        pnode.cfg["data"].update(tt.stats());
    }
}

// A transfer is a special node type that connects a tail port to a
// head port.  It crudely models transport such as across TCP.
// It's configuration is that of an EDGE
event_t transfer(sim_t& sim, pnode_t& pnode, rnd_t rnd)
{
    time_tracker_t tt{sim};

    std::string rando = pnode.cfg.value("delay","random:zeros");
    auto& del = rnd(rando);

    while (true) {

        const double delay = del();

        auto msg = (co_await pnode.iports[0]->pop());
        tt.recv(msg);

        // simulate transmission delay
        co_await sim.timeout(delay);

        tt.send(msg);
        co_await pnode.oports[0]->push(msg);

        pnode.cfg["data"].update(tt.stats());
    }
}

// A node running a zipper.  This has a single input and output port.
event_t zipit(sim_t& sim, pnode_t& pnode, rnd_t rnd)
{
    time_tracker_t tt{sim};

    const size_t ident = pnode.cfg["/data/ident"_json_pointer];
    std::cerr << "zipper " << pnode.cfg["name"] << " ident=" << ident << "\n";

    size_t cardinality = pnode.cfg.value("/data/cardinality"_json_pointer, 0);
    pnode.cfg["/data/cardinality"_json_pointer] = cardinality;

    double maxlat = pnode.cfg.value("/data/max_latency"_json_pointer, 0.0);
    pnode.cfg["/data/max_latency"_json_pointer] = maxlat;
    merge_t::duration_t max_latency = timepoint(maxlat).time_since_epoch();
    merge_t zip(cardinality, max_latency);

    while (true) {
        auto msg = (co_await pnode.iports[0]->pop());
        tt.recv(msg);

        const auto debut = msg.debut;

        bool ok = zip.feed(msg);
        if (!ok) {
            std::cerr << "our ordering is broken\n";
        }

        std::cerr << "pre feed stream="<< msg.identity << " to "<< pnode.cfg["name"]
                  << ": ident=" << ident << " cardinality=" << cardinality << " maxlat=" << maxlat
                  << " size=" << zip.size() << " complete=" << zip.complete() << "\n";

        std::vector<message_t> got;
        if (maxlat > 0) {
            zip.drain_prompt(std::back_inserter(got), debut);
        }
        else {
            zip.drain_waiting(std::back_inserter(got));
        }
        for (auto one : got) {
            one.identity = ident;

            tt.send(one);
            co_await pnode.oports[0]->push(one);
        }
        pnode.cfg["data"].update(tt.stats());
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
        // std::cerr << edge << "\n";
        const std::string tkey = edge.at("/tail/node"_json_pointer);
        const std::string hkey = edge.at("/head/node"_json_pointer);

        auto tnode = get(tkey);
        auto hnode = get(hkey);

        const size_t tind = edge.at("/tail/port"_json_pointer);
        const size_t hind = edge.at("/head/port"_json_pointer);

        if (tind >= tnode.oports.size() or hind >= hnode.iports.size()) {
            throw std::invalid_argument("port out of bounds for edge:"
                                        +edge.dump()+" tail:"
                                        +tnode.cfg.dump()+" head:"
                                        +hnode.cfg.dump());
        }
        auto tport = tnode.oports.at(tind);
        auto hport = hnode.iports.at(hind);

        auto ekey = make_edge_key(edge);
        
        store.emplace(ekey, pnode_t(edge, {tport}, {hport}));

        return ekey;
    }
};

struct node_types_t {
    std::unordered_map<std::string, node_t> types = {    
        {"source",source}, {"sink",sink}, {"transfer",transfer}, {"zipit",zipit}
    };
    node_t operator()(const std::string& type) const {
        auto it = types.find(type);
        if (it == types.end()) {
            throw std::invalid_argument("no such node type: " + type);
        }
        return it->second;
    }
};

struct context_t
{
    simcpp20::simulation<> sim;

    node_types_t node_types;

    cfg_t cfg;
    rnd_t rnd;

    pnode_store_t pnodes;


    context_t(const cfg_t& c)
        : cfg(c)
        , rnd(c.value("/main/seed"_json_pointer, 123456))
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
            auto func = node_types(type);

            // Call coroutine
            func(sim, pnode, rnd);
        }
    }

    void run()
    {
        double run_time = cfg.value("/main/run_time"_json_pointer, 1.0);
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

    std::string summary() const
    {
        std::stringstream ss;
        for (auto node : cfg["nodes"]) {
            if (is_service(node)) {
                continue;
            } 

            auto key = make_key(node);
            const auto& pnode = pnodes.get(key);
            auto cfg = pnode.cfg;
            ss << key << "\n"
               << "\twire: " << cfg["/data/wstr"_json_pointer] << "\n"
               << "\theld: " << cfg["/data/hstr"_json_pointer] << "\n";
        }
        return ss.str();
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

    std::cerr << ctx.summary() << std::endl;

    return 0;
}
