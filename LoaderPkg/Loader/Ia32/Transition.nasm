; @file
; Copyright (c) 2020, ISP RAS. All rights reserved.
; SPDX-License-Identifier: BSD-3-Clause

bits 32

SECTION .text
    align 4

%macro GDT_DESC 2
    dw 0xFFFF, 0
    db 0, %1, %2, 0
%endmacro

GDT_BASE:
    dq  0x0             ; NULL segment
LINEAR_CODE_SEL:        equ $ - GDT_BASE
    GDT_DESC 0x9A, 0xCF
LINEAR_DATA_SEL:        equ $ - GDT_BASE
    GDT_DESC 0x92, 0xCF
LINEAR_CODE64_SEL:      equ $ - GDT_BASE
    GDT_DESC 0x9A, 0xAF
LINEAR_DATA64_SEL:      equ $ - GDT_BASE
    GDT_DESC 0x92, 0xCF

GDT_DESCRIPTOR:
    dw 0x28 - 1 
    dd GDT_BASE
    dd 0x0

KERNEL_ENTRY:
    dd 0

LOADER_PARAMS:
    dd 0
    
PAGE_TABLE:
    dd 0

global ASM_PFX(IsCpuidSupportedAsm)
ASM_PFX(IsCpuidSupportedAsm):
    ; Store original EFLAGS for later comparison.
    pushf
    ; Store current EFLAGS.
    pushf
    ; Invert the ID bit in stored EFLAGS.
    xor dword [esp], 0x200000
    ; Load stored EFLAGS (with ID bit inverted).
    popf
    ; Store EFLAGS again (ID bit may or may not be inverted).
    pushf
    ; Read modified EFLAGS (ID bit may or may not be inverted).
    pop eax
    ; Enable bits in RAX to whichver bits in EFLAGS were changed.
    xor eax, [esp]
    ; Restore stack pointer.
    popf
    ; Leave only the ID bit EFLAGS change result in RAX.
    and eax, 0x200000
    ; Shift it to the lowest bit be boolean compatible.
    shr eax, 21
    ; Return.
    ret

global ASM_PFX(CallKernelThroughGateAsm)
ASM_PFX(CallKernelThroughGateAsm):
    ; Transitioning from protected mode to long mode is described in Intel SDM
    ; 9.8.5 Initializing IA-32e Mode. More detailed explanation why paging needs
    ; to be disabled is explained in 4.1.2 Paging-Mode Enabling.

    ; Disable interrupts.
    cli

    ; Drop return pointer as we no longer need it.
    pop ecx

    ; Save kernel entry point passed by the bootloader.
    pop ecx
    mov eax, KERNEL_ENTRY
    mov [eax], ecx

    ; Save loading params address passed by the bootloader.
    pop ecx
    mov eax, LOADER_PARAMS
    mov [eax], ecx

    ; Save identity page table passed by the bootloader.
    pop ecx
    mov eax, PAGE_TABLE
    mov [eax], ecx

    ; 1. Disable paging.
    ; LAB 2: Your code here:

    ; We need to disable paging:
    ; CRO.PG = 0
    ; PG is 31 bit in CRO
    mov ebx, cr0
    btr ebx, 31
    mov cr0, ebx


    ; 2. Switch to our GDT that supports 64-bit mode and update CS to LINEAR_CODE_SEL.
    ; LAB 2: Your code here:

    lgdt [GDT_DESCRIPTOR]
    jmp LINEAR_CODE_SEL:AsmWithOurGdt

AsmWithOurGdt:

    ; 3. Reset all the data segment registers to linear mode (LINEAR_DATA_SEL).
    ; LAB 2: Your code here:

    mov ebx, LINEAR_DATA_SEL
    ; mov cs, bx
    mov ds, bx
    mov ss, bx
    mov es, bx
    mov fs, bx
    mov gs, bx


    ; 4. Enable PAE/PGE in CR4, which is required to transition to long mode.
    ; This may already be enabled by the firmware but is not guaranteed.
    ; LAB 2: Your code here:

    ; CR4.PAE = 1, 32-bit virtual physical addresses -> 36-bit physical addresses (5th bit CR4).
    ; CR4.PGE = 1, address translations (PDE or PTE records) may be shared between address spaces (7th bit CR4).
    mov ebx, cr4
    bts ebx, 5
    bts ebx, 7
    mov cr4, ebx


    ; 5. Update page table address register (C3) right away with the supplied PAGE_TABLE.
    ; This does nothing as paging is off at the moment as paging is disabled.
    ; LAB 2: Your code here:

    mov ebx, [PAGE_TABLE]
    mov cr3, ebx

    ; 6. Enable long mode (LME) and execute protection (NXE) via the EFER MSR register.
    ; LAB 2: Your code here:
    
    ; Extended Feature Enable Register (EFER) is a model-specific register added in the
    ; AMD K6 processor, to allow enabling the SYSCALL/SYSRET instruction, and later for
    ; entering and exiting long mode. This register becomes architectural in AMD64 and
    ; has been adopted by Intel as IA32_EFER. Its MSR number is 0xC0000080.

    mov ecx, 0xC0000080
    rdmsr
    bts eax, 8 ; set LME ~ Long Mode Enable
    bts eax, 11 ; set NXE ~ No-Execute Enable
    wrmsr


    ; 7. Enable paging as it is required in 64-bit mode.
    ; LAB 2: Your code here:

    mov ebx, cr0
    bts ebx, 1 ; Monitor co-processor
    bts ebx, 16 ; Write protect: CPU cannot write to RO pages whene privilege level is 0
    bts ebx, 18 ; Alignment mask
    bts ebx, 31 ; Paging
    mov cr0, ebx


    ; 8. Transition to 64-bit mode by updating CS with LINEAR_CODE64_SEL.
    ; LAB 2: Your code here:

    jmp LINEAR_CODE64_SEL:AsmInLongMode

AsmInLongMode:
    BITS 64

    ; 9. Reset all the data segment registers to linear 64-bit mode (LINEAR_DATA64_SEL).
    ; LAB 2: Your code here:

    mov rbx, LINEAR_DATA64_SEL
    mov ds, ebx
    mov gs, ebx
    mov ss, ebx
    mov fs, ebx
    mov es, ebx

    ; 10. Jump to the kernel code.
    mov ecx, [REL LOADER_PARAMS]
    mov ebx, [REL KERNEL_ENTRY]
    jmp rbx

noreturn:
    hlt
    jmp noreturn
