/*
 * Execution Context
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once

#include "compiler.h"
#include "regs.h"
#include "tss.h"
#include "kalloc.h"
#include "memory.h"

class Ec {
    void (*cont)();

    Exc_regs regs;

    REGPARM(1)
    static void handle_exc(Exc_regs *) asm ("exc_handler");

    NORETURN
    static void handle_tss() asm ("tss_handler");

    static bool handle_exc_ts(Exc_regs *);

    ALWAYS_INLINE
    Sys_regs *sys_regs() { return &regs; }

    ALWAYS_INLINE
    Exc_regs *exc_regs() { return &regs; }

public:
    unsigned ec_priority = 0;
    static Ec *current;

    static void sys_create_ec() __attribute__((noreturn));

    static void sys_yield() __attribute__((noreturn));

    static void sys_block() __attribute__((noreturn));

    static void sys_unblock_all() __attribute__((noreturn));

    Ec(void (*)(), mword = 0);

    Ec(mword eip, mword esp) : Ec(eip, esp, 0) {
    }

    Ec(mword eip, mword esp, unsigned prio = 0);

    Ec *next_item = nullptr;

    ALWAYS_INLINE NORETURN
    void make_current() {
        current = this;

        Tss::run.sp0 = reinterpret_cast<mword>(exc_regs() + 1);

        asm volatile ("mov %0, %%esp;"
            "jmp *%1"
            : : "g" (KSTCK_ADDR + PAGE_SIZE), "rm" (cont) : "memory");
        UNREACHED;
    }

    HOT NORETURN
    static void ret_user_sysexit();

    NORETURN
    static void ret_user_iret() asm ("ret_user_iret");

    NORETURN
    static void root_invoke();

    HOT NORETURN REGPARM(1)
    static void syscall_handler(uint8) asm ("syscall_handler");

    NORETURN
    static void sys_dump();

    ALWAYS_INLINE
    void *operator new(size_t) { return Kalloc::allocator.alloc(sizeof(Ec)); }

    ALWAYS_INLINE
    void operator delete(void *) {
        /* nop */
    }
};
