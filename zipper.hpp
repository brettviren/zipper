#ifndef ZIPPER_HPP
#define ZIPPER_HPP

#include <queue>
#include <unordered_map>

namespace zipper {

    /**
       A k-way merged queue. 

       A "node" or "element" in the queue is a pair of values:
       (priority, identity)

       The "priority" is any value that can be partially ordered (has
       a "less than" operator or one can be provided as the "Compare"
       template parameter).

       The "identity" is any value that can be hashed and is used
       define a "stream".  That is, all "priority" objects with a
       common "identity" are so grouped.

    */
    template <typename Node,
              typename Sequence = std::vector<Node>,
              typename Compare = std::less<Node> >
    class merged_queue  {
    public:
        using node_t = Node;
        using queue_t = std::priority_queue<Node, Sequence, Compare>;
        using priority_t = typename Node::first_type;
        using identity_t = typename Node::second_type;

        explicit merged_queue (size_t k,
                               const Compare& comp = Compare())
            : waiting(comp)
            , cardinality(k)
        {
        }

        /// Add a node to the queue.
        void feed(const node_t& node) {
            const auto ident = node.second;
            waiting.push(node);
            occupancy[ident] += 1;
        }
        /// Sugar to add a node from its consituent parts.
        void feed(const priority_t& pri, const identity_t& ident)
        {
            feed(node_t(pri, ident));
        }

        /// Unconditionally pop and return the top node.
        node_t drain() {
            if (waiting.empty()) {
                throw std::out_of_range("attempt to drain empty queue");
            }
            // top() and pop() produced UB if called on empty
            auto node = waiting.top();
            waiting.pop();
            occupancy[node.second] -= 1;
            return node;
        }

        /// Check what the next drain() would return.
        const node_t& peek() const {
            if (waiting.empty()) {
                throw std::out_of_range("attempt to peek empty queue");
            }
            return waiting.top();
        }

        bool empty() const { return waiting.empty(); }
        size_t size() const { return waiting.size(); }

        /**
           Return true if the queue has sufficient representation (at
           least one element from each stream not counting top) to
           pop.  This is an O(k) check.
         */
        bool complete() const {
            if (empty()) {
                return false;
            }

            auto ident = peek().second;
            size_t missing = cardinality;
            for (const auto& it : occupancy) {
                size_t have = it.second;
                if (it.first == ident) {
                    //assert(have > 0);
                    have -= 1;
                }
                if (have > 0) {
                    --missing;
                }
            }
            return missing == 0;
        }

    private:
        
        const size_t cardinality;
        queue_t waiting;
        std::unordered_map<identity_t, size_t> occupancy;
    };


    
    // template <typename MQueue, typename Time = time_t>
    // class tardy_queue {
    //     MQueue 

    // public:
    //     tardy_queue(
    // };

}
#endif
