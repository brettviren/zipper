#include <cassert>
#include "zipper.hpp"

using namespace zipper;

void test_merge_queue()
{
    merged_queue< int, int, std::greater<int> > mq(3);

    // 0: 1, 2, 3, 4
    // 1: 2, 4,10
    // 2: 0, 3, 5, 9

    mq.push(1, 0);
    assert(!mq.complete());
    assert(mq.top().element == 1);

    mq.push(2, 1);
    assert(!mq.complete());
    assert(mq.top().element == 1);

    mq.push(0, 2);
    assert(!mq.complete());
    assert(mq.top().element == 0);

    mq.push(2, 0);
    assert(!mq.complete());
    assert(mq.top().element == 0);
           
    mq.push(4, 1);
    mq.push(3, 2);
    assert(mq.complete());
    assert(mq.top().element == 0);
    mq.pop();
    assert(mq.top().element == 1);
    assert(mq.complete());

    mq.push(3, 0);
    mq.push(4, 0);
    mq.push(10,1);
    mq.push(5,2);
    mq.push(9,2);

    mq.pop();
    assert(mq.top().element == 2);
    mq.pop();
    assert(mq.top().element == 2);
    mq.pop();
    assert(mq.top().element == 3);
    mq.pop();
    assert(mq.top().element == 3);
    mq.pop();
    assert(mq.top().element == 4);
    mq.pop();
    assert(mq.top().element == 4);
    mq.pop();
    assert(mq.top().element == 5);
    assert(mq.top().stream == 2);
    assert(!mq.complete());
    mq.pop();
    assert(mq.top().element == 9);
    assert(mq.top().stream == 2);
    mq.pop();
    assert(mq.top().element == 10);
    assert(mq.top().stream == 1);
    mq.pop();
    
}


int main()
{
    test_merge_queue();


    return 0;
}
