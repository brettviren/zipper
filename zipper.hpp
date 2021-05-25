#ifndef ZIPPER_HPP
#define ZIPPER_HPP

#include <queue>
#include <unordered_map>

namespace zipper {

    // template <typename Element, typename Stream>
    // struct Node {
    //     Element element;
    //     Stream stream;
    // };

    // template <typename Element, typename Stream>
    // struct NodeAscending {
    //     using node_t = Node<Element, Stream>;
    //     bool operator()(const node_t& a, const node_t& b) {
    //         return a.element > b.element;
    //     }
    // };
    // template <typename Element, typename Stream>
    // struct NodeDescending {
    //     using node_t = Node<Element, Stream>;
    //     bool operator()(const node_t& a, const node_t& b) {
    //         return a.element < b.element;
    //     }
    // };

    /**
       A k-way merged queue. 

       A "node" is a pair: (priority, identity)

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

        /// Push as Node.
        void push(const node_t& node) {
            const auto ident = node.second;
            waiting.push(node);
            occupancy[ident] += 1;
        }
        void push(const priority_t& pri, const identity_t& ident)
        {
            push(node_t(pri, ident));
        }

        const node_t& top() const {
            return waiting.top();
        }

        /// Pop the top.
        void pop() {
            const auto ident = top().second;
            waiting.pop();
            occupancy[ident] -= 1;
        }

        bool empty() const { return waiting.empty(); }
        size_t size() const { return waiting.size(); }

        /**
           Return true if the queue has sufficient representation (at
           least one element from each stream not counting top) to
           pop.  This is an O(k) check.
         */
        bool complete() const {
            auto ident = top().second;
            size_t missing = cardinality;
            for (const auto& it : occupancy) {
                size_t have = it.second;
                if (it.first == ident) {
                    assert(have > 0);
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
