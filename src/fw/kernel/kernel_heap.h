/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/heap.h"

#include <stddef.h>
#include <stdint.h>

void kernel_heap_init(void);

Heap* kernel_heap_get(void);

void *kernel_heap_malloc(size_t bytes, uintptr_t client_pc);
void *kernel_heap_calloc(size_t count, size_t size, uintptr_t client_pc);
void *kernel_heap_realloc(void *ptr, size_t bytes, uintptr_t client_pc);
void kernel_heap_free(void *ptr, uintptr_t client_pc);
