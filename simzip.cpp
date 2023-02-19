// A simcpp20-based simulation of a hiearchy of zippers and other messages processors.
//
// This program communicates via stdin/stdout via JSON text.
// See simzip/examples.jsonnet for example how to generate input.
// See xxxx.yyy for how to consume output.

#include "zipper.hpp"
#include "simzip/util.hpp"
#include "simzip/stats.hpp"

// #define JSON_DIAGNOSTICS 1
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

// Mock up a TP/TA/TC message.  Generally, anything that may be subject to a zipper.
// https://github.com/DUNE-DAQ/detdataformats/blob/develop/include/detdataformats/trigger/TriggerPrimitive.hpp
struct Payload {
    int a{0}, b{0}, c{0}, d{0}, e{0}, f{0};
};
using message_t = zipper::Node<Payload>; // NOT a pgraph "node"
using merge_t = zipper::merge<message_t>;

using event_t = simcpp20::event<>;
using sim_t = simcpp20::simulation<>;
using mqueue_t = simzip::bufque<message_t>;
using message_event_t = simcpp20::value_event<message_t>;

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
struct rando_uniint_t : public rando_t {
    std::default_random_engine& rangen;
    std::uniform_int_distribution<> dist;
    rando_uniint_t(std::default_random_engine& r, int a, int b)
        : rangen(r)
        , dist(a,b) {}

    // Caller should cast to int.
    virtual double operator()() { return dist(rangen); }
};
struct rando_unireal_t : public rando_t {
    std::default_random_engine& rangen;
    std::uniform_real_distribution<> dist;
    rando_unireal_t(std::default_random_engine& r, double a, double b)
        : rangen(r)
        , dist(a,b) {}

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
        if (dtype == "uniint") {
            int a = cfg["/data/vmin"_json_pointer];
            int b = cfg["/data/vmax"_json_pointer];
            dists[key] = std::make_shared<rando_uniint_t>(rangen, a,b);
            return *dists[key];
        }
        if (dtype == "unireal") {
            double a = cfg["/data/vmin"_json_pointer];
            double b = cfg["/data/vmax"_json_pointer];
            dists[key] = std::make_shared<rando_unireal_t>(rangen, a,b);
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
    for (const auto& one : got) {
        size_t capacity = one;
        ret.push_back(std::make_shared<mqueue_t>(sim, capacity));
    }
    return ret;
}

// Every node/coroutine gets a node_t by reference.
struct node_t {

    // Connection to the running sim.  simcpp20 framework REQUIRES a
    // member of this name to point to "the simulation".
    sim_t& sim;

    // Connection to randos
    rnd_t& rnd;

    // The configuration for the node.  The node function may fill
    // addition info.
    cfg_t cfg;

    // Vector of input and output ports.
    ports_t iports, oports;

    // Keep stats about messages just received in to a node.
    simzip::Stats recved{false};

    // Keep stats about messages just prior to sending out node.
    simzip::Stats sended{false};

    // Keep stats about time to send out a message
    simzip::Stats doneed{false};


    // Owning ports
    node_t(sim_t& sim, rnd_t& rnd, const cfg_t& cfg, bool samples=true)
        : sim(sim)
        , rnd(rnd)
        , cfg(cfg)
        , iports(make_ports(sim, cfg, "ibox"))
        , oports(make_ports(sim, cfg, "obox"))
        , recved{samples}
        , sended{samples}
        , doneed{samples}
    {
        if (iports.empty() and oports.empty()) {
            throw std::invalid_argument("no ports in node:" + cfg.dump());
        }
    }

    // Usin others ports
    node_t(sim_t& sim, rnd_t& rnd, const cfg_t& cfg,
           ports_t iports, ports_t oports)
        : sim(sim)
        , rnd(rnd)
        , cfg(cfg)
        , iports(iports), oports(oports) {}
            

    // Call after a co_await on pop.  This will set message debut time
    // to simulation now() after storing the time between ordering and
    // the current simulation now time giving total latency.
    void recv(message_t& m)
    {
        const double then = m.ordering * tick_period;
        const double now = sim.now();        
        recved(now-then);
        m.debut = timepoint(now);
    }

    // Call just before a co_await on push.  This will reset message
    // debut time to simulation now() after storing how much
    // simulation time has passed giving relative latency.
    void send(message_t& m)
    {
        const double then = timepoint(m.debut);
        const double now = sim.now();        
        sended(now-then);
        m.debut = timepoint(now);
        
        size_t ident = cfg.value("/data/ident"_json_pointer, 0);
        if (ident) {
            m.identity = ident;
        }
    }

    // Call after message is sent
    void done(message_t& m)
    {
        const double then = timepoint(m.debut);
        const double now = sim.now();        
        doneed(now-then);
    }

    // Dump self as cfg obj
    cfg_t stats() const
    {
        auto ret = cfg_t{
            {"Rn",   recved.S0},
            {"Rmu",  recved.mean()},
            {"Rrms", recved.rms()},
            {"Rstr", recved.dump()},
            {"Sn",   sended.S0},
            {"Smu",  sended.mean()},
            {"Srms", sended.rms()},
            {"Sstr", sended.dump()},
            {"Dn",   doneed.S0},
            {"Dmu",  doneed.mean()},
            {"Drms", doneed.rms()},
            {"Dstr", doneed.dump()},
        };
        if (recved.sample) {
            ret["Rsamples"] = recved.samples;
        }
        if (sended.sample) {
            ret["Ssamples"] = sended.samples;
        }
        if (doneed.sample) {
            ret["Dsamples"] = doneed.samples;
        }
        return ret;
    }


};

// We need to share between context and node function.
// using node_ptr = std::shared_ptr<node_t>;

// Every pgraph node is a coroutine called like this.
using node_function = std::function<event_t(node_t& ctx)>;


// A source that produces a sequence of messages its port.
// Expect this in cfg data:
// - delay :: a random <distribution>:<name> for a delay 
event_t source(node_t& ctx) 
{
    sim_t& sim = ctx.sim;
    auto& del = ctx.rnd(ctx.cfg["/data/delay"_json_pointer]);

    while (true) {

        const double now = sim.now();        
        const message_t::ordering_t ordering = now / tick_period;
            
        message_t msg{{}, ordering, 0, timepoint(now)};

        /// we are source, recv times are meaningless
        // ctx.recv(msg);

        // Now spend some "time" making the message
        const double delay = del();
        co_await sim.timeout(delay);

        ctx.send(msg);
        co_await ctx.oports[0]->push(msg);
        ctx.done(msg);
    }
}

// A source that produces a burst of messages its port.  It uses two randos:
//
//  /data/delay :: return time to next burst
//  /data/count :: return number of messages per burst
event_t burst(node_t& ctx)
{
    // std::cerr << sim << "source: " << ctx.cfg << "\n";
    auto& delay = ctx.rnd(ctx.cfg["/data/delay"_json_pointer]);
    auto& count = ctx.rnd(ctx.cfg["/data/count"_json_pointer]);

    // std::cerr << sim << "source: " << delay << "\n";

    while (true) {

        const double now = ctx.sim.now();
        const message_t::ordering_t ordering = now / tick_period;
            
        message_t msg{{}, ordering, 0, timepoint(now)};

        /// we are source, recv times are meaningless
        // ctx.recv(msg);

        // Now spend some "time" making the message
        co_await ctx.sim.timeout(delay());

        ctx.send(msg);
        for (int num = count(); num > 0; --num) {
            co_await ctx.oports[0]->push(msg);
        }
        ctx.done(msg);
    }
}
    
// A coherent pseudo-source.  Each received messages is sent 
// number of times.  Each sent message is made unique with one
// identity in a configured sequence "streams".
// 
// The number of consecutive streams to which messages are sent is
// determined by a draw of the "span" rando and the first stream is
// given by a draw of the "start" rando.
event_t coherent(node_t& ctx)
{
    auto& rstart = ctx.rnd(ctx.cfg["/data/start"_json_pointer]);
    auto& rspan = ctx.rnd(ctx.cfg["/data/span"_json_pointer]);

    const std::vector<size_t> streams = ctx.cfg["/data/streams"_json_pointer];
    const size_t nstreams = streams.size();
    if (nstreams != ctx.oports.size()) {
        throw std::invalid_argument("stream and port count do not match");
    }

    while (true) {
        auto msg = co_await ctx.iports[0]->pop();
        ctx.recv(msg);

        // half-protect against broken user input
        size_t beg = std::min((size_t)rstart(), nstreams-1);
        size_t end = std::min(beg+(size_t)rspan(), nstreams);
        
        // std::cerr << "coher:" << ctx.cfg["name"] << " recv:"
        //           << " stream=" << msg.identity
        //           << " order=" << msg.ordering
        //           << " span:["<<beg<<","<<end<<"]"
        //           << "\n";

        while (beg < end) {
            auto copy = msg;
            ctx.send(copy);
            copy.identity = streams[beg];
            co_await ctx.oports[beg]->push(copy);
            ++beg;
        }        
        ctx.done(msg);
    }
}

// A sink with a single input port
event_t sink(node_t& ctx)
{
    while (true) {
        auto msg = co_await ctx.iports[0]->pop();
        ctx.recv(msg);
    }
}

// A transfer is a special node type that connects a tail port to a
// head port.  It crudely models transport such as across TCP.
// It's configuration is that of an EDGE
event_t transfer(node_t& ctx)
{
    std::cerr << "TRANSFER: " << ctx.cfg << "\n";
    auto rname = ctx.cfg.value("/data/delay"_json_pointer,"random:zeros");

    auto& del = ctx.rnd(rname);

    while (true) {

        double delay = del();
        // std::cerr << "transfer delay: " << rname << ": " << delay << std::endl;
        auto msg = (co_await ctx.iports[0]->pop());
        ctx.recv(msg);

        // simulate transmission delay
        if (delay > 0) {
            co_await ctx.sim.timeout(delay);
        }

        ctx.send(msg);
        co_await ctx.oports[0]->push(msg);
        ctx.done(msg);
    }
}

// A node running a zipper.  This has a single input and output port.
event_t zipit(node_t& ctx)
{
    size_t cardinality = ctx.cfg.value("/data/cardinality"_json_pointer, 0);
    ctx.cfg["/data/cardinality"_json_pointer] = cardinality;

    double maxlat = ctx.cfg.value("/data/max_latency"_json_pointer, 0.0);
    ctx.cfg["/data/max_latency"_json_pointer] = maxlat;
    merge_t::duration_t max_latency = timepoint(maxlat).time_since_epoch();
    merge_t zip(cardinality, max_latency);

    bool next_needed = true;
    message_event_t next_message = ctx.sim.timeout<message_t>(0);

    while (true) {
        message_t msg, dummy_msg{{-1}};

        if (next_needed) {
            next_message = ctx.iports[0]->pop();
            next_needed = false;
        }
        // at this point we do not know how long it takes for message
        // to be received.  If we abserve max latency we can't wait
        // too long for the message
        if (maxlat > 0.0) {
            std::vector<message_event_t> ves = {
                next_message,
                ctx.sim.timeout<message_t>(0.01*maxlat, dummy_msg)
            };
            msg = co_await simzip::any_of(ctx.sim, ves);
        }
        else {
            msg = co_await next_message;
        }

        if (msg.payload.a < 0) { 
            // We got the dummy message due to timeout.
            std::cerr << ctx.sim << " timeout\n";
        }
        else {
            // Will got real message, will need fresh one.
            next_needed = true; 
            ctx.recv(msg);
            bool ok = zip.feed(msg);
            if (!ok) {
                std::cerr << ctx.sim << " tardy: " << msg << "\n";
            }

            std::string status = ok ? " ack" : " rej";
            std::cerr << "zipit="<< msg.identity << " to "<< ctx.cfg["name"]
                      << status
                      << " ordering=" << msg.ordering
                      << " debut=" << timepoint(msg.debut)
                      << " origin=" << zip.get_origin()
                      << " size=" << zip.size()
                      << " complete=" << zip.complete() << "\n";
            // std::cerr << "zipit:" << ctx.cfg["name"] << " recv:"
            //           << ok
            //           << " stream=" << msg.identity
            //           << " order=" << msg.ordering
            //           << " size=" << zip.size()   << "\n";
        }

        // std::cerr << "zipit="<< msg.identity << " to "<< ctx.cfg["name"]
        //           << " ordering=" << msg.ordering
        //           << " origin=" << zip.get_origin()
        //           << " size=" << zip.size() << " complete=" << zip.complete() << "\n";

        std::vector<message_t> got;
        if (maxlat > 0) {
            zip.drain_prompt(std::back_inserter(got), timepoint(ctx.sim.now()));
        }
        else {
            zip.drain_waiting(std::back_inserter(got));
        }
        for (auto one : got) {
            ctx.send(one);

            // std::cerr << "zipit:" << ctx.cfg["name"] << " send:"
            //           << " stream=" << msg.identity
            //           << " order=" << msg.ordering
            //           << " size=" << zip.size() 
            //           << "\n";
            // const auto t1 = ctx.sim.now();
            co_await ctx.oports[0]->push(one);
            // const auto t2 = ctx.sim.now();
            // std::cerr << "zipit: " << t2-t1 << " send delay\n";
            ctx.send(one);
        }

        ctx.cfg["/data/zipsize"_json_pointer] = zip.size();
        ctx.cfg["/data/zipcomplete"_json_pointer] = zip.complete();
    }        
}

struct node_store_t {

    sim_t& sim;
    rnd_t rnd;

    // using nodes_t = std::unordered_map<std::string, node_ptr>;
    using nodes_t = std::unordered_map<std::string, node_t>;
    nodes_t store;

    node_t& get(const std::string& key) 
    {
        auto got = store.find(key);
        if (got == store.end()) {
            throw std::invalid_argument("no such node: " + key);
        }
        return got->second;
    }
    const node_t& get(const std::string& key) const
    {
        auto got = store.find(key);
        if (got == store.end()) {
            throw std::invalid_argument("no such node: " + key);
        }
        return got->second;
    }

    // set a "regular" node, return its key
    std::string set(const cfg_t& cfg)
    {
        auto key = make_key(cfg);
        // store[key] = make_shared<node_t>(sim, rnd, cfg);
        store.emplace(key, node_t(sim, rnd, cfg));
        return key;
    }

    // set a special node representing an edge between regular nodes
    std::string set_edge(const cfg_t& edge)
    {
        // std::cerr << edge << "\n";
        const std::string tkey = edge.at("/tail/node"_json_pointer);
        const std::string hkey = edge.at("/head/node"_json_pointer);

        auto& tnode = get(tkey);
        auto& hnode = get(hkey);

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
        std::cerr << "EDGE: " << edge << "\n";
        store.emplace(ekey, node_t(sim, rnd, edge, {tport}, {hport}));
        return ekey;
    }
};

struct node_types_t {
    std::unordered_map<std::string, node_function> types = {    
        {"source",source}, {"burst",burst}, {"sink",sink},
        {"transfer",transfer}, {"zipit",zipit}, {"coherent", coherent}
    };
    node_function operator()(const std::string& type) const {
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

    node_store_t node_store;

    context_t(const cfg_t& c)
        : cfg(c)
        , node_store{sim,c.value("/main/seed"_json_pointer, 123456)}
    {
        // Loop once to dispatch non-node configurations or build
        // ports and params for nodes.
        for (auto& node : cfg["nodes"]) {
            if (is_random(node)) {
                node_store.rnd.declare(node);
                continue;
            }
            node_store.set(node);
        }

        // Link up edges
        for (auto& edge : cfg["edges"])
        {
            auto ekey = node_store.set_edge(edge);
            auto& ectx = node_store.get(ekey);
            transfer(ectx);                
        }

        // Loop again to execute nodes
        for (auto& node : cfg["nodes"]) {
            if (is_service(node)) {
                continue;
            }

            auto key = make_key(node);
            auto& nctx = node_store.get(key);
            std::string type = nctx.cfg["type"];
            auto func = node_types(type);

            // Call coroutine
            func(nctx);
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
            const auto& nctx = node_store.get(key);
            auto config = nctx.cfg;
            auto stats = nctx.stats();
            config["data"].update(stats, true);
            nodes.push_back(config);
        }

        cfg_t edges = cfg_t::array();
        for (auto edge : cfg["edges"]) {
            auto ekey = make_edge_key(edge);
            auto& nctx = node_store.get(ekey);
            auto config = nctx.cfg;
            auto stats = nctx.stats();
            config["data"].update(stats, true);
            edges.push_back(config);
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
            const auto& nctx = node_store.get(key);
            // auto cfg = nctx.cfg;
            auto cfg = nctx.stats();
            ss << key << "\n"
               << "\trecved: " << cfg["Rstr"] << "\n"
               << "\tsended: " << cfg["Sstr"] << "\n"
               << "\tdoneed: " << cfg["Dstr"] << "\n";
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
