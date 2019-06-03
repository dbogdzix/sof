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

/*
 * Function should return 0
 */
static void test_mux_reset(void **state)
{
	struct comp_dev *dev;

	assert_int_equal(mux_reset(dev), 0);
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

	int ret = mux_trigger(dev, cmd);

	assert_int_equal(ret, 0);
	free(dev);
}

static void test_demux_prepare_comp_state_non_zero(void **state)
{
	struct comp_dev *dev = malloc(sizeof(*dev));

	/* TRGGER PAUSE should enforce returning invalid argument */
	dev->state = COMP_TRIGGER_PAUSE;

	int ret = demux_prepare(dev);

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

	int ret = demux_prepare(dev);

	/* test if device is ready */
	assert_int_equal(dev->state, COMP_STATE_READY);

	/* test if error is returned */
	assert_int_equal(ret, -EINVAL);
	free(cd);
	free(dev);
}

static void test_demux_prepare_positiv(void **state)
{
	int i;
	int ret;
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct comp_data *cd = malloc(sizeof(*cd)
				      + MUX_MAX_STREAMS
				      * sizeof(struct mux_stream_data));

	dev->private = cd;

	for (i = 0; i < get_mux_func_map_size(); ++i) {
		dev->state = COMP_STATE_READY;

		/* set frame format */
		cd->config.frame_format = mux_func_map[i].frame_format;

		/* if adequate frame format is selected then 0 is returned */
		ret = demux_prepare(dev);
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

	int ret = mux_prepare(dev);

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

	int ret = mux_prepare(dev);

	/* checj output as the frame format is not recognized */
	assert_int_equal(ret, -EINVAL);
	free(cd);
	free(dev);
}

static void test_mux_prepare_positiv(void **state)
{
	int i;
	int ret;
	struct comp_dev *dev = malloc(sizeof(*dev));
	struct comp_data *cd = malloc(sizeof(*cd)
				      + MUX_MAX_STREAMS
				      * sizeof(struct mux_stream_data));

	dev->private = cd;

	for (i = 0; i < get_mux_func_map_size(); ++i) {
		dev->state = COMP_STATE_READY;

		/* set frame format */
		cd->config.frame_format = mux_func_map[i].frame_format;

		/* if adequate frame format is selected then 0 is returned */
		ret = mux_prepare(dev);
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

	int ret = mux_cmd(dev, cmd, data, max_data_size);

	/* expect retruning error */
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
	int ret = mux_params(dev);

	/* checking if component data is set as desired */
	assert_int_equal(cd->config.num_channels, 2);
	assert_int_equal(cd->config.frame_format, SOF_IPC_FRAME_S16_LE);

	/* return 0 if success */
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

	/* should return 0 if success*/
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

	int ret = mux_cmd(dev, cmd, cd, max_data_size);

	/* expect retruning 0 */
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
		cmocka_unit_test(test_demux_prepare_positiv),
		cmocka_unit_test(test_mux_prepare_comp_state_non_zero),
		cmocka_unit_test(test_mux_prepare_comp_proc_not_found),
		cmocka_unit_test(test_mux_prepare_positiv),
		cmocka_unit_test(test_mux_cmd_not_set_data_cmd),
		cmocka_unit_test(test_mux_cmd_valid),
		cmocka_unit_test(test_mux_params),
		cmocka_unit_test(test_mux_ctrl_set_cmd_invalid),
		cmocka_unit_test(test_mux_ctrl_set_cmd_valid),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
