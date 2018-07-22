//
//  Created by Ryan Pendleton on 6/28/18.
//  Copyright © 2018 Ryan Pendleton. All rights reserved.
//

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "vm.h"

// MARK: - Types

#define VM_ADDR_MAX UINT16_MAX
#define VM_REG_COUNT 10
#define VM_STATUS_BIT (1 << 15)

typedef uint16_t vm_byte;
typedef uint16_t vm_addr;
typedef size_t vm_reg;

enum {
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
};

enum {
    VM_ADDR_KBSR = 0xFE00,
    VM_ADDR_KBDR = 0xFE02,
    VM_ADDR_DSR  = 0xFE04,
    VM_ADDR_DDR  = 0xFE06,
    VM_ADDR_MCR  = 0xFFFE,
};

enum {
    VM_REG_PC = 8,
    VM_REG_PSR = 9,
};

struct vm_impl {
    vm_byte mem[VM_ADDR_MAX];
    vm_byte reg[VM_REG_COUNT];
};

// MARK: - Helpers

static uint16_t swap16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

static uint16_t zextend(uint16_t val, uint16_t n) {
    return val & (0xffff >> (16 - n));
}

static uint16_t sextend(uint16_t val, uint16_t n) {
    uint16_t m = 1 << (n - 1);
    return (zextend(val, n) ^ m) - m;
}

// MARK: - Creation

vm_ctx vm_create(void) {
    vm_ctx vm = calloc(1, sizeof(struct vm_impl));

    vm->reg[VM_REG_PC] = 0x3000;
    vm->reg[VM_REG_PSR] = 0b010;
    vm->mem[VM_ADDR_MCR] = VM_STATUS_BIT;

    return vm;
}

void vm_destroy(vm_ctx vm) {
    free(vm);
}

// MARK: - Memory

static vm_byte vm_read(vm_ctx vm, vm_addr addr) {
    if (addr == VM_ADDR_KBSR) {
        static fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        static struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;

        if (select(1, &readfds, NULL, NULL, &timeout)) {
            return VM_STATUS_BIT;
        }
        else {
            return 0;
        }
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

void vm_load_file(vm_ctx vm, const char *file) {
    int fd, ret;
    struct stat statbuf;
    unsigned char *data;

    if ((fd = open(file, O_RDONLY)) < 0) {
        abort();
    }

    if ((ret = fstat(fd, &statbuf)) < 0) {
        abort();
    }

    if ((data = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
        abort();
    }

    vm_load_data(vm, data, statbuf.st_size);

    munmap(data, statbuf.st_size);
    close(fd);
}

void vm_load_data(vm_ctx vm, unsigned const char *data, size_t length) {
    vm_addr load_addr = swap16(*((vm_addr*)data));
    size_t load_length = (length - sizeof(vm_addr)) / sizeof(vm_byte);

    assert(load_addr + load_length < VM_ADDR_MAX);

    vm_byte *dest = vm->mem + load_addr;
    vm_byte *source = (vm_byte*)(data + sizeof(vm_addr));

    while (length-- > 0) {
        *(dest++) = swap16(*(source++));
    }
}

// MARK: - Execution

static void vm_setcc(vm_ctx vm, vm_reg reg) {
    vm_byte val = vm->reg[reg];
    vm_byte n = (val >= 0x8000) ? 0b100 : 0;
    vm_byte z = (val == 0) ? 0b010 : 0;
    vm_byte p = (val < 0x8000 && val > 0) ? 0b001 : 0;

    vm->reg[VM_REG_PSR] &= ~0b111;
    vm->reg[VM_REG_PSR] |= n | z | p;
}

static void vm_perform(vm_ctx vm, vm_byte instr) {
    switch (instr >> 12) {
        case VM_OPCODE_ADD: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_reg sr1 = (instr >> 6) & 0b111;

            if (instr & (1 << 5)) {
                vm_byte imm5 = sextend(instr, 5);
                vm->reg[dr] = vm->reg[sr1] + imm5;
            }
            else {
                vm_reg sr2 = instr & 0b111;
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
                vm->reg[dr] = vm->reg[sr1] & imm5;
            }
            else {
                vm_reg sr2 = instr & 0b111;
                vm->reg[dr] = vm->reg[sr1] & vm->reg[sr2];
            }

            vm_setcc(vm, dr);
            break;
        }

        case VM_OPCODE_BR: {
            vm_byte current_nzp = vm->reg[VM_REG_PSR] & 0b111;
            vm_byte desired_nzp = (instr >> 9) & 0b111;

            if (current_nzp & desired_nzp) {
                vm_addr pc_offset9 = sextend(instr, 9);
                vm->reg[VM_REG_PC] += pc_offset9;
            }

            break;
        }

        case VM_OPCODE_JMP: {
            vm_reg baser = (instr >> 6) & 0b111;
            vm->reg[VM_REG_PC] = vm->reg[baser];
            break;
        }

        case VM_OPCODE_JSR: {
            vm->reg[7] = vm->reg[VM_REG_PC];

            if (instr & (1 << 11)) {
                vm_addr pc_offset11 = sextend(instr, 11);
                vm->reg[VM_REG_PC] += pc_offset11;
            }
            else {
                vm_reg baser = (instr >> 6) & 0b111;
                vm->reg[VM_REG_PC] = vm->reg[baser];
            }

            break;
        }

        case VM_OPCODE_LD: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_addr pc_offset9 = sextend(instr, 9);

            vm->reg[dr] = vm_read(vm, vm->reg[VM_REG_PC] + pc_offset9);

            vm_setcc(vm, dr);
            break;
        }

        case VM_OPCODE_LDI: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_addr pc_offset9 = sextend(instr, 9);

            vm->reg[dr] = vm_read(vm, vm_read(vm, vm->reg[VM_REG_PC] + pc_offset9));

            vm_setcc(vm, dr);
            break;
        }

        case VM_OPCODE_LDR: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_reg baser = (instr >> 6) & 0b111;
            vm_addr offset6 = sextend(instr, 6);

            vm->reg[dr] = vm_read(vm, vm->reg[baser] + offset6);

            vm_setcc(vm, dr);
            break;
        }

        case VM_OPCODE_LEA: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_addr pc_offset9 = sextend(instr, 9);

            vm->reg[dr] = vm->reg[VM_REG_PC] + pc_offset9;

            vm_setcc(vm, dr);
            break;
        }

        case VM_OPCODE_NOT: {
            vm_reg dr = (instr >> 9) & 0b111;
            vm_reg sr = (instr >> 6) & 0b111;

            vm->reg[dr] = ~vm->reg[sr];

            vm_setcc(vm, dr);
            break;
        }

        case VM_OPCODE_RTI: {
            abort();
            break;
        }

        case VM_OPCODE_ST: {
            vm_reg sr = (instr >> 9) & 0b111;
            vm_addr pc_offset9 = sextend(instr, 9);

            vm_write(vm, vm->reg[VM_REG_PC] + pc_offset9, vm->reg[sr]);

            break;
        }

        case VM_OPCODE_STI: {
            vm_reg sr = (instr >> 9) & 0b111;
            vm_addr pc_offset9 = sextend(instr, 9);

            vm_write(vm, vm_read(vm, vm->reg[VM_REG_PC] + pc_offset9), vm->reg[sr]);

            break;
        }

        case VM_OPCODE_STR: {
            vm_reg sr = (instr >> 9) & 0b111;
            vm_reg baser = (instr >> 6) & 0b111;
            vm_addr offset6 = sextend(instr, 6);

            vm_write(vm, vm->reg[baser] + offset6, vm->reg[sr]);

            break;
        }

        case VM_OPCODE_TRAP: {
            vm_addr trapvect8 = zextend(instr, 8);

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
            abort();
            break;
    }
}

void vm_run(vm_ctx vm) {
    while (vm_read(vm, VM_ADDR_MCR) & VM_STATUS_BIT) {
        vm_perform(vm, vm_read(vm, vm->reg[VM_REG_PC]++));
    }
}
