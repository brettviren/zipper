local sz = import "simzip.jsonnet";
local pg = sz.pg;

local body = pg.pipeline([sz.source(0, "source0",
                                    sz.exponential_distribution("delay", 0.1)),
                          sz.sink(0,"sink0")]);

pg.graph(body) {main: { run_time: 2 }}
