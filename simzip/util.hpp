#pragma once

#include <queue>
#include <iostream>

// waf configure --simulation=/path/to/source/for/simcpp20
#include "fschuetz04/simcpp20.hpp"

namespace simzip {

    // A buffering message queue with fixed capacity
    template<typename Message>
    class bufque {
      public:

        using push_event_t = simcpp20::event<>;
        using pop_event_t = simcpp20::value_event<Message>;

        bufque(simcpp20::simulation<> &sim, uint64_t capacity)
            : sim{sim}, capacity_{capacity} {}

        size_t buffer_size() const { return buffer.size(); };
        size_t inbox_size() const  { return inbox.size(); }
        size_t outbox_size() const  { return outbox.size(); }
        size_t capacity() const { return capacity_; };

        // Push a message to the queue.  The returned event is
        // triggered once there is room in the buffer to actually
        // accept.
        push_event_t push(Message msg)
        {
            // std::cerr << "[" << sim.now() << "] bufque::push(): in="<<inbox.size() << " buf=" << buffer.size() << " out=" << outbox.size() << "\n";

            waiting_message_t ele{msg, sim.event()};
            inbox.push(ele);
            proc();
            return ele.eve;
        }

        // Return event which yeilds the next message.
        pop_event_t pop()
        {
            // std::cerr << "[" << sim.now() << "] bufque::pop(): in="<<inbox.size() << " buf=" << buffer.size() << " out=" << outbox.size() << "\n";

            auto eve = sim.event<Message>();
            outbox.push(eve);
            proc();
            return eve;            
        }

      private:
        
        void proc()
        {
            // std::cerr << "[" << sim.now() << "] bufque::proc(): in="<<inbox.size() << " buf=" << buffer.size() << " out=" << outbox.size() << "\n";

            // service waiting pops
            while (buffer.size() and outbox.size()) {
                auto eve = outbox.front();
                outbox.pop();
                if (eve.aborted()) {
                    // std::cerr << "[" << sim.now() << "] bufque::proc(): pop aborted\n";
                    continue;
                }
                auto msg = buffer.front();
                buffer.pop();
                // std::cerr << "[" << sim.now() << "] bufque::proc(): trigger pop\n";
                eve.trigger(msg);             
            }
            
            // service waiting pushes
            while (buffer.size() < capacity_ and inbox.size()) {
                auto ele = inbox.front();
                inbox.pop();
                if (ele.eve.aborted()) {
                    // std::cerr << "[" << sim.now() << "] bufque::proc(): push aborted\n";
                    continue;
                }
                buffer.push(ele.msg);
                // std::cerr << "[" << sim.now() << "] bufque::proc(): trigger push\n";
                ele.eve.trigger();
            }

            // pushing could have replenished so maybe go again.
            if (buffer.size() and outbox.size()) {
                proc();
            }
        }
            
        struct waiting_message_t { Message msg; push_event_t eve; };
        std::queue<waiting_message_t> inbox;
        std::queue<Message> buffer;
        std::queue<pop_event_t> outbox;

        simcpp20::simulation<> &sim;
        uint64_t capacity_;

    };

}
