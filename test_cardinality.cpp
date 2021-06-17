#include "zipper.hpp"

#include <cassert>
#include <iostream>

using node_t = zipper::Node<std::string>;
using merge_t = zipper::merge<node_t>;

int main(int argc, char* argv[])
{
    bool ok=false;
    merge_t mq(2);

    // payload, order, ident
    mq.feed("a1", 0, 1);
    mq.feed("b1", 0, 2);
    mq.feed("a2", 1, 1);

    std::vector<node_t> got;

    mq.drain_waiting(std::back_inserter(got));
    std::cerr << "got: " << got.size() << std::endl;
    assert (got.size() == 1);
    got.clear();
    
    mq.feed("b1", 1, 2);
    mq.set_cardinality(0);
    mq.drain_waiting(std::back_inserter(got));
    std::cerr << "got: " << got.size() << std::endl;
    assert (got.size() == 1);   // top is always left
    std::cerr << got.size() << " " << got[0].payload << std::endl;
    got.clear();

    mq.clear();
    assert (mq.empty());
    assert (mq.size() == 0);
    mq.set_cardinality(2);

    ok = mq.feed("a1", 0, 1);
    assert(ok);
    ok = mq.feed("b1", 0, 2);
    assert(ok);
    assert(mq.size() == 2);

    mq.drain_waiting(std::back_inserter(got));
    std::cerr << got.size() << std::endl;
    assert (got.size() == 0);
    got.clear();

    mq.feed("a2", 1, 1);
    mq.feed("c2", 0, 3);
    assert(mq.size() == 4);
    mq.set_cardinality(3);
    mq.drain_waiting(std::back_inserter(got));
    assert (got.size() == 1);
    assert (got[0].payload == "a1");
    got.clear();

    mq.set_cardinality(2);
    mq.drain_waiting(std::back_inserter(got));
    assert (got.size() == 0);
    got.clear();

    return 0;
}
