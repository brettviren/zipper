// This generates config for layers of zippers
//
// usage:
//
// jsonnet -A cardinality="c1 c2 ..." layers.jsonnet

local sz = import "simzip.jsonnet";
local pg = sz.pg;

// Call gen(ident, name, params) on idents with name made from pattern%ident
// A name is given 
local series(gen, pattern, params, idents) =
    [gen(ident, pattern%ident, params) for ident in idents];

local sources(params, idents) =
    series(sz.source, "source%d", params, idents);
          
local all_params = {
    sources: {rando: sz.exponential_distribution("delay", 0.1)},
};

// Return an aggregate pnode with edge of given buffer capacity going from each in tails to head.
local fanin(head, tails, capacity=1) =
    pg.intern(innodes=tails,
              outnodes=[head],
              edges=[pg.edge(tail, head) + {capacity:capacity} for tail in tails]);
             

// Return a pnode aggregating a layered hierarchy of pnodes.  The
// "top" provides the "head" of the hierarchy.  A number of "tail"
// pnodes in a next lower "layer" are connected to the "head".  The
// generation recurses and each "tail" pnode becomes a "top" until the
// hierarchy is exhausted.

// The "layers" argument provides an array of object providing
// describing each layer of the hierarchy.  Each layer object provides:
//
// - count :: number of "tail" children nodes in the layer connected
// to a single "head" node in the previous layer.
//
// - generator :: a function taking an "ident" number that is opaque
// but unique to the node in the entire graph.  It must return the
// coresponding pnode.

local node_id(path) = 
    std.parseInt(std.join("", [std.toString(i+1) for i in path]));

local hierarchy_layer(top, layers, layer=0, path=[]) =
    local ntogo = std.length(layers);
    if ntogo == 0
    then
        top
    else
        local l = layers[0];
        local children = [l.generator(node_id(path+[index])) for index in std.range(0,l.count-1)];
        if ntogo == 1
        then
            fanin(top, children)
        else
            fanin(top, [hierarchy_layer(children[index], layers[1:], layer+1, path+[index]) for index in std.range(0,l.count-1)]);

        

local layer_node_name(layer, pre) = std.toString(layer) + "-" + pre;

// Map a type name to a function producing a layer generator of nodes of that type.  The function takes a layer_number used to assure unique naming when multiple layers may have same type.
local layer_generators = {
    source(delay, obox=1, params={}) :: function(ident)
        sz.source(ident, delay, obox, {stream:ident} + params),
    burst(delay, count, obox=1, params={}) :: function(ident)
        sz.burst(ident, delay, count, obox, {stream:ident} + params),
    sink(ibox=1, params={}) :: function(ident)
        sz.sink(ident, ibox, params),
    zipit(cardinality=0, max_latency=0, params={}) :: function(ident)
        sz.zipit(ident, cardinality, max_latency, {stream:ident}+params),
    transfer(delay, params={}) :: function(ident)
        sz.transfer(ident, delay, params),
    coherent(streams, start, span, params={}) :: function(ident)
        sz.coherent(ident, streams, start, span, params),
};

local simple_sink = pg.pnode({type:"sink", name:"", data:{ ibox:1 }}, nin=1, nout=0);



// local pdsp = hierarchy_layer(simple_sink, [
//         {count:2, generator:layer_generators.source(sz.rando.exponential("delay",1.0))}
    

// Generate messages that are coherent across different adjacent
// output streams and use zippers to bring them together with
// incoherent sources.  It's a bit "weird" as coherent noise would
// subject zipper to same-time/same-stream input.

local cohsrcgen(ident) = 
    local streams = std.range(0,9);
    local cap = layer_generators.zipit(10)(ident);
    local zips = [layer_generators.zipit(2)(ident*10+ind)
                  for ind in streams];
    local srcs = [layer_generators.source(sz.rando.exponential("cohdelay",1.0))(ident*10+ind)
                  for ind in streams];
    local cohfan = layer_generators.coherent(std.range(0,9),
                                             sz.rando.uniint("cohstart", 0, 9),
                                             sz.rando.fixed("cohspan", 2)
                                            )(ident);
    local cohnoi = layer_generators.source(sz.rando.exponential("srcdelay",1.0))(ident);
    local final = pg.intern(outnodes=[cap],
                            centernodes=[cohfan, cohnoi]+srcs+zips,
                            edges = [pg.edge(zips[ind], cap) for ind in streams] +
                                    [pg.edge(srcs[ind], zips[ind]) for ind in streams] +
                                    [pg.edge(cohfan,    zips[ind], ind,0) for ind in streams] +
                                    [pg.edge(cohnoi, cohfan)]);
    final;

local second = 1.0;
local millisecond = second/1000.0;
local microsecond = second/1000000.0;

local per_face_ardk_period = 2.2e-5 * second;
//local per_face_ardk_period = 1e-6 * second; // testing
local tp_zipper_maxlat = 100 * microsecond;

// fixme: move the layer number into hierarchy_layer() passing it to the generator.
local test_scenarios = {

    // Just source-zipper-sink, dumbest thing evar.
    ziptwo : hierarchy_layer(simple_sink, [
        {count:1, generator:layer_generators.zipit(2, 1.0)},
        {count:2, generator:layer_generators.source(sz.rando.exponential("delay",1.0))}]),

    sourcesink : hierarchy_layer(simple_sink, [
        {count:1, generator:layer_generators.transfer(sz.rando.fixed("trans_delay", 1.0),
                                                      {ibox:1000,obox:1000})},
        {count:1, generator:layer_generators.source(sz.rando.exponential("delay",1.0))}]),

    ticktock : hierarchy_layer(simple_sink, [ {
        count:1,
        generator:layer_generators.transfer(sz.rando.fixed("trans_delay", 10.0))
    }, {
        count:1,
        generator:layer_generators.source(sz.rando.fixed("delay",1.0))}]),

    ticktockbuff : hierarchy_layer(simple_sink, [ {
        count:1,
        generator:layer_generators.transfer(sz.rando.fixed("trans_delay", 10.0),
                                            {ibox:1000,obox:1000})
    }, {
        count:1,
        generator:layer_generators.source(sz.rando.fixed("delay",1.0))}]),

    onetwozip : hierarchy_layer(simple_sink, [ {
        count:2,
        generator:layer_generators.zipit(2)
    }, {
        count:1,
        generator:layer_generators.transfer(sz.rando.exponential("delay", 1.0))
    }, {
        count:2,
        generator:layer_generators.zipit(2)
    }, {
        count:1,
        generator:layer_generators.transfer(sz.rando.exponential("delay", 1.0))
    }, {
        count:2,
        generator:layer_generators.source(sz.rando.exponential("delay",1.0))}
    ]),

    cohsrc : hierarchy_layer(simple_sink, [
        {count:1, generator:cohsrcgen}
    ]),
        
    aparad : hierarchy_layer(simple_sink, [
        // zip 10 links to one
        {count: 1, generator:layer_generators.zipit(10, tp_zipper_maxlat)}, 
        // {count: 10, generator:layer_generators.transfer(
        //     sz.rando.exponential("transfer_delay", 1.0))},
        // (1.0 becquerel / kg )  * 10 kiloton / 200 -> 45 kHz / 2.2e-5 second
        {count: 10, generator:layer_generators.burst(
            sz.rando.exponential("decay_delay", per_face_ardk_period),
            sz.rando.uniint("burst_count", 1, 10))}]),
};

// function (cardinality=1) 
//     local lsizes = std.map(std.parseInt, std.split(cardinality," "));
// {
//     srcs: sources(all_params.sources, std.range(0,8))

// }
function(scenario="aparad")
    pg.graph(test_scenarios[scenario])
    
// pg.graph(cohsrcgen(101))
// test_scenarios.apa
