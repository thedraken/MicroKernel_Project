//
// Created by draken on 21/05/2026.
//

#ifndef MICROKERNEL_PROJECT_FIFO_H
#define MICROKERNEL_PROJECT_FIFO_H
#pragma once
#include "ec.h"

class FiFo {
public:
    FiFo() : head(nullptr), tail(nullptr) {}

    // Enqueue at tail
    void enqueue(Ec *ec) {
        printf("FIFO: enqueue EC %p\n", ec);
        if (ec == nullptr) return;
        ec->next_item = nullptr;          // clean link
        if (tail)
            tail->next_item = ec;
        else
            head = ec;
        tail = ec;
        printf("FIFO: enqueue EC %p\n", ec);
    }

    // Dequeue from head; returns nullptr if empty
    Ec *dequeue() {
        printf("FIFO: dequeue EC head=%p\n", head);
        if (!head) return nullptr;
        Ec *ec = head;
        head = head->next_item;
        if (!head)
            tail = nullptr;
        ec->next_item = nullptr;
        printf("FIFO: dequeue EC %p\n", ec);
        return ec;
    }

    bool empty() const { return head == nullptr; }

private:
    Ec *head, *tail;
};

extern FiFo fi_fo;

#endif //MICROKERNEL_PROJECT_FIFO_H
