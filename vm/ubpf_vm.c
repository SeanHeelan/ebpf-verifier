/*
 * Copyright 2015 Big Switch Networks, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include "ebpf.h"
#include "ubpf.h"

#define MAX_INSTS 65536

struct ubpf_vm {
    struct ebpf_inst *insts;
    uint16_t num_insts;
};

static bool validate(const struct ebpf_inst *insts, uint32_t num_insts, char **errmsg);

static char *
error(const char *fmt, ...)
{
    char *msg;
    va_list ap;
    va_start(ap, fmt);
    if (vasprintf(&msg, fmt, ap) < 0) {
        msg = NULL;
    }
    va_end(ap);
    return msg;
}

struct ubpf_vm *
ubpf_create(const void *code, uint32_t code_len, char **errmsg)
{
    *errmsg = NULL;

    if (code_len % 8 != 0) {
        *errmsg = error("code_len must be a multiple of 8");
        return NULL;
    }

    if (!validate(code, code_len/8, errmsg)) {
        return NULL;
    }

    struct ubpf_vm *vm = malloc(sizeof(*vm));
    if (!vm) {
        return NULL;
    }

    vm->insts = malloc(code_len);
    if (vm->insts == NULL) {
        return NULL;
    }

    memcpy(vm->insts, code, code_len);
    vm->num_insts = code_len/sizeof(vm->insts[0]);

    return vm;
}

void
ubpf_destroy(struct ubpf_vm *vm)
{
    free(vm->insts);
    free(vm);
}

uint64_t
ubpf_exec(const struct ubpf_vm *vm, void *ctx)
{
    uint16_t pc = 0;
    const struct ebpf_inst *insts = vm->insts;
    uint16_t num_insts = vm->num_insts;
    uint64_t reg[16];

    /* TODO remove this when the verifier can prove uninitialized registers are
     * not read from */
    memset(reg, 0xff, sizeof(reg));

    while (1) {
        const uint16_t cur_pc = pc;
        if (pc >= num_insts) {
            /* TODO validate control flow */
            fprintf(stderr, "reached end of instructions\n");
            return UINT64_MAX;
        }

        struct ebpf_inst inst = insts[pc++];

        switch (inst.opcode) {
        case EBPF_OP_ADD_IMM:
            reg[inst.dst] += inst.imm;
            reg[inst.dst] &= UINT32_MAX;
            break;
        case EBPF_OP_ADD_REG:
            reg[inst.dst] += reg[inst.src];
            reg[inst.dst] &= UINT32_MAX;
            break;
        case EBPF_OP_MOV_IMM:
            reg[inst.dst] = inst.imm;
            reg[inst.dst] &= UINT32_MAX;
            break;
        case EBPF_OP_MOV_REG:
            reg[inst.dst] = reg[inst.src];
            reg[inst.dst] &= UINT32_MAX;
            break;
        case EBPF_OP_JGT_IMM:
            if (reg[inst.dst] > (uint32_t)inst.imm) {
                pc += inst.offset;
            }
            break;
        case EBPF_OP_JGT_REG:
            if (reg[inst.dst] > reg[inst.src]) {
                pc += inst.offset;
            }
            break;
        case EBPF_OP_JGE_IMM:
            if (reg[inst.dst] >= (uint32_t)inst.imm) {
                pc += inst.offset;
            }
            break;
        case EBPF_OP_JGE_REG:
            if (reg[inst.dst] >= reg[inst.src]) {
                pc += inst.offset;
            }
            break;
        case EBPF_OP_EXIT:
            return reg[0];
        default:
            /* Should have been caught by validate() */
            fprintf(stderr, "internal uBPF error: unknown opcode 0x%02x at PC %u\n", inst.opcode, cur_pc);
            return UINT64_MAX;
        }
    }
}

static bool
validate(const struct ebpf_inst *insts, uint32_t num_insts, char **errmsg)
{
    /* TODO validate registers */
    /* TODO validate jmp offsets */

    if (num_insts >= MAX_INSTS) {
        *errmsg = error("too many instructions (max %u)", MAX_INSTS);
        return false;
    }

    int i;
    for (i = 0; i < num_insts; i++) {
        struct ebpf_inst inst = insts[i];
        switch (inst.opcode) {
        case EBPF_OP_ADD_REG:
        case EBPF_OP_ADD_IMM:
        case EBPF_OP_MOV_REG:
        case EBPF_OP_MOV_IMM:
        case EBPF_OP_JGE_REG:
        case EBPF_OP_JGE_IMM:
        case EBPF_OP_JGT_REG:
        case EBPF_OP_JGT_IMM:
        case EBPF_OP_EXIT:
            break;

        default:
            *errmsg = error("unknown opcode 0x%02x at PC %d", inst.opcode, i);
            return false;
        }
    }

    return true;
}