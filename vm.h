//
//  Created by Ryan Pendleton on 6/28/18.
//  Copyright © 2018 Ryan Pendleton. All rights reserved.
//

#pragma once

typedef struct vm_impl* vm_ctx;

vm_ctx vm_create(void);
void vm_destroy(vm_ctx vm);

void vm_load_os(vm_ctx vm);
void vm_load_file(vm_ctx vm, const char *file);
void vm_load_data(vm_ctx vm, unsigned const char *data, size_t length);

void vm_run(vm_ctx vm);
