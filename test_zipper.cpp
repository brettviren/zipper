#include "zipper.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <iterator>
#include <chrono>
#include <string>
#include <sstream>

using node_t = zipper::Node<std::string>;

using merge_t = zipper::merge<node_t>;

std::vector<std::string> data = {
    "abcd", "efgh", "ijkl"
};
typedef std::chrono::duration<int> seconds;
// std::vector<merge_t::timepoint_t> nows = {
//     seconds(5), seconds(10), seconds(50), seconds(55)
// };


bool feed(merge_t& m, size_t stream, size_t index)
{
    const auto s = data[stream].substr(index,1);

    return m.feed(s, (size_t)s[0], stream);
}

int feedn(merge_t& m, size_t n, size_t start=0)
{
    int errors = 0;
    for (size_t i=start; i < start+n; ++i) {
        for (auto str : {0, 1, 2}) {
            bool okay = feed(m, str, i);
            if (not okay) ++errors;
         }
    }
    return errors;
}    

std::string stringify(const std::vector<node_t>& nodes) {
    std::stringstream ss;
    for (const auto& node : nodes) {
        ss << node.payload;
    }
    return ss.str();
}

// no latency bounds
void test_drain_full()
{
    merge_t mq(3);
    {
        bool caught = false;
        try {
            mq.peek();
        }
        catch (std::out_of_range& err) {
            caught = true;
        }
        assert(caught);
    }
    {
        bool caught = false;
        try {
            mq.next();
        }
        catch (std::out_of_range& err) {
            caught = true;
        }
        assert(caught);
    }

    assert(!mq.complete());
    assert(mq.empty());

    int nerr = feedn(mq, 2);
    assert(!nerr);
    std::vector<node_t> got;
    mq.drain_full(std::back_inserter(got));
    assert(got.size() == 6);
    assert(mq.empty());
    assert(!mq.complete());
    auto str = stringify(got);
    std::cerr << str << std::endl;
    assert(str == "abefij");
}

void test_drain_waiting()
{
    merge_t mq(3);

    int nerr = feedn(mq, 2);
    assert(!nerr);
    std::vector<node_t> got;
    mq.drain_waiting(std::back_inserter(got));
    assert(got.size() == 1);
    assert(mq.size() == 5);
    assert(mq.peek().payload == "b");
    assert(!mq.complete());  // "b" is in top so 0 is not rep'ped

    nerr = feedn(mq, 1, 2);
    assert(!nerr);
    assert(mq.peek().payload == "b");
    assert(mq.complete());
    
    mq.drain_waiting(std::back_inserter(got));
    assert(mq.peek().payload == "c");
    assert(got.size() == 2);
    assert(mq.size() == 7);

    nerr = feedn(mq, 1, 3);
    mq.drain_waiting(std::back_inserter(got));
    assert(mq.peek().payload == "d");

    mq.drain_full(std::back_inserter(got));
    assert(mq.empty());
    auto str = stringify(got);
    std::cerr << str << std::endl;
    assert(str == "abcd" "efgh" "ijkl");

}    


void test_merge_speed()
{
    std::cerr << "speed test...\n";
    auto t0 = std::chrono::steady_clock::now();
    merge_t mq(3);

    size_t count = 0;
    int nele = 10000000;
    for (int ele=0; ele<nele; ++ele) {

        for (auto ind : {0, 1, 2, 3}) {
            for (auto str : {0, 1, 2}) {
                const auto s = data[str].substr(ind,1);            
                mq.feed(s, ele + s[0], str);
            }
        }
        
        // emulate a delayed drain
        if (mq.size() < 100) {
            continue;
        }
        std::vector<node_t> got;
        mq.drain_waiting(std::back_inserter(got));
        count += got.size();
    }

    auto t1 = std::chrono::steady_clock::now();
    double dt = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    double rate = nele / dt;
    std::cerr << "N="<< nele*1e-6 << " M, " << dt*1e-6 << " s, " << rate << " MHz" << std::endl;
    std::cerr << "drained " << count << std::endl;
}


int main()
{
    std::cerr << "test_zipper\n";
    test_drain_full();
    test_drain_waiting();
    test_merge_speed();
 
    return 0;
}
