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
// - generator :: a function taking an name to use to construct a
// pnode to serve as a "tail".

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

        
local make_simple_gen(tname, data={}) =
    function(ident) pg.pnode({ type:tname, name:std.toString(ident), data:data }, nin=1, nout=1);

local layer_node_name(layer, pre) = std.toString(layer) + "-" + pre;

// Map a type name to a function producing a layer generator of nodes of that type.  The function takes a layer_number used to assure unique naming when multiple layers may have same type.
local layer_generators = {
    source(delay, obox=1, params={}) :: function(ident)
        sz.source(ident, delay, obox, params),
    sink(ibox=1, params={}) :: function(ident)
        sz.sink(ident, ibox, params),
    zipit(cardinality=0, max_latency=0, params={}) :: function(ident)
        sz.zipit(ident, cardinality, max_latency, params),
    transfer(delay, params={}) :: function(ident)
        sz.transfer(ident, delay, params),
};

local simple_sink = pg.pnode({type:"sink", name:"", data:{ ibox:1 }}, nin=1, nout=0);

// fixme: move the layer number into hierarchy_layer() passing it to the generator.
local test_scenarios = {
    onetwo : hierarchy_layer(simple_sink, [
        {count:2, generator:make_simple_gen("one")},
        {count:2, generator:make_simple_gen("two")}]),

    sourcesink : hierarchy_layer(simple_sink, [
        {count:2, generator:layer_generators.zipit(3)},
        {count:3, generator:layer_generators.source(sz.rando.exponential("delay",1.0))}]),

    onetwozip : hierarchy_layer(simple_sink, [
        {count:2, generator:layer_generators.zipit(2)},
        {count:1, generator:layer_generators.transfer(sz.rando.exponential("delay", 1.0))},
        {count:2, generator:layer_generators.zipit(2)},
        {count:1, generator:layer_generators.transfer(sz.rando.exponential("delay", 1.0))},
        {count:2, generator:layer_generators.source(sz.rando.exponential("delay",1.0))}
    ]),
};

// function (cardinality=1) 
//     local lsizes = std.map(std.parseInt, std.split(cardinality," "));
// {
//     srcs: sources(all_params.sources, std.range(0,8))

// }
function(scenario="onetwo")
    pg.graph(test_scenarios[scenario])
    
