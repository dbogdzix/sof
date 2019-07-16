// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Daniel Bogdzia <danielx.bogdzia@linux.intel.com>

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

#define TEST_CASE(type, cd, config, name) \
	{  (type), (cd), (config), (name)}

enum test_type {
	TEST_STREAM_NUM_INVALID,
	TEST_STREAM_VALS_INVALID,
	TEST_EXCEED_CHANNELS_MAX,
	TEST_CHECK_COPY_CONF_VALS,
	TEST_CHECK_COPY_STREAMS_VALID,
};

struct test_case {
	enum test_type type;
	struct comp_data *cd;
	struct sof_mux_config *config;
	const char *name;
};

static int setup(void **state)
{
	struct test_case *tc = *((struct test_case **)state);

	tc->cd = malloc(sizeof(*tc->cd)
			+ MUX_MAX_STREAMS * sizeof(struct mux_stream_data));
	tc->config = malloc(sizeof(*tc->config)
			    + MUX_MAX_STREAMS
			    * sizeof(struct mux_stream_data));

	/* set numbeer of streams*/
	tc->config->num_streams = MUX_MAX_STREAMS;

	/* assigning streams */
	tc->config->streams[0] = (struct mux_stream_data)
		{ 1, 2, {1, 2, 3, 4}, {0, 0, 0} };
	tc->config->streams[1] = (struct mux_stream_data)
		{ 3, 2, {1, 2, 3, 4}, {0, 0, 0} };
	tc->config->streams[2] = (struct mux_stream_data)
		{ 5, 2, {1, 2, 3, 4}, {0, 0, 0} };
	tc->config->streams[3] = (struct mux_stream_data)
		{ 7, 2, {1, 2, 3, 4}, {0, 0, 0} };

	return 0;
}

static int teardown(void **state)
{
	struct test_case *tc = *((struct test_case **)state);

	free(tc->cd);
	free(tc->config);

	return 0;
}

static void test_mux_set_values_stream_number_invalid(struct test_case *tc)
{
	/* testing reaction for exceeded number of streams */
	tc->config->num_streams = MUX_MAX_STREAMS + 1;

	int ret = mux_set_values(tc->cd, tc->config);

	assert_int_equal(ret, -EINVAL);
}

static void test_mux_set_values_stream_values_invalid(struct test_case *tc)
{
	/* setting repeating stream ID*/
	tc->config->streams[3] = (struct mux_stream_data)
		{ 1, 2, {1, 2, 3, 4}, {0, 0, 0} };

	int ret = mux_set_values(tc->cd, tc->config);

	assert_int_equal(ret, -EINVAL);
}

static void test_mux_set_values_exceed_channels_max(struct test_case *tc)
{
	/* number of channels */
	tc->config->num_streams = MUX_MAX_STREAMS + 1;

	int ret = mux_set_values(tc->cd, tc->config);

	assert_int_equal(ret, -EINVAL);
}

static void test_mux_set_values_copy_config_to_cd(struct test_case *tc)
{
	int ret = mux_set_values(tc->cd, tc->config);

	/* check if configuration is successfully copied to component data */
	assert_int_equal(tc->cd->config.num_channels,
			 tc->config->num_channels);
	assert_int_equal(tc->cd->config.frame_format,
			 tc->config->frame_format);

	assert_int_equal(ret, 0);
}

static void test_mux_set_values_streams_copy_check(struct test_case *tc)
{
	int ret = mux_set_values(tc->cd, tc->config);
	uint8_t i;
	uint8_t j;

	for (i = 0; i < tc->config->num_streams; ++i) {
		/* check if streams are successfully copied to component data */
		assert_int_equal(tc->cd->config.streams[i].num_channels,
				 tc->config->streams[i].num_channels);
		assert_int_equal(tc->cd->config.streams[i].pipeline_id,
				 tc->config->streams[i].pipeline_id);
		for (j = 0; j < tc->config->streams[i].num_channels; ++j)
			assert_int_equal(tc->cd->config.streams[i].mask[j],
					 tc->config->streams[i].mask[j]);
	}

	assert_int_equal(ret, 0);
}

static void test_audio_mux(void **state)
{
	struct test_case *tc = *((struct test_case **)state);

	switch (tc->type) {
	case TEST_STREAM_NUM_INVALID:
		test_mux_set_values_stream_number_invalid(tc);
		break;

	case TEST_STREAM_VALS_INVALID:
		test_mux_set_values_stream_values_invalid(tc);
		break;

	case TEST_EXCEED_CHANNELS_MAX:
		test_mux_set_values_exceed_channels_max(tc);
		break;

	case TEST_CHECK_COPY_CONF_VALS:
		test_mux_set_values_copy_config_to_cd(tc);
		break;

	case TEST_CHECK_COPY_STREAMS_VALID:
		test_mux_set_values_streams_copy_check(tc);
		break;
	}
}

static struct test_case test_cases[] = {
	TEST_CASE(TEST_STREAM_NUM_INVALID, NULL, NULL,
		  "test_mux_set_values_stream_number_invalid"),
	TEST_CASE(TEST_STREAM_VALS_INVALID, NULL, NULL,
		  "test_mux_set_values_stream_values_invalid"),
	TEST_CASE(TEST_EXCEED_CHANNELS_MAX, NULL, NULL,
		  "test_mux_set_values_exceed_channels_max"),
	TEST_CASE(TEST_CHECK_COPY_CONF_VALS, NULL, NULL,
		  "test_mux_set_values_cp_cfg_2_cd"),
	TEST_CASE(TEST_CHECK_COPY_STREAMS_VALID, NULL, NULL,
		  "test_mux_set_values_streams_copy_check"),
};

int main(void)
{
	struct CMUnitTest tests[ARRAY_SIZE(test_cases)];
	int i;

	for (i = 0; i < ARRAY_SIZE(test_cases); ++i) {
		struct CMUnitTest *t = &tests[i];

		t->name = test_cases[i].name;
		t->test_func = test_audio_mux;
		t->initial_state = &test_cases[i];
		t->setup_func = setup;
		t->teardown_func = teardown;
	}

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
