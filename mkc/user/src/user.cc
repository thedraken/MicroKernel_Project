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

static void sys_yield()
{
    syscall1(2);
}

static void sys_create_ec(mword eip, mword esp)
{
    syscall3(1, eip, esp);
}

static char stack_b[4096];
static char stack_c[4096];

static void thread_b()
{
    for (int i = 0; i < 3; i++) {
        syscall1(0);
        sys_yield();
    }
    while (1) ;
}

static void thread_c()
{
    for (int i = 0; i < 3; i++) {
        syscall1(0);
        sys_yield();
    }
    while (1) ;
}

EXTERN_C NORETURN
void main_func()
{
    sys_create_ec((mword)thread_b, (mword)(stack_b + sizeof(stack_b)));
    sys_create_ec((mword)thread_c, (mword)(stack_c + sizeof(stack_c)));

    for (int i = 0; i < 5; i++)
        sys_yield();

    while (1) ;
}