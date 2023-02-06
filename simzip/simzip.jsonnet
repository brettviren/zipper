// Support code for producing simzip input.

local pg = import "pgraph.jsonnet";


{
    pg: pg,

    exponential_distribution(name, lifetime) :: {
        type: "random",
        name: name,
        data: {
            distribution: "exponential",
            lifetime: lifetime,
        }
    },

    // Generate a source config
    source(ident, name, rdist) :: pg.pnode({
        type: "source",
        name: name,
        data: {
            ident: ident,
            delay: pg.tn(rdist)
        }
    }, nin=0, nout=1, uses=[rdist]),

    sink(ident, name) :: pg.pnode({
        type: "sink",
        name: name,
        data: {
            ident: ident
        }
    }, nin=1, nout=0),

}
    
        
