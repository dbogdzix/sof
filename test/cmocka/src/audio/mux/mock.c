// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>

#include "mock.h"

#include <sof/trace.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmocka.h>

void rfree(void *ptr)
{
	free(ptr);
}

void *_zalloc(int zone, uint32_t caps, size_t bytes)
{
	(void)zone;
	(void)caps;
	return calloc(bytes, 1);
}

int comp_set_state(struct comp_dev *dev, int cmd)
{
	switch (cmd) {
	case COMP_TRIGGER_RESET:
		dev->state = COMP_STATE_READY;
		break;

	case COMP_TRIGGER_PREPARE:
		if (dev->state == COMP_STATE_READY)
			dev->state = COMP_STATE_PREPARE;
		else
			return -EINVAL;
		break;
	}
	return 0;
}

void pipeline_xrun(struct pipeline *p, struct comp_dev *dev, int32_t bytes)
{
}

void comp_update_buffer_produce(struct comp_buffer *buffer, uint32_t bytes)
{
}

void comp_update_buffer_consume(struct comp_buffer *buffer, uint32_t bytes)
{
}
