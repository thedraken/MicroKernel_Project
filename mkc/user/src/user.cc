#define NORETURN __attribute__((noreturn))
#define EXTERN_C extern "C"

typedef unsigned int mword;

static unsigned syscall1(unsigned w0)
{
    asm volatile (
        "mov %%esp, %%ecx ;"
        "mov $1f,   %%edx ;"
        "sysenter         ;"
        "1:               ;"
        : "+a" (w0) : : "ecx", "edx");
    return w0;
}

static unsigned syscall2(unsigned w0, unsigned w1)
{
    asm volatile (
        "mov %%esp, %%ecx ;"
        "mov $1f,   %%edx ;"
        "sysenter         ;"
        "1:               ;"
        : "+a" (w0) : "S" (w1) : "ecx", "edx");
    return w0;
}

static unsigned syscall3(unsigned w0, unsigned w1, unsigned w2)
{
    asm volatile (
        "mov %%esp, %%ecx ;"
        "mov $1f,   %%edx ;"
        "sysenter         ;"
        "1:               ;"
        : "+a" (w0) : "S" (w1), "D" (w2) : "ecx", "edx");
    return w0;
}

static unsigned syscall4(unsigned w0, unsigned w1, unsigned w2, unsigned w3) {
    asm volatile (
        "mov %%esp, %%ecx ;"
        "mov $1f,   %%edx ;"
        "sysenter         ;"
        "1:               ;"
        : "+a" (w0) : "S" (w1), "D" (w2), "b" (w3) : "ecx", "edx");
    return w0;
}

static void sys_yield()
{
    syscall1(2);
}

static void sys_create_ec(mword eip, mword esp, unsigned prio) {
    syscall4(1, eip, esp, prio);
}

static void sys_block() {
    syscall1(3);
}

static void sys_unblock_all() {
    syscall1(4);
}

static char stack_b1[4096];
static char stack_b2[4096];
static char stack_c1[4096];
static char stack_c2[4096];

static void thread_b()
{
    syscall1(0);
    sys_block();

    for (int i = 0; i < 3; i++) {
        syscall1(0);
        sys_yield();
    }
    while (1) ;
}

static void thread_c()
{
    syscall1(0);
    sys_block();

    for (int i = 0; i < 3; i++) {
        syscall1(0);
        sys_yield();
    }
    while (1) ;
}

EXTERN_C NORETURN
void main_func()
{
    sys_create_ec((mword) thread_b, (mword) (stack_b1 + sizeof(stack_b1)), 2);
    sys_create_ec((mword) thread_b, (mword) (stack_b2 + sizeof(stack_b2)), 2);
    sys_create_ec((mword) thread_c, (mword) (stack_c1 + sizeof(stack_c1)), 1);
    sys_create_ec((mword) thread_c, (mword) (stack_c2 + sizeof(stack_c2)), 1);

    for (int i = 0; i < 6; i++)
        sys_yield();

    sys_unblock_all();

    for (int i = 0; i < 10; i++)
        sys_yield();

    while (1) ;
}
