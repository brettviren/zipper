// Support code for producing simzip input.

local pg = import "pgraph.jsonnet";


{
    pg: pg,

    // A random distribution services.

    // Random distributions
    rando :: {
        // Base distribution has a name unique to all distributions, a
        // dtype giving the distribution type and dtype specific
        // params.
        distribution(dtype, name, params={}) :: {
            type: "random", name: name, data: {
                distribution: dtype} + params },

        // An exponential distribution
        exponential(name, lifetime) :: 
            self.distribution("exponential", name, {lifetime:lifetime}),

        // A fixed point distribution
        fixed(name, value=0) :: 
            self.distribution("fixed", name, {value:value}),

    },

    // A "node" instance in a simzip graph is of a "type" maching an
    // entry in a node factory in the application and a "name" that is
    // unique to the type.  The "params" is a flat object which may
    // have type-specific attributes and the following node level
    // attributes:
    //
    // - ibox :: inbox specification
    // - obox :: outbox specification
    //
    // A "box" specification is optional, may be null, an integer or
    // array of integer.  An integer value is interpreted as the
    // capacity of the buffer queue representing the port.

    box_cardinality(box=null) :: 
        if std.type(box) == "null" then
            0
        else if std.type(box) == "number" then
            1
        else
            std.length(box),

    // Create a source configuration.  The name must be unique to all
    // sources, the delay is a rando.  The obox gives output port
    // spec.
    source(name, delay, obox=1, params={}) :: pg.pnode({
        type:"source",
        name: name,
        data:{
            obox: obox,
            delay: pg.tn(delay),
        } + params,
    }, nin=0, nout=$.box_cardinality(obox), uses=[delay]),

    sink(name, ibox=1, params = {}) :: pg.pnode({
        type: "sink",
        name: name,
        data: {
            ibox: ibox
        } + params,
    }, nin=1, nout=0),

}
    
        
