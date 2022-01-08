/* Simple command-line kernel monitor useful for
 * controlling the kernel and exploring the system interactively. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/env.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/tsc.h>
#include <kern/timer.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/kclock.h>
#include <kern/alloc.h>

#define WHITESPACE "\t\r\n "
#define MAXARGS    16

/* Functions implementing monitor commands */
int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);
int mon_backtrace(int argc, char **argv, struct Trapframe *tf);
int mon_printsomething(int argc, char **argv, struct Trapframe *tf);
int mon_dumpcmos(int argc, char **argv, struct Trapframe *tf);
int mon_start(int argc, char **argv, struct Trapframe *tf);
int mon_stop(int argc, char **argv, struct Trapframe *tf);
int mon_frequency(int argc, char **argv, struct Trapframe *tf);
int mon_memory(int argc, char **argv, struct Trapframe *tf);
int mon_pagetable(int argc, char **argv, struct Trapframe *tf);
int mon_virt(int argc, char **argv, struct Trapframe *tf);
int mon_call(int argc, char **argv, struct Trapframe *tf);
int mon_funcinfo(int argc, char** argv, struct Trapframe* tf);

struct Command {
    const char *name;
    const char *desc;
    /* return -1 to force monitor to exit */
    int (*func)(int argc, char **argv, struct Trapframe *tf);
};

static struct Command commands[] = {
        {"help", "Display this list of commands", mon_help},
        {"kerninfo", "Display information about the kernel", mon_kerninfo},
        {"backtrace", "Print stack backtrace", mon_backtrace},
        {"printsomething", "Print something", mon_printsomething},
        {"dumpcmos", "Print CMOS contents", mon_dumpcmos},
        {"timer_start", "Start timer", mon_start},
        {"timer_stop", "Stop timer", mon_stop},
        {"timer_freq", "Timer frequency", mon_frequency},
        {"dump_virt_tree", "Print virtual tree map", mon_virt},
        {"dump_mem_lists", "Print free memory lists", mon_memory},
        {"dump_pagetable", "Print page table", mon_pagetable},
        {"call", "Call function", mon_call},
        {"funcinfo", "Get info about function", mon_funcinfo}};

#define NCOMMANDS (sizeof(commands) / sizeof(commands[0]))

/* Implementations of basic kernel monitor commands */

int
mon_help(int argc, char **argv, struct Trapframe *tf) {
    for (size_t i = 0; i < NCOMMANDS; i++) {
        cprintf("%s - %s\n", commands[i].name, commands[i].desc);
    }
    return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf) {
    extern char _head64[], entry[], etext[], edata[], end[];

    cprintf("Special kernel symbols:\n");
    cprintf("  _head64 %16lx (virt)  %16lx (phys)\n", (unsigned long)_head64, (unsigned long)_head64);
    cprintf("  entry   %16lx (virt)  %16lx (phys)\n", (unsigned long)entry, (unsigned long)entry - KERN_BASE_ADDR);
    cprintf("  etext   %16lx (virt)  %16lx (phys)\n", (unsigned long)etext, (unsigned long)etext - KERN_BASE_ADDR);
    cprintf("  edata   %16lx (virt)  %16lx (phys)\n", (unsigned long)edata, (unsigned long)edata - KERN_BASE_ADDR);
    cprintf("  end     %16lx (virt)  %16lx (phys)\n", (unsigned long)end, (unsigned long)end - KERN_BASE_ADDR);
    cprintf("Kernel executable memory footprint: %luKB\n", (unsigned long)ROUNDUP(end - entry, 1024) / 1024);
    return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf) {
    // LAB 2: Your code here
    cprintf("Stack backtrace:\n");

    uintptr_t current_rbp_value = read_rbp();
    uintptr_t current_rip_value = 0;

    while (current_rbp_value != 0) {
        current_rip_value = *((unsigned long long *)current_rbp_value + 1);
        cprintf("  rbp %016lx  rip %016lx\n", current_rbp_value, current_rip_value);

        uintptr_t my_rip_address = current_rip_value;
        struct Ripdebuginfo my_info;
        int function_call_result;

        function_call_result = debuginfo_rip(my_rip_address, &my_info);

        cprintf("    %s:%d: %s+%ld\n", my_info.rip_file, my_info.rip_line, my_info.rip_fn_name, current_rip_value - my_info.rip_fn_addr);

        /*
        cprintf("  rip_file = %s\n", my_info.rip_file);
        cprintf("  rip_line = %d\n", my_info.rip_line);
        cprintf("  rip_fn_name = %s\n", my_info.rip_fn_name);
        cprintf("  rip_fn_namelen = %d\n", my_info.rip_fn_namelen);
        cprintf("  rip_fn_addr = %16lx\n", my_info.rip_fn_addr);
        cprintf("  rip_fn_narg = %d\n\n", my_info.rip_fn_narg);
        */

        current_rbp_value = *((unsigned long long *)current_rbp_value);
    }
    return 0;
}

int
mon_printsomething(int argc, char **argv, struct Trapframe *tf) {
    if (argc == 1) {
        cprintf("I will not say the day is done nor bid the stars farewell!\n");
    }
    for (int i = 1; i < argc; i++) {
        cprintf("Hello %s!\n", argv[i]);
    }
    return 0;
}

int
mon_dumpcmos(int argc, char **argv, struct Trapframe *tf) {
    // Dump CMOS memory in the following format:
    // 00: 00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF
    // 10: 00 ..
    // Make sure you understand the values read.
    // Hint: Use cmos_read8()/cmos_write8() functions.
    // LAB 4: Your code here
    for (uint8_t cmos_address = 0; cmos_address < 128;) {
        cprintf("%02X: ", cmos_address);
        for (uint8_t i = 0; i < 16; cmos_address++, i++) {
            cprintf("%02X ", cmos_read8(cmos_address));
        }
        cputchar('\n');
    }
    return 0;
}

/* Implement timer_start (mon_start), timer_stop (mon_stop), timer_freq (mon_frequency) commands. */
// LAB 5: Your code here:

int
mon_start(int argc, char **argv, struct Trapframe *tf) {
    if (argc < 2) {
        cprintf("Not enough arguments!");
        return 1;
    }
    timer_start(argv[1]);
    return 0;
}

int
mon_stop(int argc, char **argv, struct Trapframe *tf) {
    timer_stop();
    return 0;
}

int
mon_frequency(int argc, char **argv, struct Trapframe *tf) {
    if (argc < 2) {
        cprintf("Not enough arguments!");
        return 1;
    }
    timer_cpu_frequency(argv[1]);
    return 0;
}

/* Implement memory (mon_memory) command.
 * This command should call dump_memory_lists()
 */
// LAB 6: Your code here

int
mon_memory(int argc, char **argv, struct Trapframe *tf) {
    dump_memory_lists();
    return 0;
}

/* Implement mon_pagetable() and mon_virt()
 * (using dump_virtual_tree(), dump_page_table())*/
// LAB 7: Your code here

int
mon_pagetable(int argc, char **argv, struct Trapframe *tf) {
    dump_page_table(current_space->pml4);
    return 0;
}

int
mon_virt(int argc, char **argv, struct Trapframe *tf) {
    dump_virtual_tree(current_space->root, current_space->root->class);
    return 0;
}

void test_call();

int
mon_call(int argc, char **argv, struct Trapframe *tf) {
    test_call();
    return 0;
}

/* Kernel monitor command interpreter */

static int
runcmd(char *buf, struct Trapframe *tf) {
    int argc = 0;
    char *argv[MAXARGS];

    argv[0] = NULL;

    /* Parse the command buffer into whitespace-separated arguments */
    for (;;) {
        /* gobble whitespace */
        while (*buf && strchr(WHITESPACE, *buf)) *buf++ = 0;
        if (!*buf) break;

        /* save and scan past next arg */
        if (argc == MAXARGS - 1) {
            cprintf("Too many arguments (max %d)\n", MAXARGS);
            return 0;
        }
        argv[argc++] = buf;
        while (*buf && !strchr(WHITESPACE, *buf)) buf++;
    }
    argv[argc] = NULL;

    /* Lookup and invoke the command */
    if (!argc) return 0;
    for (size_t i = 0; i < NCOMMANDS; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            return commands[i].func(argc, argv, tf);
        }
    }

    cprintf("Unknown command '%s'\n", argv[0]);
    return 0;
}

void
monitor(struct Trapframe *tf) {

    cprintf("Welcome to the JOS kernel monitor!\n");
    cprintf("Type 'help' for a list of commands.\n");

    if (tf) print_trapframe(tf);

    char *buf;
    do buf = readline("K> ");
    while (!buf || runcmd(buf, tf) >= 0);
}

#define INT_TYPE   8
#define FLOAT_TYPE -8

struct ValueAndType {
    unsigned long long value;
    unsigned long long another_value;
    long long type;
    //char* buffer_for_string;
    //REPLACED ALLOC
    char buffer_for_string[250];
};

int
call(unsigned long long *args) {
    unsigned long long rax_value = 0;
    cprintf("=========== FUNCTION STDOUT ===========\n");
    
    asm volatile(
            "pushq %%rcx;"
            "pushq %%rdx;"
            "pushq %%rsi;"
            "pushq %%rdi;"
            "pushq %%r8;"
            "pushq %%r9;"
            "pushq %%r10;"
            "pushq %%r11;"
            /*"movsd -136(%%rax), %%xmm0;"
            "movsd -120(%%rax), %%xmm1;"
            "movsd -104(%%rax), %%xmm2;"
            "movsd -88(%%rax), %%xmm3;"
            "movsd -72(%%rax), %%xmm4;"
            "movsd -56(%%rax), %%xmm5;"
            "movsd -40(%%rax), %%xmm6;"
            "movsd -24(%%rax), %%xmm7;"*/
            "mov $0, %%rdi;"
            "for_begin_label:"
            "cmpq %%rdi, (%%rax);"
            "jna for_end_label;"
            "pushq 64(%%rax, %%rdi, 8);"
            "add $1, %%rdi;"
            "jmp for_begin_label;"
            "for_end_label:"
            "mov 16(%%rax), %%rdi;"
            "movq 24(%%rax), %%rsi;"
            "movq 32(%%rax), %%rdx;"
            "movq 40(%%rax), %%rcx;"
            "movq 48(%%rax), %%r8;"
            "movq 56(%%rax), %%r9;"
            "movq %%rax, %%rbx;"
            "addq $8, %%rbx;"
            "mov -8(%%rax), %%rax;"
            "call *(%%rbx);"
            "subq $8, %%rbx;"
            "movq (%%rbx), %%rbx;"
            "shlq $3, %%rbx;"
            "add %%rbx, %%rsp;"
            "popq %%r11;"
            "popq %%r10;"
            "popq %%r9;"
            "popq %%r8;"
            "popq %%rdi;"
            "popq %%rsi;"
            "popq %%rdx;"
            "popq %%rcx;"
            : "=a"(rax_value)
            : "a"(args)
            : "rbx");
            
    cprintf("\n========== THE END OF STDOUT ==========\n");
    cprintf("Out: %lld\n", rax_value);
    return 0;
}

unsigned long long *
argswt2args(unsigned long long func_address, struct ValueAndType *args_with_types, size_t len) {
    int int_type_counter = 0;
    int float_type_counter = 0;

    for (int i = 0; i < len; i++) {
        if (args_with_types[i].type > 0) {
            int_type_counter += 1;
        } else {
            float_type_counter += 1;
        }
    }

    int on_stack_int_type_counter = (int_type_counter - 6 >= 0) ? int_type_counter - 6 : 0;
    int on_stack_float_type_counter = (float_type_counter - 8 >= 0) ? float_type_counter - 8 : 0;
    int on_stack_alignment = (on_stack_int_type_counter + on_stack_float_type_counter) % 2;

    size_t total_size = 2 * 8 + 1 + 1 + 1 + 6 + on_stack_alignment + on_stack_int_type_counter + on_stack_float_type_counter;
    static unsigned long long result_row[250];
    unsigned long long *result = result_row + 2 * 8 + 1;

    result[-1] = float_type_counter;
    result[0] = on_stack_int_type_counter + on_stack_float_type_counter + on_stack_alignment;
    result[1] = func_address;

    int current_int_type_counter = 0;
    int current_float_type_counter = 0;
    int current_stack_position = total_size - 2 * 8 - 1 - 1;
    for (int i = 0; i < len; i++) {
        if (args_with_types[i].type > 0) {
            if (current_int_type_counter < 6) {
                result[2 + current_int_type_counter] = args_with_types[i].value;
            } else {
                result[current_stack_position] = args_with_types[i].value;
                current_stack_position -= 1;
            }
            current_int_type_counter += 1;
        } else {
            if (current_float_type_counter < 8) {
                result_row[2 * current_float_type_counter] = args_with_types[i].value;
            } else {
                result[current_stack_position] = args_with_types[i].value;
                current_stack_position -= 1;
            }
            current_float_type_counter += 1;
        }
    }

    return result;
}

void
fcall(unsigned long long func_address, struct ValueAndType *args_with_types, size_t n) {
    unsigned long long *result = argswt2args(func_address, args_with_types, n);
    call(result);
}

void
cvtss2sd(unsigned long long *value_address) {
    float f;
    double d;
    memcpy(&f, value_address, sizeof(f));
    d = (double)f;
    memcpy(value_address, &d, sizeof(d));
}

void
test_call() {;
    const size_t MAX_STRING_ARG_LEN = 250;
    char func_name[250];
    readvalue("%s", func_name);
    unsigned long long func_address = /*(unsigned long long)cprintf;*/ find_function_s(func_name);
    if (func_address == 0) {
        cprintf("Cannot find this function!\n");
        return;
    }
    int n;
    cprintf("Input number of function arguments: \n");
    readvalue("%d", &n);
    cprintf("Arguments number = %d\n", n);
    static struct ValueAndType args[250];
    for (int i = 0; i < n; i++) {
        memset(args[i].buffer_for_string, 0, MAX_STRING_ARG_LEN);
    }
    for (int i = 0; i < n; i++) {
        cprintf("%d argument / %d \n", i + 1, n);
        char qualifier[] = {'%', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
        cprintf("Input type: c - char, d - int, llu - unsigned long long, s - string, ...\n");
        readvalue("%s", qualifier + 1);
        cprintf("Readed qualifier = %s\n", qualifier);
        unsigned long long value = 0;
        if (strcmp(qualifier, "%s") == 0) {
            cprintf("Input string, qualifier = %s\n", qualifier);
            readvalue("%s", args[i].buffer_for_string);
            value = (unsigned long long)args[i].buffer_for_string;
        } else {
            cprintf("Input value, qualifier = %s\n", qualifier);
            readvalue(qualifier, &value);
        }
        if ((strcmp(qualifier, "%f") == 0) || (strcmp(qualifier, "%lf") == 0)) {
            args[i].type = FLOAT_TYPE;
            if (strcmp(qualifier, "%f") == 0) {
                cvtss2sd(&value);
            }
        } else {
            args[i].type = INT_TYPE;
        }
        args[i].value = value;
    }
    cprintf("Calling %s\n", func_name);
    fcall(func_address, args, n);
}

int
mon_funcinfo(int argc, char **argv, struct Trapframe *tf) {
    char *fname = argv[1];
    get_arguments(fname);
    return 0;
}