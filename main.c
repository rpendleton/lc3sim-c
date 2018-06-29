//
//  Created by Ryan Pendleton on 6/28/18.
//  Copyright © 2018 Ryan Pendleton. All rights reserved.
//

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/termios.h>

#include "vm.h"

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
        return -1;
    }

    disable_input_buffering();
    signal(SIGINT, handle_signal);

    vm_ctx vm = vm_create();
    vm_load_data(vm, lc3os_obj, lc3os_obj_len);
    vm_load_file(vm, argv[1]);
    vm_run(vm);
    vm_destroy(vm);

    restore_input_buffering();

    return 0;
}
