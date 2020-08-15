//
//  Created by Ryan Pendleton on 6/28/18.
//  Copyright © 2018 Ryan Pendleton. All rights reserved.
//

#pragma once

#include <stdint.h>

typedef struct vm_impl* vm_ctx;

typedef enum {
    VM_SUCCESS,
    VM_INPUT_NOT_FOUND,
    VM_INPUT_TOO_LARGE,
    VM_OPCODE_NOT_IMPLEMENTED,
} vm_result;

vm_ctx vm_create(void);
void vm_destroy(vm_ctx vm);

void vm_load_os(vm_ctx vm);
vm_result vm_load_file(vm_ctx vm, const char *file);
vm_result vm_load_data(vm_ctx vm, unsigned const char *data, size_t length);

vm_result vm_run(vm_ctx vm);
