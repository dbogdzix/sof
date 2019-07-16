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

#include <mock_trace.h>

#define INPUT_DATA_NUMBER 6

#define TEST_ANY_VALUE 0

#define TEST_CASE(setting, type, comp, name) \
	{ (setting), (type), (comp), (name "_set" #setting)}

TRACE_IMPL()

enum test_type {
	TEST_CALC_16_MASK0,
	TEST_CALC_24_MASK0,
	TEST_CALC_32_MASK0,
	TEST_CALC_16,
	TEST_CALC_24,
	TEST_CALC_32,
	TEST_DEMUX_16_TEST,
	TEST_DEMUX_24_TEST,
	TEST_DEMUX_32_TEST,
	TEST_MUX_16_TEST,
	TEST_MUX_24_TEST,
	TEST_MUX_32_TEST,
	TEST_MUX_GET_PROCESS_FUNC_DIFFERENT_FRAMES_FORMATS,
	TEST_MUX_GET_PROCESS_FUNC_FRAMES_FORMAT_VALID,
	TEST_DEMUX_GET_PROCESS_FUNC_DIFFERENT_FRAMES_FORMATS,
	TEST_DEMUX_GET_PROCESS_FUNC_FRAMES_FORMAT_VALID,
};

struct calc_sample_setting {
	uint8_t num_ch;
	uint32_t offset;
	uint8_t mask;
};

struct calc_sample_setting calc_set[] = {
	{2, 0, 1},
	{3, 0, 2},
	{5, 0, 3},
};

struct test_params {
	struct comp_dev *dev;
	struct comp_data *cd;
	struct comp_buffer *sink;
	struct comp_buffer *source;
	struct mux_stream_data *data;
	uint32_t *value;
};

struct test_case {
	int setting;
	enum test_type type;
	struct sof_ipc_comp_mux *comp;
	const char *name;
	struct test_params params;
};

int32_t input[][6] = {
	{ 14, 15, 16, 17, 18, 19 },
	{ 20, 14, 43,  6, 53,  2 },
	{  2,  5, 74, 47,  8,  3 },
};

int32_t expected_results_mux_s16[][3] = {
	{ 56, 60,  64 },
	{ 80, 56, 172 },
	{  8, 20, 296 },
};

int32_t expected_results_demux_s16[][INPUT_DATA_NUMBER] = {
	{ 14, 14,  0,  0, 15, 15 },
	{ 20, 20,  0,  0, 14, 14 },
	{  2,  2,  0,  0,  5,  5 },
};

int32_t expected_results_demux[][INPUT_DATA_NUMBER] = {
	{ 14, 14, 15, 15, 43, 43 },
	{ 20, 20, 14, 14, 43, 43 },
	{  2,  2,  5,  5, 43, 43 },
};

int32_t expected_results_mux[][3] = {
	{ 56, 60,  64 },
	{ 80, 56, 172 },
	{  8, 20, 296 },
};

int32_t input_calc[] = { 2, 54, 43, 6, 3, 62};

/*
 * Calc sample functions should return 0 whilst mask is set as 0
 */

static void test_calc_sample_s16le_mask_0(struct test_case *tc)
{
	uint8_t num_ch;
	uint32_t offset;
	uint8_t mask = 0;

	int32_t ret =  calc_sample_s16le(tc->params.source, num_ch, offset,
					 mask);

	assert_int_equal(ret, 0);
}

static void test_calc_sample_s24le_mask_0(struct test_case *tc)
{
	uint8_t num_ch;
	uint32_t offset;
	uint8_t mask = 0;

	int32_t ret =  calc_sample_s24le(tc->params.source, num_ch, offset,
					 mask);

	assert_int_equal(ret, 0);
}

static void test_calc_sample_s32le_mask_0(struct test_case *tc)
{
	uint8_t num_ch;
	uint32_t offset;
	uint8_t mask = 0;

	int64_t ret =  calc_sample_s32le(tc->params.source, num_ch, offset,
					 mask);

	assert_int_equal(ret, 0);
}

/* setting calc functions with proper values and checking the result */

static void test_calc_sample_s16le_test(struct test_case *tc)
{
	int expected_results[] = { 2, 0, 2 };

	memcpy_s(tc->params.value, sizeof(input_calc), input_calc,
		 sizeof(input_calc));

	int32_t ret =  calc_sample_s16le(tc->params.source,
					 calc_set[tc->setting].num_ch,
					 calc_set[tc->setting].offset,
					 calc_set[tc->setting].mask);

	assert_int_equal(ret, expected_results[tc->setting]);
}

static void test_calc_sample_s24le_test(struct test_case *tc)
{
	int expected_results[] = { 2, 54, 56 };

	memcpy_s(tc->params.value, sizeof(input_calc), input_calc,
		 sizeof(input_calc));

	int32_t ret =  calc_sample_s24le(tc->params.source,
					 calc_set[tc->setting].num_ch,
					 calc_set[tc->setting].offset,
					 calc_set[tc->setting].mask);

	assert_int_equal(ret, expected_results[tc->setting]);
}

static void test_calc_sample_s32le_test(struct test_case *tc)
{
	int expected_results[] = { 2, 54, 56 };

	memcpy_s(tc->params.value, sizeof(input_calc), input_calc,
		 sizeof(input_calc));

	int64_t ret =  calc_sample_s24le(tc->params.source,
					 calc_set[tc->setting].num_ch,
					 calc_set[tc->setting].offset,
					 calc_set[tc->setting].mask);

	assert_int_equal(ret, expected_results[tc->setting]);
}

static void test_demux_s16le_test(struct test_case *tc)
{
	int i;
	uint32_t frames = 3;
	int16_t *sink_write = malloc(tc->params.data->num_channels * frames
			      * sizeof(*sink_write));

	tc->params.data->num_channels = 2;
	tc->params.cd->config.num_channels = 1;

	/* copy input data into demux configuration */
	memcpy_s(tc->params.value, frames * tc->params.data->num_channels,
		 &input[tc->setting][0],
		 frames * tc->params.data->num_channels);

	/* set mask */
	memset(tc->params.data->mask, 1,
	       sizeof(tc->params.data->mask) / sizeof(uint8_t));

	/* sink write memory assign */
	tc->params.sink->w_ptr = sink_write;

	/* test demux */
	demux_s16le(tc->params.dev, tc->params.sink, tc->params.source, frames,
		    tc->params.data);

	/* check output */
	for (i = 0; i < tc->params.data->num_channels * frames; ++i)
		assert_int_equal(*(int16_t *)(tc->params.sink->w_ptr
				 + i * sizeof(int16_t)),
				 expected_results_demux_s16[tc->setting][i]);

	free(sink_write);
}

static void test_demux_s24le_test(struct test_case *tc)
{
	int i;
	uint32_t frames = 3;
	int32_t *sink_write = malloc(tc->params.data->num_channels * frames
			      * sizeof(*sink_write));

	tc->params.data->num_channels = 2;
	tc->params.cd->config.num_channels = 1;

	/* copy input data into demux configuration */
	memcpy_s(tc->params.value, frames * tc->params.data->num_channels,
		 &input[tc->setting][0],
		 frames * tc->params.data->num_channels);

	/* set mask */
	memset(tc->params.data->mask, 1,
	       sizeof(tc->params.data->mask) / sizeof(uint8_t));

	/* sink write memory assign */
	tc->params.sink->w_ptr = sink_write;

	/* test demux */
	demux_s24le(tc->params.dev, tc->params.sink, tc->params.source, frames,
		    tc->params.data);

	/* check output */
	for (i = 0; i < tc->params.data->num_channels * frames; ++i)
		assert_int_equal(*(int32_t *)(tc->params.sink->w_ptr
				 + i * sizeof(int32_t)),
				 expected_results_demux[tc->setting][i]);
	free(sink_write);
}

static void test_demux_s32le_test(struct test_case *tc)
{
	int i;
	uint32_t frames = 3;
	int32_t *sink_write = malloc(tc->params.data->num_channels
			      * frames * sizeof(*sink_write));

	tc->params.data->num_channels = 2;
	tc->params.cd->config.num_channels = 1;

	/* copy input data into demux configuration */
	memcpy_s(tc->params.value, frames * tc->params.data->num_channels,
		 &input[tc->setting][0],
		 frames * tc->params.data->num_channels);

	/* set mask */
	memset(tc->params.data->mask, 1,
	       sizeof(tc->params.data->mask) / sizeof(uint8_t));

	/* sink write memory assign */
	tc->params.sink->w_ptr = sink_write;

	/* test demux */
	demux_s32le(tc->params.dev, tc->params.sink, tc->params.source,
		    frames, tc->params.data);

	/* check output */
	for (i = 0; i < tc->params.data->num_channels * frames; ++i)
		assert_int_equal(*(int32_t *)(tc->params.sink->w_ptr
				 + i * sizeof(int32_t)),
				 expected_results_demux[tc->setting][i]);
	free(sink_write);
}

static void test_mux_s16le_test(struct test_case *tc)
{
	struct comp_buffer **sources = malloc(INPUT_DATA_NUMBER
					      * sizeof(struct comp_buffer));
	int i;
	int j;
	uint32_t frames = 3;

	int32_t *sink_write = malloc(20 * sizeof(*sink_write));

	tc->params.cd->config.num_channels = 1;

	/* allocate memory 4 sources*/
	for (i = 0; i < INPUT_DATA_NUMBER; ++i) {
		sources[i] = malloc(sizeof(struct comp_buffer));
		sources[i]->r_ptr = &input[tc->setting][0];
	}

	/* set number of channels & mask*/
	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		tc->params.data[i].num_channels = 2;
		for (j = 0; j < tc->params.data->num_channels; ++j)
			tc->params.data[i].mask[j] = 1;
	}

	/* sink write memory assign */
	tc->params.sink->w_ptr = sink_write;

	/* test mux */
	mux_s16le(tc->params.dev, tc->params.sink, sources, frames,
		  tc->params.data);

	/* check output */
	for (i = 0; i < frames; ++i)
		assert_int_equal(*(int16_t *)(sink_write + i),
				 expected_results_mux_s16[tc->setting][i]);

	for (i = 0; i < INPUT_DATA_NUMBER; ++i)
		free(sources[i]);

	free(sources);
	free(sink_write);
}

static void test_mux_s24le_test(struct test_case *tc)
{
	struct comp_buffer **sources = malloc(INPUT_DATA_NUMBER
					      * sizeof(struct comp_buffer));
	int i;
	int j;
	uint32_t frames = 3;
	int32_t *sink_write = malloc(20 * sizeof(*sink_write));

	tc->params.cd->config.num_channels = 1;

	/* allocate memory for sources*/
	for (i = 0; i < INPUT_DATA_NUMBER; ++i) {
		sources[i] = malloc(sizeof(struct comp_buffer));
		sources[i]->r_ptr = &input[tc->setting][0];
	}

	/* set number of channels & mask*/
	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		tc->params.data[i].num_channels = 2;
		for (j = 0; j < tc->params.data->num_channels; ++j)
			tc->params.data[i].mask[j] = 1;
	}

	/* sink write memory assign */
	tc->params.sink->w_ptr = sink_write;

	/* test mux */
	mux_s24le(tc->params.dev, tc->params.sink, sources, frames,
		  tc->params.data);

	/* check output */
	for (i = 0; i < frames; ++i)
		assert_int_equal(*(int16_t *)(sink_write + i),
				 expected_results_mux[tc->setting][i]);

	for (i = 0; i < INPUT_DATA_NUMBER; ++i)
		free(sources[i]);

	free(sources);
	free(sink_write);
}

static void test_mux_s32le_test(struct test_case *tc)
{
	struct comp_buffer **sources = malloc(INPUT_DATA_NUMBER
					      * sizeof(struct comp_buffer));
	int i;
	int j;
	uint32_t frames = 3;
	int32_t *sink_write = malloc(20 * sizeof(*sink_write));

	tc->params.cd->config.num_channels = 1;

	/* allocate memory for sources*/
	for (i = 0; i < INPUT_DATA_NUMBER; ++i) {
		sources[i] = malloc(sizeof(struct comp_buffer));
		sources[i]->r_ptr = &input[tc->setting][0];
	}

	/* set number of channels & mask*/
	for (i = 0; i < MUX_MAX_STREAMS; ++i) {
		tc->params.data[i].num_channels = 2;
		for (j = 0; j < tc->params.data->num_channels; ++j)
			tc->params.data[i].mask[j] = 1;
	}

	/* sink write memory assign */
	tc->params.sink->w_ptr = sink_write;

	/* test mux */
	mux_s32le(tc->params.dev, tc->params.sink, sources, frames,
		  tc->params.data);

	/* check output */
	for (i = 0; i < frames; ++i)
		assert_int_equal(*(int16_t *)(sink_write + i),
				 expected_results_mux[tc->setting][i]);

	for (i = 0; i < INPUT_DATA_NUMBER; ++i)
		free(sources[i]);

	free(sources);
	free(sink_write);
}

static void test_mux_get_processing_function_invalid_float
(struct test_case *tc)
{
	/* setting frame format which is absent in nux_func_map */
	tc->params.cd->config.frame_format =  SOF_IPC_FRAME_FLOAT;

	/* frame format should not be recognized and thus return NULL */
	assert_null(mux_get_processing_function(tc->params.dev));
}

static void test_mux_get_processing_function_valid
(struct test_case *tc)
{
	static uint16_t formats[] = {
		SOF_IPC_FRAME_S16_LE,
		SOF_IPC_FRAME_S24_4LE,
		SOF_IPC_FRAME_S32_LE,
	};
	static mux_func functions[] = {
		mux_s16le,
		mux_s24le,
		mux_s32le
	};

	int i;

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		/* setting valid frame format*/
		tc->params.cd->config.frame_format = formats[i];

		/* expect assinging the respective function
		 * corresponding to frame format
		 */
		assert_ptr_equal(mux_get_processing_function(tc->params.dev),
				 functions[i]);
	}
}

static void test_demux_get_processing_function_invalid_float
(struct test_case *tc)
{
	/* setting frame format which is absent in nux_func_map */
	tc->params.cd->config.frame_format = SOF_IPC_FRAME_FLOAT;

	/* frame format should not be recognized and thus return NULL */
	assert_null(demux_get_processing_function(tc->params.dev));
}

static void test_demux_get_processing_function_valid
(struct test_case *tc)
{
	static uint16_t formats[] = {
		SOF_IPC_FRAME_S16_LE,
		SOF_IPC_FRAME_S24_4LE,
		SOF_IPC_FRAME_S32_LE,
	};
	static demux_func functions[] = {
		demux_s16le,
		demux_s24le,
		demux_s32le
	};

	int i;

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		/* setting valid frame format*/
		tc->params.cd->config.frame_format = formats[i];

		/* expect assinging the respective function
		 * corresponding to frame format
		 */
		assert_ptr_equal(demux_get_processing_function(tc->params.dev),
				 functions[i]);
	}
}

static int setup(void **state)
{
	/* prepare test_case structure */
	struct test_case *tc = *((struct test_case **)state);

	tc->params.dev = malloc(sizeof(struct comp_dev));
	tc->params.cd = malloc(sizeof(struct comp_data)
			       + MUX_MAX_STREAMS
			       * sizeof(struct mux_stream_data));
	tc->params.sink = malloc(10 * sizeof(struct comp_buffer));
	tc->params.source = malloc(sizeof(struct comp_buffer));
	tc->params.data = malloc(sizeof(struct mux_stream_data));
	tc->params.value = malloc(INPUT_DATA_NUMBER * sizeof(uint32_t));

	tc->params.dev->private = tc->params.cd;
	tc->params.source->r_ptr = tc->params.value;

	return 0;
}

static int teardown(void **state)
{
	struct test_case *tc = *((struct test_case **)state);

	free((tc->params.source)->r_ptr);
	free(tc->params.data);
	free(tc->params.source);
	free(tc->params.sink);
	free(tc->params.cd);
	free(tc->params.dev);

	return 0;
}

static struct test_case test_cases[] = {
	TEST_CASE(TEST_ANY_VALUE, TEST_CALC_16_MASK0, NULL,
		  "test_calc_sample_s16le_mask0"),
	TEST_CASE(TEST_ANY_VALUE, TEST_CALC_24_MASK0, NULL,
		  "test_calc_sample_s24le_mask0"),
	TEST_CASE(TEST_ANY_VALUE, TEST_CALC_32_MASK0, NULL,
		  "test_calc_sample_s32le_mask0"),

	TEST_CASE(0, TEST_CALC_16, NULL, "test_calc_sample_s16le_test_value"),
	TEST_CASE(0, TEST_CALC_24, NULL, "test_calc_sample_s24le_test_value"),
	TEST_CASE(0, TEST_CALC_32, NULL, "test_calc_sample_s32le_test_value"),

	TEST_CASE(1, TEST_CALC_16, NULL, "test_calc_sample_s16le_test_value"),
	TEST_CASE(1, TEST_CALC_24, NULL, "test_calc_sample_s24le_test_value"),
	TEST_CASE(1, TEST_CALC_32, NULL, "test_calc_sample_s32le_test_value"),

	TEST_CASE(2, TEST_CALC_16, NULL, "test_calc_sample_s16le_test_value"),
	TEST_CASE(2, TEST_CALC_24, NULL, "test_calc_sample_s24le_test_value"),
	TEST_CASE(2, TEST_CALC_32, NULL, "test_calc_sample_s32le_test_value"),

	TEST_CASE(0, TEST_DEMUX_16_TEST, NULL, "test_demux_s16le"),
	TEST_CASE(0, TEST_DEMUX_24_TEST, NULL, "test_demux_s24le"),
	TEST_CASE(0, TEST_DEMUX_32_TEST, NULL, "test_demux_s32le"),

	TEST_CASE(1, TEST_DEMUX_16_TEST, NULL, "test_demux_s16le"),
	TEST_CASE(1, TEST_DEMUX_24_TEST, NULL, "test_demux_s24le"),
	TEST_CASE(1, TEST_DEMUX_32_TEST, NULL, "test_demux_s32le"),

	TEST_CASE(2, TEST_DEMUX_16_TEST, NULL, "test_demux_s16le"),
	TEST_CASE(2, TEST_DEMUX_24_TEST, NULL, "test_demux_s24le"),
	TEST_CASE(2, TEST_DEMUX_32_TEST, NULL, "test_demux_s32le"),

	TEST_CASE(0, TEST_MUX_16_TEST, NULL, "test_mux_s16le"),
	TEST_CASE(0, TEST_MUX_24_TEST, NULL, "test_mux_s24le"),
	TEST_CASE(0, TEST_MUX_32_TEST, NULL, "test_mux_s32le"),

	TEST_CASE(1, TEST_MUX_16_TEST, NULL, "test_mux_s16le"),
	TEST_CASE(1, TEST_MUX_24_TEST, NULL, "test_mux_s24le"),
	TEST_CASE(1, TEST_MUX_32_TEST, NULL, "test_mux_s32le"),

	TEST_CASE(2, TEST_MUX_16_TEST, NULL, "test_mux_s16le"),
	TEST_CASE(2, TEST_MUX_24_TEST, NULL, "test_mux_s24le"),
	TEST_CASE(2, TEST_MUX_32_TEST, NULL, "test_mux_s32le"),

	TEST_CASE(TEST_ANY_VALUE,
		  TEST_MUX_GET_PROCESS_FUNC_DIFFERENT_FRAMES_FORMATS, NULL,
		  "test_mux_get_processing_function_invalid_float"),
	TEST_CASE(TEST_ANY_VALUE,
		  TEST_MUX_GET_PROCESS_FUNC_FRAMES_FORMAT_VALID, NULL,
		  "test_mux_get_processing_function_valid"),

	TEST_CASE(TEST_ANY_VALUE,
		  TEST_DEMUX_GET_PROCESS_FUNC_DIFFERENT_FRAMES_FORMATS, NULL,
		  "test_demux_get_processing_function_invalid_float"
		  ),
	TEST_CASE(TEST_ANY_VALUE,
		  TEST_DEMUX_GET_PROCESS_FUNC_FRAMES_FORMAT_VALID, NULL,
		  "test_demux_get_processing_function_valid"),
};

static void test_audio_mux(void **state)
{
	struct test_case *tc = *((struct test_case **)state);

	switch (tc->type) {
	case TEST_CALC_16_MASK0:
		test_calc_sample_s16le_mask_0(tc);
		break;

	case TEST_CALC_24_MASK0:
		test_calc_sample_s24le_mask_0(tc);
		break;

	case TEST_CALC_32_MASK0:
		test_calc_sample_s32le_mask_0(tc);
		break;

	case TEST_CALC_16:
		test_calc_sample_s16le_test(tc);
		break;

	case TEST_CALC_24:
		test_calc_sample_s24le_test(tc);
		break;

	case TEST_CALC_32:
		test_calc_sample_s32le_test(tc);
		break;

	case TEST_DEMUX_16_TEST:
		test_demux_s16le_test(tc);
		break;

	case TEST_DEMUX_24_TEST:
		test_demux_s24le_test(tc);
		break;

	case TEST_DEMUX_32_TEST:
		test_demux_s32le_test(tc);
		break;

	case TEST_MUX_16_TEST:
		test_mux_s16le_test(tc);
		break;

	case TEST_MUX_24_TEST:
		test_mux_s24le_test(tc);
		break;

	case TEST_MUX_32_TEST:
		test_mux_s32le_test(tc);
		break;

	case TEST_MUX_GET_PROCESS_FUNC_DIFFERENT_FRAMES_FORMATS:
		test_mux_get_processing_function_invalid_float(tc);
		break;

	case TEST_MUX_GET_PROCESS_FUNC_FRAMES_FORMAT_VALID:
		test_mux_get_processing_function_valid(tc);
		break;

	case TEST_DEMUX_GET_PROCESS_FUNC_DIFFERENT_FRAMES_FORMATS:
		test_demux_get_processing_function_invalid_float(tc);
		break;

	case TEST_DEMUX_GET_PROCESS_FUNC_FRAMES_FORMAT_VALID:
		test_demux_get_processing_function_valid(tc);
		break;
	}
}

int main(void)
{
	int i;
	struct CMUnitTest tests[ARRAY_SIZE(test_cases)];

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
