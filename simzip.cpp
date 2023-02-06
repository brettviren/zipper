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
    std::string type = cfg["type"];
    std::string name = cfg.value("name","");
    return make_key(type, name);
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

// shareable ports
struct inbox_t {
    std::shared_ptr<mqueue_t> inbox;
    mqueue_t::pop_event_t operator()() { return inbox->pop(); }
};
struct outbox_t {
    std::shared_ptr<mqueue_t> outbox;
    mqueue_t::push_event_t operator()(message_t msg) { return outbox->push(msg); }
};

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
struct rnd_t {
    using rando_ptr = std::shared_ptr<rando_t>;
    std::unordered_map<std::string, rando_ptr> dists;
    std::default_random_engine rangen;

    rnd_t(size_t seed=123456) : rangen(seed) {}

    rando_t& declare(const cfg_t& cfg) {
        std::string type = cfg["type"];
        std::string name = cfg["name"];

        auto key = make_key(type, name);

        std::string distro = cfg["data"]["distribution"];
        if (distro == "exponential") {
            double lifetime = cfg["data"]["lifetime"];
            dists[key] = std::make_shared<rando_expo_t>(rangen, lifetime);
            return *dists[key];
        }
        throw std::invalid_argument("unsupported distribution: " + key);
    }
    rando_t& operator()(const std::string& type, const std::string& name) {
        std::string key = make_key(type, name);
        return (*this)(key);
    }
    rando_t& operator()(const std::string& key) {
        return *dists[key];
    }
};


// Every pgraph node is a function which gets a "sim" and a "params".
// The cfg value has type/name/data attributes from input.  Initially
// "data" has type-specific config.  The function should agument
// "data" with additional information (results, statistics).  The
// "cfg" object will be saved to final output JSON.
struct params_t {
    cfg_t cfg;
    rnd_t rnd;
    inbox_t ibox;
    outbox_t obox;
};

// A pgraph node specifier
using node_t = std::function<event_t(sim_t& sim, params_t& par)>;


// A source that produces sequence of messages with delay.
// configuration parameters:
// - ident :: integer denoting a stream ID unique to any sibilings
// - delay :: a random <distribution>:<name> for a delay 
event_t source(sim_t& sim, params_t& par) 
{
    auto& del = par.rnd(par.cfg["data"]["delay"]);
    size_t ident = par.cfg["data"].value("ident", 0);

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
        co_await par.obox(msg);

        // std::cerr << sim << ident << " -> " << msg << "\n";
        ++count;
        par.cfg["count"] = count;
    }
}
    
event_t sink(sim_t& sim, params_t& par)
{
    size_t count = 0;
    while (true) {
        co_await par.ibox();
        ++count;
        par.cfg["count"] = count;
    }
}

event_t transfer(sim_t& sim, params_t& par)
{
    auto& del = par.rnd(par.cfg["data"]["delay"]);
    
    size_t count = 0;
    while (true) {
        auto msg = (co_await par.ibox());
        co_await sim.timeout(del());
        co_await par.obox(msg);
        ++count;
        par.cfg["count"] = count;
    }
}

event_t zipit(sim_t& sim, params_t& par)
{
    size_t cardinality = par.cfg["data"].value("cardinality", 10);
    merge_t::duration_t max_latency = timepoint(par.cfg["data"].value("max_latency", 0.0)).time_since_epoch();
    merge_t zip(cardinality, max_latency);

    size_t in_count{0}, out_count{0};

    while (true) {
        auto msg = (co_await par.ibox());

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
        for (const auto& one : got) {
            co_await par.obox(one);
            ++out_count;
        }

        par.cfg["in_count"] = in_count;
        par.cfg["out_count"] = out_count;
    }        
}





struct context_t
{
    using node_types_t = std::unordered_map<std::string, node_t>;
    node_types_t node_types = {
        {"source",source}, {"sink",sink}, {"transfer",transfer}, {"zipper",zipit}
    };

    using param_store_t = std::unordered_map<std::string, params_t>;
    param_store_t param_store;
    
    using edge_t = std::shared_ptr<mqueue_t>;
    using boxes_t = std::unordered_map<std::string, edge_t>;
    boxes_t inboxes, outboxes;

    cfg_t cfg;
    rnd_t rnd;
    simcpp20::simulation<> sim;

    edge_t outbox(const std::string& key)
    {
        auto got = outboxes.find(key);
        if (got == outboxes.end()) {
            static edge_t dummy;
            return dummy;
        }
        return got->second;
    }
    edge_t inbox(const std::string& key)
    {
        auto got = inboxes.find(key);
        if (got == inboxes.end()) {
            static edge_t dummy;
            return dummy;
        }
        return got->second;
    }

    context_t(const cfg_t& c)
        : cfg(c)
        , rnd(c["main"].value("seed", 123456))
    {
        // Edge has tail/head/capacity
        for (auto& edge : cfg["edges"]) {
            uint64_t capacity = edge.value("capacity",1);
            auto q = std::make_shared<mqueue_t>(sim, capacity);
            outboxes[edge["tail"]["node"]] = q;
            inboxes[edge["head"]["node"]] = q;
        }

        // Loop once looking for non-node configurations
        for (auto& node : cfg["nodes"]) {
            if (is_random(node)) {
                rnd.declare(node);
                continue;
            }
        }

        // Loop again to make and execute nodes
        for (auto& node : cfg["nodes"]) {
            if (is_service(node)) {
                continue;
            }

            std::string type = node["type"];

            std::string name = node.value("name","");
            auto key = make_key(type, name);
            param_store.emplace(key,params_t{node, rnd, inbox(key), outbox(key)});
            auto func = node_types[type];

            // Call coroutine
            auto& param = param_store[key];
            func(sim, param);
        }
    }

    void run()
    {
        double run_time = cfg["main"].value("run_time", 1.0);
        sim.run_until(run_time);
    }

    cfg_t state()
    {
        cfg_t nodes = cfg_t::array();
        for (auto node : cfg["nodes"]) {
            if (is_service(node)) {
                nodes.push_back(node);
                continue;
            }
            auto key = make_key(node);
            const auto& ncfg = param_store[key].cfg;
            std::cerr << key << "\n" << ncfg << "\n";
            node.update(ncfg, true);
            nodes.push_back(node);
        }
        cfg_t ret = cfg;
        ret["nodes"] = nodes;
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

    // const size_t buffer_size = 1000;

    // simcpp20::simulation<> sim;
    // auto s1 = std::make_shared<mqueue_t>(sim, buffer_size);
    // auto s2 = std::make_shared<mqueue_t>(sim, buffer_size);

    // // mean period of TP generation in sim time (seconds);
    // const double lifetime = 0.0001;
    // std::default_random_engine rangen(123456);
    // std::exponential_distribution<> expo(1.0/lifetime);

    // delay_t sdel{rangen, expo};
    // // delay_t tdel{rangen, expo};
    
    // source_stat_t source_stat;
    // if (true) {
    //     source(sim, source_stat, 1, {s1}, sdel);
    // }
    // // transfer_stat_t transfer_stat;
    // // transfer(sim, transfer_stat, 1, {s1}, {s2}, tdel);
    // zipit_stat_t zipit_stat;
    // zipit(sim, zipit_stat, 1, {s1}, {s2});
    // sink_stat_t sink_stat;
    // sink(sim, sink_stat, 1, {s2});

    // sim.run_until(10);

    // std::cerr << source_stat.count << " -> "
    //           // << transfer_stat.count << " -> "
    //           << zipit_stat.in_count << "/"
    //           << zipit_stat.out_count << " -> "
    //           << sink_stat.count << "\n";
    return 0;
}
