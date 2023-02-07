local sz = import "simzip.jsonnet";
local pg = sz.pg;

local delay = sz.rando.exponential("delay", 0.1);

local body = pg.pipeline([sz.source("source0", delay),
                          sz.sink("sink0")]);

pg.graph(body) {main: { run_time: 2 }}
