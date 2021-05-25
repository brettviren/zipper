#include <cassert>
#include "zipper.hpp"

using namespace zipper;

using node_t = std::pair<int, int>;

using merged_ints_t = merged_queue< node_t, std::vector<node_t>,
                                    std::greater<node_t> >;

void test_merge_queue()
{
    merged_ints_t mq(3);

    // 0: 1, 2, 3, 4
    // 1: 2, 4,10
    // 2: 0, 3, 5, 9

    mq.push(1, 0);
    assert(!mq.complete());
    assert(mq.top().first == 1);

    mq.push(2, 1);
    assert(!mq.complete());
    assert(mq.top().first == 1);

    mq.push(0, 2);
    assert(!mq.complete());
    assert(mq.top().first == 0);

    mq.push(2, 0);
    assert(!mq.complete());
    assert(mq.top().first == 0);
           
    mq.push(4, 1);
    mq.push(3, 2);
    assert(mq.complete());
    assert(mq.top().first == 0);
    mq.pop();
    assert(mq.top().first == 1);
    assert(mq.complete());

    mq.push(3, 0);
    mq.push(4, 0);
    mq.push(10,1);
    mq.push(5,2);
    mq.push(9,2);

    mq.pop();
    assert(mq.top().first == 2);
    mq.pop();
    assert(mq.top().first == 2);
    mq.pop();
    assert(mq.top().first == 3);
    mq.pop();
    assert(mq.top().first == 3);
    mq.pop();
    assert(mq.top().first == 4);
    mq.pop();
    assert(mq.top().first == 4);
    mq.pop();
    assert(mq.top().first == 5);
    assert(mq.top().second == 2);
    assert(!mq.complete());
    mq.pop();
    assert(mq.top().first == 9);
    assert(mq.top().second == 2);
    mq.pop();
    assert(mq.top().first == 10);
    assert(mq.top().second == 1);
    mq.pop();
    
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
            mq.push(ele,k);
        }
        if (mq.size() < 100) {
            continue;
        }
        for (int k=0; k<3; ++k) {
            mq.push(ele,k);
        }            
    }
    auto t1 = std::chrono::steady_clock::now();
    double dt = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
    double rate = nele / dt;
    std::cerr << "N="<< nele*1e-6 << " M, " << dt*1e-6 << " s, " << rate << " MHz" << std::endl;
}


int main()
{
    test_merge_queue();
    test_merge_speed();

    return 0;
}
