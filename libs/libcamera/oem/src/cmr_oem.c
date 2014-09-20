/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define LOG_TAG "cmr_oem"

#include <stdlib.h>
#include <math.h>
#include <cutils/properties.h>
#include "cmr_oem.h"

/**********************************************************************************************/
#define PREVIEW_MSG_QUEUE_SIZE                       50
#define SNAPSHOT_MSG_QUEUE_SIZE                      50
#define CMR_EVT_INIT                                 (CMR_EVT_OEM_BASE)
#define CMR_EVT_START                                (CMR_EVT_OEM_BASE + 1)
#define CMR_EVT_STOP                                 (CMR_EVT_OEM_BASE + 2)
#define CMR_EVT_EXIT                                 (CMR_EVT_OEM_BASE + 3)
#define CMR_EVT_BEFORE_SET                           (CMR_EVT_OEM_BASE + 4)
#define CMR_EVT_AFTER_SET                            (CMR_EVT_OEM_BASE + 5)
#define CMR_EVT_BEFORE_CAPTURE                       (CMR_EVT_OEM_BASE + 6)
#define CMR_EVT_AFTER_CAPTURE                        (CMR_EVT_OEM_BASE + 7)
#define CMR_EVT_CONVERT_THUM                         (CMR_EVT_OEM_BASE + 8)
#define CMR_EVT_CAF_MOVE_START                       (CMR_EVT_OEM_BASE + 13)
#define CMR_EVT_CAF_MOVE_STOP                        (CMR_EVT_OEM_BASE + 14)

#define CMR_EVT_CB_BASE                              (CMR_EVT_OEM_BASE + 0x800)
#define CMR_EVT_CB_INIT                              (CMR_EVT_CB_BASE + 0)
#define CMR_EVT_CB_EXIT                              (CMR_EVT_CB_BASE + 1)
#define CMR_EVT_CB_HANDLE                            (CMR_EVT_CB_BASE + 2)

#define CAMERA_OEM_MSG_QUEUE_SIZE                    50
#define CAMERA_CB_MSG_QUEUE_SIZE                     40
#define CAMERA_RECOVER_CNT                           3

#define OEM_HANDLE_HDR                               1
#define CAMERA_PATH_SHARE                            1
/**********************************************************************************************/


/************************************internal interface ***************************************/
static void camera_send_channel_data(cmr_handle oem_handle, cmr_handle receiver_handle, cmr_uint evt, void *data);
static cmr_int camera_sensor_streamctrl(cmr_u32 on_off, void *privdata);
static cmr_int camera_isp_ctrl_done(cmr_u32 cmd, void* data);
static void camera_sensor_evt_cb(cmr_int evt, void* data, void* privdata);
static cmr_int camera_is_need_change_fmt(cmr_handle oem_handle, struct frm_info *data_ptr);
static void camera_v4l2_evt_cb(cmr_int evt, void* data, void* privdata);
static void camera_scaler_evt_cb(cmr_int evt, void* data, void *privdata);
static void camera_jpeg_evt_cb(cmr_int evt, void* data, void *privdata);
static cmr_int camera_isp_evt_cb(cmr_handle oem_handle, cmr_u32 evt, void* data, cmr_u32 data_len);
static void camera_focus_evt_cb(enum af_cb_type cb, cmr_s32 param, void *privdata);
static cmr_int camera_preview_cb(cmr_handle oem_handle, enum preview_cb_type cb_type, enum preview_func_type func, void *param);
static cmr_int camera_ipm_cb(cmr_u32 class_type, struct ipm_frame_out *cb_param);
static void camera_snapshot_cb_to_hal(cmr_handle oem_handle, enum snapshot_cb_type cb, enum snapshot_func_type func, void* param);
static void camera_snapshot_state_handle(cmr_handle oem_handle, enum snapshot_cb_type cb, enum snapshot_func_type func, void* param);
static void camera_snapshot_cb(cmr_handle oem_handle, enum snapshot_cb_type cb, enum snapshot_func_type func, void* param);
static cmr_int camera_before_set(cmr_handle oem_handle, enum preview_param_mode mode);
static cmr_int camera_after_set(cmr_handle oem_handle, struct after_set_cb_param *param);
static cmr_int camera_focus_pre_proc(cmr_handle oem_handle);
static cmr_int camera_focus_post_proc(cmr_handle oem_handle);
static cmr_int camera_get_preview_status(cmr_handle oem_handle);
static cmr_int camera_sensor_init(cmr_handle  oem_handle, cmr_uint is_autotest);
static cmr_int camera_sensor_deinit(cmr_handle  oem_handle);
static cmr_int camera_v4l2_init(cmr_handle  oem_handle);
static cmr_int camera_v4l2_deinit(cmr_handle  oem_handle);
static cmr_int camera_jpeg_init(cmr_handle  oem_handle);
static cmr_int camera_jpeg_deinit(cmr_handle  oem_handle);
static cmr_int camera_scaler_init(cmr_handle  oem_handle);
static cmr_int camera_scaler_deinit(cmr_handle  oem_handle);
static cmr_int camera_rotation_init(cmr_handle  oem_handle);
static cmr_int camera_rotation_deinit(cmr_handle  oem_handle);
static cmr_int camera_isp_init(cmr_handle  oem_handle);
static cmr_int camera_isp_deinit(cmr_handle  oem_handle);
static cmr_int camera_preview_init(cmr_handle  oem_handle);
static cmr_int camera_preview_deinit(cmr_handle  oem_handle);
static cmr_int camera_snapshot_init(cmr_handle  oem_handle);
static cmr_int camera_snapshot_deinit(cmr_handle  oem_handle);
static cmr_int camera_ipm_init(cmr_handle  oem_handle);
static cmr_int camera_ipm_deinit(cmr_handle  oem_handle);
static cmr_int camera_setting_init(cmr_handle  oem_handle);
static cmr_int camera_setting_deinit(cmr_handle  oem_handle);
static cmr_int camera_focus_init(cmr_handle  oem_handle);
static cmr_int camera_focus_deinit(cmr_handle  oem_handle);
static cmr_int camera_preview_cb_thread_proc(struct cmr_msg *message, void* data);
static cmr_int camera_snapshot_cb_thread_proc(struct cmr_msg *message, void* data);
static cmr_int camera_snapshot_secondary_thread_proc(struct cmr_msg *message, void* data);
static cmr_int camera_create_prev_thread(cmr_handle oem_handle);
static cmr_int camera_destroy_prev_thread(cmr_handle oem_handle);
static cmr_int camera_create_snp_thread(cmr_handle oem_handle);
static cmr_int camera_destroy_snp_thread(cmr_handle oem_handle);
static cmr_int camera_init_thread(cmr_handle oem_handle);
static cmr_int camera_deinit_thread(cmr_handle oem_handle);
static cmr_int camera_init_resource(cmr_handle);
static cmr_int camera_destroy_resource(cmr_handle);
static cmr_int camera_init_internal(cmr_handle oem_handle, cmr_uint is_autotest);
static cmr_int camera_deinit_internal(cmr_handle  oem_handle);
static cmr_int camera_preview_pre_proc(cmr_handle oem_handle, cmr_u32 camera_id, cmr_u32 preview_sn_mode);
static cmr_int camera_preview_post_proc(cmr_handle oem_handle, cmr_u32 camera_id);
static cmr_int camera_start_encode(cmr_handle oem_handle, cmr_handle caller_handle,
												struct img_frm *src, struct img_frm *dst, struct cmr_op_mean *mean);
static cmr_int camera_start_decode(cmr_handle oem_handle, cmr_handle caller_handle, struct img_frm *src,
                                                struct img_frm *dst, struct cmr_op_mean *mean);
static cmr_int camera_start_exif_encode(cmr_handle oem_handle, cmr_handle caller_handle,
                                                       struct img_frm *pic_src, struct img_frm *thumb_src,
                                                       void *exif, struct img_frm *dst, struct jpeg_wexif_cb_param *out_ptr);
static cmr_int camera_stop_codec(cmr_handle oem_handle);
static cmr_int camera_start_scale(cmr_handle oem_handle, cmr_handle caller_handle, struct img_frm *src,
                                             struct img_frm *dst, struct cmr_op_mean *mean);
static cmr_int camera_start_rot(cmr_handle oem_handle, cmr_handle caller_handle, struct img_frm *src,
                                         struct img_frm *dst, struct cmr_op_mean *mean);
static cmr_int camera_ipm_pre_proc(cmr_handle oem_handle, void * private_data);
static cmr_int camera_capture_pre_proc(cmr_handle oem_handle, cmr_u32 camera_id, cmr_u32 preview_mode, cmr_u32 capture_mode, cmr_u32 is_restart);
static cmr_int camera_capture_post_proc(cmr_handle oem_handle, cmr_u32 camera_id);
static cmr_int camera_open_sensor(cmr_handle oem_handle, cmr_u32 camera_id);
static cmr_int camera_close_sensor(cmr_handle oem_handle, cmr_u32 camera_id);
static cmr_int camera_raw_proc(cmr_handle oem_handle, cmr_handle caller_handle, struct raw_proc_param *param_ptr);
static cmr_int camera_isp_start_video(cmr_handle oem_handle, struct video_start_param *param_ptr);
static cmr_int camera_isp_stop_video(cmr_handle oem_handle);
static cmr_int camera_channel_cfg(cmr_handle oem_handle, cmr_handle caller_handle, cmr_u32 camera_id,
											  struct channel_start_param *param_ptr, cmr_u32 *channel_id);
static cmr_int camera_channel_start(cmr_handle oem_handle, cmr_u32 channel_bits, cmr_uint skip_number);
static cmr_int camera_channel_pause(cmr_handle oem_handle, cmr_uint channel_id, cmr_u32 reconfig_flag);
static cmr_int camera_channel_resume(cmr_handle oem_handle, cmr_uint channel_id, cmr_u32 skip_number,
	                                                cmr_u32 deci_factor, cmr_u32 frm_num);
static cmr_int camera_channel_free_frame(cmr_handle oem_handle, cmr_u32 channel_id, cmr_u32 index);
static cmr_int camera_channel_stop(cmr_handle oem_handle, cmr_u32 channel_bits);
static cmr_int camera_channel_scale_capability(cmr_handle oem_handle, cmr_u32 *width, cmr_u32 *sc_factor, cmr_u32 *sc_threshold);
static cmr_int camera_channel_get_cap_time(cmr_handle oem_handle, cmr_u32 *sec, cmr_u32 *usec);
static cmr_int camera_set_hal_cb(cmr_handle oem_handle, camera_cb_of_type hal_cb);
static cmr_int camera_ioctl_for_setting(cmr_handle oem_handle, cmr_uint cmd_type, struct setting_io_parameter *param_ptr);
static cmr_int camera_sensor_ioctl(cmr_handle	oem_handle, cmr_uint cmd_type, struct common_sn_cmd_parameter *param_ptr);
static cmr_int camera_isp_ioctl(cmr_handle oem_handle, cmr_uint cmd_type, struct common_isp_cmd_parameter *param_ptr);
static cmr_int camera_get_setting_activity(cmr_handle oem_handle, cmr_uint *is_active);
static cmr_int camera_set_preview_param(cmr_handle oem_handle, enum takepicture_mode mode,  cmr_uint is_snapshot);
static cmr_int camera_get_preview_param(cmr_handle oem_handle, enum takepicture_mode mode,
	                                                      cmr_uint is_snapshot, struct preview_param *out_param_ptr);
static cmr_int camera_get_snapshot_param(cmr_handle oem_handle, struct snapshot_param *out_ptr);
static cmr_int camera_get_sensor_info(cmr_handle oem_handle, cmr_uint sensor_id, struct sensor_exp_info *exp_info_ptr);
static cmr_int camera_get_sensor_autotest_mode(cmr_handle oem_handle, cmr_uint sensor_id, cmr_uint *is_autotest);
static cmr_int camera_set_setting(cmr_handle oem_handle, enum camera_param_type id, cmr_uint param);
static void camera_set_hdr_flag(struct camera_context *cxt, cmr_u32 hdr_flag);
static cmr_u32 camera_get_hdr_flag(struct camera_context *cxt);
static cmr_int camera_open_hdr(struct camera_context *cxt, struct ipm_open_in *in_ptr, struct ipm_open_out *out_ptr);
static cmr_int camera_close_hdr(struct camera_context *cxt);
static void camera_snapshot_channel_handle(struct camera_context *cxt, void* param);
static void camera_post_share_path_available(cmr_handle oem_handle);
static void camera_wait_share_path_available(cmr_handle oem_handle);
static void camera_set_discard_frame(cmr_handle oem_handle, cmr_uint is_discard);
static cmr_uint camera_get_is_discard_frame(cmr_handle oem_handle, struct frm_info *data);
static void camera_set_snp_req(cmr_handle oem_handle, cmr_uint is_req);
static cmr_uint camera_get_snp_req(cmr_handle oem_handle);
static cmr_int camera_get_cap_time(cmr_handle snp_handle);
static cmr_int camera_check_cap_time(cmr_handle snp_handle, struct frm_info * data);
static void camera_snapshot_started(cmr_handle oem_handle);
/**********************************************************************************************/

void camera_malloc(cmr_u32 mem_type, cmr_handle oem_handle, cmr_u32 *size_ptr,
	                        cmr_u32 *sum_ptr, cmr_uint *phy_addr, cmr_uint *vir_addr)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle || !phy_addr || !vir_addr || !size_ptr || !sum_ptr) {
		CMR_LOGE("error param 0x%lx 0x%lx 0x%lx", (cmr_uint)oem_handle, (cmr_uint)phy_addr, (cmr_uint)vir_addr);
		goto exit;
	}
	CMR_PRINT_TIME;
	CMR_LOGI("mem type %d size %d sum %d", mem_type, *size_ptr, *sum_ptr);
	if (cxt->hal_malloc) {
		cxt->hal_malloc(mem_type, size_ptr, sum_ptr, phy_addr, vir_addr, cxt->client_data);
		CMR_LOGI("malloc 0x%lx 0x%lx", (cmr_uint)phy_addr, (cmr_uint)vir_addr);
	} else {
		CMR_LOGE("malloc is null");
	}
exit:
	CMR_PRINT_TIME;
}

void camera_free(cmr_u32 mem_type, cmr_handle oem_handle,cmr_uint *phy_addr,
						cmr_uint *vir_addr, cmr_u32 sum)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle || !phy_addr || !vir_addr) {
		CMR_LOGE("error param 0x%lx 0x%lx 0x%lx", (cmr_uint)oem_handle, (cmr_uint)phy_addr, (cmr_uint)vir_addr);
		goto exit;
	}
	CMR_LOGI("mem type %d sum %d", mem_type, sum);
	CMR_LOGI("free 0x%lx 0x%lx", (cmr_uint)phy_addr, (cmr_uint)vir_addr);
	CMR_PRINT_TIME;
	cxt->hal_free(mem_type, phy_addr, vir_addr, sum, cxt->client_data);
exit:
	CMR_PRINT_TIME;
}

void camera_snapshot_started(cmr_handle oem_handle)
{
	CMR_PRINT_TIME;
	camera_snapshot_cb_to_hal(oem_handle, CAMERA_EXIT_CB_PREPARE, CAMERA_FUNC_TAKE_PICTURE, 0);
}

void camera_set_discard_frame(cmr_handle oem_handle, cmr_uint is_discard)
{
	struct camera_context           *cxt = (struct camera_context*)oem_handle;

	sem_wait(&cxt->access_sm);
	cxt->is_discard_frm = is_discard;
	sem_post(&cxt->access_sm);
	CMR_LOGI("is discard frm %ld", cxt->is_discard_frm);
}

cmr_uint camera_get_is_discard_frame(cmr_handle oem_handle, struct frm_info *data)
{
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct snapshot_context         *snp_cxt = &cxt->snp_cxt;
	cmr_uint                        is_discard = 0;

	if (snp_cxt->channel_bits & (1 << data->channel_id)) {
		sem_wait(&cxt->access_sm);
		is_discard = cxt->is_discard_frm;
		sem_post(&cxt->access_sm);
	}
	CMR_LOGI("is discard frm %ld", is_discard);
	return is_discard;
}

void camera_set_snp_req(cmr_handle oem_handle, cmr_uint is_req)
{
	struct camera_context           *cxt = (struct camera_context*)oem_handle;

	sem_wait(&cxt->access_sm);
	cxt->snp_cxt.is_req_snp = is_req;
	sem_post(&cxt->access_sm);
	CMR_LOGI("snp req %ld", cxt->snp_cxt.is_req_snp);
}

cmr_uint camera_get_snp_req(cmr_handle oem_handle)
{
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	cmr_uint                        is_req;

	sem_wait(&cxt->access_sm);
	is_req = cxt->snp_cxt.is_req_snp;
	sem_post(&cxt->access_sm);
	CMR_LOGI("get snp req %ld", is_req);

	return is_req;
}

void camera_send_channel_data(cmr_handle oem_handle, cmr_handle receiver_handle, cmr_uint evt, void *data)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct frm_info                 *frm_ptr = data;
	cmr_u32                         chn_bit;

	if (!frm_ptr) {
		CMR_LOGE("err, frame is null");
		goto exit;
	}
	chn_bit = 1 << frm_ptr->channel_id;

	CMR_LOGI("test %d %d %d %d", frm_ptr->channel_id, cxt->prev_cxt.channel_bits, cxt->snp_cxt.channel_bits, cxt->snp_cxt.total_num);
	if (cxt->prev_cxt.channel_bits & chn_bit) {
		ret = cmr_preview_receive_data(cxt->prev_cxt.preview_handle, cxt->camera_id, evt, data);
	}
	if (cxt->snp_cxt.channel_bits & chn_bit) {
		cxt->snp_cxt.cur_frm_info.data_endian = frm_ptr->data_endian;
		if (TAKE_PICTURE_NEEDED == camera_get_snp_req((cmr_handle)cxt)) {
#ifdef CAMERA_PATH_SHARE
			if (CAMERA_ZSL_MODE == cxt->snp_cxt.snp_mode || 1 != cxt->snp_cxt.total_num) {
				camera_set_discard_frame(cxt, 1);
				ret = cmr_preview_receive_data(cxt->prev_cxt.preview_handle, cxt->camera_id, PREVIEW_CHN_PAUSE, data);
				if (ret) {
					CMR_LOGE("failed to pause path %ld", ret);
				}
				ret = cmr_preview_receive_data(cxt->prev_cxt.preview_handle, cxt->camera_id, evt, data);
			} else {
				ret = cmr_preview_receive_data(cxt->prev_cxt.preview_handle, cxt->camera_id, evt, data);
				camera_post_share_path_available(oem_handle);
			}
#else
			ret = cmr_preview_receive_data(cxt->prev_cxt.preview_handle, cxt->camera_id, evt, data);
			camera_post_share_path_available(oem_handle);
#endif
			ret = cmr_snapshot_receive_data(cxt->snp_cxt.snapshot_handle, SNAPSHOT_EVT_CHANNEL_DONE, data);
		} else {
			ret = cmr_snapshot_receive_data(cxt->snp_cxt.snapshot_handle, SNAPSHOT_EVT_FREE_FRM, data);
		}
	}
exit:
	if (ret) {
		CMR_LOGE("failed to send channel data %ld", ret);
	}
}

/*
*privdata:oem handle
*/
cmr_int camera_sensor_streamctrl(cmr_u32 on_off, void *privdata)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)privdata;

	if (!cxt) {
		CMR_LOGE("error param");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	ret = cmr_sensor_stream_ctrl(cxt->sn_cxt.sensor_handle, cxt->camera_id, on_off);
	if (ret) {
		CMR_LOGE("err to set stream %ld", ret);
	}
exit:
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_isp_ctrl_done(cmr_u32 cmd, void* data)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;

	if (cmd >= ISP_CTRL_MAX) {
		CMR_LOGE("isp wrong cmd %d", cmd);
		ret =  -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}

	CMR_LOGI("isp cmd, 0x%x, ret %ld", cmd, ret);
exit:
	return ret;
}

void camera_sensor_evt_cb(cmr_int evt, void* data, void* privdata)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)privdata;

	if (!cxt || !data || CMR_EVT_SENSOR_BASE != (CMR_EVT_SENSOR_BASE & evt)) {
		CMR_LOGE("error param, handle 0x%lx data 0x%lx evt 0x%lx" , (cmr_uint)cxt, (cmr_uint)data, evt);
		goto exit;
	}
	CMR_LOGI("evt %ld, handle 0x%lx", evt, (cmr_uint)privdata);
	switch (evt) {
	case CMR_SENSOR_FOCUS_MOVE:
		if (1 == cxt->focus_cxt.inited) {
			ret = cmr_focus_sensor_handle(cxt->focus_cxt.focus_handle, CMR_SENSOR_FOCUS_MOVE, cxt->camera_id, data);
		} else {
			CMR_LOGE("err, focus hasn't been initialized");
		}
		break;
	case CMR_SENSOR_ERROR:
		if (1 == cxt->prev_cxt.inited) {
			ret = cmr_preview_receive_data(cxt->prev_cxt.preview_handle, cxt->camera_id, evt, data);
		} else {
			CMR_LOGE("err, preview hasn't been initialized");
		}
		break;
	default:
		CMR_LOGE("can't handle ths evt, 0x%lx", evt);
	}
exit:
	if (ret) {
		CMR_LOGE("failed %ld", ret);
	}
}

cmr_int camera_is_need_change_fmt(cmr_handle oem_handle, struct frm_info *data_ptr)
{
	cmr_int                         is_change_fmt = 0;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct snapshot_context         *snp_cxt = &cxt->snp_cxt;
	cmr_uint                        is_snp_frm = 0;

	is_snp_frm = ((1 << data_ptr->channel_id) & snp_cxt->channel_bits);
	if ( is_snp_frm && (1 == camera_get_hdr_flag(cxt))) {
		if (IMG_DATA_TYPE_JPEG == data_ptr->fmt || IMG_DATA_TYPE_RAW == data_ptr->fmt) {
			is_change_fmt = 1;
		}
	}
	return is_change_fmt;
}

cmr_int camera_get_cap_time(cmr_handle snp_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)snp_handle;
	cmr_u32                         sec = 0;
	cmr_u32                         usec = 0;

	sem_wait(&cxt->access_sm);
	ret = cmr_v4l2_get_cap_time(cxt->v4l2_cxt.v4l2_handle, &sec, &usec);
	CMR_LOGI("cap time %d %d", sec, usec);
	cxt->snp_cxt.cap_time_stamp = sec * 1000000000LL + usec * 1000;
	sem_post(&cxt->access_sm);
	return ret;
}

cmr_int camera_check_cap_time(cmr_handle snp_handle, struct frm_info * data)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)snp_handle;
	cmr_s64                         frame_time = data->sec * 1000000000LL + data->usec * 1000;

	CMR_LOGI("frame_time %ld, %ld", data->sec, data->usec);
	sem_wait(&cxt->access_sm);
	if (TAKE_PICTURE_NEEDED == cxt->snp_cxt.is_req_snp) {
		if (frame_time <= cxt->snp_cxt.cap_time_stamp) {
			CMR_LOGW("frame is earlier than picture, drop!");
			ret = CMR_CAMERA_FAIL;
		} else {
			CMR_LOGV("frame time OK!");
		}
	}
	sem_post(&cxt->access_sm);
	return ret;
}

void camera_v4l2_handle(cmr_int evt, void* data, void* privdata)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)privdata;
	struct ipm_context              *ipm_cxt = &cxt->ipm_cxt;
	struct frm_info                 *frame = (struct frm_info*)data;
	cmr_u32                         channel_id;
	cmr_handle                      receiver_handle;
	cmr_u32                         chn_bits = (1 <<  frame->channel_id);

	if (camera_get_is_discard_frame((cmr_handle)cxt, frame) || camera_check_cap_time((cmr_handle)cxt, frame)) {
		ret = camera_channel_free_frame((cmr_handle)cxt, frame->channel_id, frame->frame_id);
		return;
	}
	receiver_handle = cxt->v4l2_cxt.caller_handle[frame->channel_id];
	if (1 == camera_get_hdr_flag(cxt) && (0 != cxt->snp_cxt.channel_bits)
		&& (TAKE_PICTURE_NEEDED == camera_get_snp_req((cmr_handle)cxt))) {
		struct img_frm out_param;
		struct ipm_frame_in  ipm_in_param;
		struct ipm_frame_out imp_out_param;
		ret = cmr_preview_receive_data(cxt->prev_cxt.preview_handle, cxt->camera_id, evt, data);
		if (ret) {
			CMR_LOGE("failed to send a frame to preview %ld", ret);
		}
		if (camera_is_need_change_fmt((cmr_handle)cxt, frame)) {
			ret = cmr_snapshot_format_convert((cmr_handle)cxt, data, &out_param);//sync
			if (ret) {
				CMR_LOGE("failed to format convert %ld", ret);
				goto exit;
			}
		} else {
			out_param = cxt->snp_cxt.post_proc_setting.chn_out_frm[frame->frame_id-frame->base];
		}
		cxt->snp_cxt.cur_frm_info = *frame;
		ipm_cxt->frm_num++;
		ipm_in_param.src_frame = out_param;
		ipm_in_param.dst_frame = out_param;
		ipm_in_param.private_data = (void*)privdata;
		imp_out_param.dst_frame = out_param;
		imp_out_param.private_data = privdata;
		ret = ipm_transfer_frame(ipm_cxt->hdr_handle, &ipm_in_param, &imp_out_param);
		if (ret) {
			CMR_LOGE("failed to transfer frame to ipm %ld", ret);
			goto exit;
		}
		cmr_snapshot_memory_flush(cxt->snp_cxt.snapshot_handle);
		if(ipm_cxt->frm_num == ipm_cxt->hdr_num) {
			camera_post_share_path_available((cmr_handle)cxt);
			sem_wait(&cxt->hdr_sync_sm);
		}
	} else {
		camera_send_channel_data((cmr_handle)cxt, receiver_handle, evt, data);
	}
exit:
	if (ret) {
		if (cxt->camera_cb) {
			cmr_snapshot_stop((cmr_handle)cxt);
			camera_set_snp_req((cmr_handle)cxt, TAKE_PICTURE_NO);
			cxt->camera_cb(CAMERA_EXIT_CB_FAILED, cxt->client_data, CAMERA_FUNC_TAKE_PICTURE, NULL);
		}
	}
	return;
}

void camera_v4l2_evt_cb(cmr_int evt, void* data, void* privdata)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)privdata;
	struct frm_info                 *frame = (struct frm_info*)data;
	cmr_u32                         channel_id;
	cmr_handle                      receiver_handle;

	if (!cxt || !data || !privdata || CMR_EVT_V4L2_BASE != (CMR_EVT_V4L2_BASE & evt)) {
		CMR_LOGE("error param, handle 0x%lx data 0x%lx evt 0x%lx", (cmr_uint)cxt, (cmr_uint)data, evt);
		return;
	}
	CMR_LOGI("evt 0x%lx, handle 0x%lx", evt, (cmr_uint)privdata);

	channel_id = frame->channel_id;
	if (channel_id >= V4L2_CHANNEL_MAX) {
		CMR_LOGE("error param, channel id %d" , channel_id);
		return;
	}

	switch (evt) {
	case CMR_V4L2_TX_DONE:
#ifdef OEM_HANDLE_HDR
		camera_v4l2_handle(evt, data, privdata);
#else
		camera_send_channel_data((cmr_handle)cxt, receiver_handle, evt, data);
#endif
		break;
	case CMR_V4L2_TX_ERROR:
	case CMR_V4L2_TX_NO_MEM:
	case CMR_V4L2_CSI2_ERR:
	case CMR_V4L2_TIME_OUT:
		ret = cmr_preview_receive_data(cxt->prev_cxt.preview_handle, cxt->camera_id, evt, data);
		if (ret) {
			CMR_LOGE("fail to handle error, ret %ld", ret);
		}
		break;
	default:
		CMR_LOGE("don't support evt 0x%lx", evt);
		break;
	}
}

void camera_scaler_evt_cb(cmr_int evt, void* data, void *privdata)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)privdata;

	if (!cxt || !data || !privdata) {
		CMR_LOGE("err, scale callback param");
		ret = - CMR_CAMERA_INVALID_PARAM;
		return;
	}
	CMR_LOGI("evt 0x%lx, handle 0x%lx", evt, (cmr_uint)privdata);

	if (CMR_IMG_CVT_SC_DONE == evt) {
		camera_take_snapshot_step(CMR_STEP_SC_E);
		cmr_snapshot_receive_data((cmr_handle)privdata, SNAPSHOT_EVT_SC_DONE, data);
	} else {
		CMR_LOGE("err, don't support evt 0x%lx", evt);
	}
}

void camera_jpeg_evt_cb(cmr_int evt, void* data, void *privdata)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)privdata;
	cmr_u32                         temp_evt;

	if (NULL == data || !privdata || CMR_EVT_JPEG_BASE != (CMR_EVT_JPEG_BASE & evt)) {
		CMR_LOGE("err, param, 0x%lx 0x%lx", (cmr_uint)data, evt);
		return;
	}
	CMR_LOGI("evt 0x%lx, handle 0x%lx", evt, (cmr_uint)privdata);

	switch (evt) {
	case CMR_JPEG_ENC_DONE:
		camera_take_snapshot_step(CMR_STEP_JPG_ENC_E);
		temp_evt = SNAPSHOT_EVT_JPEG_ENC_DONE;
		break;
	case CMR_JPEG_DEC_DONE:
		temp_evt = SNAPSHOT_EVT_JPEG_DEC_DONE;
		break;
	case CMR_JPEG_ENC_ERR:
		temp_evt = SNAPSHOT_EVT_JPEG_ENC_ERR;
		break;
	case CMR_JPEG_DEC_ERR:
		temp_evt = SNAPSHOT_EVT_JPEG_DEC_ERR;
		break;
	default:
		ret = - CMR_CAMERA_NO_SUPPORT;
		CMR_LOGE("err, don't support evt 0x%lx", evt);
	}
	if (ret) {
		CMR_LOGE("done %ld", ret);
		return;
	}
	ret = cmr_snapshot_receive_data(cxt->snp_cxt.snapshot_handle, temp_evt, data);
	if (ret) {
		CMR_LOGE("failed %ld", ret);
	}
}

cmr_int camera_isp_evt_cb(cmr_handle oem_handle, cmr_u32 evt, void* data, cmr_u32 data_len)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	cmr_u32                         sub_type;
	cmr_u32                         cmd = evt & 0xFF;

	if (!oem_handle || CMR_EVT_ISP_BASE != (CMR_EVT_ISP_BASE & evt)) {
		CMR_LOGE("err param, 0x%lx 0x%x 0x%lx", (cmr_uint)data, evt, (cmr_uint)oem_handle);
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}

	CMR_LOGI("evt 0x%x, handle 0x%lx", evt, (cmr_uint)oem_handle);

	sub_type = (~CMR_EVT_ISP_BASE) & evt;
	CMR_LOGI("sub_type %0x", sub_type);

	if ((sub_type & ISP_EVT_MASK) == 0) {
		ret = camera_isp_ctrl_done(cmd, data);
		CMR_LOGI("ret %ld", ret);
		goto exit;
	}

	switch (sub_type & ISP_EVT_MASK) {
	case ISP_PROC_CALLBACK:
		if (camera_is_need_change_fmt((cmr_handle)cxt, data)) {
			ret = cmr_snapshot_receive_data(cxt->snp_cxt.snapshot_handle, SNAPSHOT_EVT_CVT_RAW_DATA, data);
		} else {
			ret = cmr_snapshot_receive_data(cxt->snp_cxt.snapshot_handle, SNAPSHOT_EVT_RAW_PROC, data);
		}
		break;
	case ISP_AF_NOTICE_CALLBACK:
		CMR_LOGI("ISP_AF_NOTICE_CALLBACK come here");
		if (1 == cxt->focus_cxt.inited) {
			ret = cmr_focus_isp_handle(cxt->focus_cxt.focus_handle, FOCUS_EVT_ISP_AF_NOTICE, cxt->camera_id, data);
		}
		break;
	/*case ISP_FLASH_AE_CALLBACK:
		ret = camera_isp_alg_done(data);
		break;
	case ISP_AE_BAPASS_CALLBACK:
		ret = camera_isp_alg_done(data);
		break;*/ //to do with robert

	/*case ISP_AF_STAT_CALLBACK:
		ret = camera_isp_af_stat(data);
		break;*/

	case ISP_FAST_AE_STAB_CALLBACK:
		break;

	default:
		break;
	}
exit:
	return ret;
}

void camera_focus_evt_cb(enum af_cb_type cb, cmr_s32 param, void *privdata)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)privdata;
	cmr_int                         oem_cb;

	if (!privdata) {
		CMR_LOGE("err, handle for callback");
		return;
	}
	switch (cb) {
	case AF_CB_DONE:
		oem_cb = CAMERA_EXIT_CB_DONE;
		break;
    case AF_CB_FAILED:
		oem_cb = CAMERA_EXIT_CB_FAILED;
		break;
    case AF_CB_ABORT:
		oem_cb = CAMERA_EXIT_CB_ABORT;
		break;
    case AF_CB_FOCUS_MOVE:
		oem_cb = CAMERA_EVT_CB_FOCUS_MOVE;
		break;
	default:
		CMR_LOGE("failed focus cb %d", cb);
		ret = -CMR_CAMERA_NO_SUPPORT;
	}
	if (ret) {
		CMR_LOGE("done %ld", ret);
		return;
	}
	CMR_LOGI("param =0x%lx camera_cb 0x%lx focus cb %ld, oem cb 0x%lx", (cmr_uint)param, (cmr_uint)cb, (cmr_uint)oem_cb, (cmr_uint)cxt->camera_cb);
	if (cxt->camera_cb) {
		CMR_LOGI("cxt->camera_cb run");
		cxt->camera_cb(oem_cb, cxt->client_data, CAMERA_FUNC_START_FOCUS, (void*)param);
	} else {
		CMR_LOGI("cxt->camera_cb null error");
	}
}

cmr_int camera_preview_cb(cmr_handle oem_handle, enum preview_cb_type cb_type, enum preview_func_type func, void *param)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	cmr_uint                        oem_func;
	cmr_uint                        oem_cb_type;
	CMR_MSG_INIT(message);

	if (!oem_handle) {
		CMR_LOGE("error handle");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}

	if (PREVIEW_FUNC_START_PREVIEW == func) {
		oem_func = CAMERA_FUNC_START_PREVIEW;
	} else if (PREVIEW_FUNC_STOP_PREVIEW == func) {
		oem_func = CAMERA_FUNC_STOP_PREVIEW;
	} else {
		CMR_LOGE("err, %d", func);
		goto exit;
	}
	CMR_LOGI("%ld %d", oem_func, cb_type);
	switch (cb_type) {
	case PREVIEW_RSP_CB_SUCCESS:
		oem_cb_type = CAMERA_RSP_CB_SUCCESS;
		break;
	case PREVIEW_EVT_CB_FRAME:
		oem_cb_type = CAMERA_EVT_CB_FRAME;
		break;
	case PREVIEW_EXIT_CB_FAILED:
		oem_cb_type = CAMERA_EXIT_CB_FAILED;
		break;
	case PREVIEW_EVT_CB_FLUSH:
		oem_cb_type = CAMERA_EVT_CB_FLUSH;
		break;
	case PREVIEW_EVT_CB_FD:
		oem_cb_type = CAMERA_EVT_CB_FD;
		break;
	default:
		CMR_LOGE("err, %d", cb_type);
		ret = -CMR_CAMERA_NO_SUPPORT;
	}
	if (ret) {
		goto exit;
	}

	if (CAMERA_FUNC_STOP_PREVIEW == oem_func && CAMERA_RSP_CB_SUCCESS == oem_cb_type) {
		CMR_LOGI("stop preview response, notify directly");
		if (cxt->camera_cb) {
			cxt->camera_cb(oem_cb_type, cxt->client_data, oem_func, param);
		}
		return ret;
	}

	if (param) {
		message.data = malloc(sizeof(struct camera_frame_type));
		if (!message.data) {
			CMR_LOGE("failed to malloc msg");
			ret = -CMR_CAMERA_NO_MEM;
			goto exit;
		}
		message.alloc_flag = 1;
		memcpy(message.data, param, sizeof(struct camera_frame_type));
	}
	message.msg_type = oem_func;
	message.sub_msg_type = oem_cb_type;
	message.sync_flag  = CMR_MSG_SYNC_RECEIVED;
	ret = cmr_thread_msg_send(cxt->prev_cb_thr_handle, &message);
	if (ret) {
		CMR_LOGE("failed to send msg, ret %ld", ret);
		free(message.data);
	}
exit:
	CMR_LOGD("out ret %ld", ret);
	return ret;
}

cmr_int camera_ipm_cb(cmr_u32 class_type, struct ipm_frame_out *cb_param)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = NULL;
	struct frm_info                 frame;

	CMR_LOGI("s");
	if (!cb_param || !cb_param->private_data) {
		CMR_LOGE("error param");
		return -CMR_CAMERA_INVALID_PARAM;
	}
	cxt = (struct camera_context*)cb_param->private_data;
#ifdef OEM_HANDLE_HDR
	cxt->ipm_cxt.frm_num = 0;
	frame = cxt->snp_cxt.cur_frm_info;
	cmr_snapshot_memory_flush(cxt->snp_cxt.snapshot_handle);
	sem_post(&cxt->hdr_sync_sm);
#endif
	ret = cmr_snapshot_receive_data(cxt->snp_cxt.snapshot_handle, SNAPSHOT_EVT_HDR_DONE, &frame);
	if (ret) {
		CMR_LOGE("fail to send frame to snp %ld", ret);
	}
	CMR_LOGI("done %ld", ret);
	return ret;
}

void camera_snapshot_cb_to_hal(cmr_handle oem_handle, enum snapshot_cb_type cb, enum snapshot_func_type func, void* param)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	cmr_uint                        oem_func;
	cmr_uint                        oem_cb_type = CAMERA_CB_TYPE_MAX;
	cmr_handle                      send_thr_handle = cxt->snp_cb_thr_handle;
	CMR_MSG_INIT(message);

	switch (func) {
	case SNAPSHOT_FUNC_RELEASE_PICTURE:
		oem_func = CAMERA_FUNC_RELEASE_PICTURE;
		break;
    case SNAPSHOT_FUNC_TAKE_PICTURE:
		oem_func = CAMERA_FUNC_TAKE_PICTURE;
		break;
    case SNAPSHOT_FUNC_ENCODE_PICTURE:
		oem_func = CAMERA_FUNC_ENCODE_PICTURE;
		break;
    default:
		oem_func = func;
		break;
	}
	switch (cb) {
	case SNAPSHOT_RSP_CB_SUCCESS:
		oem_cb_type = CAMERA_RSP_CB_SUCCESS;
		break;
	case SNAPSHOT_EVT_CB_FLUSH:
		oem_cb_type = CAMERA_EVT_CB_FLUSH;
		break;
	case SNAPSHOT_EVT_CB_ZSL_NEW_FRM:
		oem_cb_type = CAMERA_EVT_CB_ZSL_FRM;
		break;
	case SNAPSHOT_EXIT_CB_FAILED:
		oem_cb_type = CAMERA_EXIT_CB_FAILED;
		break;
	case SNAPSHOT_EVT_CB_SNAPSHOT_DONE:
		oem_cb_type = CAMERA_EVT_CB_SNAPSHOT_DONE;
		break;
	case SNAPSHOT_EXIT_CB_DONE:
		oem_cb_type = CAMERA_EXIT_CB_DONE;
		break;
	case SNAPSHOT_EVT_CB_CAPTURE_FRAME_DONE:
		oem_cb_type = CAMERA_EVT_CB_CAPTURE_FRAME_DONE;
		send_thr_handle = cxt->snp_secondary_thr_handle;
		break;
	case SNAPSHOT_EVT_CB_SNAPSHOT_JPEG_DONE:
		oem_cb_type = CAMERA_EVT_CB_SNAPSHOT_JPEG_DONE;
		break;
	default:
		oem_cb_type = cb;
		break;
	}
	CMR_LOGI("camera_cb %ld %ld", oem_cb_type, oem_func);
	if (param) {
		message.data = malloc(sizeof(struct camera_frame_type));
		if (!message.data) {
			CMR_LOGE("failed to malloc msg");
			ret = -CMR_CAMERA_NO_MEM;
			return;
		}
		message.alloc_flag = 1;
		memcpy(message.data, param, sizeof(struct camera_frame_type));
	}
	message.msg_type = oem_func;
	message.sub_msg_type = oem_cb_type;
	if (CAMERA_EVT_CB_CAPTURE_FRAME_DONE == oem_cb_type) {
		message.sync_flag  = CMR_MSG_SYNC_RECEIVED;
	} else if ((CAMERA_EXIT_CB_PREPARE == oem_cb_type) || (CAMERA_EVT_CB_SNAPSHOT_JPEG_DONE == oem_cb_type)) {
		message.sync_flag  = CMR_MSG_SYNC_NONE;
	} else {
		message.sync_flag  = CMR_MSG_SYNC_PROCESSED;
	}
	ret = cmr_thread_msg_send(send_thr_handle, &message);
	if (ret) {
		CMR_LOGE("failed to send msg %ld", ret);
		if (message.data) {
			free(message.data);
		}
	}
}

void camera_set_hdr_flag(struct camera_context *cxt, cmr_u32 hdr_flag)
{
	CMR_LOGI("flag %d", hdr_flag);
	sem_wait(&cxt->hdr_flag_sm);
	cxt->snp_cxt.is_hdr = hdr_flag;
	sem_post(&cxt->hdr_flag_sm);
}

cmr_u32 camera_get_hdr_flag(struct camera_context *cxt)
{
	cmr_u32                         hdr_flag = 0;
	sem_wait(&cxt->hdr_flag_sm);
	hdr_flag = cxt->snp_cxt.is_hdr;
	sem_post(&cxt->hdr_flag_sm);
	CMR_LOGI("flag %d", hdr_flag);
	return hdr_flag;
}

cmr_int camera_open_hdr(struct camera_context *cxt, struct ipm_open_in *in_ptr, struct ipm_open_out *out_ptr)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;

	CMR_LOGI("start");
	sem_wait(&cxt->hdr_flag_sm);
	ret = cmr_ipm_open(cxt->ipm_cxt.ipm_handle, IPM_TYPE_HDR, in_ptr, out_ptr, &cxt->ipm_cxt.hdr_handle);
	sem_post(&cxt->hdr_flag_sm);
	CMR_LOGI("end");
	return ret;
}

cmr_int camera_close_hdr(struct camera_context *cxt)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;

	sem_post(&cxt->hdr_flag_sm);
	if (cxt->ipm_cxt.hdr_handle) {
		ret = cmr_ipm_close(cxt->ipm_cxt.hdr_handle);
		cxt->ipm_cxt.hdr_handle = 0;
	}
	sem_wait(&cxt->hdr_flag_sm);
	CMR_LOGE("failed to close hdr %ld", ret);
	return ret;
}

void camera_snapshot_channel_handle(struct camera_context *cxt, void* param)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct frm_info                 frame;
	struct camera_jpeg_param        *jpeg_param_ptr = (struct camera_jpeg_param*)param;
	cmr_uint                        i;
	cmr_uint                        is_need_resume = 0;;
#ifdef CAMERA_PATH_SHARE
	if (CAMERA_ZSL_MODE == cxt->snp_cxt.snp_mode) {
		is_need_resume = 1;
	} else if (1 != cxt->snp_cxt.total_num) {
		if (jpeg_param_ptr) {
			if (1 != jpeg_param_ptr->need_free) {
				is_need_resume = 1;
			}
		} else {
			CMR_LOGE("param is error");
		}
	} else {
		is_need_resume = 0;
	}
	if (1 == is_need_resume) {
		for (i=0 ; i<V4L2_CHANNEL_MAX ; i++) {
			if (cxt->snp_cxt.channel_bits & (1<<i)) {
				break;
			}
		}
		frame.channel_id = i;
		ret = cmr_preview_receive_data(cxt->prev_cxt.preview_handle, cxt->camera_id, PREVIEW_CHN_RESUME, &frame);
		if (ret) {
			CMR_LOGE("failed to resume path %ld", ret);
		}
		CMR_LOGI("done");
	}
#endif
}

void camera_snapshot_state_handle(cmr_handle oem_handle, enum snapshot_cb_type cb, enum snapshot_func_type func, void* param)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;

	CMR_PRINT_TIME;
	if (SNAPSHOT_FUNC_STATE == func) {
		switch (cb) {
		case SNAPSHOT_EVT_START_ROT:
			CMR_LOGI("start rot");
			break;
		case SNAPSHOT_EVT_ROT_DONE:
			CMR_LOGI("rot done");
			break;
		case SNAPSHOT_EVT_START_SCALE:
			CMR_LOGI("start scaler");
			break;
		case SNAPSHOT_EVT_SCALE_DONE:
			CMR_LOGI("scaler done");
			break;
		case SNAPSHOT_EVT_START_ENC:
			CMR_LOGI("start jpeg enc");
			break;
		case SNAPSHOT_EVT_ENC_DONE:
			if (1 == camera_get_hdr_flag(cxt)) {
				ret = camera_close_hdr(cxt);
			}
			CMR_LOGI("jpeg enc done");
			break;
		case SNAPSHOT_EVT_START_CONVERT_THUMB:
			CMR_LOGI("start to convert thumb");
			break;
		case SNAPSHOT_EVT_CONVERT_THUMB_DONE:
			CMR_LOGI("convert thumb done");
			break;
		case SNAPSHOT_EVT_ENC_THUMB:
			CMR_LOGI("start to enc thumb");
			break;
		case SNAPSHOT_EVT_ENC_THUMB_DONE:
			CMR_LOGI("thumb enc done");
			break;
		case SNAPSHOT_EVT_START_DEC:
			CMR_LOGI("start to jpeg dec");
			break;
		case SNAPSHOT_EVT_DEC_DONE:
			CMR_LOGI("jpeg dec done");
			break;
		case SNAPSHOT_EVT_START_EXIF_JPEG:
			CMR_LOGI("start to compound jpeg with exif");
			break;
		case SNAPSHOT_EVT_EXIF_JPEG_DONE:
			CMR_LOGI("jpeg done with exif");
	//		camera_snapshot_channel_handle(cxt);
			break;
		case SNAPSHOT_EVT_STATE:
			cxt->snp_cxt.status = (cmr_u32)param;
			CMR_LOGI("snapshot state is %d", cxt->snp_cxt.status);
			break;
		default:
			CMR_LOGE("don't support cb %d", cb);
			break;
		}
	}
    if (SNAPSHOT_FUNC_RECOVERY == func) {
		//to do
    }
}

void camera_post_share_path_available(cmr_handle oem_handle)
{
	struct camera_context           *cxt = (struct camera_context*)oem_handle;

	CMR_LOGI("post beging");
	sem_post(&cxt->share_path_sm);
	CMR_LOGI("post end");
}

void camera_wait_share_path_available(cmr_handle oem_handle)
{
	struct camera_context           *cxt = (struct camera_context*)oem_handle;

	CMR_LOGI("wait beging");
	sem_wait(&cxt->share_path_sm);
	CMR_LOGI("wait end");
}

void camera_snapshot_cb(cmr_handle oem_handle, enum snapshot_cb_type cb, enum snapshot_func_type func, void* param)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	cmr_uint                        oem_func;
	cmr_uint                        oem_cb_type;

	if (!oem_handle) {
		CMR_LOGE("error handle");
		return;
	}
	CMR_LOGI("func %d", func);
	if (SNAPSHOT_FUNC_TAKE_PICTURE == func || SNAPSHOT_FUNC_ENCODE_PICTURE == func) {
		if ((SNAPSHOT_FUNC_ENCODE_PICTURE == func) && (SNAPSHOT_EXIT_CB_DONE == cb) && param) {
			struct camera_jpeg_param enc_param = *(struct camera_jpeg_param*)param;
			if (enc_param.need_free) {
				camera_set_snp_req(oem_handle, TAKE_PICTURE_NO);
				camera_set_discard_frame(cxt, 0);
			}
		}
		camera_snapshot_cb_to_hal(oem_handle, cb, func, param);
		if ((SNAPSHOT_FUNC_ENCODE_PICTURE == func) && (SNAPSHOT_EXIT_CB_DONE == cb)) {
			camera_snapshot_channel_handle(cxt, param);
		}
		if ((SNAPSHOT_FUNC_TAKE_PICTURE == func) && (SNAPSHOT_RSP_CB_SUCCESS == cb)) {
			camera_wait_share_path_available(oem_handle);
		}
	} else if (SNAPSHOT_FUNC_RELEASE_PICTURE == func) {
		if (cxt->camera_cb) {
			cxt->camera_cb(CAMERA_RSP_CB_SUCCESS, cxt->client_data, CAMERA_FUNC_RELEASE_PICTURE, NULL);
		}
	} else {
		camera_snapshot_state_handle(oem_handle, cb, func, param);
	}

}

cmr_int camera_before_set(cmr_handle oem_handle, enum preview_param_mode mode)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle) {
		CMR_LOGE("error handle");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_PRINT_TIME;
	ret = cmr_preview_before_set_param(cxt->prev_cxt.preview_handle, cxt->camera_id, mode);
	CMR_PRINT_TIME;
exit:
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_after_set(cmr_handle oem_handle, struct after_set_cb_param *param)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle) {
		CMR_LOGE("error handle");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_PRINT_TIME;
	ret = cmr_preview_after_set_param(cxt->prev_cxt.preview_handle, cxt->camera_id, param->re_mode, param->skip_mode, param->skip_number);
	CMR_PRINT_TIME;
exit:
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_focus_pre_proc(cmr_handle oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct setting_cmd_parameter    setting_param;

	/*open flash*/
	if (CAMERA_ZSL_MODE != cxt->snp_cxt.snp_mode) {
	//	setting_param.flash_param.capture_mode = cxt->snp_cxt.snapshot_sn_mode;//robert
		setting_param.ctrl_flash.is_active = 1;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, SETTING_CTRL_FLASH, &setting_param);
		if (ret) {
			CMR_LOGE("failed to open flash");
		}
	}

	return ret;
}

cmr_int camera_focus_post_proc(cmr_handle oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct setting_cmd_parameter    setting_param;

	/*close flash*/
	if (CAMERA_ZSL_MODE != cxt->snp_cxt.snp_mode) {
//		setting_param.flash_param.capture_mode = cxt->snp_cxt.snapshot_sn_mode;//robert
		setting_param.ctrl_flash.is_active = 0;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, SETTING_CTRL_FLASH, &setting_param);
		if (ret) {
			CMR_LOGE("failed to open flash %ld", ret);
		}
	}
	return ret;
}

cmr_int camera_get_preview_status(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct preview_context         *prev_cxt = &cxt->prev_cxt;
	struct snapshot_context        *snp_cxt = &cxt->snp_cxt;
	struct setting_context         *setting_cxt = &cxt->setting_cxt;

	if (1 != prev_cxt->inited) {
		CMR_LOGE("err, don't init preview");
		return ERROR;
	}

	return cmr_preview_get_status(cxt->prev_cxt.preview_handle, cxt->camera_id);
}

cmr_int camera_sensor_init(cmr_handle  oem_handle, cmr_uint is_autotest)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct sensor_context           *sn_cxt = &cxt->sn_cxt;
	struct sensor_init_param        init_param;
	cmr_handle                      sensor_handle;
	cmr_u32                         camera_id_bits = 0;

	if (1 == sn_cxt->inited) {
		CMR_LOGD("sensor has been intialized");
		goto exit;
	}
	CMR_PRINT_TIME;
	cmr_bzero(&init_param, sizeof(init_param));
	init_param.oem_handle = oem_handle;
	init_param.is_autotest = is_autotest;
	ret = cmr_sensor_init(&init_param, &sensor_handle);
	if (ret) {
		CMR_LOGE("failed to init sensor %ld", ret);
		ret = -CMR_CAMERA_NO_SUPPORT;
		goto exit;
	}
	sn_cxt->sensor_handle = sensor_handle;
	sn_cxt->inited = 1;
	CMR_PRINT_TIME;
	if (ret) {
		CMR_LOGE("failed to set auto test mode %ld", ret);
	}
	camera_id_bits = 1 << cxt->camera_id;
	ret = cmr_sensor_open(sensor_handle, camera_id_bits);
	if (ret) {
		CMR_LOGE("open %d sensor failed %ld", cxt->camera_id, ret);
		goto sensor_exit;
	}
	CMR_PRINT_TIME;

	cmr_sensor_event_reg(sensor_handle, cxt->camera_id, camera_sensor_evt_cb);
	cxt->sn_cxt.sensor_handle = sensor_handle;
    goto exit;

sensor_exit:
	cmr_sensor_deinit(sn_cxt->sensor_handle);
	sn_cxt->inited = 0;
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_sensor_deinit(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	cmr_handle                      sensor_handle;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct sensor_context           *sn_cxt;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	sn_cxt = &cxt->sn_cxt;
	if (0 == sn_cxt->inited) {
		CMR_LOGI("sensor has been de-intialized");
		goto exit;
	}
	sensor_handle = sn_cxt->sensor_handle;
	CMR_PRINT_TIME;
	cmr_sensor_close(sensor_handle, (1 << cxt->camera_id));
	CMR_PRINT_TIME;
    cmr_sensor_deinit(sensor_handle);
	CMR_PRINT_TIME;
	cmr_bzero(sn_cxt,sizeof(*sn_cxt));

exit:
	return ret;
}

cmr_int camera_v4l2_init(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct v4l2_context             *v4l2_cxt = &cxt->v4l2_cxt;
	cmr_handle                      v4l2_handle;
	struct v4l2_init_param          v4l2_param;

	if (0 == v4l2_cxt->inited) {
		v4l2_param.oem_handle = oem_handle;
		ret = cmr_v4l2_init(&v4l2_param, &v4l2_handle);
		if (ret) {
			CMR_LOGE("failed to init v4l2 %ld", ret);
			ret = -CMR_CAMERA_NO_SUPPORT;
			goto exit;
		}
		cmr_v4l2_evt_reg(v4l2_handle, camera_v4l2_evt_cb);
		cmr_v4l2_stream_cb(v4l2_handle, camera_sensor_streamctrl);
		v4l2_cxt->inited = 1;
		v4l2_cxt->v4l2_handle = v4l2_handle;
	}

exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_v4l2_deinit(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct v4l2_context             *v4l2_cxt;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	v4l2_cxt = &cxt->v4l2_cxt;
	if (0 == v4l2_cxt->inited) {
		CMR_LOGD("V4L2 has been de-intialized");
		goto exit;
	}

	ret = cmr_v4l2_deinit(v4l2_cxt->v4l2_handle);
	if (ret) {
		CMR_LOGE("failed to de-init v4l2 %ld", ret);
		goto exit;
	}
	cmr_bzero(v4l2_cxt, sizeof(*v4l2_cxt));
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_jpeg_init(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct jpeg_context             *jpeg_cxt = &cxt->jpeg_cxt;

	if (0 == jpeg_cxt->inited) {
		ret = jpeg_init(oem_handle, &jpeg_cxt->jpeg_handle);
		if (CMR_CAMERA_SUCCESS == ret) {
			jpeg_evt_reg(jpeg_cxt->jpeg_handle, camera_jpeg_evt_cb);
			jpeg_cxt->inited = 1;
		} else {
			CMR_LOGE("failed to init jpeg codec %ld", ret);
			ret = -CMR_CAMERA_NO_SUPPORT;
		}
	}
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_jpeg_deinit(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct jpeg_context             *jpeg_cxt;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	jpeg_cxt = &cxt->jpeg_cxt;
	if (0 == jpeg_cxt->inited) {
		CMR_LOGD("jpeg codec has been de-intialized");
		goto exit;
	}

	ret = jpeg_deinit(jpeg_cxt->jpeg_handle);
	if (ret) {
		CMR_LOGE("failed to de-init jpeg codec %ld", ret);
		goto exit;
	}
	cmr_bzero(jpeg_cxt, sizeof(*jpeg_cxt));

exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_scaler_init(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct scaler_context           *scaler_cxt = &cxt->scaler_cxt;

	if (1 == scaler_cxt->inited) {
		CMR_LOGD("scaler has been intialized");
		goto exit;
	}

	ret = cmr_scale_open(&scaler_cxt->scaler_handle);
	if (ret) {
		CMR_LOGE("failed to init scaler %ld", ret);
		ret = -CMR_CAMERA_NO_SUPPORT;
	} else {
//		cmr_scale_evt_reg(camera_scaler_evt_cb);
		scaler_cxt->inited = 1;
	}

exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_scaler_deinit(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct scaler_context           *scaler_cxt;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	scaler_cxt = &cxt->scaler_cxt;
	if (0 == scaler_cxt->inited) {
		CMR_LOGD("scaler has been de-intialized");
		goto exit;
	}

	ret = cmr_scale_close(scaler_cxt->scaler_handle);
	if (ret) {
		CMR_LOGE("failed to de-init scaler %ld", ret);
		goto exit;
	}
	cmr_bzero(scaler_cxt, sizeof(*scaler_cxt));
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_rotation_init(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct rotation_context         *rot_cxt = &cxt->rot_cxt;
	cmr_handle                      rot_handle;

	if (1 == rot_cxt->inited) {
		CMR_LOGD("rot has been intialized");
		goto exit;
	}

	ret = cmr_rot_open(&rot_handle);
	if (ret) {
		CMR_LOGE("failed to init rot %ld", ret);
		ret = -CMR_CAMERA_NO_SUPPORT;
	} else {
		rot_cxt->rotation_handle = rot_handle;
		rot_cxt->inited = 1;
	}

exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_rotation_deinit(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct rotation_context         *rot_cxt;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	rot_cxt = &cxt->rot_cxt;
	if (0 == rot_cxt->inited) {
		CMR_LOGD("rot has been de-intialized");
		goto exit;
	}

	ret = cmr_rot_close(rot_cxt->rotation_handle);
	if (ret) {
		CMR_LOGE("failed to de-init rot %ld", ret);
		goto exit;
	}
	cmr_bzero(rot_cxt, sizeof(*rot_cxt));

exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_isp_init(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct isp_context              *isp_cxt = &cxt->isp_cxt;
	struct sensor_context           *sn_cxt = &cxt->sn_cxt;
	struct sensor_exp_info          *sensor_info_ptr;
	struct isp_init_param           isp_param;
	struct isp_video_limit          isp_limit;

	if (1 == isp_cxt->inited) {
		CMR_LOGD("isp has been intialized");
		goto exit;
	}

	ret = cmr_sensor_get_info(sn_cxt->sensor_handle, cxt->camera_id, &sn_cxt->sensor_info);
	if (ret) {
		CMR_LOGE("fail to get sensor info ret %ld", ret);
		goto exit;
	}

	if (IMG_DATA_TYPE_RAW != sn_cxt->sensor_info.image_format) {
		CMR_LOGD("no need to init isp %d ", sn_cxt->sensor_info.image_format);
		goto exit;
	}

	isp_param.isp_id = ISP_ID_SC9630;
	sensor_info_ptr = &sn_cxt->sensor_info;
	isp_param.setting_param_ptr = sensor_info_ptr->raw_info_ptr;
	if (0 != sensor_info_ptr->mode_info[SENSOR_MODE_COMMON_INIT].width) {
		isp_param.size.w = sensor_info_ptr->mode_info[SENSOR_MODE_COMMON_INIT].width;
		isp_param.size.h = sensor_info_ptr->mode_info[SENSOR_MODE_COMMON_INIT].height;
	} else {
		isp_param.size.w = sensor_info_ptr->mode_info[SENSOR_MODE_PREVIEW_ONE].width;
		isp_param.size.h = sensor_info_ptr->mode_info[SENSOR_MODE_PREVIEW_ONE].height;
	}
	isp_param.ctrl_callback = camera_isp_evt_cb;
	isp_param.oem_handle = oem_handle;

	CMR_LOGD("w %d h %d", isp_param.size.w,isp_param.size.h);
	CMR_PRINT_TIME;
	ret = isp_init(&isp_param);
	if (ret) {
		CMR_LOGE("failed to init isp %ld", ret);
		ret = -CMR_CAMERA_NO_SUPPORT;
		goto exit;
	}
	CMR_PRINT_TIME;
	ret = isp_capability(ISP_VIDEO_SIZE, &isp_limit);
	if (ret) {
		CMR_LOGE("failed to get the limitation of isp %ld", ret);
		ret = -CMR_CAMERA_NO_SUPPORT;
		goto isp_deinit;
	}
	isp_cxt->width_limit = isp_limit.width;
	isp_cxt->inited = 1;
	goto exit;

isp_deinit:
	isp_deinit();
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_isp_deinit(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct isp_context              *isp_cxt;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	isp_cxt = &cxt->isp_cxt;
	if (0 == isp_cxt->inited) {
		CMR_LOGD("isp has been de-intialized");
		goto exit;
	}

	ret = isp_deinit();
	if (ret) {
		CMR_LOGE("failed to de-init isp %ld", ret);
		goto exit;
	}
	cmr_bzero(isp_cxt, sizeof(*isp_cxt));

exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_preview_init(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct preview_context          *prev_cxt = &cxt->prev_cxt;
    struct preview_init_param       init_param;

	CMR_PRINT_TIME;
	if (1 == prev_cxt->inited) {
		CMR_LOGD("preview has been intialized");
		goto exit;
	}
	init_param.oem_handle = oem_handle;
	init_param.ipm_handle = cxt->ipm_cxt.ipm_handle;
	init_param.ops.channel_cfg = camera_channel_cfg;
	init_param.ops.channel_start = camera_channel_start;
	init_param.ops.channel_pause = camera_channel_pause;
	init_param.ops.channel_resume = camera_channel_resume;
	init_param.ops.channel_free_frame = camera_channel_free_frame;
	init_param.ops.channel_stop = camera_channel_stop;
	init_param.ops.isp_start_video = camera_isp_start_video;
	init_param.ops.isp_stop_video = camera_isp_stop_video;
	init_param.ops.start_rot = camera_start_rot;
	init_param.ops.preview_pre_proc = camera_preview_pre_proc;
	init_param.ops.preview_post_proc = camera_preview_post_proc;
	init_param.ops.get_sensor_info = camera_get_sensor_info;
	init_param.ops.get_sensor_autotest_mode = camera_get_sensor_autotest_mode;
	init_param.ops.channel_scale_capability = camera_channel_scale_capability;
	init_param.ops.channel_get_cap_time = camera_channel_get_cap_time;
	init_param.ops.capture_pre_proc = camera_capture_pre_proc;
	init_param.ops.capture_post_proc = camera_capture_post_proc;
	init_param.ops.sensor_open = camera_open_sensor;
	init_param.ops.sensor_close = camera_close_sensor;
	init_param.oem_cb = camera_preview_cb;
/*	init_param.private_data =*/
	init_param.sensor_bits = (1 << cxt->camera_id);
	ret = cmr_preview_init(&init_param, &prev_cxt->preview_handle);
	if (ret) {
		CMR_LOGE("failed to init preview,ret %ld", ret);
		ret = -CMR_CAMERA_NO_SUPPORT;
		goto exit;
	}
	prev_cxt->inited = 1;
	CMR_PRINT_TIME;
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_preview_deinit(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct preview_context          *prev_cxt;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	prev_cxt = &cxt->prev_cxt;
	if (0 == prev_cxt->inited) {
		CMR_LOGD("preview has been de-intialized");
		goto exit;
	}
	CMR_PRINT_TIME;
	ret = cmr_preview_deinit(prev_cxt->preview_handle);
	if (ret) {
		CMR_LOGE("failed to de-init preview %ld", ret);
		goto exit;
	}
	CMR_PRINT_TIME;
	cmr_bzero(prev_cxt, sizeof(*prev_cxt));
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_snapshot_init(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct snapshot_context         *snp_cxt = &cxt->snp_cxt;
	struct snapshot_init_param      init_param;

	CMR_PRINT_TIME;
	if (1 == snp_cxt->inited) {
		CMR_LOGD("snp has been intialized");
		goto exit;
	}
	init_param.id = cxt->camera_id;
	init_param.oem_handle = oem_handle;
	init_param.ipm_handle = cxt->ipm_cxt.ipm_handle;
	init_param.oem_cb = camera_snapshot_cb;
	init_param.ops.start_encode = camera_start_encode;
	init_param.ops.start_decode = camera_start_decode;
	init_param.ops.start_exif_encode = camera_start_exif_encode;
	init_param.ops.start_scale = camera_start_scale;
	init_param.ops.start_rot = camera_start_rot;
	init_param.ops.capture_pre_proc = camera_capture_pre_proc;
	init_param.ops.capture_post_proc = camera_capture_post_proc;
	init_param.ops.raw_proc = camera_raw_proc;
	init_param.ops.isp_start_video = camera_isp_start_video;
	init_param.ops.isp_stop_video = camera_isp_stop_video;
	init_param.ops.channel_start = camera_channel_start;
	init_param.ops.channel_pause = camera_channel_pause;
	init_param.ops.channel_resume = camera_channel_resume;
	init_param.ops.channel_free_frame = camera_channel_free_frame;
	init_param.ops.channel_stop = camera_channel_stop;
	init_param.ops.get_sensor_info = camera_get_sensor_info;
	init_param.ops.stop_codec = camera_stop_codec;
/*	init_param.private_data */
	ret = cmr_snapshot_init(&init_param, &snp_cxt->snapshot_handle);
	if (ret) {
		CMR_LOGE("failed to init snapshot,ret %ld", ret);
		ret = -CMR_CAMERA_NO_SUPPORT;
		goto exit;
	}
	snp_cxt->inited = 1;
	CMR_PRINT_TIME;
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_snapshot_deinit(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct snapshot_context         *snp_cxt;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	snp_cxt = &cxt->snp_cxt;
	if (0 == snp_cxt->inited) {
		CMR_LOGD("snp has been de-intialized");
		goto exit;
	}
	CMR_PRINT_TIME;
	ret = cmr_snapshot_deinit(snp_cxt->snapshot_handle);
	if (ret) {
		CMR_LOGE("failed to de-init snapshot %ld", ret);
		goto exit;
	}
	CMR_PRINT_TIME;
	cmr_bzero(snp_cxt, sizeof(*snp_cxt));
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_ipm_init(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct ipm_context              *ipm_cxt = &cxt->ipm_cxt;
	struct ipm_init_in              init_param;

	if (1 == ipm_cxt->inited) {
		CMR_LOGD("ipm has been intialized");
		goto exit;
	}
	CMR_PRINT_TIME;
	init_param.oem_handle = oem_handle;
	init_param.ipm_sensor_ioctl = camera_sensor_ioctl;
	ret = cmr_ipm_init(&init_param, &ipm_cxt->ipm_handle);
	if (ret) {
		CMR_LOGE("failed to init ipm,ret %ld", ret);
		ret = -CMR_CAMERA_NO_SUPPORT;
		goto exit;
	}
	ipm_cxt->inited = 1;
	CMR_PRINT_TIME;
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_ipm_deinit(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct ipm_context              *ipm_cxt;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	ipm_cxt = &cxt->ipm_cxt;
	if (0 == ipm_cxt->inited) {
		CMR_LOGD("ipm has been de-intialized");
		goto exit;
	}
	CMR_PRINT_TIME;
	ret = cmr_ipm_deinit(ipm_cxt->ipm_handle);
	if (ret) {
		CMR_LOGE("failed to de-init ipm %ld", ret);
		goto exit;
	}
	CMR_PRINT_TIME;
	cmr_bzero(ipm_cxt, sizeof(*ipm_cxt));
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_setting_init(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct setting_context          *setting_cxt = &cxt->setting_cxt;
	struct setting_init_in          init_param;

	CMR_PRINT_TIME;
	if (1 == setting_cxt->inited) {
		CMR_LOGD("setting has been de-intialized");
		goto exit;
	}
	init_param.oem_handle = oem_handle;
	init_param.camera_id_bits = (1 << cxt->camera_id);
	init_param.io_cmd_ioctl = camera_ioctl_for_setting;
	init_param.setting_sn_ioctl = camera_sensor_ioctl;
	init_param.setting_isp_ioctl = camera_isp_ioctl;
	init_param.get_setting_activity = camera_get_setting_activity;
	init_param.before_set_cb = camera_before_set;
	init_param.after_set_cb = camera_after_set;
	ret = cmr_setting_init(&init_param, &setting_cxt->setting_handle);
	if (ret) {
		CMR_LOGE("failed to init setting %ld", ret);
		ret = -CMR_CAMERA_NO_SUPPORT;
		goto exit;
	}
	setting_cxt->inited = 1;
	CMR_PRINT_TIME;
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_setting_deinit(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct setting_context          *setting_cxt;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	setting_cxt = &cxt->setting_cxt;
	if (0 == setting_cxt->inited) {
		CMR_LOGD("setting has been de-intialized");
		goto exit;
	}
	CMR_PRINT_TIME;
	ret = cmr_setting_deinit(setting_cxt->setting_handle);
	if (ret) {
		CMR_LOGE("failed to de-init setting %ld", ret);
		goto exit;
	}
	CMR_PRINT_TIME;
	cmr_bzero(setting_cxt, sizeof(*setting_cxt));
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_focus_init(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct focus_context            *focus_cxt;
	struct af_init_param            init_param;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_PRINT_TIME;
	focus_cxt = &cxt->focus_cxt;
	if (1 == focus_cxt->inited) {
		CMR_LOGD("focus has been intialized");
		goto exit;
	}
	init_param.oem_handle             = oem_handle;
	init_param.evt_cb                 = camera_focus_evt_cb;
	init_param.ops.af_pre_proc        = camera_focus_pre_proc;
	init_param.ops.af_post_proc       = camera_focus_post_proc;
	init_param.ops.af_isp_ioctrl      = camera_isp_ioctl;
	init_param.ops.get_preview_status = camera_get_preview_status;
	init_param.ops.af_sensor_ioctrl   = camera_sensor_ioctl;
	init_param.ops.get_sensor_info    = camera_get_sensor_info;
	ret = cmr_focus_init(&init_param, cxt->camera_id, &focus_cxt->focus_handle);
	if (ret) {
		CMR_LOGE("failed to init focus,ret %ld", ret);
		ret = -CMR_CAMERA_NO_SUPPORT;
		goto exit;
	}
	focus_cxt->inited = 1;
	CMR_PRINT_TIME;
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_focus_deinit(cmr_handle  oem_handle)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct focus_context            *focus_cxt;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	focus_cxt = &cxt->focus_cxt;
	if (0 == focus_cxt->inited) {
		CMR_LOGD("focus has been de-intialized");
		goto exit;
	}
	CMR_PRINT_TIME;
	ret = cmr_focus_deinit(focus_cxt->focus_handle);
	if (ret) {
		CMR_LOGE("failed to de-init focus %ld", ret);
		goto exit;
	}
	CMR_PRINT_TIME;
	cmr_bzero(focus_cxt, sizeof(*focus_cxt));

exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}


cmr_int camera_preview_cb_thread_proc(struct cmr_msg *message, void* data)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)data;
	camera_cb_of_type              callback;

	if (!message || !data) {
		CMR_LOGE("param error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}

	CMR_LOGD("msg_type 0x%x, sub msg type 0x%x client_data 0x%lx", message->msg_type, message->sub_msg_type, (cmr_uint)cxt->client_data);
	callback = cxt->camera_cb;
	CMR_PRINT_TIME;
	if (callback) {
		callback(message->sub_msg_type, cxt->client_data, message->msg_type, message->data);
	} else {
		CMR_LOGE("err, camera cb is null");
		ret = -CMR_CAMERA_INVALID_PARAM;
	}
	CMR_PRINT_TIME;
exit:
	CMR_LOGD("out ret %ld", ret);
	return ret;
}

cmr_int camera_snapshot_cb_thread_proc(struct cmr_msg *message, void* data)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)data;
	camera_cb_of_type              callback;

	if (!message || !data) {
		CMR_LOGE("param error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_LOGD("msg_type 0x%x, sub msg type 0x%x", message->msg_type, message->sub_msg_type);
	CMR_PRINT_TIME;
	callback = cxt->camera_cb;
	if (callback) {
		callback(message->sub_msg_type, cxt->client_data, message->msg_type, message->data);
	} else {
		CMR_LOGE("err, camera cb is null");
		ret = -CMR_CAMERA_INVALID_PARAM;
	}
	CMR_PRINT_TIME;
exit:
	return ret;
}

cmr_int camera_snapshot_secondary_thread_proc(struct cmr_msg *message, void* data)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)data;
	camera_cb_of_type              callback;

	if (!message || !data) {
		CMR_LOGE("param error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_LOGD("msg_type 0x%x, sub msg type 0x%x", message->msg_type, message->sub_msg_type);
	CMR_PRINT_TIME;
	callback = cxt->camera_cb;
	if (callback) {
		callback(message->sub_msg_type, cxt->client_data, message->msg_type, message->data);
	} else {
		CMR_LOGE("err, camera cb is null");
		ret = -CMR_CAMERA_INVALID_PARAM;
	}
	CMR_PRINT_TIME;
exit:
	return ret;
}

cmr_int camera_create_prev_thread(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	ret = cmr_thread_create(&cxt->prev_cb_thr_handle, PREVIEW_MSG_QUEUE_SIZE,
							camera_preview_cb_thread_proc,(void*)oem_handle);

	if (CMR_MSG_SUCCESS != ret) {
		CMR_LOGE("create preview thread fail");
		ret = -CMR_CAMERA_NO_SUPPORT;
	}
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_destroy_prev_thread(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}

	if (cxt->prev_cb_thr_handle) {
		ret = cmr_thread_destroy(cxt->prev_cb_thr_handle);
		if (!ret) {
			cxt->prev_cb_thr_handle = (cmr_handle)0;
		} else {
			CMR_LOGE("failed to destroy prev thr");
		}
	}
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_create_snp_thread(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	ret = cmr_thread_create(&cxt->snp_cb_thr_handle, SNAPSHOT_MSG_QUEUE_SIZE,
							camera_snapshot_cb_thread_proc, (void*)oem_handle);

	if (CMR_MSG_SUCCESS != ret) {
		CMR_LOGE("failed to create snapshot thread %ld", ret);
		ret = -CMR_CAMERA_NO_SUPPORT;
	}
	ret = cmr_thread_create(&cxt->snp_secondary_thr_handle, SNAPSHOT_MSG_QUEUE_SIZE,
							camera_snapshot_secondary_thread_proc, (void*)oem_handle);

	if (CMR_MSG_SUCCESS != ret) {
		CMR_LOGE("failed to create snapshot thread %ld", ret);
		if (cxt->snp_cb_thr_handle) {
			ret = cmr_thread_destroy(cxt->snp_cb_thr_handle);
			if (!ret) {
				cxt->snp_cb_thr_handle = (cmr_handle)0;
			} else {
				CMR_LOGE("failed to destroy snp thr %ld", ret);
			}
		}
		ret = -CMR_CAMERA_NO_SUPPORT;
	}
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_destroy_snp_thread(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}

	if (cxt->snp_cb_thr_handle) {
		ret = cmr_thread_destroy(cxt->snp_cb_thr_handle);
		if (!ret) {
			cxt->snp_cb_thr_handle = (cmr_handle)0;
		} else {
			CMR_LOGE("failed to destroy snp thr %ld", ret);
		}
	}
	if (cxt->snp_secondary_thr_handle) {
		ret = cmr_thread_destroy(cxt->snp_secondary_thr_handle);
		if (!ret) {
			cxt->snp_secondary_thr_handle = (cmr_handle)0;
		} else {
			CMR_LOGE("failed to destroy snp thr %ld", ret);
		}
	}
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_init_thread(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	/*create preview thread*/
	ret = camera_create_prev_thread(oem_handle);
	if (ret) {
		CMR_LOGE("failed to create preview thread");
		ret = -CMR_CAMERA_NO_SUPPORT;
		goto exit;
	}
	/*create snapshot thread*/
	ret = camera_create_snp_thread(oem_handle);
	if (ret) {
		CMR_LOGE("failed to create snapshot thread");
		ret = -CMR_CAMERA_NO_SUPPORT;
		goto prev_thr_destroy;
	} else {
		goto exit;
	}
prev_thr_destroy:
	camera_destroy_prev_thread(oem_handle);
exit:
	return ret;
}

cmr_int camera_deinit_thread(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;

	camera_destroy_prev_thread(oem_handle);
	camera_destroy_snp_thread(oem_handle);
	CMR_LOGI("done %ld", ret);
	return ret;
}
cmr_int camera_init_resource(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
#ifdef OEM_HANDLE_HDR
	sem_init(&cxt->hdr_sync_sm, 0, 0);
#endif
	sem_init(&cxt->hdr_flag_sm, 0, 1);
	sem_init(&cxt->share_path_sm, 0, 0);
	sem_init(&cxt->access_sm, 0, 1);
	return ret;
}

cmr_int camera_destroy_resource(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (cxt->camera_cb) {
		cxt->camera_cb(CAMERA_EXIT_CB_DONE, cxt->client_data, CAMERA_FUNC_STOP, 0);
	} else {
		CMR_LOGE("err, camera_cb is null, don't notify HAL");
	}
#ifdef OEM_HANDLE_HDR
	sem_destroy(&cxt->hdr_sync_sm);
#endif
	sem_destroy(&cxt->hdr_flag_sm);
	sem_destroy(&cxt->share_path_sm);
	sem_destroy(&cxt->access_sm);
	return ret;
}

cmr_int camera_init_internal(cmr_handle  oem_handle, cmr_uint is_autotest)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;

	CMR_PRINT_TIME;
	camera_init_resource(oem_handle);

	ret = camera_ipm_init(oem_handle);
	if (ret) {
		CMR_LOGE("failed to init ipm %ld", ret);
		goto resource_deinit;
	}

	ret = camera_setting_init(oem_handle);
	if (ret) {
		CMR_LOGE("failed to init setting %ld", ret);
		goto ipm_deinit;
	}

	ret = camera_focus_init(oem_handle);
	if (ret) {
		CMR_LOGE("failed to init focus %ld", ret);
		goto setting_deinit;
	}

	ret = camera_jpeg_init(oem_handle);
	if (ret) {
		CMR_LOGE("failed to init jpeg %ld", ret);
		goto focus_deinit;
	}
	ret = camera_sensor_init(oem_handle, is_autotest);
	if (ret) {
		CMR_LOGE("failed to init sensor %ld", ret);
		goto jpeg_deinit;
	}

	ret = camera_v4l2_init(oem_handle);
	if (ret) {
		CMR_LOGE("failed to init v4l2 %ld", ret);
		goto sensor_deinit;
	}

	ret = camera_scaler_init(oem_handle);
	if (ret) {
		CMR_LOGE("failed to init scaler %ld", ret);
		goto v4l2_deinit;
	}

	ret = camera_rotation_init(oem_handle);
	if (ret) {
		CMR_LOGE("failed to init rotation %ld", ret);
		goto scaler_deinit;
	}

	ret = camera_isp_init(oem_handle);
	if (ret) {
		CMR_LOGE("failed to init isp %ld", ret);
		goto rotation_deinit;
	}

	ret= camera_preview_init(oem_handle);
	if (ret) {
		CMR_LOGE("failed to init preview %ld", ret);
		goto isp_deinit;
	}

	ret = camera_snapshot_init(oem_handle);
	if (ret) {
		CMR_LOGE("failed to init snapshot %ld", ret);
		goto preview_deinit;
	} else {
		CMR_LOGI("init mds is ok");
		ret = camera_init_thread(oem_handle);
		if (ret) {
			goto snapshot_deinit;
		} else {
			goto exit;
		}
	}
snapshot_deinit:
	camera_snapshot_deinit(oem_handle);
preview_deinit:
	camera_preview_deinit(oem_handle);
isp_deinit:
	camera_isp_deinit(oem_handle);
rotation_deinit:
	camera_rotation_deinit(oem_handle);
scaler_deinit:
	camera_scaler_deinit(oem_handle);
v4l2_deinit:
	camera_v4l2_deinit(oem_handle);
sensor_deinit:
	camera_sensor_deinit(oem_handle);
focus_deinit:
	camera_focus_deinit(oem_handle);
jpeg_deinit:
	camera_jpeg_deinit(oem_handle);
setting_deinit:
	camera_setting_deinit(oem_handle);
ipm_deinit:
	camera_ipm_deinit(oem_handle);
resource_deinit:
	camera_destroy_resource(oem_handle);
exit:
	CMR_PRINT_TIME;
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_deinit_internal(cmr_handle  oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;

	CMR_PRINT_TIME;
	camera_snapshot_deinit(oem_handle);

	camera_preview_deinit(oem_handle);

	camera_isp_deinit(oem_handle);

	camera_rotation_deinit(oem_handle);

	camera_scaler_deinit(oem_handle);

	camera_v4l2_deinit(oem_handle);

	camera_sensor_deinit(oem_handle);

	camera_jpeg_deinit(oem_handle);

	camera_focus_deinit(oem_handle);

	camera_setting_deinit(oem_handle);

	camera_ipm_deinit(oem_handle);

	camera_deinit_thread(oem_handle);

	camera_destroy_resource(oem_handle);

	CMR_PRINT_TIME;
	return ret;
}

cmr_int camera_start_encode(cmr_handle oem_handle, cmr_handle caller_handle,
                                         struct img_frm *src, struct img_frm *dst, struct cmr_op_mean *mean)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct jpeg_enc_in_param       enc_in_param;
	cmr_uint                       stream_size;

	if (!caller_handle || !oem_handle || !src || !dst || !mean) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_LOGI("src phy addr 0x%lx 0x%lx src vir addr 0x%lx 0x%lx", src->addr_phy.addr_y, src->addr_phy.addr_u,
			 src->addr_vir.addr_y, src->addr_vir.addr_u);
	CMR_LOGI("dst phr addr 0x%lx vir addr 0x%lx", dst->addr_phy.addr_y, dst->addr_vir.addr_y);
	CMR_LOGI("src size %d %d", src->size.width, src->size.height);
	CMR_LOGI("out size %d %d", dst->size.width, dst->size.height);
	CMR_LOGD("caller_handle 0x%lx jpeg handle 0x%lx", (cmr_uint)caller_handle, (cmr_uint)cxt->jpeg_cxt.jpeg_handle);
	CMR_PRINT_TIME;
	enc_in_param.slice_height = mean->slice_height;
	enc_in_param.slice_mod = mean->slice_mode;
	enc_in_param.quality_level = mean->quality_level;
	enc_in_param.stream_buf_phy = dst->addr_phy.addr_y;
	enc_in_param.stream_buf_vir = dst->addr_vir.addr_y;
	enc_in_param.stream_buf_size = dst->buf_size;
	enc_in_param.jpeg_handle = cxt->jpeg_cxt.jpeg_handle;
	enc_in_param.size = src->size;
	enc_in_param.out_size = dst->size;/*actual picture size*/
	enc_in_param.src_addr_phy = src->addr_phy;
	enc_in_param.src_addr_vir = src->addr_vir;
	enc_in_param.src_endian = src->data_end;
	cxt->jpeg_cxt.enc_caller_handle = caller_handle;
	if (1 != mean->is_thumb) {
		ret = jpeg_enc_start(&enc_in_param);
	} else {
		ret = jpeg_enc_thumbnail(&enc_in_param, &stream_size);
		dst->reserved = (void*)stream_size;
	}
	if (ret) {
		cxt->jpeg_cxt.enc_caller_handle = (cmr_handle)0;
		CMR_LOGE("failed to enc start %ld", ret);
	}
	CMR_PRINT_TIME;
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_start_decode(cmr_handle  oem_handle, cmr_handle caller_handle, struct img_frm *src,
                                        struct img_frm *dst, struct cmr_op_mean *mean)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct jpeg_dec_in_param       dec_in_param;

	if (!caller_handle || !oem_handle || !src || !dst || !mean) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_LOGI("src phy addr 0x%lx src vir addr 0x%lx", src->addr_phy.addr_y, src->addr_vir.addr_y);
	CMR_LOGI("dst phr addr 0x%lx  0x%lx vir addr 0x%lx 0x%lx", dst->addr_phy.addr_y, dst->addr_phy.addr_u,
			 dst->addr_vir.addr_y, dst->addr_vir.addr_u);
	CMR_LOGI("src size %d %d", src->size.width, src->size.height);
	CMR_LOGI("out size %d %d", dst->size.width, dst->size.height);
	CMR_LOGI("temp size %d", mean->temp_buf.buf_size);
	CMR_LOGD("caller_handle 0x%lx", (cmr_uint)caller_handle);
	CMR_PRINT_TIME;
	mean->out_param = 0;
	dec_in_param.stream_buf_phy = src->addr_phy.addr_y;
	dec_in_param.stream_buf_vir = src->addr_vir.addr_y;
	dec_in_param.stream_buf_size = (cmr_u32)src->reserved;
	dec_in_param.slice_height = mean->slice_height;
	dec_in_param.slice_mod = mean->slice_mode;
	dec_in_param.dst_fmt = dst->fmt;
	dec_in_param.temp_buf_phy = mean->temp_buf.addr_phy.addr_y;
	dec_in_param.temp_buf_vir = mean->temp_buf.addr_vir.addr_y;
	dec_in_param.temp_buf_size = mean->temp_buf.buf_size;
	dec_in_param.jpeg_handle = cxt->jpeg_cxt.jpeg_handle;
	dec_in_param.size = dst->size;
	dec_in_param.dst_addr_phy = dst->addr_phy;
	dec_in_param.dst_addr_vir = dst->addr_vir;
	dec_in_param.dst_endian = dst->data_end;
	cxt->jpeg_cxt.dec_caller_handle = caller_handle;
	if (1 != mean->is_sync) {
		ret = jpeg_dec_start(&dec_in_param);
	} else {
		struct jpeg_dec_cb_param      out_param;
		ret = jpeg_dec_start_sync(&dec_in_param, &out_param);
		mean->out_param = out_param.data_endian.uv_endian;
	}
    if (ret) {
		cxt->jpeg_cxt.dec_caller_handle = (cmr_handle)0;
		CMR_LOGE("dec start fail ret %ld", ret);
    }
	CMR_PRINT_TIME;
exit:
	CMR_LOGD("done %ld", ret);
	return ret;
}

cmr_int camera_stop_codec(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	ret = jpeg_stop(cxt->jpeg_cxt.jpeg_handle);
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_start_exif_encode(cmr_handle oem_handle, cmr_handle caller_handle, struct img_frm *pic_src,
												struct img_frm *thumb_src, void *exif, struct img_frm *dst, struct jpeg_wexif_cb_param *out_ptr)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct jpeg_enc_exif_param     enc_exif_param;
	struct jpeg_wexif_cb_param     out_pram;
	struct setting_cmd_parameter   setting_param;

	if (!caller_handle || !oem_handle || !pic_src || !dst || !thumb_src || !out_ptr) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_PRINT_TIME;
	enc_exif_param.jpeg_handle = cxt->jpeg_cxt.jpeg_handle;
	enc_exif_param.src_jpeg_addr_virt = pic_src->addr_vir.addr_y;
	enc_exif_param.thumbnail_addr_virt = thumb_src->addr_vir.addr_y;
	enc_exif_param.target_addr_virt = dst->addr_vir.addr_y;
	enc_exif_param.src_jpeg_size = pic_src->buf_size;//pic_src->size.width;
	enc_exif_param.thumbnail_size = thumb_src->buf_size;
	enc_exif_param.target_size = dst->buf_size;
	setting_param.camera_id = cxt->camera_id;
	ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, SETTING_GET_EXIF_INFO, &setting_param);
	enc_exif_param.exif_ptr = setting_param.exif_all_info_ptr;
/*	enc_exif_param.exif_ptr->primary.data_char_ptr = NULL;
	enc_exif_param.exif_ptr->primary.data_struct_ptr = NULL;
	enc_exif_param.exif_ptr->primary.img_desc_ptr = NULL;
	enc_exif_param.exif_ptr->gps_ptr = NULL;
	enc_exif_param.exif_ptr->inter_ptr = NULL;
	enc_exif_param.exif_ptr->spec_ptr = NULL;//wjp*/
	enc_exif_param.exif_isp_info = NULL;
	ret = jpeg_enc_add_eixf(&enc_exif_param, &out_pram);
	if (!ret) {
		*out_ptr = out_pram;
		CMR_LOGI("out addr 0x%lx size %ld", out_ptr->output_buf_virt_addr, out_ptr->output_buf_size);
	} else {
		CMR_LOGE("failed to compund exif jpeg %ld", ret);
	}
	CMR_PRINT_TIME;
exit:
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_start_scale(cmr_handle oem_handle, cmr_handle caller_handle, struct img_frm *src,
                                      struct img_frm *dst, struct cmr_op_mean *mean)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!caller_handle || !oem_handle || !src || !dst || !mean) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_LOGI("caller_handle 0x%lx is_sync %d", (cmr_uint)caller_handle, mean->is_sync);
	CMR_LOGI("src phy addr 0x%lx 0x%lx, dst phy addr 0x%lx 0x%lx",
		src->addr_phy.addr_y,
		src->addr_phy.addr_u,
		dst->addr_phy.addr_y,
		dst->addr_phy.addr_u);

	CMR_LOGI("src size %d %d dst size %d %d rect %d %d %d %d endian %d %d %d %d",
		src->size.width,
		src->size.height,
		dst->size.width,
		dst->size.height,
		src->rect.start_x,
		src->rect.start_y,
		src->rect.width,
		src->rect.height,
		src->data_end.y_endian,
		src->data_end.uv_endian,
		dst->data_end.y_endian,
		dst->data_end.uv_endian);

	CMR_PRINT_TIME;
	if (1 != mean->is_sync) {
		ret = cmr_scale_start(cxt->scaler_cxt.scaler_handle, src, dst, camera_scaler_evt_cb, (void*)caller_handle);
	} else {
		ret = cmr_scale_start(cxt->scaler_cxt.scaler_handle, src, dst, (cmr_evt_cb)NULL, NULL);
	}
	if (ret) {
		CMR_LOGE("failed to start scaler, ret %ld", ret);
	}
	CMR_PRINT_TIME;
exit:
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_start_rot(cmr_handle oem_handle, cmr_handle caller_handle, struct img_frm *src,
                                   struct img_frm *dst, struct cmr_op_mean *mean)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct cmr_rot_param           rot_param;

	if (!caller_handle || !oem_handle || !src || !dst || !mean) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_PRINT_TIME;
	camera_take_snapshot_step(CMR_STEP_ROT_S);
	rot_param.angle = mean->rot;
	rot_param.handle = cxt->rot_cxt.rotation_handle;
	rot_param.src_img = *src;
	rot_param.dst_img = *dst;
	ret = cmr_rot(&rot_param);
	if (ret) {
		CMR_LOGE("failed to rotate %ld", ret);
	}
	camera_take_snapshot_step(CMR_STEP_ROT_E);
	CMR_PRINT_TIME;
exit:
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_preview_pre_proc(cmr_handle oem_handle, cmr_u32 camera_id, cmr_u32 preview_sn_mode)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct setting_cmd_parameter   setting_param;
	cmr_u32                        sensor_mode;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_LOGI("camera id %d", camera_id);

	/*
	if (CAMERA_ZSL_MODE != cxt->snp_cxt.snp_mode) {
		sensor_mode = cxt->prev_cxt.preview_sn_mode;
	} else {
		sensor_mode = cxt->snp_cxt.snapshot_sn_mode;
	} */
	sensor_mode = preview_sn_mode;
	cxt->prev_cxt.preview_sn_mode = preview_sn_mode;

	CMR_LOGI("sensor work mode %d", sensor_mode);
	CMR_PRINT_TIME;
	ret = cmr_sensor_set_mode(cxt->sn_cxt.sensor_handle, camera_id, sensor_mode);
	if (ret) {
		CMR_LOGE("failed to set sensor work mode %ld", ret);
		goto exit;
	}
	ret = cmr_sensor_set_mode_done(cxt->sn_cxt.sensor_handle, camera_id);
	if (ret) {
		CMR_LOGE("failed to wait done %ld", ret);
		goto exit;
	}
	CMR_PRINT_TIME;

exit:
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_preview_post_proc(cmr_handle oem_handle, cmr_u32 camera_id)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
exit:
	return ret;
}

cmr_int camera_capture_pre_proc(cmr_handle oem_handle, cmr_u32 camera_id, cmr_u32 preview_mode, cmr_u32 capture_mode, cmr_u32 is_restart)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct snapshot_context        *snp_cxt;
	struct setting_cmd_parameter   setting_param;

	CMR_PRINT_TIME;
	if (!oem_handle || (camera_id != cxt->camera_id)) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	snp_cxt = &cxt->snp_cxt;

	CMR_LOGI("camera id %d, capture mode %d preview mode %d is_restart %d snapshot_sn_mode %d",
			camera_id, capture_mode, preview_mode, is_restart, snp_cxt->snp_mode);

	if ((CAMERA_ZSL_MODE != snp_cxt->snp_mode) && (!is_restart) && (1 != camera_get_hdr_flag(cxt))) {
		/*open flash*/
		setting_param.ctrl_flash.capture_mode.capture_mode= capture_mode;
		setting_param.camera_id = camera_id;
		setting_param.ctrl_flash.is_active = 1;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, SETTING_CTRL_FLASH, &setting_param);
		if (ret) {
			CMR_LOGE("failed to open flash");
		}
	}
	if ((CAMERA_ZSL_MODE != snp_cxt->snp_mode) && (!is_restart)) {
		snp_cxt = &cxt->snp_cxt;
		snp_cxt->snapshot_sn_mode = capture_mode;
		cxt->prev_cxt.preview_sn_mode = preview_mode;
		/*set sensor before snapshot*/
		ret = cmr_sensor_ioctl(cxt->sn_cxt.sensor_handle, camera_id, SENSOR_BEFORE_SNAPSHOT,
			                  (capture_mode | (cxt->prev_cxt.preview_sn_mode << CAP_MODE_BITS)));
		if (ret) {
			CMR_LOGE("failed to set sensor %ld", ret);
		}
	}

	if (1 == camera_get_hdr_flag(cxt)) {
		ret = cmr_ipm_pre_proc(cxt->ipm_cxt.hdr_handle);
		if (ret) {
			CMR_LOGE("failed to ipm pre proc, %ld", ret);
		}
	}
exit:
	CMR_PRINT_TIME;
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_capture_post_proc(cmr_handle oem_handle, cmr_u32 camera_id)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct snapshot_context        *snp_cxt;
	struct setting_cmd_parameter   setting_param;

	CMR_PRINT_TIME;
	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	snp_cxt = &cxt->snp_cxt;
	/*close flash*/
	setting_param.ctrl_flash.capture_mode.capture_mode= snp_cxt->snapshot_sn_mode;
	setting_param.ctrl_flash.is_active = 0;
	ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, SETTING_CTRL_FLASH, &setting_param);
	if (ret) {
		CMR_LOGE("failed to open flash");
	}
	/*set sensor after snapshot*/
	if ((CAMERA_ZSL_MODE != snp_cxt->snp_mode) && (CAMERA_ISP_TUNING_MODE != snp_cxt->snp_mode) ) {
		ret = cmr_sensor_ioctl(cxt->sn_cxt.sensor_handle, cxt->camera_id, SENSOR_AFTER_SNAPSHOT,
			               cxt->prev_cxt.preview_sn_mode);
		if (ret) {
			CMR_LOGE("failed to set sensor %ld", ret);
		}
	}
exit:
	CMR_PRINT_TIME;
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_open_sensor(cmr_handle oem_handle, cmr_u32 camera_id)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct sensor_context           *sn_cxt = &cxt->sn_cxt;
	cmr_handle                      sensor_handle;
	cmr_u32                         camera_id_bits = 0;

	CMR_PRINT_TIME;
	sensor_handle  = sn_cxt->sensor_handle;
	camera_id_bits = 1 << cxt->camera_id;
	ret = cmr_sensor_open(sensor_handle, camera_id_bits);
	if (ret) {
		CMR_LOGE("open %d sensor failed %ld", cxt->camera_id, ret);
		return CMR_CAMERA_FAIL;
	}
	CMR_PRINT_TIME;

	return ret;
}

cmr_int camera_close_sensor(cmr_handle oem_handle, cmr_u32 camera_id)
{
	cmr_int                         ret = CMR_CAMERA_SUCCESS;
	struct camera_context           *cxt = (struct camera_context*)oem_handle;
	struct sensor_context           *sn_cxt = &cxt->sn_cxt;
	cmr_handle                      sensor_handle;
	cmr_u32                         camera_id_bits = 0;

	CMR_PRINT_TIME;
	sensor_handle  = sn_cxt->sensor_handle;
	camera_id_bits = 1 << cxt->camera_id;
	ret = cmr_sensor_close(sensor_handle, camera_id_bits);
	if (ret) {
		CMR_LOGE("open %d sensor failed %ld", cxt->camera_id, ret);
		return CMR_CAMERA_FAIL;
	}
	CMR_PRINT_TIME;

	return ret;
}

cmr_int camera_raw_proc(cmr_handle oem_handle, cmr_handle caller_handle, struct raw_proc_param *param_ptr)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle || !param_ptr || !caller_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}

	CMR_LOGI("slice num %d avail height %d slice height %d", param_ptr->slice_num, param_ptr->src_avail_height,
		     param_ptr->src_slice_height);
	CMR_LOGI("src addr 0x%lx 0x%lx,dst addr 0x%lx 0x%lx", param_ptr->src_frame.addr_phy.addr_y,
			 param_ptr->src_frame.addr_phy.addr_u, param_ptr->dst_frame.addr_phy.addr_y,
			 param_ptr->dst_frame.addr_phy.addr_u);

	if (1 == param_ptr->slice_num) {
		struct ips_in_param  in_param;
		struct ips_out_param out_param;
		in_param.src_frame.img_fmt = param_ptr->src_frame.fmt;
		in_param.src_frame.img_size.w = param_ptr->src_frame.size.width;
		in_param.src_frame.img_size.h = param_ptr->src_frame.size.height;
		in_param.src_frame.img_addr_phy.chn0 = param_ptr->src_frame.addr_phy.addr_y;
		in_param.src_frame.img_addr_phy.chn1 = param_ptr->src_frame.addr_phy.addr_u;
		in_param.src_frame.img_addr_vir.chn0 = param_ptr->src_frame.addr_vir.addr_y;
		in_param.src_frame.img_addr_vir.chn1 = param_ptr->src_frame.addr_vir.addr_u;
		in_param.src_avail_height = param_ptr->src_avail_height;
		in_param.src_slice_height = param_ptr->src_slice_height;
		in_param.dst_frame.img_fmt = param_ptr->dst_frame.fmt;
		in_param.dst_frame.img_size.w = param_ptr->dst_frame.size.width;
		in_param.dst_frame.img_size.h = param_ptr->dst_frame.size.height;
		in_param.dst_frame.img_addr_phy.chn0 = param_ptr->dst_frame.addr_phy.addr_y;
		in_param.dst_frame.img_addr_phy.chn1 = param_ptr->dst_frame.addr_phy.addr_u;
		in_param.dst_frame.img_addr_vir.chn0 = param_ptr->dst_frame.addr_vir.addr_y;
		in_param.dst_frame.img_addr_vir.chn1 = param_ptr->dst_frame.addr_vir.addr_u;
		in_param.dst_slice_height = param_ptr->dst_slice_height;
		ret = isp_proc_start(&in_param, &out_param);
		if (ret) {
			CMR_LOGE("failed to start proc %ld", ret);
		}
	} else {
		struct ipn_in_param in_param;
		struct ips_out_param out_param;
		in_param.src_avail_height = param_ptr->src_avail_height;
		in_param.src_slice_height = param_ptr->src_slice_height;
		in_param.src_addr_phy.chn0 = param_ptr->src_frame.addr_phy.addr_y;
		in_param.src_addr_phy.chn1 = param_ptr->src_frame.addr_phy.addr_u;
		in_param.dst_addr_phy.chn0 = param_ptr->dst_frame.addr_phy.addr_y;
		in_param.dst_addr_phy.chn1 = param_ptr->dst_frame.addr_phy.addr_u;
		ret = isp_proc_next(&in_param, &out_param);
		if (ret) {
			CMR_LOGE("failed to start proc %ld", ret);
		}
	}
	if (CMR_CAMERA_SUCCESS == ret) {
		cxt->isp_cxt.caller_handle = caller_handle;
		CMR_LOGD("caller handle 0x%lx", (cmr_uint)caller_handle);
	}
exit:
	return ret;
}

cmr_int camera_isp_start_video(cmr_handle oem_handle, struct video_start_param *param_ptr)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct isp_context             *isp_cxt = &cxt->isp_cxt;
	struct isp_video_start         isp_param;

	if (!param_ptr || !oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	isp_param.size.w = param_ptr->size.width;
	isp_param.size.h = param_ptr->size.height;
	isp_param.format = ISP_DATA_NORMAL_RAW10;
	isp_param.mode = param_ptr->video_mode;

	CMR_LOGI("isp w h, %d %d", isp_param.size.w, isp_param.size.h);
	CMR_PRINT_TIME;
	ret = isp_video_start(&isp_param);
	if (!ret) {
		isp_cxt->is_work = 1;
	} else {
		isp_cxt->is_work = 0;
		CMR_LOGE("failed to start isp, ret %ld", ret);
	}
	CMR_PRINT_TIME;
exit:
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_isp_stop_video(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	if (cxt->isp_cxt.is_work) {
	CMR_PRINT_TIME;
		ret = isp_video_stop();
		if (ret) {
			CMR_LOGE("failed to stop isp %ld", ret);
		} else {
			cxt->isp_cxt.is_work = 0;
		}
	CMR_PRINT_TIME;
	}
exit:
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_channel_cfg(cmr_handle oem_handle, cmr_handle caller_handle, cmr_u32 camera_id,
										struct channel_start_param *param_ptr, cmr_u32 *channel_id)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct sensor_context          *sn_cxt = &cxt->sn_cxt;
	struct sensor_exp_info         sensor_info;
	struct sensor_mode_info        *sensor_mode_info;
	struct sn_cfg                  sensor_cfg;

	CMR_PRINT_TIME;
	if (!oem_handle || !caller_handle || !param_ptr || !channel_id) {
		CMR_LOGE("in parm error 0x%lx 0x%lx 0x%lx 0x%lx", (cmr_uint)oem_handle, (cmr_uint)caller_handle,
				(cmr_uint)param_ptr, (cmr_uint)channel_id);
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}

	if (param_ptr->is_lightly) {
		ret = cmr_v4l2_cap_cfg_lightly(cxt->v4l2_cxt.v4l2_handle, &param_ptr->cap_inf_cfg, *channel_id);
		if (ret) {
			CMR_LOGE("failed to cap cfg %ld", ret);
			goto exit;
		}
		return ret;
	}

	ret = cmr_v4l2_if_cfg(cxt->v4l2_cxt.v4l2_handle, &param_ptr->sn_if);
	if (ret) {
		CMR_LOGE("failed interface cfg %ld", ret);
		goto exit;
	}
	ret = cmr_sensor_get_info(sn_cxt->sensor_handle, camera_id, &sensor_info);
	if (ret) {
		CMR_LOGE("failed to get sensor info %ld", ret);
		goto exit;
	}

	CMR_LOGI("frm_num 0x%x", param_ptr->frm_num);

	sensor_mode_info = &sensor_info.mode_info[param_ptr->sensor_mode];
	sensor_cfg.sn_size.width = sensor_mode_info->width;
	sensor_cfg.sn_size.height = sensor_mode_info->height;
	sensor_cfg.frm_num = param_ptr->frm_num;
	sensor_cfg.sn_trim.start_x = sensor_mode_info->trim_start_x;
	sensor_cfg.sn_trim.start_y = sensor_mode_info->trim_start_y;
	sensor_cfg.sn_trim.width = sensor_mode_info->trim_width;
	sensor_cfg.sn_trim.height = sensor_mode_info->trim_height;
	ret = cmr_v4l2_sn_cfg(cxt->v4l2_cxt.v4l2_handle, &sensor_cfg);
	if (ret) {
		CMR_LOGE("failed to sn cfg %ld", ret);
		goto exit;
	}
	if (caller_handle == cxt->prev_cxt.preview_handle) {
		cxt->prev_cxt.rect = param_ptr->cap_inf_cfg.cfg.src_img_rect;
		CMR_LOGI("prev rect %d %d %d %d", cxt->prev_cxt.rect.start_x, cxt->prev_cxt.rect.start_y,
				  cxt->prev_cxt.rect.width, cxt->prev_cxt.rect.height);
	}
	ret = cmr_v4l2_cap_cfg(cxt->v4l2_cxt.v4l2_handle, &param_ptr->cap_inf_cfg, channel_id);
	if (ret) {
		CMR_LOGE("failed to cap cfg %ld", ret);
		goto exit;
	}

	param_ptr->buffer.channel_id = *channel_id; /*update the channel id*/
	ret = cmr_v4l2_buff_cfg(cxt->v4l2_cxt.v4l2_handle, &param_ptr->buffer);
	if (ret) {
		CMR_LOGE("failed to buf cfg %ld", ret);
		goto exit;
	}
	cxt->v4l2_cxt.caller_handle[*channel_id] = caller_handle;
	cxt->v4l2_cxt.skip_number[*channel_id] = param_ptr->skip_num;
	CMR_LOGI("channel id %d, caller_handle 0x%lx, skip num %d",
			  *channel_id, (cmr_uint)caller_handle, param_ptr->skip_num);
exit:
	CMR_PRINT_TIME;
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_channel_start(cmr_handle oem_handle, cmr_u32 channel_bits, cmr_uint skip_number)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	CMR_PRINT_TIME;
	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_LOGI("skip num %ld", skip_number);
	camera_take_snapshot_step(CMR_STEP_CAP_S);
	ret = cmr_v4l2_cap_start(cxt->v4l2_cxt.v4l2_handle, skip_number);
	if (ret) {
		CMR_LOGE("failed to start cap %ld", ret);
		goto exit;
	}
exit:
	CMR_PRINT_TIME;
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_channel_pause(cmr_handle oem_handle, cmr_uint channel_id, cmr_u32 reconfig_flag)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	CMR_PRINT_TIME;
	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_LOGI("channel id %ld, reconfig flag %d", channel_id, reconfig_flag);
	ret = cmr_v4l2_cap_pause(cxt->v4l2_cxt.v4l2_handle, channel_id, reconfig_flag);
	if (ret) {
		CMR_LOGE("failed to pause channel %ld", ret);
		goto exit;
	}
exit:
	if (TAKE_PICTURE_NEEDED == camera_get_snp_req((cmr_handle)cxt)) {
		camera_post_share_path_available(oem_handle);
	}
	CMR_PRINT_TIME;
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_channel_resume(cmr_handle oem_handle, cmr_uint channel_id, cmr_u32 skip_number,
	                                          cmr_u32 deci_factor, cmr_u32 frm_num)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	CMR_PRINT_TIME;
	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_LOGI("channel id %ld, skip num %d, deci %d, frm num %d",
		      channel_id, skip_number,deci_factor,frm_num);
	camera_set_discard_frame(cxt, 0);
	ret = cmr_v4l2_cap_resume(cxt->v4l2_cxt.v4l2_handle, channel_id, skip_number, deci_factor, frm_num);
	if (ret) {
		CMR_LOGE("failed to resume channel,ret %ld", ret);
		goto exit;
	}
exit:
	CMR_PRINT_TIME;
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_channel_free_frame(cmr_handle oem_handle, cmr_u32 channel_id, cmr_u32 index)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	ret = cmr_v4l2_free_frame(cxt->v4l2_cxt.v4l2_handle, channel_id, index);
	if (ret) {
		CMR_LOGE("failed to free frame %d %ld", channel_id, ret);
		goto exit;
	}
exit:
	return ret;
}

cmr_int camera_channel_stop(cmr_handle oem_handle, cmr_u32 channel_bits)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_PRINT_TIME;
	ret = cmr_v4l2_cap_stop(cxt->v4l2_cxt.v4l2_handle);
	if (ret) {
		CMR_LOGE("failed to stop channel %ld", ret);
		goto exit;
	}
exit:
	CMR_PRINT_TIME;
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_channel_scale_capability(cmr_handle oem_handle, cmr_u32 *width, cmr_u32 *sc_factor, cmr_u32 *sc_threshold)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle || !width | !sc_factor) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	ret = cmr_v4l2_scale_capability(cxt->v4l2_cxt.v4l2_handle, width, sc_factor, sc_threshold);
exit:
	return ret;
}

cmr_int camera_channel_get_cap_time(cmr_handle oem_handle, cmr_u32 *sec, cmr_u32 *usec)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle || !sec | !usec) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	ret = cmr_v4l2_get_cap_time(cxt->v4l2_cxt.v4l2_handle, sec, usec);
exit:
	return ret;
}

cmr_int camera_get_sensor_info(cmr_handle oem_handle, cmr_uint sensor_id, struct sensor_exp_info *exp_info_ptr)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle || !exp_info_ptr) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_LOGD("sensor id %ld", sensor_id);
	ret = cmr_sensor_get_info(cxt->sn_cxt.sensor_handle, sensor_id, exp_info_ptr);
	if (ret) {
		CMR_LOGE("failed to get sensor info %ld", ret);
		goto exit;
	}
exit:
	return ret;
}

cmr_int camera_get_sensor_autotest_mode(cmr_handle oem_handle, cmr_uint sensor_id, cmr_uint *is_autotest)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle || !is_autotest) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_LOGI("sensor id %ld", sensor_id);
	ret = cmr_sensor_get_autotest_mode(cxt->sn_cxt.sensor_handle, sensor_id, is_autotest);
	if (ret) {
		CMR_LOGE("failed to get sensor info %ld", ret);
		goto exit;
	}
exit:
	CMR_LOGI("ret %ld, is_autotest %ld", ret, *is_autotest);
	return ret;
}

cmr_int camera_ioctl_for_setting(cmr_handle oem_handle, cmr_uint cmd_type, struct setting_io_parameter *param_ptr)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	cmr_handle                     v4l2_handle = cxt->v4l2_cxt.v4l2_handle;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}

	switch (cmd_type) {
	case SETTING_IO_GET_CAPTURE_SIZE:
		param_ptr->size_param = cxt->snp_cxt.post_proc_setting.snp_size;
		break;
	case SETTING_IO_GET_ACTUAL_CAPTURE_SIZE:
		param_ptr->size_param = cxt->snp_cxt.post_proc_setting.actual_snp_size;
		break;
	case SETTING_IO_CTRL_FLASH:
		cmr_v4l2_flash_cb(v4l2_handle, param_ptr->cmd_value);
		break;
	case SETTING_IO_GET_PREVIEW_MODE:
		param_ptr->cmd_value = cxt->prev_cxt.preview_sn_mode;
		break;
	default:
		CMR_LOGE("don't support cmd %ld", cmd_type);
		ret = CMR_CAMERA_NO_SUPPORT;
		break;
	}
exit:
	return ret;
}

cmr_int camera_sensor_ioctl(cmr_handle oem_handle, cmr_uint cmd_type, struct common_sn_cmd_parameter *param_ptr)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	cmr_uint                       cmd = SENSOR_CMD_MAX;
	cmr_uint                       sensor_param = 0;
	cmr_uint                       set_exif_flag = 0;
	SENSOR_EXIF_CTRL_E             exif_cmd;


	if (!oem_handle || !param_ptr) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	CMR_LOGE("cmd_type =%ld",cmd_type);

	switch (cmd_type) {
	case COM_SN_GET_AUTO_FLASH_STATE:
		cmd = SENSOR_CHECK_NEED_FLASH;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_BRIGHTNESS:
		cmd = SENSOR_BRIGHTNESS;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_BRIGHTNESSVALUE;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_CONTRAST:
		cmd = SENSOR_CONTRAST;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_CONTRAST;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_SATURATION:
		cmd = SENSOR_SATURATION;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_SATURATION;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_SHARPNESS:
		cmd = SENSOR_SHARPNESS;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_SHARPNESS;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_IMAGE_EFFECT:
		cmd = SENSOR_IMAGE_EFFECT;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_EXPOSURE_COMPENSATION:
		cmd = SENSOR_EXPOSURE_COMPENSATION;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_EXPOSURE_LEVEL:
		cmd = SENSOR_WRITE_EV;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_WB_MODE:
		cmd = SENSOR_SET_WB_MODE;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_LIGHTSOURCE;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_PREVIEW_MODE:
//		cmd = ??;
//		sensor_param = param_ptr->cmd_value;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_SCENECAPTURETYPE;
		break;
	case COM_SN_SET_ANTI_BANDING:
		cmd = SENSOR_FLIP;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_ISO:
		cmd = SENSOR_ISO;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_ISOSPEEDRATINGS;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_VIDEO_MODE:
		cmd = SENSOR_VIDEO_MODE;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_BEFORE_SNAPSHOT:
		cmd = SENSOR_BEFORE_SNAPSHOT;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_AFTER_SNAPSHOT:
		cmd = SENSOR_AFTER_SNAPSHOT;
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_EXT_FUNC:
		break;
	case COM_SN_SET_AE_ENABLE://don't support
		break;
	case COM_SN_SET_EXIF_FOCUS:
		sensor_param = param_ptr->cmd_value;
		break;
	case COM_SN_SET_FOCUS:
		cmd = SENSOR_FOCUS;
		sensor_param = (cmr_uint)&(param_ptr->yuv_sn_af_param);
		CMR_LOGE("COM_SN_SET_FOCUS cmd =%ld ",cmd);
		break;
	case COM_SN_GET_PREVIEW_MODE:
		break;
	case COM_SN_GET_CAPTURE_MODE:
		break;
	case COM_SN_GET_SENSOR_ID:
		break;
	case COM_SN_GET_VIDEO_MODE:
		break;
	case COM_SN_GET_EXIF_IMAGE_INFO:
		ret = cmr_sensor_get_exif(cxt->sn_cxt.sensor_handle, cxt->camera_id, &param_ptr->exif_pic_info);
		if (ret) {
			CMR_LOGE("sn get exif image info failed!");
		}
		return ret;
	case COM_SN_SET_HDR_EV:
	{
		SENSOR_EXT_FUN_PARAM_T hdr_ev_param;
		cmd = SENSOR_SET_HDR_EV;
		hdr_ev_param.cmd = SENSOR_EXT_EV;
		hdr_ev_param.param = param_ptr->cmd_value;
		sensor_param = (cmr_uint)&hdr_ev_param;
		break;
	}
	case COM_SN_GET_INFO:
		ret = cmr_sensor_get_info(cxt->sn_cxt.sensor_handle, cxt->camera_id, &param_ptr->sensor_static_info);
		if (ret) {
			CMR_LOGE("sn get info failed!");
		}
		return ret;
	default:
		CMR_LOGE("don't support cmd %ld", cmd_type);
		ret = CMR_CAMERA_NO_SUPPORT;
		break;
	}
	if (!ret) {
		ret = cmr_sensor_ioctl(cxt->sn_cxt.sensor_handle, cxt->camera_id, cmd, sensor_param);//to do
		if (ret) {
			CMR_LOGE("failed to sn ioctrl %ld", ret);
		}

		if (set_exif_flag) {
			CMR_LOGD("ERIC set exif");
			cmr_sensor_set_exif(cxt->sn_cxt.sensor_handle, cxt->camera_id, exif_cmd, sensor_param);
		}
	}
exit:
	return ret;
}

cmr_int camera_isp_ioctl(cmr_handle oem_handle, cmr_uint cmd_type, struct common_isp_cmd_parameter *param_ptr)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	cmr_u32                        isp_cmd = ISP_CTRL_MAX;
	cmr_u32                        isp_param = 0;
	void                           *isp_param_ptr = NULL;
	cmr_u32                        ptr_flag = 0;
	cmr_uint                       set_exif_flag = 0;
	SENSOR_EXIF_CTRL_E             exif_cmd;

	if (!oem_handle || !param_ptr) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}

	switch (cmd_type) {
	case COM_ISP_SET_AE_MODE:
		isp_cmd = ISP_CTRL_AE_MODE;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_SCENECAPTURETYPE;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("ae mode %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_AE_MEASURE_LUM:
		isp_cmd = ISP_CTRL_AE_MEASURE_LUM;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("aw measure lum %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_AE_METERING_AREA:
		isp_cmd = ISP_CTRL_AE_TOUCH;
		isp_param = param_ptr->cmd_value;
		break;
	case COM_ISP_SET_BRIGHTNESS:
		isp_cmd = ISP_CTRL_BRIGHTNESS;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_BRIGHTNESSVALUE;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("brightness %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_CONTRAST:
		isp_cmd = ISP_CTRL_CONTRAST;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_CONTRAST;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("contrast %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_SATURATION:
		isp_cmd = ISP_CTRL_SATURATION;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_SATURATION;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("saturation %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_SHARPNESS:
		isp_cmd = ISP_CTRL_SHARPNESS;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_SHARPNESS;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("sharpness %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_SPECIAL_EFFECT:
		isp_cmd = ISP_CTRL_SPECIAL_EFFECT;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("effect %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_EV:
		isp_cmd = ISP_CTRL_EV;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("ev %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_AWB_MODE:
		isp_cmd = ISP_CTRL_AWB_MODE;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_LIGHTSOURCE;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("awb mode %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_ANTI_BANDING:
		isp_cmd = ISP_CTRL_FLICKER;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("flicker %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_ISO:
		isp_cmd = ISP_CTRL_ISO;
		set_exif_flag = 1;
		exif_cmd = SENSOR_EXIF_CTRL_ISOSPEEDRATINGS;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("iso %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_VIDEO_MODE:
		isp_cmd = ISP_CTRL_VIDEO_MODE;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("isp video mode %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_FLASH_EG:
		isp_cmd = ISP_CTRL_FLASH_EG;
		isp_param = param_ptr->cmd_value;
		break;
	case COM_ISP_GET_EXIF_IMAGE_INFO:
		isp_cmd = ISP_CTRL_GET_EXIF_INFO;
		ptr_flag = 1;
		isp_param_ptr = (void*)&param_ptr->exif_pic_info;
		break;
	case COM_ISP_SET_AF:
		isp_cmd = ISP_CTRL_AF;
		ptr_flag = 1;
		isp_param_ptr = (void*)&param_ptr->af_param;
		CMR_LOGI("isp_cmd =%d", isp_cmd);
		break;
	case COM_ISP_SET_AF_MODE:
		isp_cmd = ISP_CTRL_AF_MODE;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("af mode %d", param_ptr->cmd_value);
		break;
	case COM_ISP_SET_AF_STOP:
		isp_cmd = ISP_CTRL_AF_STOP;
		isp_param = param_ptr->cmd_value;
		CMR_LOGI("isp_cmd =%d af mode %d", isp_cmd,param_ptr->cmd_value);
		break;
	case COM_ISP_GET_LOW_LUX_EB:
		isp_cmd = ISP_LOW_LUX_EB;
		isp_param_ptr = (void*)&param_ptr->cmd_value;
		ret = isp_capability(isp_cmd, isp_param_ptr);
		if (ret) {
			CMR_LOGE("Failed to read isp capability ret = %ld", ret);
		}
		return ret;
	default:
		CMR_LOGE("don't support cmd %ld", cmd_type);
		ret = CMR_CAMERA_NO_SUPPORT;
		break;
	}

	if (ptr_flag) {
		ret = isp_ioctl(isp_cmd, isp_param_ptr);
		if (ret) {
			CMR_LOGE("failed isp ioctl %ld", ret);
		}
		CMR_LOGI("done %ld and direct return", ret);
		return ret;
	}

	ret = isp_ioctl(isp_cmd, (void*)&isp_param);
	if (ret) {
		CMR_LOGE("failed isp ioctl %ld", ret);
	}

	if (set_exif_flag) {
		CMR_LOGD("ERIC set exif");
		cmr_sensor_set_exif(cxt->sn_cxt.sensor_handle, cxt->camera_id, exif_cmd, isp_param);
	}
exit:
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_get_setting_activity(cmr_handle oem_handle, cmr_uint *is_active)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle || !is_active) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	*is_active = cxt->setting_cxt.is_active;
	CMR_LOGI("setting active %ld", (cmr_uint)*is_active);
exit:
	return ret;
}

cmr_int camera_get_preview_param(cmr_handle oem_handle, enum takepicture_mode mode,
	                                               cmr_uint is_snapshot, struct preview_param *out_param_ptr)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct preview_context         *prev_cxt = &cxt->prev_cxt;
	struct setting_context         *setting_cxt = &cxt->setting_cxt;
	struct jpeg_context            *jpeg_cxt = &cxt->jpeg_cxt;
	struct snapshot_context        *snp_cxt = &cxt->snp_cxt;
	struct setting_cmd_parameter   setting_param;
	cmr_u32                        is_cfg_snp = 0;

	CMR_PRINT_TIME;
	setting_param.camera_id = cxt->camera_id;
	cmr_bzero((void*)out_param_ptr, sizeof(*out_param_ptr));

	out_param_ptr->memory_setting.alloc_mem = camera_malloc;
	out_param_ptr->memory_setting.free_mem = camera_free;

	if (CAMERA_SNAPSHOT != is_snapshot) {
		ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_PREVIEW_FORMAT, &setting_param);
		if (ret) {
			CMR_LOGE("failed to get preview fmt %ld", ret);
			goto exit;
		}
		out_param_ptr->preview_fmt = setting_param.cmd_type_value;
		out_param_ptr->is_fd_on = cxt->fd_on_off;
		out_param_ptr->preview_eb = 1;
		out_param_ptr->is_support_fd = cxt->is_support_fd;
		/*get prev rot*/
		ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_PREVIEW_ANGLE, &setting_param);
		if (ret) {
			CMR_LOGE("failed to get preview angle %ld", ret);
			goto exit;
		}
		out_param_ptr->prev_rot = setting_param.cmd_type_value;
		/*get prev size*/
		ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_PREVIEW_SIZE, &setting_param);
		if (ret) {
			CMR_LOGE("failed to get prev size %ld", ret);
			goto exit;
		}
		prev_cxt->size = setting_param.size_param;
		out_param_ptr->preview_size = prev_cxt->size;
		/*get video size*/
		ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_VIDEO_SIZE, &setting_param);
		if (ret) {
			CMR_LOGE("failed to get video size %ld", ret);
			goto exit;
		}
		prev_cxt->video_size = setting_param.size_param;
		out_param_ptr->video_size = prev_cxt->video_size;
		if ((0 != out_param_ptr->video_size.width) && (0 != out_param_ptr->video_size.height)) {
			out_param_ptr->video_eb = 1;
		}
		if (CAMERA_ZSL_MODE == mode) {
			is_cfg_snp = 1;
		}
	} else {
		is_cfg_snp = 1;
	}
	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_CAPTURE_FORMAT, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get cap fmt %ld", ret);
		goto exit;
	}
	out_param_ptr->cap_fmt = setting_param.cmd_type_value;
	/*get zoom param*/
	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_ZOOM_PARAM, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get zoom param %ld", ret);
		goto exit;
	}
	out_param_ptr->zoom_setting = setting_param.zoom_param;
	cxt->snp_cxt.snp_mode = mode;
	if (0 == is_cfg_snp) {
		CMR_LOGI("don't need cfg snp");
		goto exit;
	}
	out_param_ptr->snapshot_eb = is_cfg_snp;

	out_param_ptr->isp_width_limit = cxt->isp_cxt.width_limit;
	out_param_ptr->tool_eb = 0;
	if (CAMERA_ISP_TUNING_MODE == mode || CAMERA_UTEST_MODE == mode || CAMERA_AUTOTEST_MODE == mode) {
		out_param_ptr->tool_eb = 1;
	}
	/*get snapshot size*/
	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_CAPTURE_SIZE, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get capture size %ld", ret);
		goto exit;
	}
	out_param_ptr->picture_size = setting_param.size_param;
	cxt->snp_cxt.request_size = setting_param.size_param;
	/*get snapshot angle*/
	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_CAPTURE_ANGLE, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get capture rot %ld", ret);
		goto exit;
	}
	out_param_ptr->cap_rot = setting_param.cmd_type_value;
	/*get rotation snapshot cfg*/
	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_ROTATION_CAPTURE, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get capture rot %ld", ret);
		goto exit;
	}
	out_param_ptr->is_cfg_rot_cap = setting_param.cmd_type_value;
	cxt->snp_cxt.is_cfg_rot_cap = out_param_ptr->is_cfg_rot_cap;

/*	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_ENVIRONMENT, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get environment %d", ret);
		goto exit;
	}
	out_param_ptr->is_dv = setting_param.cmd_type_value;*/ //to do for robert
	/*get hdr flag*/
	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_HDR, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get envir %ld", ret);
		goto exit;
	}
	camera_set_hdr_flag(cxt, setting_param.cmd_type_value);
	out_param_ptr->is_hdr = setting_param.cmd_type_value;
	/*get android zsl flag*/
	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_ANDROID_ZSL_FLAG, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get android zsl %ld", ret);
		goto exit;
	}
	cxt->is_android_zsl = setting_param.cmd_type_value;
	/*get jpeg param*/
	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_JPEG_QUALITY, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get image quality %ld", ret);
		goto exit;
	}
	jpeg_cxt->param.quality = setting_param.cmd_type_value;

	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_THUMB_QUALITY, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get thumb quality %ld", ret);
		goto exit;
	}
	jpeg_cxt->param.thumb_quality = setting_param.cmd_type_value;

	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_THUMB_SIZE, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get thumb size %ld", ret);
		goto exit;
	}
	jpeg_cxt->param.thum_size = setting_param.size_param;
	out_param_ptr->thumb_size = jpeg_cxt->param.thum_size;

	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_ENCODE_ANGLE, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get enc angle %ld", ret);
		goto exit;
	}
	jpeg_cxt->param.set_encode_rotation = setting_param.cmd_type_value;
	if (cxt->snp_cxt.is_cfg_rot_cap) {
		cxt->snp_cxt.cfg_cap_rot = jpeg_cxt->param.set_encode_rotation;
	} else {
		cxt->snp_cxt.cfg_cap_rot = 0;
	}

	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_SHOT_NUMBER, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get shot num %ld", ret);
		goto exit;
	}
	cxt->snp_cxt.total_num = setting_param.cmd_type_value;

	if (1 == camera_get_hdr_flag(cxt)) {
		struct ipm_open_in  in_param;
		struct ipm_open_out out_param;
		in_param.frame_size.width = CAMERA_ALIGNED_16(cxt->snp_cxt.request_size.width);
		in_param.frame_size.height = CAMERA_ALIGNED_16(cxt->snp_cxt.request_size.height);
		in_param.frame_rect.start_x = 0;
		in_param.frame_rect.start_y = 0;
		in_param.frame_rect.width = in_param.frame_size.width;
		in_param.frame_rect.height = in_param.frame_size.height;
		in_param.reg_cb = camera_ipm_cb;
		ret = camera_open_hdr(cxt, &in_param, &out_param);
		if (ret) {
			CMR_LOGE("failed to open hdr %ld", ret);
			goto exit;
		} else {
			cxt->ipm_cxt.hdr_num = out_param.total_frame_number;
			CMR_LOGI("get hdr num %d", cxt->ipm_cxt.hdr_num);
		}
	}
	if (CAMERA_ZSL_MODE == mode) {
		out_param_ptr->frame_count = FRAME_NUM_MAX;
		out_param_ptr->frame_ctrl = FRAME_CONTINUE;
	} else {
		if (camera_get_hdr_flag(cxt)) {
			out_param_ptr->frame_count = cxt->ipm_cxt.hdr_num;
			out_param_ptr->frame_ctrl = FRAME_STOP;
		} else {
			out_param_ptr->frame_count = cxt->snp_cxt.total_num;
			if (1 == cxt->snp_cxt.total_num) {
				out_param_ptr->frame_ctrl = FRAME_STOP;
			} else {
				out_param_ptr->frame_ctrl = FRAME_CONTINUE;
			}
		}
	}
exit:
	CMR_LOGI("prev size %d %d pic size %d %d", out_param_ptr->preview_size.width, out_param_ptr->preview_size.height,
		     out_param_ptr->picture_size.width, out_param_ptr->picture_size.height);
	CMR_LOGI("video size %d %d android zsl flag %d", out_param_ptr->video_size.width, out_param_ptr->video_size.height, cxt->is_android_zsl);
	CMR_LOGI("prev rot %ld snp rot %d rot snp %d", out_param_ptr->prev_rot, out_param_ptr->cap_rot, out_param_ptr->is_cfg_rot_cap);
	CMR_LOGI("zoom mode %ld fd %ld is dv %d tool eb %d", out_param_ptr->zoom_setting.mode, out_param_ptr->is_fd_on,
			out_param_ptr->is_dv, out_param_ptr->tool_eb);
	CMR_LOGI("quality %d thumb quality %d encode angle %d thumb size %d %d", jpeg_cxt->param.quality,
		     jpeg_cxt->param.thumb_quality, jpeg_cxt->param.set_encode_rotation,
		     jpeg_cxt->param.thum_size.width, jpeg_cxt->param.thum_size.height);
	CMR_LOGI("frame count %d", out_param_ptr->frame_count);

	return ret;
}

cmr_int camera_set_preview_param(cmr_handle oem_handle, enum takepicture_mode mode,  cmr_uint is_snapshot)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct preview_context         *prev_cxt = &cxt->prev_cxt;
	struct snapshot_context        *snp_cxt = &cxt->snp_cxt;
	struct setting_context         *setting_cxt = &cxt->setting_cxt;
	struct preview_param           start_param;
	struct preview_out_param       preview_out;

	CMR_PRINT_TIME;
	if (1 != prev_cxt->inited) {
		CMR_LOGE("err, don't init preview");
		ret = -CMR_CAMERA_INVALID_STATE;
		goto exit;
	}
	if (PREVIEWING == cmr_preview_get_status(cxt->prev_cxt.preview_handle, cxt->camera_id)) {
		CMR_LOGI("prev has been started");
		goto exit;
	}

	ret = camera_get_preview_param(oem_handle, mode, is_snapshot, &start_param);
	if (ret) {
		CMR_LOGE("failed to get prev param %ld", ret);
		goto exit;
	}

	cmr_bzero(&preview_out, sizeof(struct preview_out_param));
	ret = cmr_preview_set_param(prev_cxt->preview_handle, cxt->camera_id, &start_param, &preview_out);
	if (ret) {
		CMR_LOGE("failed to set prev param %ld", ret);
		goto exit;
	}
	prev_cxt->preview_sn_mode = preview_out.preview_sn_mode;
	prev_cxt->channel_bits = preview_out.preview_chn_bits;
	prev_cxt->video_sn_mode = preview_out.video_sn_mode;
	prev_cxt->video_channel_bits = preview_out.video_chn_bits;
	snp_cxt->snapshot_sn_mode = preview_out.snapshot_sn_mode;
	snp_cxt->channel_bits = preview_out.snapshot_chn_bits;
	snp_cxt->post_proc_setting = preview_out.post_proc_setting;
	cmr_copy((void*)&snp_cxt->post_proc_setting, (void*)&preview_out.post_proc_setting, sizeof(snp_cxt->post_proc_setting));
	CMR_LOGI("prev mode %d prev chn %d snp mode %d snp chn %d",
		      prev_cxt->preview_sn_mode, prev_cxt->channel_bits, snp_cxt->snapshot_sn_mode, snp_cxt->channel_bits);
	CMR_LOGI("rot angle %ld", snp_cxt->post_proc_setting.rot_angle);

exit:
	CMR_PRINT_TIME;
	return ret;
}

cmr_int camera_get_snapshot_param(cmr_handle oem_handle, struct snapshot_param *out_ptr)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct jpeg_context            *jpeg_cxt = &cxt->jpeg_cxt;
	struct setting_context         *setting_cxt = &cxt->setting_cxt;
	struct setting_cmd_parameter   setting_param;
	cmr_int                        i;
	cmr_u32                        chn_bits = cxt->snp_cxt.channel_bits;

	out_ptr->total_num = 0;
	out_ptr->rot_angle = 0;
	out_ptr->hdr_handle = cxt->ipm_cxt.hdr_handle;
	out_ptr->hdr_need_frm_num = cxt->ipm_cxt.hdr_num;
	setting_param.camera_id = cxt->camera_id;
	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_HDR, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get envir %ld", ret);
		goto exit;
	}
	camera_set_hdr_flag(cxt, setting_param.cmd_type_value);

	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_SHOT_NUMBER, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get shot num %ld", ret);
		goto exit;
	}
	out_ptr->total_num = setting_param.cmd_type_value;
	cxt->snp_cxt.total_num = out_ptr->total_num;
	ret = cmr_setting_ioctl(setting_cxt->setting_handle, SETTING_GET_ENCODE_ANGLE, &setting_param);
	if (ret) {
		CMR_LOGE("failed to get enc angle %ld", ret);
		goto exit;
	}
	out_ptr->rot_angle = setting_param.cmd_type_value;
	jpeg_cxt->param.set_encode_rotation = setting_param.cmd_type_value;
	ret = cmr_preview_get_post_proc_param(cxt->prev_cxt.preview_handle, cxt->camera_id,
									(cmr_u32)setting_param.cmd_type_value, &out_ptr->post_proc_setting);
	if (ret) {
		CMR_LOGE("failed to get rot angle %ld", ret);
		goto exit;
	}
	out_ptr->camera_id = cxt->camera_id;
	out_ptr->is_hdr = camera_get_hdr_flag(cxt);
	out_ptr->is_android_zsl = cxt->is_android_zsl;
	out_ptr->mode = cxt->snp_cxt.snp_mode;
	out_ptr->is_cfg_rot_cap = cxt->snp_cxt.is_cfg_rot_cap;
	out_ptr->jpeg_setting = jpeg_cxt->param;
	out_ptr->req_size = cxt->snp_cxt.request_size;
	CMR_LOGI("chn_bits %d actual size %d %d", chn_bits, out_ptr->post_proc_setting.actual_snp_size.width,
			out_ptr->post_proc_setting.actual_snp_size.height);
	out_ptr->channel_id = V4L2_CHANNEL_MAX+1;
	for ( i=0 ; i<V4L2_CHANNEL_MAX ; i++) {
		if (chn_bits & (1 << i)) {
			out_ptr->channel_id = i;
			break;
		}
	}

exit:
	CMR_LOGI("done,total num %d enc angle %d", out_ptr->total_num, out_ptr->rot_angle);
	return ret;
}

cmr_int camera_set_setting(cmr_handle oem_handle, enum camera_param_type id, cmr_uint param)
{
	cmr_int                          ret = CMR_CAMERA_SUCCESS;
	struct camera_context            *cxt = (struct camera_context*)oem_handle;
	struct setting_cmd_parameter     setting_param;

	setting_param.camera_id = cxt->camera_id;
	switch (id) {
	case CAMERA_PARAM_ZOOM:
		if (param) {
			setting_param.zoom_param = *((struct cmr_zoom_param*)param);
			ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		} else {
			CMR_LOGE("err, zoom param is null");
			ret = -CMR_CAMERA_INVALID_PARAM;
		}
		break;
	case CAMERA_PARAM_ENCODE_ROTATION:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_CONTRAST:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_BRIGHTNESS:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_SHARPNESS:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_WB:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_EFFECT:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_FLASH:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_ANTIBANDING:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_AUTO_EXPOSURE_MODE:
		setting_param.ae_param = *(struct cmr_ae_param*)param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_ISO:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_EXPOSURE_COMPENSATION:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_PREVIEW_FPS:
		if (param) {
			setting_param.preview_fps_param = *(struct cmr_preview_fps_param*)param;
			ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		} else {
			CMR_LOGE("err, fps param is null");
			ret = -CMR_CAMERA_INVALID_PARAM;
		}
		break;
	case CAMERA_PARAM_SATURATION:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_SCENE_MODE:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_JPEG_QUALITY:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_THUMB_QUALITY:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_SENSOR_ORIENTATION:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_FOCAL_LENGTH:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_SENSOR_ROTATION:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_SHOT_NUM:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_ROTATION_CAPTURE:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_POSITION:
		if (param) {
			setting_param.position_info = *(struct camera_position_type*)param;
			ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		} else {
			CMR_LOGE("err, postition param is null");
			ret = -CMR_CAMERA_INVALID_PARAM;
		}
		break;
	case CAMERA_PARAM_PREVIEW_SIZE:
		if (param) {
			setting_param.size_param = *(struct img_size*)param;
			ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		} else {
			CMR_LOGE("err, prev size param is null");
			ret = -CMR_CAMERA_INVALID_PARAM;
		}
		break;
	case CAMERA_PARAM_PREVIEW_FORMAT:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_CAPTURE_SIZE:
		if (param) {
			setting_param.size_param = *(struct img_size*)param;
			ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		} else {
			CMR_LOGE("err, capture size param is null");
			ret = -CMR_CAMERA_INVALID_PARAM;
		}
		break;
	case CAMERA_PARAM_CAPTURE_FORMAT:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_CAPTURE_MODE:
		setting_param.cmd_type_value = param;
		ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		break;
	case CAMERA_PARAM_THUMB_SIZE:
		if (param) {
			setting_param.size_param = *(struct img_size*)param;
			ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, id, &setting_param);
		} else {
			CMR_LOGE("err, thumb size param is null");
			ret = -CMR_CAMERA_INVALID_PARAM;
		}
		break;
	default:
		CMR_LOGI("don't support %d", id);
	}
	CMR_LOGI("done %ld", ret);
	return ret;
}

/*****************************************external function*****************************************/

cmr_int camera_local_int(cmr_u32 camera_id, camera_cb_of_type callback,
						          void *client_data, cmr_uint is_autotest, cmr_handle *oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = NULL;

	if (!oem_handle) {
		CMR_LOGE("in parm error");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}

	*oem_handle = (cmr_handle)0;

	cxt = (struct camera_context*)malloc(sizeof(struct camera_context));
	if (NULL == cxt) {
		CMR_LOGE("failed to create context");
		ret = -CMR_CAMERA_NO_MEM;
		goto exit;
	}
	cmr_bzero(cxt, sizeof(*cxt));
	cxt->camera_id = camera_id;
	cxt->camera_cb = callback;
	cxt->client_data = client_data;

	CMR_LOGI("create handle 0x%lx 0x%lx", (cmr_uint)cxt, (cmr_uint)cxt->client_data);
	CMR_PRINT_TIME;
	ret = camera_init_internal((cmr_handle)cxt, is_autotest);
	CMR_PRINT_TIME;
exit:
	CMR_LOGI("ret %ld", ret);
	if (CMR_CAMERA_SUCCESS == ret) {
		cxt->inited = 1;
	    *oem_handle = (cmr_handle)cxt;
	} else {
		if (cxt) {
			free((void*)cxt);
		}
	}
	return ret;
}

cmr_int camera_local_deinit(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	CMR_PRINT_TIME;
	camera_deinit_internal(oem_handle);
	free((void*)oem_handle);
	CMR_PRINT_TIME;
	return ret;
}


cmr_int camera_local_start_preview(cmr_handle oem_handle, enum takepicture_mode mode,  cmr_uint is_snapshot)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct preview_context         *prev_cxt = &cxt->prev_cxt;
	struct setting_cmd_parameter   setting_param;

	ret = camera_set_preview_param(oem_handle, mode, is_snapshot);
	if (ret) {
		CMR_LOGE("failed to set prev param %ld", ret);
		goto exit;
	}

	cxt->setting_cxt.is_active = 1;
	setting_param.camera_id = cxt->camera_id;
	ret = cmr_setting_ioctl(cxt->setting_cxt.setting_handle, SETTING_SET_ENVIRONMENT, &setting_param);

	ret = cmr_preview_start(prev_cxt->preview_handle, cxt->camera_id);
	if (ret) {
		CMR_LOGE("failed to start prev %ld", ret);
	}
	cxt->camera_mode = mode;
	CMR_LOGI("camera mode %d", cxt->camera_mode);
#if 1
	ret = cmr_sensor_focus_init(cxt->sn_cxt.sensor_handle ,cxt->camera_id);
#endif
exit:
	CMR_PRINT_TIME;
	return ret;
}

cmr_int camera_local_stop_preview(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	cmr_int                        prev_ret = CMR_CAMERA_SUCCESS;
	cmr_int                        snp_ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	CMR_PRINT_TIME;
	if (PREVIEWING != cmr_preview_get_status(cxt->prev_cxt.preview_handle, cxt->camera_id)) {
		CMR_LOGI("don't previewing");
		goto exit;
	}
	if (CAMERA_ZSL_MODE == cxt->camera_mode) {
		if (IDLE != cxt->snp_cxt.status) {
			snp_ret = cmr_snapshot_stop(cxt->snp_cxt.snapshot_handle);
			if (snp_ret) {
				CMR_LOGE("failed to stop snp %ld", ret);
			}
		}
	}
	prev_ret = cmr_preview_stop(cxt->prev_cxt.preview_handle, cxt->camera_id);
	if (ret) {
		CMR_LOGE("failed to stop prev %ld", ret);
	}
	ret = prev_ret | snp_ret;
	CMR_PRINT_TIME;
exit:
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_local_start_snapshot(cmr_handle oem_handle, enum takepicture_mode mode, cmr_uint is_snapshot)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct preview_context         *prev_cxt;
	struct snapshot_param          snp_param;

	CMR_PRINT_TIME;
	if (!oem_handle) {
		CMR_LOGE("error handle");
		goto exit;
	}
	camera_take_snapshot_step(CMR_STEP_TAKE_PIC);
	prev_cxt = &cxt->prev_cxt;

	if (CAMERA_ZSL_MODE != mode) {
		ret = camera_set_preview_param(oem_handle, mode, is_snapshot);
	}
	ret = camera_get_snapshot_param(oem_handle, &snp_param);
	if (ret) {
		CMR_LOGE("failed to get snp num %ld", ret);
		goto exit;
	}

	ret = cmr_snapshot_post_proc(cxt->snp_cxt.snapshot_handle, &snp_param);
	if (ret) {
		CMR_LOGE("failed to snp post proc %ld", ret);
		goto exit;
	}
	if (CAMERA_ZSL_MODE != mode) {
		ret = cmr_preview_start(prev_cxt->preview_handle, cxt->camera_id);
		if (ret) {
			CMR_LOGE("failed to start prev %ld", ret);
		}
		cxt->camera_mode = mode;
	}
	camera_set_snp_req((cmr_handle)cxt, TAKE_PICTURE_NEEDED);
	camera_snapshot_started((cmr_handle)cxt);
	ret = camera_get_cap_time((cmr_handle)cxt);
	cxt->snp_cxt.status = SNAPSHOTING;
exit:
	CMR_PRINT_TIME;
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_local_stop_snapshot(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	CMR_PRINT_TIME;
	if (TAKE_PICTURE_NEEDED == camera_get_snp_req(oem_handle)) {
		camera_channel_stop(oem_handle, cxt->snp_cxt.channel_bits);
	}
	camera_set_snp_req(oem_handle, TAKE_PICTURE_NO);
	ret = cmr_snapshot_stop(cxt->snp_cxt.snapshot_handle);
	if (ret) {
		CMR_LOGE("failed to stop snp %ld", ret);
		goto exit;
	}
	if (camera_get_hdr_flag(cxt)) {
#ifdef OEM_HANDLE_HDR
		if (0 != cxt->ipm_cxt.frm_num) {
			cxt->ipm_cxt.frm_num = 0;
			sem_post(&cxt->hdr_sync_sm);
		}
#endif
		ret = camera_close_hdr(cxt);
	}
	cxt->snp_cxt.status = IDLE;

exit:
	CMR_PRINT_TIME;
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_local_get_prev_rect(cmr_handle oem_handle, struct img_rect *param_ptr)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!param_ptr) {
		CMR_LOGE("error param");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}
	//*param_ptr = cxt->prev_cxt.rect;
	ret = cmr_preview_get_prev_rect(cxt->prev_cxt.preview_handle, cxt->camera_id, param_ptr);
	CMR_LOGI("%d %d %d %d", param_ptr->start_x, param_ptr->start_y, param_ptr->width, param_ptr->height);
exit:
	return ret;
}

cmr_int camera_local_get_zsl_info(cmr_handle oem_handle, cmr_uint *is_support,
                                        cmr_uint *max_width, cmr_uint *max_height)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct preview_zsl_info        zsl_info;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	if (!oem_handle || !is_support || !max_width || !max_height) {
		CMR_LOGE("error param");
		ret = -CMR_CAMERA_INVALID_PARAM;
		goto exit;
	}

	ret = cmr_preview_is_support_zsl(cxt->prev_cxt.preview_handle, cxt->camera_id, is_support);
	if (ret) {
		CMR_LOGE("failed to get zsl info %ld", ret);
		goto exit;
	}

	ret = cmr_preview_get_max_cap_size(cxt->prev_cxt.preview_handle, cxt->camera_id, max_width, max_height);
	if (ret) {
		CMR_LOGE("failed to get max cap size %ld", ret);
		goto exit;
	}

exit:
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_local_redisplay_data(cmr_handle oem_handle, cmr_uint output_addr,
                                                   cmr_uint output_width, cmr_uint output_height,
                                                   cmr_uint input_addr_y, cmr_uint input_addr_uv,
                                                   cmr_uint input_width, cmr_uint input_height)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;
	struct img_frm                 src_img;
	struct img_frm                 dst_img;
	struct cmr_rot_param           rot_param;
	struct img_rect                rect;
	enum img_angle                 angle = IMG_ANGLE_0;
	cmr_uint                       img_len = (cmr_uint)(output_width * output_height);

	CMR_LOGI("0x%lx %d", (cmr_uint)oem_handle, cxt->snp_cxt.cfg_cap_rot);
	if (!oem_handle) {
		CMR_LOGE("err, handle is null");
		goto exit;
	}

	CMR_PRINT_TIME;
	rect.start_x = 0;
	rect.start_y = 0;
	rect.width = input_width;
	rect.height = input_height;
	src_img.size.width = input_width;
	src_img.size.height = input_height;
	src_img.fmt = IMG_DATA_TYPE_YUV420;
	cmr_v4l2_get_dcam_endian(&cxt->snp_cxt.cur_frm_info.data_endian, &src_img.data_end);
	src_img.addr_phy.addr_y = input_addr_y;
	src_img.addr_phy.addr_u = input_addr_uv;
	if (IMG_ANGLE_90 == cxt->snp_cxt.cfg_cap_rot || IMG_ANGLE_270 == cxt->snp_cxt.cfg_cap_rot) {
		dst_img.size.width = output_height;
		dst_img.size.height = output_width;
		dst_img.addr_phy.addr_y = output_addr + ((img_len * 3) >> 1);
		dst_img.addr_phy.addr_u = dst_img.addr_phy.addr_y + img_len;
	}else if (IMG_ANGLE_180 == cxt->snp_cxt.cfg_cap_rot) {
		dst_img.size.width = output_width;
		dst_img.size.height = output_height;
		dst_img.addr_phy.addr_y = output_addr + ((img_len * 3) >> 1);
		dst_img.addr_phy.addr_u = dst_img.addr_phy.addr_y + img_len;
	} else {
		dst_img.size.width = output_width;
		dst_img.size.height = output_height;
		dst_img.addr_phy.addr_y = output_addr;
		dst_img.addr_phy.addr_u = dst_img.addr_phy.addr_y + img_len;
	}
	dst_img.fmt = IMG_DATA_TYPE_YUV420;
	cmr_v4l2_get_dcam_endian(&cxt->snp_cxt.cur_frm_info.data_endian, &dst_img.data_end);
	CMR_LOGI("data_end %d %d", cxt->snp_cxt.cur_frm_info.data_endian.y_endian, cxt->snp_cxt.cur_frm_info.data_endian.uv_endian);
	rect.start_x = 0;
	rect.start_y = 0;
	rect.width = input_width;
	rect.height = input_height;
	ret = camera_get_trim_rect(&rect, 0, &dst_img.size);
	if (ret) {
		CMR_LOGE("failed to get trim %ld", ret);
		goto exit;
	}
	src_img.rect = rect;
	ret = cmr_scale_start(cxt->scaler_cxt.scaler_handle, &src_img, &dst_img, (cmr_evt_cb)NULL, NULL);
	if (ret) {
		CMR_LOGE("failed to start start %ld", ret);
		ret = - CMR_CAMERA_FAIL;
		goto exit;
	}
	CMR_PRINT_TIME;
	/* start roattion*/
	if (IMG_ANGLE_0 != cxt->snp_cxt.cfg_cap_rot) {
		if (IMG_ANGLE_90 == cxt->snp_cxt.cfg_cap_rot) {
			angle = IMG_ANGLE_270;
		} else if (IMG_ANGLE_270 == cxt->snp_cxt.cfg_cap_rot) {
			angle = IMG_ANGLE_90;
		} else {
			angle = cxt->snp_cxt.cfg_cap_rot;
		}
		rect.start_x = 0;
		rect.start_y = 0;
		rect.width = dst_img.size.width;
		rect.height = dst_img.size.height;
		src_img.addr_phy.addr_y = dst_img.addr_phy.addr_y;
		src_img.addr_phy.addr_u = dst_img.addr_phy.addr_u;
		src_img.size.width = dst_img.size.width;
		src_img.size.height = dst_img.size.height;
		src_img.fmt = IMG_DATA_TYPE_YUV420;
		src_img.data_end = cxt->snp_cxt.cur_frm_info.data_endian;
		dst_img.addr_phy.addr_y = output_addr;
		dst_img.addr_phy.addr_u = dst_img.addr_phy.addr_y + img_len;
		dst_img.data_end = cxt->snp_cxt.cur_frm_info.data_endian;

		rot_param.handle = cxt->rot_cxt.rotation_handle;
		rot_param.angle = angle;
		rot_param.src_img = src_img;
		rot_param.dst_img = dst_img;
		src_img.rect = rect;
		ret = cmr_rot(&rot_param);
		if (ret) {
			CMR_LOGI("failed to rotate %ld", ret);
			goto exit;
		}
	}
exit:
	CMR_PRINT_TIME;
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_local_fd_start(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	CMR_LOGI("fd %d", cxt->fd_on_off);
	if (PREVIEWING == cmr_preview_get_status(cxt->prev_cxt.preview_handle, cxt->camera_id)) {
		ret = cmr_preview_ctrl_facedetect(cxt->prev_cxt.preview_handle, cxt->camera_id, cxt->fd_on_off);
		if (ret) {
			CMR_LOGE("failed to fd ctrl %ld", ret);
		}
	}
	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_local_set_param(cmr_handle oem_handle, enum camera_param_type id, cmr_uint param)
{
	cmr_int                          ret = CMR_CAMERA_SUCCESS;
	struct camera_context            *cxt = (struct camera_context*)oem_handle;

	switch (id) {
	case CAMERA_PARAM_FOCUS_RECT:
		CMR_LOGI("set focus CAMERA_PARAM_FOCUS_RECT param =0x%lx",param);
		ret = cmr_focus_set_param(cxt->focus_cxt.focus_handle, cxt->camera_id, id, (void*)param);
		break;
	case CAMERA_PARAM_AF_MODE:
		CMR_LOGI("set focus CAMERA_PARAM_AF_MODE param =0x%lx",param);
		ret = cmr_focus_set_param(cxt->focus_cxt.focus_handle, cxt->camera_id, id, (void*)param);
		break;
	case CAMERA_PARAM_ZOOM:
		ret = cmr_preview_update_zoom(cxt->prev_cxt.preview_handle, cxt->camera_id, (struct cmr_zoom_param*)param);
		if (ret) {
			CMR_LOGE("failed to update zoom %ld", ret);
		}
		ret = camera_set_setting(oem_handle, id, param);
		break;
	default:
		ret = camera_set_setting(oem_handle, id, param);
		break;
	}
	if (ret) {
		CMR_LOGE("failed to set param %ld", ret);
	}
exit:
	return ret;
}

cmr_int camera_local_start_focus(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	ret = cmr_focus_start(cxt->focus_cxt.focus_handle, cxt->camera_id);

	CMR_LOGI("done %ld", ret);
	return ret;
}

cmr_int camera_local_cancel_focus(cmr_handle oem_handle)
{
	cmr_int                        ret = CMR_CAMERA_SUCCESS;
	struct camera_context          *cxt = (struct camera_context*)oem_handle;

	ret = cmr_focus_stop(cxt->focus_cxt.focus_handle, cxt->camera_id, 1);

	CMR_LOGI("done %ld", ret);
	return ret;
}