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

/* This value could be any number*/
#define TEST_UNCARING_VALUE 0

#define TEST_CASE(setting, type, comp, name, setup) \
	{ (setting), (type), (comp), (name "_set" #setup), (setup)}

enum test_type {
	TEST_VALID_SIZE,
	TEST_INVALID_SIZE,
	TEST_MEM_CPY_CMP_2_DEV,
	TEST_MEM_CPY_DATA_2_PRIV,
	TEST_STATE_CHECK,
	TEST_DEV_NULL,
};

struct test_case {
	int setting;
	enum test_type type;
	struct sof_ipc_comp_mux *comp;
	const char *name;
	int setup_setting;
};

struct sof_ipc_comp settings[] = {
	{ .hdr = { .size = 14, .cmd = 45 }, .id = 2543, .type = SOF_COMP_MUX,
		.pipeline_id = 3245, .reserved[0] = 24, .reserved[1] = 46 },
	{ .hdr = { .size = 3245, .cmd =  646 }, .id = 5, .type = SOF_COMP_NONE,
		.pipeline_id = 5436, .reserved[0] = 345, .reserved[1] = 76 },
	{ .hdr = { .size = 7654, .cmd =  37 }, .id = 2758,
		.type = SOF_COMP_MUX, .pipeline_id = 457, .reserved[0] = 568,
		.reserved[1] = 344 },
	{ .hdr = { .size = 47, .cmd =  498 }, .id = 361, .type = SOF_COMP_NONE,
		.pipeline_id = 230, .reserved[0] = 650, .reserved[1] = 32 },
};

static int setup(void **state)
{
	/* prepare test_case structure */
	struct test_case *tc = *((struct test_case **)state);
	struct sof_ipc_comp_mux *mux_comp;

	/* simulating rzalloc behavior, it is required to be able to test
	 * null allocation response (it happens for values other than 0)
	 */
	set_mock_function(0);

	tc->comp = test_malloc(sizeof(*tc->comp));

	/* assigning ipc component setting*/
	mux_comp->comp = settings[tc->setup_setting];

	return 0;
}

static int teardown(void **state)
{
	struct test_case *tc = *((struct test_case **)state);

	test_free(tc->comp);
	return 0;
}

/*
 * This function simulates mux_new due to header size configuration.
 * Pointer should have to be a valid address as header size is valid.
 */
static void test_size_check_valid_value(struct test_case *tc)
{
	struct sof_ipc_comp_mux *mux_data = tc->comp;

	/* mux_new should check if header size is set properly */
	mux_data->config.hdr.size = sizeof(mux_data->config);

	struct comp_dev *ret = mux_new((struct sof_ipc_comp *)mux_data);

	assert_non_null(ret);
	mux_free(ret);
}

/*
 * This function simulates mux_new due to header size configuration.
 * Null pointer should be returned as header size is not correct.
 */
static void test_size_check_invalid_value(struct test_case *tc)
{
	struct sof_ipc_comp_mux *mux_data = tc->comp;

	/* mux_new should check if header size is set properly */
	mux_data->config.hdr.size = tc->setting;

	struct comp_dev *ret = mux_new((struct sof_ipc_comp *)mux_data);

	assert_null(ret);
}

/*
 * Function tests if ipc component data is properly copied to mux data
 */
static void test_cpy_cmp_to_dev(struct test_case *tc)
{
	struct sof_ipc_comp_mux *mux_data = tc->comp;
	size_t ipc_process_size = sizeof((struct sof_ipc_comp_process *)
		mux_data);

	/* mux_new should check if header size is set properly */
	mux_data->config.hdr.size = sizeof(mux_data->config);

	struct comp_dev *ret = mux_new((struct sof_ipc_comp *)mux_data);

	assert_memory_equal(mux_data, &ret->comp, ipc_process_size);
	mux_free(ret);
}

/*
 * Function tests if mux data is copied to configuration data structure
 */
static void test_cpy_data_2_priv(struct test_case *tc)
{
	struct sof_ipc_comp_mux *mux_data = tc->comp;
	struct sof_ipc_comp_process *ipc_process =
		(struct sof_ipc_comp_process *)tc;

	/* mux_new should check if header size is set properly */
	mux_data->config.hdr.size = sizeof(mux_data->config);

	struct comp_dev *ret = mux_new((struct sof_ipc_comp *)mux_data);

	/* retrieve configuration data from muxc configuration*/
	struct comp_data *cd = ret->private;

	assert_memory_equal(((struct sof_ipc_comp_process *)mux_data)->data,
			    &cd->config, ipc_process->size);
	mux_free(ret);
}

/*
 * When mux_new executed succuesfully it should change device
 * state into COMP_STATE_READY
 */
static void test_state_check(struct test_case *tc)
{
	struct sof_ipc_comp_mux *mux_data = tc->comp;

	/* mux_new should check if header size is set properly */
	mux_data->config.hdr.size = sizeof(mux_data->config);

	struct comp_dev *ret = mux_new((struct sof_ipc_comp *)mux_data);

	assert_int_equal(ret->state, COMP_STATE_READY);
	mux_free(ret);
}

/*
 * When mux_new does not assign memory for device
 * then it should return null pointer
 */
static void test_dev_null(struct test_case *tc)
{
	struct sof_ipc_comp_mux *mux_data = tc->comp;

	/* mux_new should check if header size is set properly */
	mux_data->config.hdr.size = sizeof(mux_data->config);

	/* Imposing situation in which not enough memory is available for dev*/
	set_mock_function(tc->setting);

	struct comp_dev *ret = mux_new((struct sof_ipc_comp *)mux_data);

	assert_null(ret);
}

static void test_audio_mux(void **state)
{
	struct test_case *tc = *((struct test_case **)state);

	switch (tc->type) {
	case TEST_VALID_SIZE:
		test_size_check_valid_value(tc);
		break;

	case TEST_INVALID_SIZE:
		test_size_check_invalid_value(tc);
		break;

	case TEST_MEM_CPY_CMP_2_DEV:
		test_cpy_cmp_to_dev(tc);
		break;

	case TEST_MEM_CPY_DATA_2_PRIV:
		test_cpy_data_2_priv(tc);
		break;

	case TEST_STATE_CHECK:
		test_state_check(tc);
		break;

	case TEST_DEV_NULL:
		test_dev_null(tc);
		break;
	}
}

static struct test_case test_cases[] = {
	TEST_CASE(TEST_UNCARING_VALUE, TEST_VALID_SIZE, NULL,
		  "test_mux_new_size_check_valid_value", 0),
	TEST_CASE(TEST_UNCARING_VALUE, TEST_VALID_SIZE, NULL,
		  "test_mux_new_size_check_valid_value", 1),
	TEST_CASE(TEST_UNCARING_VALUE, TEST_VALID_SIZE, NULL,
		  "test_mux_new_size_check_valid_value", 2),
	TEST_CASE(TEST_UNCARING_VALUE, TEST_VALID_SIZE, NULL,
		  "test_mux_new_size_check_valid_value", 3),

	TEST_CASE(0, TEST_INVALID_SIZE, NULL,
		  "test_mux_new_size_check_invalid_value_0", 0),
	TEST_CASE(0, TEST_INVALID_SIZE, NULL,
		  "test_mux_new_size_check_invalid_value_0", 1),
	TEST_CASE(0, TEST_INVALID_SIZE, NULL,
		  "test_mux_new_size_check_invalid_value_0", 2),
	TEST_CASE(0, TEST_INVALID_SIZE, NULL,
		  "test_mux_new_size_check_invalid_value_0", 3),

	TEST_CASE(1, TEST_INVALID_SIZE, NULL,
		  "test_mux_new_size_check_invalid_value_1", 0),
	TEST_CASE(1, TEST_INVALID_SIZE, NULL,
		  "test_mux_new_size_check_invalid_value_1", 1),
	TEST_CASE(1, TEST_INVALID_SIZE, NULL,
		  "test_mux_new_size_check_invalid_value_1", 2),
	TEST_CASE(1, TEST_INVALID_SIZE, NULL,
		  "test_mux_new_size_check_invalid_value_1", 3),

	TEST_CASE(100, TEST_INVALID_SIZE, NULL,
		  "test_mux_new_size_check_invalid_value_100", 0),
	TEST_CASE(100, TEST_INVALID_SIZE, NULL,
		  "test_mux_new_size_check_invalid_value_100", 1),
	TEST_CASE(100, TEST_INVALID_SIZE, NULL,
		  "test_mux_new_size_check_invalid_value_100", 2),
	TEST_CASE(100, TEST_INVALID_SIZE, NULL,
		  "test_mux_new_size_check_invalid_value_100", 3),

	TEST_CASE(TEST_UNCARING_VALUE, TEST_MEM_CPY_CMP_2_DEV, NULL,
		  "test_mux_new_copy_comp_2_dev", 0),
	TEST_CASE(TEST_UNCARING_VALUE, TEST_MEM_CPY_CMP_2_DEV, NULL,
		  "test_mux_new_copy_comp_2_dev", 1),
	TEST_CASE(TEST_UNCARING_VALUE, TEST_MEM_CPY_CMP_2_DEV, NULL,
		  "test_mux_new_copy_comp_2_dev", 2),
	TEST_CASE(TEST_UNCARING_VALUE, TEST_MEM_CPY_CMP_2_DEV, NULL,
		  "test_mux_new_copy_comp_2_dev", 3),

	TEST_CASE(TEST_UNCARING_VALUE, TEST_MEM_CPY_DATA_2_PRIV, NULL,
		  "test_mux_new_copy_data_2_priv", 0),
	TEST_CASE(TEST_UNCARING_VALUE, TEST_MEM_CPY_DATA_2_PRIV, NULL,
		  "test_mux_new_copy_data_2_priv", 1),
	TEST_CASE(TEST_UNCARING_VALUE, TEST_MEM_CPY_DATA_2_PRIV, NULL,
		  "test_mux_new_copy_data_2_priv", 2),
	TEST_CASE(TEST_UNCARING_VALUE, TEST_MEM_CPY_DATA_2_PRIV, NULL,
		  "test_mux_new_copy_data_2_priv", 3),

	TEST_CASE(TEST_UNCARING_VALUE, TEST_STATE_CHECK, NULL,
		  "test_mux_new_state_check", 0),
	TEST_CASE(TEST_UNCARING_VALUE, TEST_STATE_CHECK, NULL,
		  "test_mux_new_state_check", 1),
	TEST_CASE(TEST_UNCARING_VALUE, TEST_STATE_CHECK, NULL,
		  "test_mux_new_state_check", 2),
	TEST_CASE(TEST_UNCARING_VALUE, TEST_STATE_CHECK, NULL,
		  "test_mux_new_state_check", 3),

	TEST_CASE(1, TEST_DEV_NULL, NULL,
		  "test_mux_new_dev_null_alloc", 0),
	TEST_CASE(1, TEST_DEV_NULL, NULL,
		  "test_mux_new_dev_null_alloc", 1),
	TEST_CASE(1, TEST_DEV_NULL, NULL,
		  "test_mux_new_dev_null_alloc", 2),
	TEST_CASE(1, TEST_DEV_NULL, NULL,
		  "test_mux_new_dev_null_alloc", 3),

	TEST_CASE(2, TEST_DEV_NULL, NULL,
		  "test_mux_new_cd_null_alloc", 0),
	TEST_CASE(2, TEST_DEV_NULL, NULL,
		  "test_mux_new_cd_null_alloc", 1),
	TEST_CASE(2, TEST_DEV_NULL, NULL,
		  "test_mux_new_cd_null_alloc", 2),
	TEST_CASE(2, TEST_DEV_NULL, NULL,
		  "test_mux_new_cd_null_alloc", 3),
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
