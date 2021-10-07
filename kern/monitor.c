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

#define WHITESPACE "\t\r\n "
#define MAXARGS    16

/* Functions implementing monitor commands */
int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);
int mon_backtrace(int argc, char **argv, struct Trapframe *tf);
int mon_printsomething(int argc, char **argv, struct Trapframe* tf);

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
        {"printsomething", "Print something", mon_printsomething}
};
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
mon_printsomething(int argc, char** argv, struct Trapframe* tf) {
    if (argc == 1) {
        cprintf("I will not say the day is done nor bid the stars farewell!\n");
    }
    for (int i = 1; i < argc; i++) {
        cprintf("Hello %s!\n", argv[i]);
    }
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

    char *buf;
    do buf = readline("K> ");
    while (!buf || runcmd(buf, tf) >= 0);
}
