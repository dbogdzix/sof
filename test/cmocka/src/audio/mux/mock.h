/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <sof/alloc.h>
#include <sof/audio/component.h>

void rfree(void *ptr);
void *_zalloc(int zone, uint32_t caps, size_t bytes);
void *rzalloc(int zone, uint32_t caps, size_t bytes);

int comp_set_state(struct comp_dev *dev, int cmd);
void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes);
void comp_update_buffer_produce(struct comp_buffer *buffer, uint32_t bytes);
void comp_update_buffer_consume(struct comp_buffer *buffer, uint32_t bytes);
