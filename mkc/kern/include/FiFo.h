#pragma once

class Ec;

struct FifoNode {
    Ec *ec = nullptr;
    FifoNode *next = nullptr;
};

struct FifoQueue {
    FifoNode *head = nullptr;
    FifoNode *tail = nullptr;

    bool empty() const { return head == nullptr; }

    bool enqueue(Ec *ec);

    Ec *dequeue();
};

class FiFo {
public:
    static constexpr unsigned NUM_PRIO = 4; //priorities 0->3
    static constexpr unsigned POOL_SIZE = 64; //max live ECs + nodes

    void init();

    void enqueue(Ec *ec, unsigned prio = ~0u);

    Ec *dequeue();

    void block(Ec *ec);

    unsigned unblock_all();

private:
    FifoQueue ready[NUM_PRIO]; //ready[0] = lowest, ready[3] = highest
    FifoQueue blocked; //ECs waiting to be unblocked

    FifoNode pool[POOL_SIZE];
    FifoNode *free_list = nullptr;

    FifoNode *alloc_node();

    void free_node(FifoNode *n);
};

extern FiFo fi_fo;
