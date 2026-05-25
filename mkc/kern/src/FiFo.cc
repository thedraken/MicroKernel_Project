
#include "FiFo.h"
#include "ec.h"
FiFo fi_fo;

void FiFo::init() {
    for (unsigned i = 0; i < POOL_SIZE - 1; ++i)
        pool[i].next = &pool[i + 1];
    pool[POOL_SIZE - 1].next = nullptr;
    free_list = &pool[0];
}

FifoNode *FiFo::alloc_node() {
    if (!free_list)
        return nullptr;
    FifoNode *n = free_list;
    free_list = n->next;
    n->next = nullptr;
    return n;
}

void FiFo::free_node(FifoNode *n) {
    n->ec = nullptr;
    n->next = free_list;
    free_list = n;
}

bool FifoQueue::enqueue(Ec *ec) {
    (void) ec;
    return false;
}

Ec *FifoQueue::dequeue() {
    if (!head)
        return nullptr;
    FifoNode *n = head;
    head = n->next;
    if (!head)
        tail = nullptr;
    Ec *ec = n->ec;
    return ec;
}

void FiFo::enqueue(Ec *ec, unsigned prio) {
    if (prio >= NUM_PRIO)
        prio = ec->ec_priority;
    if (prio >= NUM_PRIO)
        prio = 0;
    ec->ec_priority = prio;
    FifoNode *n = alloc_node();
    if (!n) {
        while (1);
    }
    n->ec = ec;
    n->next = nullptr;

    FifoQueue &q = ready[prio];
    if (!q.tail) {
        q.head = q.tail = n;
    } else {
        q.tail->next = n;
        q.tail = n;
    }
}

Ec *FiFo::dequeue() {
    for (int p = (int) NUM_PRIO - 1; p >= 0; --p) {
        FifoQueue &q = ready[p];
        if (q.empty())
            continue;

        FifoNode *n = q.head;
        q.head = n->next;
        if (!q.head)
            q.tail = nullptr;

        Ec *ec = n->ec;
        free_node(n);
        return ec;
    }
    return nullptr;
}

void FiFo::block(Ec *ec) {
    FifoNode *n = alloc_node();
    if (!n)
        while (1);

    n->ec = ec;
    n->next = nullptr;

    if (!blocked.tail) {
        blocked.head = blocked.tail = n;
    } else {
        blocked.tail->next = n;
        blocked.tail = n;
    }
}

unsigned FiFo::unblock_all() {
    unsigned count = 0;
    while (!blocked.empty()) {
        FifoNode *n = blocked.head;
        blocked.head = n->next;
        if (!blocked.head)
            blocked.tail = nullptr;

        Ec *ec = n->ec;
        free_node(n);
        enqueue(ec, ec->ec_priority);
        ++count;
    }
    return count;
}
