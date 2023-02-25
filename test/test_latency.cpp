#include "zipper.hpp"
#include <cassert>
#include <iostream>

using  payload_t = std::chrono::steady_clock::time_point;
using message_t = zipper::Node<payload_t>;
using merge_t = zipper::merge<message_t>;

std::ostream& operator<<(std::ostream& o, const message_t& msg)
{
    o << " ident=" << msg.identity 
      << " pay=" << msg.payload.time_since_epoch().count()
      << " ord=" << msg.ordering
      << " debut=" << msg.debut.time_since_epoch().count();
    return o;
}

merge_t::timepoint_t us(int micros)
{
    merge_t::timepoint_t ret;
    ret += std::chrono::microseconds(micros);
    return ret;
}

bool feed(merge_t& zip, int ident, int time)
{
    auto now = us(time);
    return zip.feed({now}, time, ident, now);
}

std::vector<message_t> drain(merge_t& zip, int time)
{
    std::vector<message_t> got;
    zip.drain_prompt(std::back_inserter(got), us(time));
    return got;
}

int main ()
{
    merge_t zip(2, std::chrono::microseconds(10));
    assert (!zip.complete());

    assert (!zip.complete());
    assert (drain(zip, 0).empty());

    // t=1
    assert (feed(zip, 1, 1));
    assert (!zip.complete());
    assert (drain(zip, 1).empty());

    // t=11
    assert (feed(zip, 1, 11));        
    {
        auto got = drain(zip, 1);
        assert(got.size() == 1);
        std::cerr << got[0] << "\n";
        assert(got[0].payload == us(1));
        assert(zip.get_origin() == 1);
    }

    assert(! feed(zip, 2, 0));  // tardy
    assert(  feed(zip, 2, 1));

    {
        auto got = drain(zip, 2);
        assert(got.size() == 0);
    }
    {
        auto got = drain(zip, 12);
        assert(got.size() == 1);
        std::cerr << got[0] << "\n";
        assert(got[0].identity == 2);
        assert(got[0].payload == us(1));
    }
    {
        auto got = drain(zip, 22);
        assert(got.size() == 1);
        std::cerr << got[0] << "\n";
        assert(got[0].identity == 1);
        assert(got[0].payload == us(11));
    }

    return 0;
}
