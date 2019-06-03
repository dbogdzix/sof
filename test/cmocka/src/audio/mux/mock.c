/*
 * Copyright (c) 2019, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
 */
#include "mock.h"

#include <sof/trace.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

static int mock_select_function;

void rfree(void *ptr)
{
	free(ptr);
}

void set_mock_function(int x)
{
	mock_select_function = x;
}

void *_zalloc(int zone, uint32_t caps, size_t bytes)
{
	/* this provide ability to simulate NULL allocation
	 *  if multiple _zalloc occurs in tested function
	 */
	if (mock_select_function == 1)
		return NULL;
	(void)zone;
	(void)caps;
	mock_select_function--;
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
	return mock_select_function;
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
