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

#include "bits.h"
#include "ec.h"
#include "assert.h"
#include "cpu.h"
#include "ptab.h"
#include "multiboot.h"
#include "elf.h"

Ec * Ec::current = 0;

// solely used for root_invoke()
Ec::Ec (void (*f)(), mword mbi) : cont (f)
{
    regs.eax = mbi;
    regs.cs  = SEL_USER_CODE;
    regs.ds  = SEL_USER_DATA;
    regs.es  = SEL_USER_DATA;
    regs.ss  = SEL_USER_DATA;
    regs.efl = 0x200;           // IF = 1
}

// only used by syscall create thread (EC+SC)
Ec::Ec (mword eip, mword esp)
{
    cont = ret_user_iret;
    regs.cs  = SEL_USER_CODE;
    regs.ds  = SEL_USER_DATA;
    regs.es  = SEL_USER_DATA;
    regs.ss  = SEL_USER_DATA;
    regs.efl = 0x200;           // IF = 1
    regs.eip = eip;
    regs.esp = esp;
}

void Ec::ret_user_sysexit()
{
    asm volatile ("lea %0, %%esp;"
                  "popa;"
                  "sti;"
                  "sysexit"
                  : : "m" (current->regs) : "memory");

    UNREACHED;
}

void Ec::ret_user_iret()
{
    asm volatile ("lea %0, %%esp;"
                  "popa;"
                  "pop %%gs;"
                  "pop %%fs;"
                  "pop %%es;"
                  "pop %%ds;"
                  "add $8, %%esp;"
                  "iret"
                  : : "m" (current->regs) : "memory");

    UNREACHED;
}

void Ec::root_invoke()
{
    // find multi boot info
    Multiboot * mbi = static_cast<Multiboot *>(Ptab::remap (current->regs.eax));

    if (!(mbi->flags & 8) || (mbi->mods_count != 1))
        panic ("exactly ONE multi boot module is required.\n");

    // load module desciptor
    Multiboot_module mod = *static_cast<Multiboot_module *>(Ptab::remap (mbi->mods_addr));

    printf ("load module from %x - %x (%u bytes) : ", mod.mod_start, mod.mod_end, mod.mod_end - mod.mod_start);
    char * cmd = static_cast<char *>(Ptab::remap (mod.cmdline));
    printf ("%s\n",cmd);

    // remap elf header
    Eh * e = static_cast<Eh *>(Ptab::remap (mod.mod_start));
    if (e->ei_magic != 0x464c457f || e->ei_data != 1 || e->type != 2)
        panic ("No ELF\n");

    unsigned count = e->ph_count;
    current->regs.eip = e->entry;

    // remap program headers
    Ph * p = static_cast<Ph *>(Ptab::remap (mod.mod_start + e->ph_offset));

    for (; count--; p++) {

        if (p->type == Ph::PT_LOAD) {

            unsigned attr = p->flags & Ph::PF_W ? 7 : 5;

            if (p->f_size != p->m_size || p->v_addr % PAGE_SIZE != p->f_offs % PAGE_SIZE)
                panic ("Bad ELF\n");

            mword phys = align_dn (p->f_offs + mod.mod_start, PAGE_SIZE);
            mword virt = align_dn (p->v_addr, PAGE_SIZE);
            mword size = align_up (p->f_size, PAGE_SIZE);

            while (size) {
                Ptab::insert_mapping (virt, phys, attr);
                virt += PAGE_SIZE;
                phys += PAGE_SIZE;
                size -= PAGE_SIZE;
            }
        }
    }

    ret_user_iret();

    FAIL;
}

void Ec::handle_tss()
{
    panic ("Task gate invoked\n");
}

void Ec::syscall_handler (uint8 n)
{
    switch (n) {

    case 0:
        sys_dump();
        break;

    default:
        printf ("syscall %d - unknown\n", n);
        break;
	}

    ret_user_sysexit();

    UNREACHED;
}

void Ec::sys_dump()
{
    printf ("EC:%p SYS_DUMP : %#lx, %#lx\n", current, current->sys_regs()->esi, current->sys_regs()->edi);

    ret_user_sysexit();
}

bool Ec::handle_exc_ts (Exc_regs *r)
{
    if (r->user())
        return false;

    // SYSENTER with EFLAGS.NT=1 and IRET faulted
    r->efl &= ~0x4000; // nested task eflag

    return true;
}

void Ec::handle_exc (Exc_regs *r)
{
    if (r->vec == Cpu::EXC_TS && handle_exc_ts (r))
        return;

    if (r->vec == Cpu::EXC_GP)
        panic ("%s GP (EIP=%#lx CR2=%#lx)\n", r->eip < LINK_ADDR ? "User" : "Kernel", r->eip, r->cr2);

    if (r->vec == Cpu::EXC_PF)
        panic ("%s PF (EIP=%#lx CR2=%#lx)\n", r->eip < LINK_ADDR ? "User" : "Kernel", r->eip, r->cr2);

    panic ("%s EXC %#lx (EIP=%#lx CR2=%#lx)\n", r->eip < LINK_ADDR ? "User" : "Kernel", r->vec, r->eip, r->cr2);

    UNREACHED;
}
