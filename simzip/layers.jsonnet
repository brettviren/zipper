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
// "top" provides the "head" of the hiearchy.  A number of "tail"
// pnodes in a next lower "layer" are connected to the "head".  The
// generation recurses and each "tail" pnode becomes a "top" until the
// hiearchy is exhausted.

// The "layers" argument provides an array of object providing
// describing each layer of the hiearchy.  Each layer object provides:
//
// - count :: number of "tail" nodes connected to "top"
//
// - generator :: a function taking an index which will be unique to
// the layer and returning a pnode to serve as a "tail".

local hierarchy_layer(top, layers, offset=0) =
    local ntogo = std.length(layers);
    if ntogo == 0
    then
        top
    else
        local l = layers[0];
        local children = [l.generator(offset*l.count+index) for index in std.range(0,l.count-1)];
        if ntogo == 1
        then
            fanin(top, children)
        else
            fanin(top, [hierarchy_layer(children[index], layers[1:], index) for index in std.range(0,l.count-1)]);

        
local make_test_gen(tname, namepat) =
    function(index) pg.pnode({ type:tname, name:namepat%index }, nin=1, nout=1);
                                 
local test_hier = hierarchy_layer(pg.pnode({type:"top", name:""}, nin=1, nout=0), [
    {count:2, generator:make_test_gen("one", "one%d")},
    {count:2, generator:make_test_gen("two", "two%d")},
    ]);


// function (cardinality=1) 
//     local lsizes = std.map(std.parseInt, std.split(cardinality," "));
// {
//     srcs: sources(all_params.sources, std.range(0,8))

// }
pg.graph(test_hier)
    
