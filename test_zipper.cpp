#include "zipper.hpp"
#include <cassert>
#include <iostream>
#include <vector>

using namespace zipper;

using node_t = std::pair<int, int>;

using merged_ints_t = merged_queue< node_t, std::vector<node_t>,
                                    std::greater<node_t> >;

void test_pq ()
{
    std::priority_queue<int> pq;

    // top() on an empty pq is undefined behavior!
    // auto got = pq.top();
    // std::cerr << "test_pq: " << got << std::endl;

    // also undefined
    // pq.pop();

    /// throws std::out_of_range
    // std::vector<int> v;
    // v.at(0);

}


void test_merge_queue()
{
    merged_ints_t mq(3);

    // 0: 1, 2, 3, 4
    // 1: 2, 4,10
    // 2: 0, 3, 5, 9

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
            mq.drain();
        }
        catch (std::out_of_range& err) {
            caught = true;
        }
        assert(caught);
    }

    assert(!mq.complete());

    mq.feed(1, 0);
    assert(!mq.complete());
    assert(mq.peek().first == 1);

    mq.feed(2, 1);
    assert(!mq.complete());
    assert(mq.peek().first == 1);

    mq.feed(0, 2);
    assert(!mq.complete());
    assert(mq.peek().first == 0);

    mq.feed(2, 0);
    assert(!mq.complete());
    assert(mq.peek().first == 0);
           
    mq.feed(4, 1);
    mq.feed(3, 2);
    assert(mq.complete());
    assert(mq.peek().first == 0);
    mq.drain();
    assert(mq.peek().first == 1);
    assert(mq.complete());

    mq.feed(3, 0);
    mq.feed(4, 0);
    mq.feed(10,1);
    mq.feed(5,2);
    mq.feed(9,2);

    mq.drain();
    assert(mq.peek().first == 2);
    mq.drain();
    assert(mq.peek().first == 2);
    mq.drain();
    assert(mq.peek().first == 3);
    mq.drain();
    assert(mq.peek().first == 3);
    mq.drain();
    assert(mq.peek().first == 4);
    mq.drain();
    assert(mq.peek().first == 4);
    mq.drain();
    assert(mq.peek().first == 5);
    assert(mq.peek().second == 2);
    assert(!mq.complete());
    mq.drain();
    assert(mq.peek().first == 9);
    assert(mq.peek().second == 2);
    mq.drain();
    assert(mq.peek().first == 10);
    assert(mq.peek().second == 1);
    mq.drain();
    
}
#include <chrono>
#include <iostream>

void test_merge_speed()
{
    auto t0 = std::chrono::steady_clock::now();
    merged_ints_t mq(3);

    int nele = 10000000;
    for (int ele=0; ele<nele; ++ele) {
        for (int k=0; k<3; ++k) {
            mq.feed(ele,k);
        }

        // emulate a delayed drain
        if (mq.size() < 100) {
            continue;
        }
        for (int k=0; k<3; ++k) {
            mq.drain();
        }            
    }
    auto t1 = std::chrono::steady_clock::now();
    double dt = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    double rate = nele / dt;
    std::cerr << "N="<< nele*1e-6 << " M, " << dt*1e-6 << " s, " << rate << " MHz" << std::endl;
}


int main()
{
    test_pq();
    test_merge_queue();
    test_merge_speed();

    return 0;
}
