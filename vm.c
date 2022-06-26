//
//  Created by Ryan Pendleton on 6/28/18.
//  Copyright © 2018 Ryan Pendleton. All rights reserved.
//

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "vm.h"

#ifdef TRACE
#   define DEBUG_TRACE(...) fprintf(stderr, __VA_ARGS__)
#else
#   define DEBUG_TRACE(...)
#endif

extern unsigned char lc3os_obj[];
extern unsigned int lc3os_obj_len;

// MARK: - Types

enum {
    VM_ADDR_MAX = UINT16_MAX,
    VM_ADDR_INITIAL = 0x3000,
    VM_SIGN_BIT = 1 << 15,
    VM_STATUS_BIT = 1 << 15,
};

typedef uint16_t vm_byte;
typedef uint16_t vm_addr;

typedef enum {
    VM_OPCODE_ADD  = 0b0001,
    VM_OPCODE_AND  = 0b0101,
    VM_OPCODE_BR   = 0b0000,
    VM_OPCODE_JMP  = 0b1100,
    VM_OPCODE_JSR  = 0b0100,
    VM_OPCODE_LD   = 0b0010,
    VM_OPCODE_LDI  = 0b1010,
    VM_OPCODE_LDR  = 0b0110,
    VM_OPCODE_LEA  = 0b1110,
    VM_OPCODE_NOT  = 0b1001,
    VM_OPCODE_RTI  = 0b1000,
    VM_OPCODE_ST   = 0b0011,
    VM_OPCODE_STI  = 0b1011,
    VM_OPCODE_STR  = 0b0111,
    VM_OPCODE_TRAP = 0b1111,
    VM_OPCODE_RESERVED = 0b1101,
} vm_opcode;

typedef enum {
    VM_ADDR_KBSR = 0xfe00,
    VM_ADDR_KBDR = 0xfe02,
    VM_ADDR_DSR  = 0xfe04,
    VM_ADDR_DDR  = 0xfe06,
    VM_ADDR_MCR  = 0xfffe,
} vm_addr_special;

typedef enum {
    VM_REG_0 = 0,
    VM_REG_1,
    VM_REG_2,
    VM_REG_3,
    VM_REG_4,
    VM_REG_5,
    VM_REG_6,
    VM_REG_7,
    VM_REG_PC,
    VM_REG_PSR,
    VM_REG_COUNT
} vm_reg;

typedef enum {
    VM_FLAG_NEGATIVE = 0b100,
    VM_FLAG_ZERO     = 0b010,
    VM_FLAG_POSITIVE = 0b001,
} vm_flag;

struct vm_impl {
    vm_byte mem[VM_ADDR_MAX];
    vm_byte reg[VM_REG_COUNT];
};

// MARK: - Helpers

static uint16_t swap16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

static uint16_t sextend(uint16_t val, uint16_t n) {
    uint16_t m = 1 << (n - 1);
    val &= ((1 << n) - 1);
    return (val ^ m) - m;
}

// MARK: - Creation

vm_ctx vm_create(void) {
    vm_ctx vm = calloc(1, sizeof(struct vm_impl));

    vm->reg[VM_REG_PC] = VM_ADDR_INITIAL;
    vm->reg[VM_REG_PSR] = VM_FLAG_ZERO;
    vm->mem[VM_ADDR_MCR] = VM_STATUS_BIT;

    return vm;
}

void vm_destroy(vm_ctx vm) {
    free(vm);
}

// MARK: - Memory

static vm_byte vm_read(vm_ctx vm, vm_addr addr) {
    assert(vm != NULL);

    if (addr == VM_ADDR_KBSR) {
        static fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        return select(1, &readfds, NULL, NULL, &timeout) ? VM_STATUS_BIT : 0;
    }
    else if (addr == VM_ADDR_KBDR) {
        if (vm_read(vm, VM_ADDR_KBSR)) {
            return getchar();
        }
        else {
            return 0;
        }
    }
    else if (addr == VM_ADDR_DSR) {
        return VM_STATUS_BIT;
    }
    else if (addr == VM_ADDR_DDR) {
        return 0;
    }

    return vm->mem[addr];
}

static void vm_write(vm_ctx vm, vm_addr addr, vm_byte val) {
    assert(vm != NULL);

    if (addr == VM_ADDR_KBSR || addr == VM_ADDR_KBDR || addr == VM_ADDR_DSR) {
        return;
    }
    else if (addr == VM_ADDR_DDR) {
        putchar(val);
        fflush(stdout);
        return;
    }

    vm->mem[addr] = val;
}

void vm_load_os(vm_ctx vm) {
    vm_load_result res = vm_load_data(vm, lc3os_obj, lc3os_obj_len);
    assert(res == VM_LOAD_SUCCESS);
}

vm_load_result vm_load_file(vm_ctx vm, const char *file) {
    int fd, ret;
    struct stat statbuf;
    unsigned char *data;

    if ((fd = open(file, O_RDONLY)) < 0) {
        return VM_LOAD_INPUT_NOT_FOUND;
    }

    if ((ret = fstat(fd, &statbuf)) < 0) {
        return VM_LOAD_INPUT_NOT_FOUND;
    }

    if ((data = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
        return VM_LOAD_INPUT_NOT_FOUND;
    }

    vm_load_result result = vm_load_data(vm, data, statbuf.st_size);

    munmap(data, statbuf.st_size);
    close(fd);

    return result;
}

vm_load_result vm_load_data(vm_ctx vm, unsigned const char *data, size_t length) {
    assert(vm != NULL);

    vm_addr load_addr = swap16(*((vm_addr*)data));
    size_t load_length = (length - sizeof(vm_addr)) / sizeof(vm_byte);

    assert(load_addr + load_length < VM_ADDR_MAX);

    vm_byte *dest = vm->mem + load_addr;
    vm_byte *source = (vm_byte*)(data + sizeof(vm_addr));

    if (dest + load_length >= vm->mem + VM_ADDR_MAX) {
        return VM_LOAD_INPUT_TOO_LARGE;
    }

    while (load_length-- > 0) {
        *(dest++) = swap16(*(source++));
    }

    vm->reg[VM_REG_PC] = load_addr;

    return VM_LOAD_SUCCESS;
}

// MARK: - Execution

static vm_flag vm_sign_flag(uint16_t val) {
    if (val == 0) {
        return VM_FLAG_ZERO;
    }
    else if (val & VM_SIGN_BIT) {
        return VM_FLAG_NEGATIVE;
    }
    else {
        return VM_FLAG_POSITIVE;
    }
}

static void vm_setcc(vm_ctx vm, vm_reg reg) {
    assert(vm != NULL);

    vm->reg[VM_REG_PSR] = vm_sign_flag(vm->reg[reg]);
}

static vm_run_result vm_perform(vm_ctx vm, vm_byte instr) {
    assert(vm != NULL);

    DEBUG_TRACE("DEBUG vm_perform instr %x REG_PC %x\n", instr, vm->reg[VM_REG_PC]);

    switch ((vm_opcode)(instr >> 12)) {
        case VM_OPCODE_ADD: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_reg sr1 = (instr >> 6) & 0b111;

            if (instr & (1 << 5)) {
                vm_byte imm5 = sextend(instr, 5);
                DEBUG_TRACE("VM_OPCODE_ADD dr %x sr1 %x imm5 %x\n", dr, sr1, imm5);

                vm->reg[dr] = vm->reg[sr1] + imm5;
            }
            else {
                vm_reg sr2 = instr & 0b111;
                DEBUG_TRACE("VM_OPCODE_ADD dr %x sr1 %x sr2 %x\n", dr, sr1, sr2);

                vm->reg[dr] = vm->reg[sr1] + vm->reg[sr2];
            }

            vm_setcc(vm, dr);
            break;
        }

        case VM_OPCODE_AND: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_reg sr1 = (instr >> 6) & 0b111;

            if (instr & (1 << 5)) {
                vm_byte imm5 = sextend(instr, 5);
                DEBUG_TRACE("VM_OPCODE_AND dr %x sr1 %x imm5 %x\n", dr, sr1, imm5);

                vm->reg[dr] = vm->reg[sr1] & imm5;
            }
            else {
                vm_reg sr2 = instr & 0b111;
                DEBUG_TRACE("VM_OPCODE_AND dr %x sr1 %x sr2 %x\n", dr, sr1, sr2);

                vm->reg[dr] = vm->reg[sr1] & vm->reg[sr2];
            }

            vm_setcc(vm, dr);
            break;
        }

        case VM_OPCODE_BR: {
            vm_byte current_nzp = vm->reg[VM_REG_PSR] & 0b111;
            vm_byte desired_nzp = (instr >> 9) & 0b111;
            vm_addr pc_offset9 = sextend(instr, 9);
            DEBUG_TRACE("VM_OPCODE_BR current_nzp %x desired_nzp %x pc_offset9 %x\n", current_nzp, desired_nzp, pc_offset9);

            if (current_nzp & desired_nzp) {
                vm->reg[VM_REG_PC] += pc_offset9;
            }

            break;
        }

        case VM_OPCODE_JMP: {
            vm_reg baser = (instr >> 6) & 0b111;
            DEBUG_TRACE("VM_OPCODE_JMP baser %x\n", baser);

            vm->reg[VM_REG_PC] = vm->reg[baser];
            break;
        }

        case VM_OPCODE_JSR: {
            // If this is a JSR R7 instruction, we need to make sure we don't accidentally overwrite the value in R7
            // before using it for the jump. These steps don't strictly match the ones in the 2nd edition of the book,
            // but according to documentation in the official lc3tools simulator, these steps perform the instruction
            // how it was intended. Future editions of the book will address this inconsistency.
            vm_addr original_pc = vm->reg[VM_REG_PC];

            if (instr & (1 << 11)) {
                vm_addr pc_offset11 = sextend(instr, 11);
                DEBUG_TRACE("VM_OPCODE_JSR pc_offset11 %x\n", pc_offset11);

                vm->reg[VM_REG_PC] += pc_offset11;
            }
            else {
                vm_reg baser = (instr >> 6) & 0b111;
                vm_reg baser_value = vm->reg[baser];
                DEBUG_TRACE("VM_OPCODE_JSR baser %x baser_value %x\n", baser, baser_value);

                vm->reg[VM_REG_PC] = baser_value;
            }

            vm->reg[7] = original_pc;

            break;
        }

        case VM_OPCODE_LD: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_addr pc_offset9 = sextend(instr, 9);
            DEBUG_TRACE("VM_OPCODE_LD dr %x pc_offset9 %x\n", dr, pc_offset9);

            vm->reg[dr] = vm_read(vm, vm->reg[VM_REG_PC] + pc_offset9);
            vm_setcc(vm, dr);

            break;
        }

        case VM_OPCODE_LDI: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_addr pc_offset9 = sextend(instr, 9);
            DEBUG_TRACE("VM_OPCODE_LDI dr %x pc_offset9 %x\n", dr, pc_offset9);

            vm->reg[dr] = vm_read(vm, vm_read(vm, vm->reg[VM_REG_PC] + pc_offset9));
            vm_setcc(vm, dr);

            break;
        }

        case VM_OPCODE_LDR: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_reg baser = (instr >> 6) & 0b111;
            vm_addr offset6 = sextend(instr, 6);
            DEBUG_TRACE("VM_OPCODE_LDR dr %x baser %x offset6 %x\n", dr, baser, offset6);

            vm->reg[dr] = vm_read(vm, vm->reg[baser] + offset6);
            vm_setcc(vm, dr);

            break;
        }

        case VM_OPCODE_LEA: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_addr pc_offset9 = sextend(instr, 9);
            DEBUG_TRACE("VM_OPCODE_LEA dr %x pc_offset9 %x\n", dr, pc_offset9);

            vm->reg[dr] = vm->reg[VM_REG_PC] + pc_offset9;
            vm_setcc(vm, dr);

            break;
        }

        case VM_OPCODE_NOT: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_reg sr = (instr >> 6) & 0b111;
            DEBUG_TRACE("VM_OPCODE_NOT dr %x sr %x\n", dr, sr);

            vm->reg[dr] = ~vm->reg[sr];
            vm_setcc(vm, dr);

            break;
        }

        case VM_OPCODE_RTI: {
            DEBUG_TRACE("VM_OPCODE_RTI\n");
            return VM_RUN_UNIMPLEMENTED_OPCODE;
        }

        case VM_OPCODE_ST: {
            vm_reg sr = (instr >> 9) & 0b111;
            vm_addr pc_offset9 = sextend(instr, 9);
            DEBUG_TRACE("VM_OPCODE_ST sr %x pc_offset9 %x\n", sr, pc_offset9);

            vm_write(vm, vm->reg[VM_REG_PC] + pc_offset9, vm->reg[sr]);

            break;
        }

        case VM_OPCODE_STI: {
            vm_reg sr = (instr >> 9) & 0b111;
            vm_addr pc_offset9 = sextend(instr, 9);
            DEBUG_TRACE("VM_OPCODE_STI sr %x pc_offset9 %x\n", sr, pc_offset9);

            vm_write(vm, vm_read(vm, vm->reg[VM_REG_PC] + pc_offset9), vm->reg[sr]);

            break;
        }

        case VM_OPCODE_STR: {
            vm_reg sr = (instr >> 9) & 0b111;
            vm_reg baser = (instr >> 6) & 0b111;
            vm_addr offset6 = sextend(instr, 6);
            DEBUG_TRACE("VM_OPCODE_STR sr %x baser %x offset6 %x\n", sr, baser, offset6);

            vm_write(vm, vm->reg[baser] + offset6, vm->reg[sr]);

            break;
        }

        case VM_OPCODE_TRAP: {
            vm_addr trapvect8 = instr & 0xff;
            DEBUG_TRACE("VM_OPCODE_TRAP trapvect8 %x\n", trapvect8);

            if (trapvect8 == 0x20) {
                // handle GETC efficiently to prevent high CPU usage when idle
                vm->reg[0] = getchar();
            }
            else {
                // fallback to OS implementation of remaining traps
                vm->reg[7] = vm->reg[VM_REG_PC];
                vm->reg[VM_REG_PC] = vm_read(vm, trapvect8);
            }

            break;
        }

        case VM_OPCODE_RESERVED:
            DEBUG_TRACE("VM_OPCODE_RESERVED\n");
            return VM_RUN_UNIMPLEMENTED_OPCODE;
    }

    return VM_RUN_SUCCESS;
}

vm_run_result vm_run(vm_ctx vm) {
    assert(vm != NULL);

    while (vm_read(vm, VM_ADDR_MCR) & VM_STATUS_BIT) {
        vm_run_result res = vm_perform(vm, vm_read(vm, vm->reg[VM_REG_PC]++));

        if (res != VM_RUN_SUCCESS) {
            return res;
        }
    }

    return VM_RUN_SUCCESS;
}
