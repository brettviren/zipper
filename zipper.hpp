#ifndef ZIPPER_HPP
#define ZIPPER_HPP

#include <queue>
#include <unordered_map>

namespace zipper {

    /**
       A k-way merged queue.

       This implements a max-heap, "larger" elements have priority.
       Caller should pass Comp = std::greater<Element> if the
       "smaller" elements should have higher priority.
     */
    template <typename Element, typename Stream,
              typename Compare = std::less<Element>>
    class merged_queue {
    public:

        struct Node {
            Element element;
            Stream stream;

            bool operator<(const Node& rhs) const {
                Compare comp;
                return comp(element, rhs.element);
            }
        };

        explicit merged_queue (size_t k) : cardinality(k) { }


        template <class InputIterator>
        merged_queue (size_t k,
                      InputIterator first, InputIterator last)
            : cardinality(k)
        {
            while (first != last) {
                this->insert(*first);
                ++first;
            }
        }

        /// Push as Node.
        void push(const Node& node) {
            const auto str = node.stream;
            waiting.push(node);
            occupancy[str] += 1;
        }

        /// Push next element from a stream.
        void push(const Element& ele, const Stream str) {
            push(Node{ele, str});
        }

        /// Pop the top.
        void pop() {
            const auto str = top().stream;
            waiting.pop();
            occupancy[str] -= 1;
        }

        /// Access the current top
        const Node& top() {
            return waiting.top();
        }

        /**
           Return true if the queue has sufficient representation (at
           least one element from each stream not counting top) to
           pop.  This is an O(k) check.
         */
        bool complete() const {
            auto str = waiting.top().stream;
            size_t missing = cardinality;
            for (const auto& it : occupancy) {
                size_t have = it.second;
                if (it.first == str) {
                    assert(have > 0);
                    have -= 1;
                }
                if (have > 0) {
                    --missing;
                }
            }
            return missing == 0;
        }

        // forward to priority_queue
        size_t size() const { return waiting.size(); }
        bool empty() const { return waiting.empty(); }

    private:
        
        const size_t cardinality;
        std::priority_queue<Node> waiting;
        std::unordered_map<Stream, size_t> occupancy;
    };



}
#endif
