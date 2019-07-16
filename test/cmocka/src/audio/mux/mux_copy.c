// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>
//         Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmocka.h>

#include <sof/sof.h>
#include <sof/alloc.h>
#include <sof/audio/component.h>
#include <sof/audio/mux.h>

#include "mock.h"
#include <mock_trace.h>

TRACE_IMPL()

struct test_data {
	struct comp_dev *dev;
	struct comp_data *cd;
	struct comp_buffer *source;
	struct comp_buffer *sink;
};

static struct comp_driver *comp_mux;
static struct comp_driver *comp_demux;

/* Intercept comp_register to get tested component driver directly */
int comp_register(struct comp_driver *drv)
{
	switch (drv->type) {
	case SOF_COMP_MUX:
		comp_mux = drv;
		break;

	case SOF_COMP_DEMUX:
		comp_demux = drv;
		break;
	}

	return 0;
}

static int group_setup(void **state)
{
	sys_comp_mux_init();

	if (!comp_mux || !comp_demux)
		return -ENOENT;

	return 0;
}

static int setup(void **state)
{
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct test_data *td = malloc(sizeof(struct test_data));

	td->dev = malloc(sizeof(struct comp_dev));
	td->sink = malloc(sizeof(struct comp_buffer));
	td->source = malloc(sizeof(struct comp_buffer));
	td->cd = malloc(sizeof(struct comp_data) + MUX_MAX_STREAMS
			* sizeof(struct mux_stream_data));

	td->dev->private = td->cd;
	td->dev->state = COMP_STATE_READY;

	/* set mux & demux processing function */
	td->cd->mux = mux_func_map[0].mux_proc_func;
	td->cd->demux = mux_func_map[0].demux_proc_func;

	/* set bsink list */
	list_init(&td->dev->bsink_list);
	list_item_prepend(&td->sink->sink_list, &td->dev->bsink_list);

	/* get sink stream */
	sink = container_of(td->dev->bsink_list.next, struct comp_buffer,
			    sink_list);

	/* alloc sink and set default parameters */
	sink->sink = malloc(sizeof(struct comp_dev));
	sink->sink->state = COMP_STATE_INIT;
	sink->sink->params.frame_fmt = SOF_IPC_FRAME_S16_LE;
	sink->sink->params.channels = 2;
	sink->free = 0;
	sink->avail = 0;

	/*set bsource list */
	list_init(&td->dev->bsource_list);
	list_item_prepend(&td->source->source_list, &td->dev->bsource_list);

	/* get source stream */
	source = list_first_item(&td->dev->bsource_list, struct comp_buffer,
				 source_list);

	/* alloc source and set default parameters */
	source->source = malloc(sizeof(struct comp_dev));
	source->source->state = COMP_STATE_INIT;
	source->source->params.frame_fmt = SOF_IPC_FRAME_S16_LE;
	source->source->params.channels = 2;
	source->free = 0;
	source->avail = 0;

	td->source = source;
	td->sink = sink;

	*state = td;

	return 0;
}

static int teardown(void **state)
{
	struct test_data *td = *state;

	free(td->source->source);
	free(td->sink->sink);

	free(td->sink);
	free(td->source);
	free(td->cd);
	free(td->dev);

	free(td);

	return 0;
}

static void test_demux_copy_no_sinks_active(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));

	/* activate bsink list without items */
	list_init(&dev->bsink_list);

	assert_int_equal(comp_demux->ops.copy(dev), 0);

	free(dev);
}

static void test_demux_copy_no_source_active(void **state)
{
	struct test_data *td = *state;

	td->sink->sink->state = COMP_STATE_READY;
	td->sink->free = 1;

	td->source->source->state = COMP_STATE_INIT;

	assert_int_equal(comp_demux->ops.copy(td->dev), 0);
}

static void test_demux_copy_underrun(void **state)
{
	struct test_data *td = *state;

	td->sink->sink->state = COMP_STATE_READY;

	td->source->source->state = COMP_STATE_READY;

	assert_int_equal(comp_demux->ops.copy(td->dev), 0);
}

static void test_demux_copy_test(void **state)
{
	struct test_data *td = *state;

	td->sink->sink->state = COMP_STATE_READY;

	td->source->source->state = COMP_STATE_READY;
	td->source->avail = 1;

	/* set device parameters */
	td->dev->params.frame_fmt = SOF_IPC_FRAME_S16_LE;
	td->dev->params.channels = 2;

	assert_int_equal(comp_demux->ops.copy(td->dev), 0);
}

static void test_mux_copy_no_sinks_active(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));

	/* activate bsink list without items */
	list_init(&dev->bsource_list);

	assert_int_equal(comp_mux->ops.copy(dev), 0);

	free(dev);
}

static void test_mux_copy_underrun(void **state)
{
	struct test_data *td = *state;

	td->sink->sink->state = COMP_STATE_READY;

	td->source->source->state = COMP_STATE_READY;

	assert_int_equal(comp_mux->ops.copy(td->dev), 0);
}

static void test_mux_copy_overrun(void **state)
{
	struct test_data *td = *state;

	td->sink->sink->state = COMP_STATE_READY;

	td->source->source->state = COMP_STATE_READY;
	td->source->avail = 1;

	assert_int_equal(comp_mux->ops.copy(td->dev), 0);
}

static void test_mux_copy_test(void **state)
{
	struct test_data *td = *state;

	td->sink->sink->state = COMP_STATE_READY;
	td->sink->free = 1;

	td->source->source->state = COMP_STATE_READY;
	td->source->avail = 1;

	assert_int_equal(comp_mux->ops.copy(td->dev), 0);
}

#define TEST_WITH_SETUP(name) \
	cmocka_unit_test_setup_teardown(name, setup, teardown)

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_mux_copy_no_sinks_active),
		TEST_WITH_SETUP(test_mux_copy_underrun),
		TEST_WITH_SETUP(test_mux_copy_overrun),
		TEST_WITH_SETUP(test_mux_copy_test),
		cmocka_unit_test(test_demux_copy_no_sinks_active),
		TEST_WITH_SETUP(test_demux_copy_no_source_active),
		TEST_WITH_SETUP(test_demux_copy_underrun),
		TEST_WITH_SETUP(test_demux_copy_test),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, group_setup, NULL);
}
