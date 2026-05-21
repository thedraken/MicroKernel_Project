#define NORETURN __attribute__((noreturn))
#define EXTERN_C extern "C"
#include <cstddef>

typedef struct EC {
    void* kernel_sp;
    void* user_stack_pointer;
    void* user_instruction_pointer;
    struct EC* next;
    int id;
} EC_t;

typedef struct {
    EC_t* head;
    EC_t* tail;
} ReadyList_t;

ReadyList_t ready_list = {NULL, NULL};
EC_t* current_ec = NULL;


unsigned syscall1 (unsigned w0)
{
    asm volatile (
        "   mov %%esp, %%ecx    ;"
        "   mov $1f, %%edx      ;"
        "   sysenter            ;"
        "1:                     ;"
        : "+a" (w0) : : "ecx", "edx");
    return w0;
}

unsigned syscall2 (unsigned w0, unsigned w1)
{
    asm volatile (
        "   mov %%esp, %%ecx    ;"
        "   mov $1f, %%edx      ;"
        "   sysenter            ;"
        "1:                     ;"
        : "+a" (w0) : "S" (w1) : "ecx", "edx");
    return w0;
}

// example syscall for yielding
unsigned sys_yield()
{
    return syscall1 (0xdeadbeaf /* TODO use correct syscall number*/);
}

EXTERN_C NORETURN
void main_func ()
{
    while (1) ;
}


