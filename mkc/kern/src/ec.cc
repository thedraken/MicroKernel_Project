#include "bits.h"
#include "ec.h"
#include "assert.h"
#include "cpu.h"
#include "ptab.h"
#include "multiboot.h"
#include "elf.h"
#include "FiFo.h"

Ec *Ec::current = 0;

Ec::Ec(void (*f)(), mword mbi) : cont(f), ec_priority(0) {
    regs.eax = mbi;
    regs.cs = SEL_USER_CODE;
    regs.ds = SEL_USER_DATA;
    regs.es = SEL_USER_DATA;
    regs.ss = SEL_USER_DATA;
    regs.efl = 0x200; // IF = 1
}

Ec::Ec(mword eip, mword esp, unsigned prio)
    : ec_priority(prio < FiFo::NUM_PRIO ? prio : 0) {
    cont = ret_user_iret;
    regs.cs = SEL_USER_CODE;
    regs.ds = SEL_USER_DATA;
    regs.es = SEL_USER_DATA;
    regs.ss = SEL_USER_DATA;
    regs.efl = 0x200; //IF = 1
    regs.eip = eip;
    regs.esp = esp;
}

void Ec::ret_user_sysexit() {
    asm volatile ("lea %0, %%esp;"
        "popa;"
        "sti;"
        "sysexit"
        : : "m" (current->regs) : "memory");
    UNREACHED;
}

void Ec::ret_user_iret() {
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

void Ec::root_invoke() {
    printf("root_invoke started\n");
    fi_fo.init();

    auto *mbi = static_cast<Multiboot *>(Ptab::remap(current->regs.eax));

    if (!(mbi->flags & 8) || (mbi->mods_count != 1))
        panic("exactly ONE multi boot module is required.\n");

    Multiboot_module mod = *static_cast<Multiboot_module *>(Ptab::remap(mbi->mods_addr));

    printf("load module from %x - %x (%u bytes) : ", mod.mod_start, mod.mod_end, mod.mod_end - mod.mod_start);
    char *cmd = static_cast<char *>(Ptab::remap(mod.cmdline));
    printf("%s\n", cmd);

    Eh *e = static_cast<Eh *>(Ptab::remap(mod.mod_start));
    if (e->ei_magic != 0x464c457f || e->ei_data != 1 || e->type != 2)
        panic("No ELF\n");

    unsigned count = e->ph_count;
    current->regs.eip = e->entry;

    Ph *p = static_cast<Ph *>(Ptab::remap(mod.mod_start + e->ph_offset));

    for (; count--; p++) {
        if (p->type == Ph::PT_LOAD) {
            unsigned attr = p->flags & Ph::PF_W ? 7 : 5;

            if (p->f_size != p->m_size || p->v_addr % PAGE_SIZE != p->f_offs % PAGE_SIZE)
                panic("Bad ELF\n");

            mword phys = align_dn(p->f_offs + mod.mod_start, PAGE_SIZE);
            mword virt = align_dn(p->v_addr, PAGE_SIZE);
            mword size = align_up(p->f_size, PAGE_SIZE);

            while (size) {
                Ptab::insert_mapping(virt, phys, attr);
                virt += PAGE_SIZE;
                phys += PAGE_SIZE;
                size -= PAGE_SIZE;
            }
        }
    }

    ret_user_iret();
    FAIL;
}

void Ec::handle_tss() {
    panic("Task gate invoked\n");
}

void Ec::syscall_handler(uint8 n) {
    switch (n) {
        case 0:
            sys_dump();
            break;
        case 1:
            sys_create_ec();
            break;
        case 2: // yield
            sys_yield();
            break;
        case 3: // block self
            sys_block();
            break;
        case 4: // unblock all
            sys_unblock_all();
            break;
        default:
            printf("syscall %d - unknown\n", n);
            break;
    }

    ret_user_sysexit();
    UNREACHED;
}

void Ec::handle_exc(Exc_regs *r) {
    if (r->vec == Cpu::EXC_TS && handle_exc_ts(r))
        return;

    if (r->vec == Cpu::EXC_GP)
        panic("%s GP (EIP=%#lx CR2=%#lx)\n", r->eip < LINK_ADDR ? "User" : "Kernel", r->eip, r->cr2);

    if (r->vec == Cpu::EXC_PF)
        panic("%s PF (EIP=%#lx CR2=%#lx)\n", r->eip < LINK_ADDR ? "User" : "Kernel", r->eip, r->cr2);

    panic("%s EXC %#lx (EIP=%#lx CR2=%#lx)\n", r->eip < LINK_ADDR ? "User" : "Kernel", r->vec, r->eip, r->cr2);
    UNREACHED;
}

bool Ec::handle_exc_ts(Exc_regs *r) {
    if (r->user())
        return false;
    r->efl &= ~0x4000;
    return true;
}

void Ec::sys_dump() {
    printf("EC:%p SYS_DUMP : %#lx, %#lx\n", current,
           current->sys_regs()->esi, current->sys_regs()->edi);
    ret_user_sysexit();
}

void Ec::sys_create_ec() {
    mword user_eip = current->sys_regs()->esi;
    mword user_esp = current->sys_regs()->edi;
    unsigned prio = current->sys_regs()->ebx;

    if (prio >= FiFo::NUM_PRIO)
        prio = 0;

    printf("EC:%p SYS_CREATE_EC: eip=%#lx esp=%#lx prio=%u\n",
           current, user_eip, user_esp, prio);

    Ec *ec = new Ec(user_eip, user_esp, prio);

    fi_fo.enqueue(ec, prio);

    printf("EC:%p created new EC %p (prio=%u)\n", current, ec, prio);

    ret_user_sysexit();
}

void Ec::sys_yield() {
    printf("EC:%p SYS_YIELD (prio=%u)\n", current, current->ec_priority);

    current->cont = ret_user_sysexit;

    fi_fo.enqueue(current);

    Ec *next = fi_fo.dequeue();
    if (!next) {
        printf("ERROR: ready list empty after self-enqueue!\n");
        current->make_current(); // resume self as a safe fallback
        UNREACHED;
    }
    next->make_current();
    UNREACHED;
}

void Ec::sys_block() {
    printf("EC:%p SYS_BLOCK (prio=%u)\n", current, current->ec_priority);

    current->cont = ret_user_sysexit;

    fi_fo.block(current);

    Ec *next = fi_fo.dequeue();
    if (!next) {
        printf("ERROR: no ready EC after blocking!\n");
        while (1);
    }
    next->make_current();
    UNREACHED;
}

void Ec::sys_unblock_all() {
    unsigned n = fi_fo.unblock_all();
    printf("EC:%p SYS_UNBLOCK_ALL: unblocked %u ECs\n", current, n);
    ret_user_sysexit();
}
