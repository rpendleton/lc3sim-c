//
//  Created by Ryan Pendleton on 6/28/18.
//  Copyright © 2018 Ryan Pendleton. All rights reserved.
//

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/errno.h>
#include <sys/termios.h>

#include "vm.h"

enum {
    VM_EXIT_SUCCESS,
    VM_EXIT_USAGE,
    VM_EXIT_INPUT_INVALID,
    VM_EXIT_OPCODE_INVALID,
};

static struct termios original_tio;

static void disable_input_buffering() {
    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

static void restore_input_buffering() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

static void handle_signal(int signal) {
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

int main(int argc, const char * argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <program.obj>\n", argv[0]);
        return VM_EXIT_USAGE;
    }

    vm_result res;
    vm_ctx vm = vm_create();
    vm_load_os(vm);

    res = vm_load_file(vm, argv[1]);

    switch (res) {
        case VM_SUCCESS:
            break;

        case VM_OPCODE_NOT_IMPLEMENTED:
            assert(0);
            break;

        case VM_INPUT_NOT_FOUND:
            fprintf(stderr, "%s: Failed to load input: %s\n", argv[0], strerror(errno));
            return VM_EXIT_INPUT_INVALID;

        case VM_INPUT_TOO_LARGE:
            fprintf(stderr, "%s: Failed to load input: Input exceeded memory space\n", argv[0]);
            return VM_EXIT_INPUT_INVALID;
    }

    disable_input_buffering();
    signal(SIGINT, handle_signal);

    res = vm_run(vm);

    restore_input_buffering();

    switch (res) {
        case VM_SUCCESS:
            break;

        case VM_INPUT_NOT_FOUND:
        case VM_INPUT_TOO_LARGE:
            assert(0);
            break;

        case VM_OPCODE_NOT_IMPLEMENTED:
            fprintf(stderr, "%s: Failed to execute input: Attempted to execute unimplemented opcode\n", argv[0]);
            return VM_EXIT_OPCODE_INVALID;
    }

    vm_destroy(vm);

    return VM_EXIT_SUCCESS;
}
