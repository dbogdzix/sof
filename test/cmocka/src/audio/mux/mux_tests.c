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

static uint16_t valid_formats[] = {
	SOF_IPC_FRAME_S16_LE,
	SOF_IPC_FRAME_S24_4LE,
	SOF_IPC_FRAME_S32_LE,
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

static int setup(void **state)
{
	sys_comp_mux_init();

	if (!comp_mux || !comp_demux)
		return -ENOENT;

	return 0;
}

static void test_mux_reset(void **state)
{
	struct comp_dev *dev;

	assert_int_equal(comp_mux->ops.reset(dev), 0);
}

static void test_mux_get_stream_index_pipe_id_no_match(void **state)
{
	int i;
	struct comp_data *cd = malloc(sizeof(*cd)
				      + MUX_MAX_STREAMS
				      * sizeof(struct mux_stream_data));
	/* set pipe_id which index to get */
	uint8_t pipe_id = 1;

	/* setting streams values, other than the pipe_id */
	for (i = 0; i < MUX_MAX_STREAMS; ++i)
		cd->config.streams[i].pipeline_id = 2 * i;

	uint8_t ret = get_stream_index(cd, pipe_id);

	/* test if function id was found */
	assert_int_equal(ret, 0);

	free(cd);
}

static void test_mux_get_stream_index_valid_values(void **state)
{
	int i;
	struct comp_data *cd = malloc(sizeof(*cd)
				      + MUX_MAX_STREAMS
				      * sizeof(struct mux_stream_data));
	uint8_t searched_stream = 3;
	/* setting pipeline id - the searched one */
	uint8_t pipe_id = 15;

	/* setting streams values, other than the pipe_id */
	for (i = 0; i < MUX_MAX_STREAMS; ++i)
		cd->config.streams[i].pipeline_id = 2 * i;

	/* setting one stream pipeline as the searched one */
	cd->config.streams[searched_stream].pipeline_id = 15;

	uint8_t ret = get_stream_index(cd, pipe_id);

	/* check if searched stream is the desired one */
	assert_int_equal(ret, searched_stream);

	free(cd);
}

static void test_mux_trigger(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));

	/* TRIGGER RESET should enforce returning 0 */
	int cmd = COMP_TRIGGER_RESET;

	int ret = comp_mux->ops.trigger(dev, cmd);

	assert_int_equal(ret, 0);

	free(dev);
}

static void test_demux_prepare_comp_state_non_zero(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));

	/* TRGGER PAUSE should enforce returning invalid argument */
	dev->state = COMP_TRIGGER_PAUSE;

	int ret = comp_demux->ops.prepare(dev);

	assert_int_equal(ret, -EINVAL);

	free(dev);
}

static void test_demux_prepare_demux_proc_not_found(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct comp_data *cd = malloc(sizeof(*cd));

	dev->private = cd;
	dev->state = COMP_STATE_READY;

	/* set demux processing function to NULL */
	cd->demux = NULL;

	int ret = comp_demux->ops.prepare(dev);

	/* test if device is ready */
	assert_int_equal(dev->state, COMP_STATE_READY);

	/* test if error is returned */
	assert_int_equal(ret, -EINVAL);

	free(cd);
	free(dev);
}

static void test_demux_prepare_valid(void **state)
{
	int i;
	int ret;
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct comp_data *cd = malloc(sizeof(*cd)
				      + MUX_MAX_STREAMS
				      * sizeof(struct mux_stream_data));

	dev->private = cd;

	for (i = 0; i < ARRAY_SIZE(valid_formats); ++i) {
		dev->state = COMP_STATE_READY;

		/* set frame format */
		cd->config.frame_format = valid_formats[i];

		/* if adequate frame format is selected then 0 is returned */
		ret = comp_demux->ops.prepare(dev);
		assert_int_equal(ret, 0);
	}

	free(cd);
	free(dev);
}

static void test_mux_prepare_comp_state_non_zero(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));

	/* STATE PAUSED should enforce returning invalid argument */
	dev->state = COMP_STATE_PAUSED;

	int ret = comp_mux->ops.prepare(dev);

	assert_int_equal(ret, -EINVAL);

	free(dev);
}

static void test_mux_prepare_comp_proc_not_found(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct comp_data *cd = malloc(sizeof(*cd)
				      + MUX_MAX_STREAMS
				      * sizeof(struct mux_stream_data));
	dev->private = cd;
	dev->state = COMP_STATE_READY;

	/* set demux processing function to NULL */
	cd->demux = NULL;

	/* set freame format value that not occurred in mux_func_map */
	cd->config.frame_format = 999;

	int ret = comp_mux->ops.prepare(dev);

	/* checj output as the frame format is not recognized */
	assert_int_equal(ret, -EINVAL);

	free(cd);
	free(dev);
}

static void test_mux_prepare_valid(void **state)
{
	int i;
	int ret;
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct comp_data *cd = malloc(sizeof(*cd)
				      + MUX_MAX_STREAMS
				      * sizeof(struct mux_stream_data));

	dev->private = cd;

	for (i = 0; i < ARRAY_SIZE(valid_formats); ++i) {
		dev->state = COMP_STATE_READY;

		/* set frame format */
		cd->config.frame_format = valid_formats[i];

		/* if adequate frame format is selected then 0 is returned */
		ret = comp_mux->ops.prepare(dev);
		assert_int_equal(ret, 0);
	}

	free(cd);
	free(dev);
}

static void test_mux_cmd_not_set_data_cmd(void **state)
{
	struct comp_dev *dev;
	void *data;
	int max_data_size;

	/* set command other that SET DATA */
	int cmd = COMP_CMD_SET_VALUE;

	int ret = comp_mux->ops.cmd(dev, cmd, data, max_data_size);

	assert_int_equal(ret, -EINVAL);
}

/*
 * Function sets compent data parameters based on decice configuration
 */
static void test_mux_params(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct comp_data *cd = dev->private;

	dev->params.channels = 2;
	dev->params.frame_fmt = SOF_IPC_FRAME_S16_LE;
	int ret = comp_mux->ops.params(dev);

	/* checking if component data is set as desired */
	assert_int_equal(cd->config.num_channels, 2);
	assert_int_equal(cd->config.frame_format, SOF_IPC_FRAME_S16_LE);

	assert_int_equal(ret, 0);

	free(cd);
	free(dev);
}

/*
 * Function expects command SOF_CTRL_CMD_BINARY, this test checks
 * what happens otherwise. It should return error.
 */
static void test_mux_ctrl_set_cmd_invalid(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct sof_ipc_ctrl_data *cdata = malloc(sizeof(*cdata));

	/* function expects command SOF_CTRL_CMD_BINARY */
	cdata->cmd =  SOF_CTRL_CMD_VOLUME;

	int ret = mux_ctrl_set_cmd(dev, cdata);

	assert_int_equal(ret, -EINVAL);

	free(cdata);
	free(dev);
}

static void test_mux_ctrl_set_cmd_valid(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct sof_ipc_ctrl_data *cd = malloc(sizeof(*cd));
	struct sof_mux_config *config = malloc(sizeof(*config)
		+ MUX_MAX_STREAMS * sizeof(struct mux_stream_data));
	struct sof_mux_config *cfg = (struct sof_mux_config *)cd->data->data;

	cfg->num_streams = MUX_MAX_STREAMS;
	cfg->streams[0] = (struct mux_stream_data)
		{ 1, 2, {1, 2, 3, 4}, {0, 0, 0} };
	cfg->streams[1] = (struct mux_stream_data)
		{ 3, 2, {1, 2, 3, 4}, {0, 0, 0} };
	cfg->streams[2] = (struct mux_stream_data)
		{ 5, 2, {1, 2, 3, 4}, {0, 0, 0} };
	cfg->streams[3] = (struct mux_stream_data)
		{ 7, 2, {1, 2, 3, 4}, {0, 0, 0} };

	/* set command */
	cd->cmd =  SOF_CTRL_CMD_BINARY;

	int ret = mux_ctrl_set_cmd(dev, cd);

	assert_int_equal(ret, 0);

	free(config);
	free(cd);
	free(dev);
}

static void test_mux_cmd_valid(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct sof_ipc_ctrl_data *cd = malloc(sizeof(*cd));
	struct sof_mux_config *config = malloc(sizeof(*config)
		+ 8 * sizeof(struct mux_stream_data));
	struct sof_mux_config *cfg = (struct sof_mux_config *)cd->data->data;

	cfg->num_streams = MUX_MAX_STREAMS;
	cfg->streams[0] = (struct mux_stream_data)
		{ 1, 2, {1, 2, 3, 4}, {0, 0, 0} };
	cfg->streams[1] = (struct mux_stream_data)
		{ 3, 2, {1, 2, 3, 4}, {0, 0, 0} };
	cfg->streams[2] = (struct mux_stream_data)
		{ 5, 2, {1, 2, 3, 4}, {0, 0, 0} };
	cfg->streams[3] = (struct mux_stream_data)
		{ 7, 2, {1, 2, 3, 4}, {0, 0, 0} };

	/* set command */
	cd->cmd =  SOF_CTRL_CMD_BINARY;
	int max_data_size;

	/* set command proper command */
	int cmd = COMP_CMD_SET_DATA;

	int ret = comp_mux->ops.cmd(dev, cmd, cd, max_data_size);

	assert_int_equal(ret, 0);

	free(config);
	free(cd);
	free(dev);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_mux_reset),
		cmocka_unit_test(test_mux_get_stream_index_pipe_id_no_match),
		cmocka_unit_test(test_mux_get_stream_index_valid_values),
		cmocka_unit_test(test_mux_trigger),
		cmocka_unit_test(test_demux_prepare_comp_state_non_zero),
		cmocka_unit_test(test_demux_prepare_demux_proc_not_found),
		cmocka_unit_test(test_demux_prepare_valid),
		cmocka_unit_test(test_mux_prepare_comp_state_non_zero),
		cmocka_unit_test(test_mux_prepare_comp_proc_not_found),
		cmocka_unit_test(test_mux_prepare_valid),
		cmocka_unit_test(test_mux_cmd_not_set_data_cmd),
		cmocka_unit_test(test_mux_cmd_valid),
		cmocka_unit_test(test_mux_params),
		cmocka_unit_test(test_mux_ctrl_set_cmd_invalid),
		cmocka_unit_test(test_mux_ctrl_set_cmd_valid),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, NULL);
}
