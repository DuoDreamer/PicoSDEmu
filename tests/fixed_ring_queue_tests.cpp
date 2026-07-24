#include <cassert>
#include <string>

#include "picosd/protocol/fixed_ring_queue.hpp"

int main() {
    using picosd::protocol::FixedRingQueue;

    FixedRingQueue<int, 3> queue;
    assert(queue.capacity() == 3U);
    assert(queue.empty());
    assert(!queue.full());

    int value = 0;
    assert(!queue.try_pop(value));
    assert(queue.try_push(10));
    assert(queue.try_push(20));
    assert(queue.try_push(30));
    assert(queue.full());
    assert(!queue.try_push(40));
    assert(queue.size() == 3U);

    assert(queue.try_pop(value));
    assert(value == 10);
    assert(queue.try_pop(value));
    assert(value == 20);
    assert(queue.try_push(40));
    assert(queue.try_push(50));
    assert(queue.full());
    assert(queue.try_pop(value));
    assert(value == 30);
    assert(queue.try_pop(value));
    assert(value == 40);
    assert(queue.try_pop(value));
    assert(value == 50);
    assert(queue.empty());

    FixedRingQueue<std::string, 1> text_queue;
    std::string text = "moved";
    assert(text_queue.try_push(std::move(text)));
    assert(text_queue.try_pop(text));
    assert(text == "moved");
    text_queue.try_push("discarded");
    text_queue.clear();
    assert(text_queue.empty());

    return 0;
}
