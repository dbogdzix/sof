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

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <sof/sof.h>
#include <sof/alloc.h>
#include <sof/audio/component.h>
#include <sof/audio/mux.h>

#include "mock.h"
#include <mock_trace.h>

TRACE_IMPL()

static void test_demux_copy_no_sinks_active(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));

	/* activate bsink list without items */
	list_init(&dev->bsink_list);

	int ret = demux_copy(dev);

	assert_int_equal(ret, 0);

	free(dev);
}

static void test_demux_copy_no_source_active(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct list_item *sink_item = malloc(2 * sizeof(*sink_item));
	struct list_item *source_item = malloc(2 * sizeof(*source_item));

	dev->state = 1;

	/* set bsink list */
	list_init(&dev->bsource_list);
	list_item_prepend(sink_item, &dev->bsink_list);

	/* get sink stream */
	sink = container_of(dev->bsink_list.next, struct comp_buffer,
			    source_list);

	/* alloc sink and set parameters */
	sink->sink = malloc(sizeof(*sink->sink));
	sink->sink->state = 1;
	sink->free = 1;

	/*set bsource list */
	list_init(&dev->bsource_list);
	list_item_prepend(source_item, &dev->bsource_list);

	/* get source stream */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
			       sink_list);

	/* alloc source and set parameters */
	source->source = malloc(sizeof(*source->source));
	source->source->state = 0;

	int ret = demux_copy(dev);

	assert_int_equal(ret, 0);

	free(source->source);
	free(sink->sink);
	free(dev);
}

static void test_demux_copy_underrun(void **state)
{
	struct comp_dev *dev = malloc(2 * sizeof(*dev));
	struct list_item *sink_item = malloc(2 * sizeof(*sink_item));
	struct list_item *source_item = malloc(sizeof(*source_item));
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct comp_data *cd = malloc(sizeof(*cd)
				      + MUX_MAX_STREAMS
				      * sizeof(struct mux_stream_data));

	dev->private = cd;
	dev->state = 1;

	/* set bsink list */
	list_init(&dev->bsink_list);
	list_item_prepend(sink_item, &dev->bsink_list);

	/* get sink stream */
	sink = container_of(dev->bsink_list.next, struct comp_buffer,
			    source_list);

	/* alloc sink and set parameters */
	sink->sink = malloc(sizeof(*sink->sink));
	sink->sink->state = 1;
	sink->sink->params.frame_fmt = SOF_IPC_FRAME_S16_LE;
	sink->avail = 0;

	/*set bsource list */
	list_init(&dev->bsource_list);
	list_item_prepend(source_item, &dev->bsource_list);

	/* get source stream */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);

	/* alloc source and set parameters */
	source->source = malloc(sizeof(*source->source));
	source->source->state = 1;
	source->avail = 0;

	int ret = demux_copy(dev);

	assert_int_equal(ret, -EIO);

	free(sink->sink);
	free(source->source);
	free(cd);
	free(dev);
}

static void test_demux_copy_test(void **state)
{
	struct comp_dev *dev = malloc(2 * sizeof(*dev));
	struct list_item *sink_item = malloc(2 * sizeof(*sink_item));
	struct list_item *source_item = malloc(sizeof(*source_item));
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct comp_data *cd = malloc(sizeof(*cd)
				      + MUX_MAX_STREAMS
				      * sizeof(struct mux_stream_data));

	dev->private = cd;
	dev->state = 1;

	/* set bsink list */
	list_init(&dev->bsink_list);
	list_item_prepend(sink_item, &dev->bsink_list);

	/* get sink stream */
	sink = container_of(dev->bsink_list.next, struct comp_buffer,
			    source_list);

	/* alloc sink and set parameters */
	sink->sink = malloc(sizeof(*sink->sink));
	sink->sink->state = 1;
	sink->sink->params.frame_fmt = SOF_IPC_FRAME_S16_LE;
	sink->avail = 0;

	/*set bsource list */
	list_init(&dev->bsource_list);
	list_item_prepend(source_item, &dev->bsource_list);

	/* get source stream */
	source = list_first_item(&dev->bsource_list, struct comp_buffer,
				 sink_list);

	/* alloc source and set parameters */
	source->source = malloc(sizeof(*source->source));
	source->source->state = 1;
	source->source->params.frame_fmt = SOF_IPC_FRAME_S16_LE;
	source->avail = 1;

	/* set device parameters */
	dev->params.channels = 2;
	cd->demux = mux_func_map[0].demux_proc_func;

	int ret = demux_copy(dev);

	assert_int_equal(ret, 0);

	free(source->source);
	free(sink->sink);
	free(cd);
	free(dev);
}

static void test_mux_copy_no_sinks_active(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));

	/* activate bsink list without items */
	list_init(&dev->bsource_list);

	int ret = mux_copy(dev);

	assert_int_equal(ret, 0);

	free(dev);
}

static void test_mux_copy_underrun(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct list_item *source_item = malloc(2 * sizeof(*source_item));
	struct list_item *sink_item = malloc(2 * sizeof(*sink_item));
	struct comp_buffer *sink;
	struct comp_buffer *source;

	dev->state = 1;

	/* set bsink list */
	list_init(&dev->bsink_list);
	list_item_prepend(sink_item, &dev->bsink_list);

	/* get sink stream */
	sink = container_of(dev->bsink_list.next, struct comp_buffer,
			    source_list);

	/* alloc sink and set parameters */
	sink->sink = malloc(sizeof(*sink->sink));
	sink->sink->state = 1;

	/* set bsource list */
	list_init(&dev->bsource_list);
	list_item_prepend(source_item, &dev->bsource_list);

	/* get source stream */
	source = container_of(source_item, struct comp_buffer, sink_list);

	/* alloc source and set parameters */
	source->source = malloc(sizeof(*source->source));
	source->source->state = 1;
	source->avail = 0;

	int ret = mux_copy(dev);

	assert_int_equal(ret, -EIO);
}

static void test_mux_copy_overrun(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct list_item *source_item = malloc(2 * sizeof(*source_item));
	struct list_item *sink_item = malloc(2 * sizeof(*sink_item));
	struct comp_buffer *sink;
	struct comp_buffer *source;

	dev->state = 1;

	/* set bsink list */
	list_init(&dev->bsink_list);
	list_item_prepend(sink_item, &dev->bsink_list);

	/* get sink stream */
	sink = container_of(dev->bsink_list.next, struct comp_buffer,
			    source_list);

	/* alloc sink and set parameters */
	sink->sink = malloc(sizeof(*sink->sink));
	sink->sink->state = 1;
	sink->free = 0;

	/* set bsource list */
	list_init(&dev->bsource_list);
	list_item_prepend(source_item, &dev->bsource_list);

	/* get source stream */
	source = container_of(source_item, struct comp_buffer, sink_list);

	/* alloc source and set parameters */
	source->source = malloc(sizeof(*source->source));
	source->source->state = 1;
	source->avail = 1;

	int ret = mux_copy(dev);

	assert_int_equal(ret, -EIO);
}

static void test_mux_copy_test(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct list_item *source_item = malloc(2 * sizeof(*source_item));
	struct list_item *sink_item = malloc(2 * sizeof(*sink_item));
	struct comp_buffer *source;
	struct comp_buffer *sink;
	struct comp_data *cd = malloc(sizeof(*cd)
				      + MUX_MAX_STREAMS
				      * sizeof(struct mux_stream_data));
	dev->private = cd;
	dev->state = 1;

	/* set bsink list */
	list_init(&dev->bsink_list);
	list_item_prepend(sink_item, &dev->bsink_list);

	/* get sink stream */
	sink = container_of(dev->bsink_list.next, struct comp_buffer,
			    source_list);

	/* alloc sink and set parameters */
	sink->sink = malloc(sizeof(*sink->sink));
	sink->sink->state = 1;
	sink->free = 1;
	sink->sink->params.frame_fmt = SOF_IPC_FRAME_S16_LE;
	sink->sink->params.channels = 2;

	/* set bsource list */
	list_init(&dev->bsource_list);
	list_item_prepend(source_item, &dev->bsource_list);

	/* get source stream */
	source = container_of(source_item, struct comp_buffer, sink_list);

	/* alloc source and set parameters */
	source->source = malloc(sizeof(*source->source));
	source->source->state = 1;
	source->avail = 1;
	source->source->params.frame_fmt = SOF_IPC_FRAME_S16_LE;
	source->source->params.channels = 2;

	/* set mux processing function */
	cd->mux = mux_func_map[0].mux_proc_func;

	int ret = mux_copy(dev);

	assert_int_equal(ret, 0);

	free(sink->sink);
	free(dev);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_mux_copy_no_sinks_active),
		cmocka_unit_test(test_mux_copy_underrun),
		cmocka_unit_test(test_mux_copy_overrun),
		cmocka_unit_test(test_mux_copy_test),
		cmocka_unit_test(test_demux_copy_no_sinks_active),
		cmocka_unit_test(test_demux_copy_no_source_active),
		cmocka_unit_test(test_demux_copy_underrun),
		cmocka_unit_test(test_demux_copy_test),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
