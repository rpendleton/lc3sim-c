//
//  Created by Ryan Pendleton on 6/28/18.
//  Copyright © 2018 Ryan Pendleton. All rights reserved.
//

#pragma once

#include <stddef.h>

typedef struct vm_impl* vm_ctx;

typedef enum {
    VM_LOAD_SUCCESS,
    VM_LOAD_INPUT_NOT_FOUND,
    VM_LOAD_INPUT_TOO_LARGE,
} vm_load_result;

typedef enum {
    VM_RUN_SUCCESS,
    VM_RUN_UNIMPLEMENTED_OPCODE,
} vm_run_result;

vm_ctx vm_create(void);
void vm_destroy(vm_ctx vm);

void vm_load_os(vm_ctx vm);
vm_load_result vm_load_file(vm_ctx vm, const char *file);
vm_load_result vm_load_data(vm_ctx vm, unsigned const char *data, size_t length);

vm_run_result vm_run(vm_ctx vm);
