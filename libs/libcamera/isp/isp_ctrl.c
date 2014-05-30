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

#include <sys/types.h>
#include "isp_app.h"
#include "isp_com.h"
#include "isp_msg.h"
#include "isp_exif.h"

/**---------------------------------------------------------------------------*
**				Micro Define					*
**----------------------------------------------------------------------------*/
#define ISP_CHIP_ID 0x0000
#define ISP_SOFT_ID 0x20140410

#define ISP_CTRL_EVT_START                (1 << 0)
#define ISP_CTRL_EVT_STOP                  (1 << 1)
#define ISP_CTRL_EVT_INIT                    (1 << 2)
#define ISP_CTRL_EVT_DEINIT                (1 << 3)
#define ISP_CTRL_EVT_CONTINUE           (1 << 4)
#define ISP_CTRL_EVT_CONTINUE_STOP (1 << 5)
#define ISP_CTRL_EVT_SIGNAL               (1 << 6)
#define ISP_CTRL_EVT_SIGNAL_NEXT     (1 << 7)
#define ISP_CTRL_EVT_IOCTRL              (1 << 8)
#define ISP_CTRL_EVT_TX                     (1 << 9)
#define ISP_CTRL_EVT_SOF                   (1 << 10)
#define ISP_CTRL_EVT_EOF                   (1 << 11)
#define ISP_CTRL_EVT_AE                     (1 << 12)
#define ISP_CTRL_EVT_AWB                  (1 << 13)
#define ISP_CTRL_EVT_AF                     (1 << 14)
#define ISP_CTRL_EVT_CTRL_SYNC        (1 << 15)
#define ISP_CTRL_EVT_CONTINUE_AF     (1 << 16)
#define ISP_CTRL_EVT_MONITOR_STOP  (1 << 31)
#define ISP_CTRL_EVT_MASK                (uint32_t)(ISP_CTRL_EVT_START|ISP_CTRL_EVT_STOP|ISP_CTRL_EVT_INIT \
					|ISP_CTRL_EVT_CONTINUE_STOP|ISP_CTRL_EVT_DEINIT|ISP_CTRL_EVT_CONTINUE \
					|ISP_CTRL_EVT_SIGNAL|ISP_CTRL_EVT_SIGNAL_NEXT|ISP_CTRL_EVT_IOCTRL \
					|ISP_CTRL_EVT_TX|ISP_CTRL_EVT_SOF|ISP_CTRL_EVT_EOF|ISP_CTRL_EVT_AWB \
					|ISP_CTRL_EVT_AE|ISP_CTRL_EVT_AF|ISP_CTRL_EVT_CTRL_SYNC|ISP_CTRL_EVT_CONTINUE_AF \
					|ISP_CTRL_EVT_MONITOR_STOP)

#define ISP_PROC_EVT_START		(1 << 0)
#define ISP_PROC_EVT_STOP		(1 << 1)
#define ISP_PROC_EVT_AE			(1 << 2)
#define ISP_PROC_EVT_AWB		(1 << 3)
#define ISP_PROC_EVT_AF			(1 << 4)
#define ISP_PROC_EVT_AF_STOP		(1 << 5)
#define ISP_PROC_EVT_CONTINUE_AF	(1 << 6)
#define ISP_PROC_EVT_CONTINUE_AF_STOP	(1 << 7)
#define ISP_PROC_EVT_STOP_HANDLER	(1 << 8)
#define ISP_PROC_EVT_MASK	(uint32_t)(ISP_PROC_EVT_START | ISP_PROC_EVT_STOP | ISP_PROC_EVT_AE | ISP_PROC_EVT_AWB \
					| ISP_PROC_EVT_AF | ISP_PROC_EVT_AF_STOP | ISP_PROC_EVT_CONTINUE_AF \
					| ISP_PROC_EVT_CONTINUE_AF_STOP | ISP_PROC_EVT_STOP_HANDLER)

#define ISP_MONITOR_EVT_START	(1 << 0)
#define ISP_MONITOR_EVT_STOP	ISP_INT_STOP
#define ISP_MONITOR_EVT_SOF	ISP_INT_FETCH_SOF
#define ISP_MONITOR_EVT_EOF	ISP_INT_FETCH_EOF
#define ISP_MONITOR_EVT_FETCH	ISP_INT_FETCH_BUF_FULL
#define ISP_MONITOR_EVT_DCAM	ISP_INT_DCAM_FULL
#define ISP_MONITOR_EVT_STORE	ISP_INT_STORE_ERR
#define ISP_MONITOR_EVT_AWB	ISP_INT_AWB
#define ISP_MONITOR_EVT_AF	ISP_INT_AF
#define ISP_MONITOR_EVT_AE	ISP_INT_AE
#define ISP_MONITOR_EVT_TX	ISP_INT_STORE

#define ISP_MONITOR_EVT_MASK	(uint32_t)(ISP_MONITOR_EVT_START | ISP_MONITOR_EVT_STOP | ISP_MONITOR_EVT_SOF \
					| ISP_MONITOR_EVT_EOF | ISP_MONITOR_EVT_AWB | ISP_MONITOR_EVT_AF \
					| ISP_MONITOR_EVT_AE | ISP_MONITOR_EVT_TX)

#define ISP_THREAD_QUEUE_NUM 50
#define ISP_ID_INVALID 0xff
/**---------------------------------------------------------------------------*
**				Data Structures 					*
**---------------------------------------------------------------------------*/

/**---------------------------------------------------------------------------*
**				extend Variables and function			*
**---------------------------------------------------------------------------*/
static int32_t _ispAemEb(uint32_t handler_id);
static int32_t _ispAwbCorrect(uint32_t handler_id);
static int _isp_proc_msg_post(struct isp_msg *message);

/**---------------------------------------------------------------------------*
**				Local Variables 					*
**---------------------------------------------------------------------------*/
static const uint8_t com_ptr[]=
{
	22,22,22,22,22,22,22,22,22,22,22,22,21,21,21,21,
	21,21,21,20,20,20,20,20,19,19,19,19,18,18,18,18,
	17,17,17,16,16,16,16,15,15,15,14,14,14,13,13,13,
	13,12,12,12,11,11,11,10,10,10,9,9,9,9,8,8,
	8,7,7,7,7,6,6,6,6,5,5,5,5,5,4,4,
	4,4,4,3,3,3,3,3,3,3,2,2,2,2,2,2,
	2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
	40,40,40,40,40,40,40,40,40,39,39,39,39,39,39,38,
	38,38,38,37,37,37,37,36,36,36,35,35,35,34,34,33,
	33,32,32,32,31,31,30,30,29,29,28,28,27,27,26,26,
	25,25,24,24,23,23,22,21,21,20,20,19,19,18,18,17,
	17,16,16,15,15,14,14,13,13,12,12,11,11,10,10,10,
	9,9,8,8,8,7,7,7,6,6,6,5,5,5,5,4,
	4,4,4,3,3,3,3,3,3,2,2,2,2,2,2,2,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0
};

static const uint8_t p2e_ptr[]=
{
	0,18,33,45,55,62,69,74,78,81,84,87,89,91,93,94,
	96,98,100,101,103,105,107,109,110,112,114,116,117,119,121,122,
	124,125,126,127,128,129,130,131,132,133,134,135,135,136,137,137,
	138,139,140,140,141,142,143,144,145,145,146,147,148,149,150,151,
	152,153,154,155,156,157,158,159,160,161,161,162,163,164,164,165,
	165,166,167,167,168,168,169,170,170,171,172,173,174,175,176,177,
	178,179,180,181,183,184,186,187,188,190,191,192,193,194,195,196,
	197,198,199,199,200,201,202,203,205,208,211,215,221,229,239,252
};

static const uint8_t e2p_ptr[]=
{
	1,2,3,3,4,4,4,4,4,4,4,4,4,4,4,4,
	4,4,4,5,5,5,6,6,7,7,8,9,9,10,11,12,
	12,13,14,15,16,17,18,19,21,22,23,24,26,27,29,31,
	32,34,36,38,41,43,45,48,51,53,56,60,63,66,70,73,
	77,81,85,89,94,98,102,107,112,116,121,126,131,136,141,146,
	151,156,161,166,170,175,180,185,189,194,198,202,206,210,214,217,
	221,224,227,230,232,235,237,239,241,243,245,246,247,248,249,250,
	251,252,252,253,253,253,253,254,254,254,254,254,255,255,255,255
};

static const uint8_t ISP_AEAWB_weight_avrg[]=
{
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,8,8,8,8,8,8,8,8,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,8,8,8,8,8,8,8,8,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,8,8,8,8,8,8,8,8,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,8,8,8,8,8,8,8,8,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,8,8,8,8,8,8,8,8,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,8,8,8,8,8,8,8,8,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,8,8,8,8,8,8,8,8,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,8,8,8,8,8,8,8,8,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,8,8,8,8,8,8,8,8,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,
	1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};

static const uint8_t ISP_AEAWB_weight_center[]=
{
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static uint16_t cce_matrix[][12]=
{
	{0x004d,0x0096,0x001d,0xffd5,0xffab,0x0080,0x0080,0xff95,0xffeb,	0xff00,0x0000,0x0000},/*normal*/
	{0x004d,0x0096,0x001d,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,	0xff00,0x0000,0x0000},/*gray*/
	{0x004d,0x0096,0x001d,0xffd5,0xffab,0x0080,0x0080,0xff95,0xffeb,	0xff00,0xffd4,0x0080},/*warm*/
	{0x004d,0x0096,0x001d,0xffd5,0xffab,0x0080,0x0080,0xff95,0xffeb,	0xff00,0xffd5,0xffca},/*green*/
	{0x004d,0x0096,0x001d,0xffd5,0xffab,0x0080,0x0080,0xff95,0xffeb,	0xff00,0x0040,0x000a},/*cool*/
	{0x004d,0x0096,0x001d,0xffd5,0xffab,0x0080,0x0080,0xff95,0xffeb,	0xff00,0xff00,0x0028},/*orange*/
	{0xffb3,0xff6a,0xffe3,0x002b,0x0055,0xff80,0xff80,0x006b,0x0015,	0x00ff,0x0000,0x0000},/*negtive*/
	{0x004d,0x0096,0x001d,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,	0xff00,0xffe2,0x0028}/*old*/
};

static struct isp_param* s_isp_param_ptr=NULL;
static pthread_mutex_t s_ctrl_mutex={0x00};
/**---------------------------------------------------------------------------*
**					Constant Variables				*
**---------------------------------------------------------------------------*/

/**---------------------------------------------------------------------------*
**					Local Function Prototypes			*
**---------------------------------------------------------------------------*/

static int32_t _ispCfgAfWin(uint32_t handler_id, struct isp_af_param* af, struct isp_af_win* src_af);


/* ispGetSystem --
*@
*@
*@ return:
*/
static struct isp_system* ispGetSystem(void)
{
	return &s_isp_param_ptr->system;
}

/* ispGetContext --
*@
*@
*@ return:
*/
struct isp_context* ispGetContext(uint32_t handler_id)
{
	return &s_isp_param_ptr->context[handler_id];
}

/* ispInitContext --
*@
*@
*@ return:
*/
uint32_t ispInitContext(void)
{
	int32_t rtn=ISP_SUCCESS;
	uint32_t handler_id = ISP_ID_INVALID;

	if (NULL==s_isp_param_ptr) {
		s_isp_param_ptr = (struct isp_param*)malloc(sizeof(struct isp_param));
		if (NULL == s_isp_param_ptr) {
			ISP_LOG("alloc context buf error");
			rtn=ISP_ALLOC_ERROR;
			return rtn;
		}
		memset((void*)s_isp_param_ptr, 0x00, sizeof(struct isp_param));
	}

	return rtn;
}

/* ispDeinitContext --
*@
*@
*@ return:
*/
uint32_t ispDeinitContext(void)
{
	int32_t rtn=ISP_SUCCESS;

	if (NULL!=s_isp_param_ptr) {
		free(s_isp_param_ptr);
		s_isp_param_ptr = NULL;
	}

	return rtn;
}

/* _isp_AppLock --
*@
*@
*@ return:
*/
static int32_t _isp_CtrlLock(void)
{
	int32_t rtn = ISP_SUCCESS;

	//ISP_LOG("--------------_isp_CtrlLock------------------");

	rtn = pthread_mutex_lock(&s_ctrl_mutex);

	return rtn;
}

/* _isp_AppUnlock --
*@
*@
*@ return:
*/
static int32_t _isp_CtrlUnlock(void)
{
	int32_t rtn=ISP_SUCCESS;

	//ISP_LOG("--------------_isp_CtrlUnlock------------------");

	rtn = pthread_mutex_unlock(&s_ctrl_mutex);

	return rtn;
}

/* ispAlgin --
*@
*@
*@ return:
*/
uint32_t ispAlgin(uint32_t pLen , uint16_t algin_blk, uint16_t algin_bits)
{
	uint32_t algin_len=0x00;

	algin_len=pLen;
	algin_len=(((algin_len+algin_blk)>>algin_bits)<<algin_bits);

	return algin_len;
}



/* _isp_StopCallbackHandler --
*@
*@
*@ return:
*/
uint32_t _isp_SofHandler(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_ae_param* ae_param_ptr = &isp_context_ptr->ae;
	uint32_t ae_stab=ISP_NO_READY;
	struct isp_af_notice af_notice;

	if (ISP_EB == isp_context_ptr->ae.ae_bypass) {
		if (ISP_ZERO == ae_param_ptr->bypass_conter) {
			ISP_LOG("callback ISP_AE_BAPASS_CALLBACK");
			isp_context_ptr->cfg.callback(handler_id, ISP_CALLBACK_EVT|ISP_AE_BAPASS_CALLBACK, (void*)&rtn, sizeof(uint32_t));
			isp_context_ptr->ae.ae_bypass=ISP_UEB;
		} else {
			ae_param_ptr->bypass_conter--;
		}
	}

	return rtn;
}

/* _isp_StopCallbackHandler --
*@
*@
*@ return:
*/
uint32_t _isp_StopCallbackHandler(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	uint32_t ae_stab=ISP_NO_READY;
	struct isp_af_notice af_notice;

	isp_ae_stop_callback_handler(handler_id);

	if(ISP_AF_CONTINUE== isp_context_ptr->af.status)
	{
		ISP_LOG("Stop ISP_AF_NOTICE_CALLBACK");
		af_notice.mode=ISP_FOCUS_MOVE_END;
		af_notice.valid_win=0x00;
		isp_context_ptr->cfg.callback(handler_id, ISP_CALLBACK_EVT|ISP_AF_NOTICE_CALLBACK, (void*)&af_notice, sizeof(struct isp_af_notice));
	}

	return rtn;
}


/* _isp_ContinueFocusInforCallback --
*@
*@
*@ return:
*/
uint32_t _isp_ContinueFocusInforCallback(uint32_t handler_id, uint32_t mode)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);

	//ISP_LOG("mode: 0x%x", mode);

	if (ISP_END_FLAG !=isp_context_ptr->af.continue_status) {
		isp_context_ptr->af.continue_stat_flag|=mode;
	} else {
		isp_context_ptr->af.continue_stat_flag=ISP_ZERO;
	}

	return rtn;
}

/* _isp_ContinueFocusHandler --
*@
*@
*@ return:
*/
uint32_t _isp_ContinueFocusHandler(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
//	ISP_LOG("status:0x%x, flag:0x%x", isp_context_ptr->af.continue_status, isp_context_ptr->af.continue_stat_flag);
	if ((ISP_IDLE_FLAG==isp_context_ptr->af.continue_status)
		&&((ISP_AWB_STAT_FLAG|ISP_AF_STAT_FLAG)&isp_context_ptr->af.continue_stat_flag)
		&&(ISP_AF_STOP == isp_context_ptr->af.status)) {
//		ISP_LOG("calc");
		isp_context_ptr->af.continue_status=ISP_RUN_FLAG;
		isp_context_ptr->af.continue_stat_flag=ISP_ZERO;
	} else {
		rtn=ISP_NO_READY;
	}
//	ISP_LOG("return %d",rtn);
	return rtn;
}


/* _isp_ContinueFocusTrigeAF --
*@
*@
*@ return:
*/
static int32_t _isp_ContinueFocusTrigeAF(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_af_win af_win;
	struct isp_af_notice af_notice;

	ISP_LOG("_isp_ContinueFocusTrigeAF");

	isp_context_ptr->af.monitor_bypass=ISP_EB;
	if((ISP_UEB == isp_context_ptr->af.bypass)
		&&(ISP_AF_CONTINUE != isp_context_ptr->af.status)
		&&(ISP_FOCUS_CONTINUE == isp_context_ptr->af.mode)){

		af_win.mode = isp_context_ptr->af.mode;
		af_win.valid_win = 0;
		_ispCfgAfWin(handler_id, &isp_context_ptr->af, &af_win);
		ISP_LOG("ae.bypass %d",isp_context_ptr->ae.bypass);
		isp_context_ptr->af.ae_status = isp_context_ptr->ae.bypass;
		isp_context_ptr->af.awb_status = isp_context_ptr->awb.bypass;
		isp_context_ptr->ae.bypass=ISP_EB;
		isp_context_ptr->awb.bypass=ISP_EB;
		isp_context_ptr->ae.monitor_bypass=ISP_EB;
		isp_context_ptr->awb.monitor_bypass=ISP_EB;
		isp_context_ptr->ae.cur_skip_num = ISP_AE_SKIP_FOREVER;
		isp_context_ptr->af.monitor_bypass=ISP_UEB;
		isp_context_ptr->tune.af=ISP_EB;

		af_notice.mode=ISP_FOCUS_MOVE_START;
		af_notice.valid_win=0x00;
		isp_context_ptr->cfg.callback(handler_id, ISP_CALLBACK_EVT|ISP_AF_NOTICE_CALLBACK, (void*)&af_notice, sizeof(struct isp_af_notice));
	}

	return rtn;

}

/* _isp_ContinueFocusTrigeAF --
*@
*@
*@ return:
*/
static int32_t _isp_ContinueFocusStartInternal(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);

	//ISP_LOG("_isp_ContinueFocusStartInternal");
	if((ISP_UEB == isp_context_ptr->af.bypass)
		&&(ISP_AF_CONTINUE == isp_context_ptr->af.status)
		&&(ISP_EB == isp_context_ptr->awb.back_bypass))
	{
		ISP_LOG("--_isp_ContinueFocusStartInternal--af bypass%d,af status%d,awb back_bypass%d", isp_context_ptr->af.bypass,isp_context_ptr->af.status,isp_context_ptr->awb.back_bypass);

	}else{
		if (ISP_END_FLAG==isp_context_ptr->af.continue_status) {
			//ISP_LOG("--_isp_ContinueFocusStartInternal start--");
			isp_context_ptr->af.continue_status=ISP_IDLE_FLAG;
			isp_context_ptr->af.monitor_bypass=ISP_UEB;
			isp_context_ptr->awbm.bypass=ISP_UEB;
			isp_context_ptr->af.continue_stat_flag=ISP_ZERO;
			isp_context_ptr->tune.af_stat_continue=ISP_EB;
		}
	}

	return rtn;
}

/* _ispCfgSaturationoffset --
*@
*@
*@ return:
*/
static int32_t _ispCfgSaturationoffset(uint32_t handler_id, uint8_t offset)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);

	isp_context_ptr->hue.offset = offset;
	isp_context_ptr->tune.hue = ISP_EB;

	return rtn;
}

/* _ispCfgHueoffset --
*@
*@
*@ return:
*/
static int32_t _ispCfgHueoffset(uint32_t handler_id, uint8_t offset)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);

	isp_context_ptr->saturation.offset = offset;
	isp_context_ptr->tune.saturation = ISP_EB;

	return rtn;
}

/* _ispGetEvLum --
*@
*@
*@ return:
*/
static int32_t _ispGetEvLum(uint32_t handler_id)
{
	return isp_ae_get_ev_lum(handler_id);;
}

/* ispLog2n --
*@
*@
*@ return:
*/
uint32_t _ispLog2n(uint32_t index)
{
	uint32_t index_num=index;
	uint32_t i=0x00;

	for(i=0x00; index_num>1; i++)
	{
		index_num>>=0x01;
	}

	return i;
}

/* _ispIoCtrlInit --
*@
*@
*@ return:
*/
static int32_t _ispIoCtrlInit(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_tune_block* isp_tune_ptr=&isp_context_ptr->tune;

	isp_tune_ptr->eb = ISP_UEB;
	isp_tune_ptr->ae = ISP_UEB;
	isp_tune_ptr->awb = ISP_UEB;
	isp_tune_ptr->lnc = ISP_UEB;
	isp_tune_ptr->lnc_load= ISP_UEB;
	isp_tune_ptr->special_effect = ISP_UEB;
	isp_tune_ptr->bright = ISP_UEB;
	isp_tune_ptr->contrast = ISP_UEB;
	isp_tune_ptr->hist = ISP_UEB;
	isp_tune_ptr->auto_contrast = ISP_UEB;
	isp_tune_ptr->saturation = ISP_UEB;
	isp_tune_ptr->af = ISP_UEB;
	isp_tune_ptr->css = ISP_UEB;
	isp_tune_ptr->hdr = ISP_UEB;
	isp_tune_ptr->global_gain = ISP_UEB;
	isp_tune_ptr->chn_gain = ISP_UEB;
	isp_context_ptr->wb_trim_conter=ISP_AWB_SKIP_FOREVER;
	isp_context_ptr->awb_win_conter = ISP_AWB_SKIP_FOREVER;
	isp_context_ptr->af.end_handler_flag = ISP_EB;

	return rtn;
}

/* _ispAwbGetMonitorInfo --
*@
*@
*@ return:
*/
static uint32_t _ispAwbGetMonitorInfo(uint32_t handler_id, void* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_awb_monitor_info* awb_monitor_info_ptr=(struct isp_awb_monitor_info*) param_ptr;

	awb_monitor_info_ptr->frame_size.w=isp_context_ptr->src.w;
	awb_monitor_info_ptr->frame_size.h=isp_context_ptr->src.h;

	ispGetAwbWinNum(handler_id, (uint16_t*)&awb_monitor_info_ptr->win_num.w, (uint16_t*)&awb_monitor_info_ptr->win_num.h);

	awb_monitor_info_ptr->win_pos.x = isp_context_ptr->awbm.win_start.x;
	awb_monitor_info_ptr->win_pos.y = isp_context_ptr->awbm.win_start.y;
	awb_monitor_info_ptr->win_size.w = isp_context_ptr->awbm.win_size.w;
	awb_monitor_info_ptr->win_size.h = isp_context_ptr->awbm.win_size.h;

	return rtn;
}

/* _ispAwbSetMonitorWin --
*@
*@
*@ return:
*/
static uint32_t _ispAwbSetMonitorWin(struct isp_pos pos, struct isp_size win_size)
{
	int32_t rtn=ISP_SUCCESS;
	uint32_t handler_id = 0x00;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_awbm_param*awbm_param_ptr=&isp_context_ptr->awbm;
	struct isp_awb_param* awb_param_ptr=&isp_context_ptr->awb;
	struct isp_ae_param* ae_param_ptr=&isp_context_ptr->ae;

	isp_context_ptr->ae.bypass=ISP_EB;
	isp_context_ptr->ae.monitor_bypass=ISP_EB;
	isp_context_ptr->ae.cur_skip_num = ISP_AE_SKIP_FOREVER;

	awb_param_ptr->back_monitor_pos.x=awbm_param_ptr->win_start.x;
	awb_param_ptr->back_monitor_pos.y=awbm_param_ptr->win_start.y;
	awb_param_ptr->back_monitor_size.w=awbm_param_ptr->win_size.w;
	awb_param_ptr->back_monitor_size.h=awbm_param_ptr->win_size.h;

	awbm_param_ptr->win_start.x=pos.x;
	awbm_param_ptr->win_start.y=pos.y;
	awbm_param_ptr->win_size.w=win_size.w;
	awbm_param_ptr->win_size.h=win_size.h;

	isp_context_ptr->tune.wb_trim=ISP_EB;
	return rtn;
}

/* _ispAwbSetMonitorWinRecover --
*@
*@
*@ return:
*/
static uint32_t _ispAwbSetMonitorWinRecover(void* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	uint32_t handler_id = 0x00;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_awbm_param*awbm_param_ptr=&isp_context_ptr->awbm;
	struct isp_awb_param* awb_param_ptr=&isp_context_ptr->awb;
	struct isp_ae_param* ae_param_ptr=&isp_context_ptr->ae;

	awbm_param_ptr->win_start.x=awb_param_ptr->back_monitor_pos.x;
	awbm_param_ptr->win_start.y=awb_param_ptr->back_monitor_pos.y;
	awbm_param_ptr->win_size.w=awb_param_ptr->back_monitor_size.w;
	awbm_param_ptr->win_size.h=awb_param_ptr->back_monitor_size.h;

	isp_context_ptr->tune.wb_trim=ISP_EB;

	ae_param_ptr->bypass=isp_context_ptr->ae.back_bypass;

	if (ISP_UEB == ae_param_ptr->bypass) {
		ae_param_ptr->monitor_conter=ae_param_ptr->skip_frame;
		ae_param_ptr->monitor_bypass=ISP_UEB;
	} else {
		ae_param_ptr->monitor_bypass=ISP_EB;
	}

	ISP_LOG("test_bypass:0x%x,:0x%x, ae bypass:%d, awb bypass:%d", ae_param_ptr->bypass, awb_param_ptr->bypass, ae_param_ptr->monitor_bypass, awb_param_ptr->monitor_bypass);

	return rtn;
}


/* _ispWbTrimParamValid --
*@
*@
*@ return:
*/
static int32_t _ispWbTrimParamValid(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);

	if(ISP_EB==isp_context_ptr->wb_trim_valid) {
		isp_context_ptr->ae.monitor.w=isp_context_ptr->awbm.win_size.w;
		isp_context_ptr->ae.monitor.h=isp_context_ptr->awbm.win_size.h;
		isp_context_ptr->wb_trim_valid=ISP_UEB;
	}

	return rtn;
}

/* _ispParamValid --
*@
*@
*@ return:
*/
static int32_t _ispParamValid(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;

	_ispWbTrimParamValid(handler_id);

	return rtn;
}

/* _ispSofWbTrimHandler --
*@
*@
*@ return:
*/
static int32_t _ispSofWbTrimHandler(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_tune_block* isp_tune_ptr=&isp_context_ptr->tune;

	if (ISP_AWB_SKIP_FOREVER!=isp_context_ptr->wb_trim_conter) {
		isp_context_ptr->wb_trim_conter++;
		if (ISP_AWB_TRIM_CONTER==isp_context_ptr->wb_trim_conter) {
			isp_context_ptr->wb_trim_conter=ISP_AWB_SKIP_FOREVER;
			isp_context_ptr->tune.wb_trim=ISP_EB;
		}
	}

	return rtn;
}

static void _ispCtrlAWB(uint32_t handler_id)
{
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_ae_param* ae_param_ptr=&isp_context_ptr->ae;
	struct isp_af_param* af_param_ptr=&isp_context_ptr->af;

	isp_context_ptr->ae.bypass=af_param_ptr->ae_status;
	if (ISP_UEB == isp_context_ptr->ae.bypass) {
		if (3 == af_param_ptr->alg_id) {
			struct isp_awbm_param temp;
			memcpy(&temp,&isp_context_ptr->awbm,sizeof(isp_context_ptr->awbm));
			temp.bypass = ISP_EB;
			ae_param_ptr->monitor_conter=ae_param_ptr->skip_frame+2;
			af_param_ptr->CfgAwbm(handler_id, &temp);

		} else {
			ae_param_ptr->monitor_conter=ae_param_ptr->skip_frame;
		}
		isp_context_ptr->ae.monitor_bypass = ISP_UEB;
	} else {
		isp_context_ptr->ae.monitor_bypass = ISP_EB;
	}

	isp_context_ptr->awb.bypass=af_param_ptr->awb_status;
	if (ISP_UEB == isp_context_ptr->awb.bypass) {
		isp_context_ptr->awb.monitor_bypass = ISP_UEB;
	} else {
		isp_context_ptr->awb.monitor_bypass = ISP_EB;
	}

	af_param_ptr->monitor_bypass = ISP_EB;
	af_param_ptr->end_handler_flag = ISP_EB;
	/*ISP_LOG("ae.bypass %d,ae.monitor_bypass %d,awb.bypass %d,awb.monitor_bypass %d.",
			isp_context_ptr->ae.bypass,isp_context_ptr->ae.monitor_bypass,
			isp_context_ptr->awb.bypass,isp_context_ptr->awb.monitor_bypass);*/
}

static int32_t _ispSofAWBWindowHandler(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_tune_block* isp_tune_ptr=&isp_context_ptr->tune;

	if (ISP_AWB_SKIP_FOREVER != isp_context_ptr->awb_win_conter) {
		ISP_LOG("wait to open ae");
		isp_context_ptr->awb_win_conter++;
		if (ISP_AWB_WINDOW_CONTER == isp_context_ptr->awb_win_conter) {
			isp_context_ptr->awb_win_conter = ISP_AWB_SKIP_FOREVER;
			ISP_LOG("open ae");
			_ispCtrlAWB(handler_id);
		}
	}

	return rtn;
}

/* _ispSofConterHandler --
*@
*@
*@ return:
*/
static int32_t _ispSofConterHandler(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;

	_ispSofWbTrimHandler(handler_id);
	_ispSofAWBWindowHandler(handler_id);

	return rtn;
}

/* _ispGetVideoMode --
*@
*@
*@ return:
*/
static enum isp_video_mode _ispGetVideoMode(uint32_t handler_id)
{
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);

	return isp_context_ptr->cfg.video_mode;
}

/* _ispGetIspParamIndex --
*@
*@
*@ return:
*/
static uint32_t _ispGetIspParamIndex(uint32_t handler_id, struct isp_size* size)
{
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	uint32_t param_index=0x01;
	uint32_t i=0x00;

	for(i=0x01; i<ISP_INPUT_SIZE_NUM_MAX; i++) {
		if(size->h==isp_context_ptr->input_size_trim[i].trim_height) {
			param_index=i;
			break;
		}
	}

	return param_index;
}

/* _ispGetIspParamMaxIndex --
*@
*@
*@ return:
*/
static uint32_t _ispGetIspParamMaxIndex(uint32_t handler_id, struct sensor_raw_info* sensor_info_ptr)
{
	struct sensor_raw_resolution_info_tab* resolution_ptr=sensor_info_ptr->resolution_info_ptr;
	uint32_t param_index=0x01;
	uint32_t i=0x00;
	uint16_t img_height=0x00;


	for(i=0x01; i<ISP_INPUT_SIZE_NUM_MAX; i++) {

		if(img_height < resolution_ptr->tab[i].height) {
			img_height = resolution_ptr->tab[i].height;
			param_index = i;
		}
	}

	return param_index;
}

/* _ispAeTouchZone --
*@
*@
*@ return:
*/
static uint32_t _ispAeTouchZone(uint32_t handler_id, struct isp_pos_rect* param_ptr)
{
#define MONITOR_STAT_UP 0x01
#define MONITOR_STAT_LEFT 0x02
#define MONITOR_STAT_RIGHT 0x04
#define MONITOR_STAT_BOTTOM 0x08
#define MONITOR_WEIGHT_BASE 0x10

	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_ae_param* ae_param_ptr=&isp_context_ptr->ae;
	struct isp_pos* win_start=&isp_context_ptr->awbm.win_start;
	struct isp_size* win_size=&isp_context_ptr->awbm.win_size;
	uint32_t weight_id=ae_param_ptr->weight_id;
	int32_t x=0x0;
	int32_t y=0x0;
	uint16_t start_x=param_ptr->start_x;
	uint16_t start_y=param_ptr->start_y;
	uint16_t end_x=param_ptr->end_x;
	uint16_t end_y=param_ptr->end_y;
	uint16_t stat_num_x = ISP_AE_STAT_WIN_X;
	uint16_t stat_num_y = ISP_AE_STAT_WIN_Y;
	uint32_t pos_flag = 0x00;
	int32_t i = 0x00;
	int32_t j = 0x00;
	uint32_t x_num = 0x00;
	uint32_t y_num = 0x00;
	uint32_t bord = 0x06;
	uint32_t half_bord = bord/2;

	if((ISP_ZERO==param_ptr->end_x)
		||(ISP_ZERO==param_ptr->end_y))
	{
		ISP_LOG("w:%d, h:%d error \n", param_ptr->end_x, param_ptr->end_y);
		rtn=ISP_PARAM_ERROR;
		return rtn;
	}

	if(ISP_ZERO==weight_id)
	{
		weight_id=ISP_ONE;
	}
	else
	{
		weight_id=ISP_ZERO;
	}

	memset((void*)&ae_param_ptr->weight_tab[weight_id], 0x00, 1024);

	for (y=0x00; y<ISP_AE_STAT_WIN_Y; y++) {
		for (x=0x00; x<ISP_AE_STAT_WIN_X; x++) {
			if((start_x<=(win_start->x+(win_size->w*x)))
				&&(end_x>=(win_start->x+(win_size->w*(x+1)))))
			{
				if((start_y<=(win_start->y+(win_size->h*y)))
					&&(end_y>=(win_start->x+(win_size->h*(y+1)))))
				{
					ae_param_ptr->weight_tab[weight_id][ISP_AE_STAT_WIN_X*y+x]=MONITOR_WEIGHT_BASE;
				}
			}
		}
	}

	for (y=0x00; y<stat_num_y; y++) {
		for (x=0x00; x<stat_num_x; x++) {
			if(MONITOR_WEIGHT_BASE == ae_param_ptr->weight_tab[weight_id][stat_num_x*y+x]) {
				pos_flag = 0x00;
				/*check the touch pos*/
				if ((0x00 < x)
					&&(MONITOR_WEIGHT_BASE != ae_param_ptr->weight_tab[weight_id][stat_num_x*y+(x-0x01)])) {
					pos_flag |= MONITOR_STAT_LEFT;
				}
				if ((0x00 < y)
					&&(MONITOR_WEIGHT_BASE != ae_param_ptr->weight_tab[weight_id][stat_num_x*(y-0x01)+x])) {
					pos_flag |= MONITOR_STAT_UP;
				}
				if ((x < (stat_num_x-1))
					&&(MONITOR_WEIGHT_BASE != ae_param_ptr->weight_tab[weight_id][stat_num_x*y+(x+0x01)])) {
					pos_flag |= MONITOR_STAT_RIGHT;
				}
				if ((y < (stat_num_y-1))
					&&(MONITOR_WEIGHT_BASE != ae_param_ptr->weight_tab[weight_id][stat_num_x*(y+0x01)+x])) {
					pos_flag |= MONITOR_STAT_BOTTOM;
				}

				/*check the touch pos*/
				if ((MONITOR_STAT_LEFT == (pos_flag&MONITOR_STAT_LEFT))
					&& (MONITOR_STAT_UP == (pos_flag&MONITOR_STAT_UP))) {

					for (j=y, y_num=0x00; y_num <=bord ; y_num++, j--) {
						for (i=x, x_num=0x00; x_num <=bord ; x_num++, i--) {
							if((0x00 <= j) && (0x00 <= i)) {
								if(((int32_t)(y-half_bord) <= j) && ((int32_t)(x-half_bord) <= i)) {
									ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/2;
								} else {
									ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/4;
								}
							}
						}
					}
				} else if ((MONITOR_STAT_UP == (pos_flag&MONITOR_STAT_UP))
					&& (MONITOR_STAT_RIGHT == (pos_flag&MONITOR_STAT_RIGHT))) {

					for (j=y, y_num=0x00; y_num <=bord ; y_num++, j--) {
						for (i=x, x_num=0x00; x_num <=bord ; x_num++, i++) {
							if((0x00 <= j) && (stat_num_x > i)) {
								if(((int32_t)(y-half_bord) <= j) && ((int32_t)(x+half_bord) >= i)) {
									ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/2;
								} else {
									ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/4;
								}
							}
						}
					}
				} else if ((MONITOR_STAT_LEFT == (pos_flag&MONITOR_STAT_LEFT))
					&& (MONITOR_STAT_BOTTOM == (pos_flag&MONITOR_STAT_BOTTOM))) {

					for (j=y, y_num=0x00; y_num <=bord ; y_num++, j++) {
						for (i=x, x_num=0x00; x_num <=bord ; x_num++, i--) {
							if((stat_num_y > j) && (0x00 <=i)) {
								if (((int32_t)(y+half_bord) >= j) && ((int32_t)(x-half_bord) <= i)) {
									ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/2;
								} else {
									ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/4;
								}
							}
						}
					}
				} else if ((MONITOR_STAT_RIGHT == (pos_flag&MONITOR_STAT_RIGHT))
					&& (MONITOR_STAT_BOTTOM == (pos_flag&MONITOR_STAT_BOTTOM))) {
					for (j=y, y_num=0x00; y_num <=bord ; y_num++, j++) {
						for (i=x, x_num=0x00; x_num <=bord ; x_num++, i++) {
							if ((stat_num_y > j) && (stat_num_x > i)) {
								if (((int32_t)(y+half_bord) >= j) &&((int32_t)(x+half_bord) >= i)) {
									ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/2;
								} else {
									ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/4;
								}
							}
						}
					}
				} else if (MONITOR_STAT_LEFT == (pos_flag&MONITOR_STAT_LEFT)) {
					for (j=y, i=x, x_num=0x00; x_num <=bord ; x_num++, i--) {
						if (0x00 <=i) {
							if((int32_t)(x-half_bord) <= i) {
								ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/2;
							} else {
								ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/4;
							}
						}
					}
				} else if (MONITOR_STAT_UP == (pos_flag&MONITOR_STAT_UP)) {
					for (j=y, i=x, y_num=0x00; y_num <=bord ; y_num++, j--) {
						if (0x00 <=j) {
							if((int32_t)(y-half_bord) <= j) {
								ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/2;
							} else {
								ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/4;
							}
						}
					}
				} else if (MONITOR_STAT_RIGHT == (pos_flag&MONITOR_STAT_RIGHT)) {
					for (j = y, i = x, x_num=0x00; x_num <=bord ; x_num++, i++) {
						if (stat_num_x > i) {
							if ((int32_t)(x+half_bord) >= i) {
								ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/2;
							} else {
								ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/4;
							}
						}
					}
				} else if (MONITOR_STAT_BOTTOM == (pos_flag&MONITOR_STAT_BOTTOM)) {
					for (j = y, i = x, y_num=0x00; y_num <=bord ; y_num++, j++) {
						if (stat_num_y > j ) {
							if ((int32_t)(y+half_bord) >= j) {
								ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/2;
							} else {
								ae_param_ptr->weight_tab[weight_id][stat_num_x*j+i]=MONITOR_WEIGHT_BASE/4;
							}
						}
					}
				}

				ae_param_ptr->weight_tab[weight_id][stat_num_x*y+x]=MONITOR_WEIGHT_BASE;

			}
		}
	}

	ae_param_ptr->weight_id=weight_id;
	ae_param_ptr->cur_weight_ptr=ae_param_ptr->weight_tab[weight_id];
	ae_param_ptr->stab_conter=ISP_ZERO;
	ae_param_ptr->weight_eb = ISP_EB;

	return rtn;
}

/* _ispAeMeasureLumSet --
*@
*@
*@ return:
*/
static uint32_t _ispAeMeasureLumSet(uint32_t handler_id, enum isp_ae_weight weight_mode)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_ae_param* ae_param_ptr=&isp_context_ptr->ae;
	uint32_t weight_id=ae_param_ptr->weight_id;
	uint32_t weight=weight_mode;

	if(ISP_ZERO==weight_id)
	{
		weight_id=ISP_ONE;
	}
	else
	{
		weight_id=ISP_ZERO;
	}

	memcpy((void*)&ae_param_ptr->weight_tab[weight_id], (void*)ae_param_ptr->weight_ptr[weight], 1024);

	ae_param_ptr->weight_id=weight_id;
	ae_param_ptr->cur_weight_ptr=ae_param_ptr->weight_tab[weight_id];
	ae_param_ptr->weight_eb = ISP_EB;

	return rtn;
}

/* _ispAwbMeasureLumSet --
*@
*@
*@ return:
*/
static uint32_t _ispAwbMeasureLumSet(uint32_t handler_id, enum isp_ae_weight weight_mode)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_awb_param* awb_param_ptr=&isp_context_ptr->awb;
	uint32_t weight_id=awb_param_ptr->weight_id;
	uint32_t weight=weight_mode;

	if(ISP_ZERO==weight_id)
	{
		weight_id=ISP_ONE;
	}
	else
	{
		weight_id=ISP_ZERO;
	}

	memcpy((void*)&awb_param_ptr->weight_tab[weight_id], (void*)awb_param_ptr->weight_ptr[weight], 1024);

	awb_param_ptr->weight_id=weight_id;

	return rtn;
}

/* _ispAeInfo --
*@
*@
*@ return:
*/
static uint32_t _ispAeInfo(uint32_t handler_id, struct isp_ae_info*param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_ae_param* ae_param_ptr=&isp_context_ptr->ae;
	struct isp_ae_information *ae_info_ptr = (struct isp_ae_information*)&ae_param_ptr->ae_info;

	ae_param_ptr->line_time=param_ptr->line_time;

	ae_info_ptr->eb = ISP_EB;
	ae_info_ptr->min_frm_rate = param_ptr->min_fps;
	ae_info_ptr->max_frm_rate = param_ptr->max_fps;
	ae_info_ptr->line_time = param_ptr->line_time;
	ae_info_ptr->gain = param_ptr->gain;

	return rtn;
}

/* _isp3AInit --
*@
*@
*@ return:
*/
static uint32_t _isp3AInit(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;

	ISP_LOG("_isp3AInit \n");

	if(ISP_VIDEO_MODE_CONTINUE==_ispGetVideoMode(handler_id))
	{
		rtn = isp_awb_init(handler_id);
		ISP_TRACE_IF_FAIL(rtn, ("isp_awb_init error"));
		_ispAwbCorrect(handler_id);

		_ispAeInfoSet(handler_id);
		rtn = isp_ae_init(handler_id);
		ISP_TRACE_IF_FAIL(rtn, ("isp_ae_init error"));

		rtn = isp_af_init(handler_id);
		ISP_TRACE_IF_FAIL(rtn, ("isp_af_init error"));
	}

	return rtn;
}

/* _isp3ADdInit --
*@
*@
*@ return:
*/
static uint32_t _isp3ADeInit(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;

	rtn = isp_ae_deinit(handler_id);
	ISP_TRACE_IF_FAIL(rtn, ("isp_ae_deinit error"));

	rtn = isp_awb_deinit(handler_id);
	ISP_TRACE_IF_FAIL(rtn, ("isp_awb_deinit error"));

	rtn = isp_af_deinit(handler_id);
	ISP_TRACE_IF_FAIL(rtn, ("isp_af_deinit error"));
	return rtn;
}

/* _ispAeCalculation --
*@
*@
*@ return:
*/
static uint32_t _ispAeCalculation(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;

	rtn = isp_ae_calculation(handler_id);

	return rtn;
}

/* _ispAwbCalculation --
*@
*@
*@ return:
*/
static uint32_t _ispAwbCalculation(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;

	rtn = isp_awb_calculation();

	return rtn;
}

/* _ispAeAwbCorrect --
*@
*@
*@ return:
*/
static uint32_t _ispAeAwbCorrect(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;

	rtn = isp_ae_calculation(handler_id);

	rtn = isp_awb_calculation();

	return rtn;
}

/* _ispFetchFormat --
*@
*@
*@ return:
*/
static enum isp_fetch_format _ispFetchFormat(enum isp_format in_format)
{
	enum isp_fetch_format format=ISP_FETCH_FORMAT_MAX;

	switch(in_format)
	{
		case ISP_DATA_YUV422_3FRAME:
		{
			format=ISP_FETCH_YUV422_3FRAME;
			break;
		}
		case ISP_DATA_YUYV:
		{
			format=ISP_FETCH_YUYV;
			break;
		}
		case ISP_DATA_UYVY:
		{
			format=ISP_FETCH_UYVY;
			break;
		}
		case ISP_DATA_YVYU:
		{
			format=ISP_FETCH_YVYU;
			break;
		}
		case ISP_DATA_VYUY:
		{
			format=ISP_FETCH_VYUY;
			break;
		}
		case ISP_DATA_YUV422_2FRAME:
		{
			format=ISP_FETCH_YUV422_2FRAME;
			break;
		}
		case ISP_DATA_YVU422_2FRAME:
		{
			format=ISP_FETCH_YVU422_2FRAME;
			break;
		}
		case ISP_DATA_NORMAL_RAW10:
		{
			format=ISP_FETCH_NORMAL_RAW10;
			break;
		}
		case ISP_DATA_CSI2_RAW10:
		{
			format=ISP_FETCH_CSI2_RAW10;
			break;
		}
		default :
		{
			break;
		}
	}

	return format;
}

/* _ispFeederDataType --
*@
*@
*@ return:
*/
static uint32_t _ispFeederDataType(enum isp_format in_format)
{
	uint32_t data_type=ISP_ZERO;

	switch(in_format)
	{
		case ISP_DATA_YUV422_3FRAME:
		case ISP_DATA_YUYV:
		case ISP_DATA_UYVY:
		case ISP_DATA_YVYU:
		case ISP_DATA_VYUY:
		case ISP_DATA_YUV422_2FRAME:
		case ISP_DATA_YVU422_2FRAME:
		{
			data_type=ISP_ONE;
			break;
		}
		case ISP_DATA_NORMAL_RAW10:
		case ISP_DATA_CSI2_RAW10:
		{
			data_type=ISP_ZERO;
			break;
		}
		default :
		{
			break;
		}
	}

	return data_type;
}

/* _ispStoreFormat --
*@
*@
*@ return:
*/
static enum isp_store_format _ispStoreFormat(enum isp_format in_format)
{
	enum isp_store_format format=ISP_STORE_FORMAT_MAX;

	switch(in_format)
	{
		case ISP_DATA_UYVY:
		{
			format=ISP_STORE_UYVY;
			break;
		}
		case ISP_DATA_YUV422_2FRAME:
		{
			format=ISP_STORE_YUV422_2FRAME;
			break;
		}
		case ISP_DATA_YVU422_2FRAME:
		{
			format=ISP_STORE_YVU422_2FRAME;
			break;
		}
		case ISP_DATA_YUV422_3FRAME:
		{
			format=ISP_STORE_YUV422_3FRAME;
			break;
		}
		case ISP_DATA_YUV420_2FRAME:
		{
			format=ISP_STORE_YUV420_2FRAME;
			break;
		}
		case ISP_DATA_YVU420_2FRAME:
		{
			format=ISP_STORE_YVU420_2FRAME;
			break;
		}
		case ISP_DATA_YUV420_3_FRAME:
		{
			format=ISP_STORE_YUV420_3FRAME;
			break;
		}
		default :
		{
			break;
		}
	}

	return format;
}

/* _ispGetByerPattern --
*@
*@
*@ return:
*/
static uint32_t _ispGetByerPattern(uint32_t bayer_mode, uint32_t start_x,uint32_t start_y)
{
	uint32_t bayer=bayer_mode;

	if(0x01&start_x)
	{
		bayer^=0x01;
	}

	if(0x01&start_y)
	{
		bayer^=0x02;
	}

	return bayer;
}

/* _ispChangeByerPattern --
*@
*@
*@ return:
*/
static uint32_t _ispChangeByerPattern(uint32_t handler_id, uint32_t* nlc_bayer, uint32_t* awbc_bayer,uint32_t* wave_bayer,uint32_t* cfa_bayer,uint32_t* gain_bayer)
{
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_slice_param* slice_ptr=&isp_context_ptr->slice;
	struct isp_size* cur_slice_ptr=&isp_context_ptr->slice.cur_slice_num;
	uint32_t bayer_mode=isp_context_ptr->com.bayer_pattern;

	*nlc_bayer=_ispGetByerPattern(bayer_mode, slice_ptr->size[ISP_BNLC].x, slice_ptr->size[ISP_BNLC].y);
	*awbc_bayer=*nlc_bayer;
	*gain_bayer=_ispGetByerPattern(bayer_mode, slice_ptr->size[ISP_GLB_GAIN].x, slice_ptr->size[ISP_GLB_GAIN].y);
	*wave_bayer=*gain_bayer;
	*cfa_bayer=_ispGetByerPattern(bayer_mode, slice_ptr->size[ISP_CFA].x, slice_ptr->size[ISP_CFA].y);

	return ISP_SUCCESS;
}

/* _ispGetStoreAddr --
*@
*@
*@ return:
*/
static int32_t _ispGetStoreAddr(uint32_t handler_id, struct isp_store_param* store_ptr)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_size* cur_slice_ptr = &isp_context_ptr->slice.cur_slice_num;
	struct isp_size* all_slice_ptr = &isp_context_ptr->slice.all_slice_num;
	uint32_t ch0_offset = ISP_ZERO;
	uint32_t ch1_offset = ISP_ZERO;
	uint32_t ch2_offset = ISP_ZERO;
	uint16_t src_width = isp_context_ptr->src.w;
	uint16_t slice_width = isp_context_ptr->slice.max_size.w;
	uint16_t slice_height = isp_context_ptr->slice.max_size.h;

	store_ptr->bypass=isp_context_ptr->store.bypass;
	store_ptr->sub_stract=isp_context_ptr->store.sub_stract;
	store_ptr->addr.chn0=isp_context_ptr->store.addr.chn0;
	store_ptr->addr.chn1=isp_context_ptr->store.addr.chn1;
	store_ptr->addr.chn2=isp_context_ptr->store.addr.chn2;
	store_ptr->pitch.chn0=isp_context_ptr->store.pitch.chn0;
	store_ptr->pitch.chn1=isp_context_ptr->store.pitch.chn1;
	store_ptr->pitch.chn2=isp_context_ptr->store.pitch.chn2;

	switch(isp_context_ptr->com.store_yuv_format)
	{
		case ISP_STORE_YUV422_3FRAME:
		case ISP_STORE_YUV420_3FRAME:
		{
			ch0_offset=slice_width*cur_slice_ptr->w;
			ch1_offset=(slice_width*cur_slice_ptr->w)>>0x01;
			ch2_offset=(slice_width*cur_slice_ptr->w)>>0x01;
			break;
		}
		case ISP_STORE_YUV422_2FRAME:
		case ISP_STORE_YVU422_2FRAME:
		case ISP_STORE_YUV420_2FRAME:
		case ISP_STORE_YVU420_2FRAME:
		{
			ch0_offset=slice_width*cur_slice_ptr->w;
			ch1_offset=slice_width*cur_slice_ptr->w;
			break;
		}
		case ISP_STORE_UYVY:
		{
			ch0_offset=(slice_width*cur_slice_ptr->w)<<0x01;
			break;
		}
		default :
		{
			break;
		}
	}

	store_ptr->addr.chn0+=ch0_offset;
	store_ptr->addr.chn1+=ch1_offset;
	store_ptr->addr.chn2+=ch2_offset;

	return rtn;
}

/* _ispGetLensGridMode --
*@
*@
*@ return:
*/
static uint8_t _ispGetLensGridMode(uint8_t grid)
{
	uint8_t mode=0x00;

	switch(grid)
	{
		case 16:
		{
			mode=ISP_ZERO;
			break;
		}
		case 32:
		{
			mode=ISP_ONE;
			break;
		}
		case 64:
		{
			mode=ISP_TWO;
			break;
		}
		default :
		{
			break;
		}
	}

	return mode;
}

/* _ispCfgAfWin --
*@
*@
*@ return:
*/
static int32_t _ispCfgAfWin(uint32_t handler_id, struct isp_af_param* af, struct isp_af_win* src_af)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	uint32_t max_win_num = ispGetAfMaxNum(handler_id);
	uint8_t i = ISP_ZERO;

	if((ISP_ZERO == src_af->valid_win) || (ISP_FOCUS_CONTINUE == src_af->mode)) {
		af->valid_win = 1;
		af->win[i][0]=((((isp_context_ptr->src.w>>ISP_ONE)-(isp_context_ptr->src.w/10))>>ISP_ONE)<<ISP_ONE);
		af->win[i][1]=((((isp_context_ptr->src.h>>ISP_ONE)-(isp_context_ptr->src.h/10))>>ISP_ONE)<<ISP_ONE);
		af->win[i][2]=((((isp_context_ptr->src.w>>ISP_ONE)+(isp_context_ptr->src.w/10))>>ISP_ONE)<<ISP_ONE);
		af->win[i][3]=((((isp_context_ptr->src.h>>ISP_ONE)+(isp_context_ptr->src.h/10))>>ISP_ONE)<<ISP_ONE);
	} else {
		if((ISP_ZERO!=src_af->valid_win)
			&&(max_win_num>=src_af->valid_win)) {
			af->valid_win = src_af->valid_win;
			for(i = 0x00; i < src_af->valid_win; i++) {
				af->win[i][0]=src_af->win[i].start_x;
				af->win[i][1]=src_af->win[i].start_y;
				af->win[i][2]=src_af->win[i].end_x;
				af->win[i][3]=src_af->win[i].end_y;
			}
		} else {
			af->valid_win = 1;
			af->win[i][0]=((((isp_context_ptr->src.w>>ISP_ONE)-(isp_context_ptr->src.w/10))>>ISP_ONE)<<ISP_ONE);
			af->win[i][1]=((((isp_context_ptr->src.h>>ISP_ONE)-(isp_context_ptr->src.h/10))>>ISP_ONE)<<ISP_ONE);
			af->win[i][2]=((((isp_context_ptr->src.w>>ISP_ONE)+(isp_context_ptr->src.w/10))>>ISP_ONE)<<ISP_ONE);
			af->win[i][3]=((((isp_context_ptr->src.h>>ISP_ONE)+(isp_context_ptr->src.h/10))>>ISP_ONE)<<ISP_ONE);
		}
	}

	return rtn;
}

/* _ispSetGamma --
*@
*@
*@ return:
*/
static int32_t _ispSetGamma(struct isp_gamma_param* gamma, struct isp_gamma_tab* tab_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	gamma->axis[0][0]=tab_ptr->axis[0][0];
	gamma->axis[0][1]=tab_ptr->axis[0][1];
	gamma->axis[0][2]=tab_ptr->axis[0][2];
	gamma->axis[0][3]=tab_ptr->axis[0][3];
	gamma->axis[0][4]=tab_ptr->axis[0][4];
	gamma->axis[0][5]=tab_ptr->axis[0][5];
	gamma->axis[0][6]=tab_ptr->axis[0][6];
	gamma->axis[0][7]=tab_ptr->axis[0][7];
	gamma->axis[0][8]=tab_ptr->axis[0][8];
	gamma->axis[0][9]=tab_ptr->axis[0][9];
	gamma->axis[0][10]=tab_ptr->axis[0][10];
	gamma->axis[0][11]=tab_ptr->axis[0][11];
	gamma->axis[0][12]=tab_ptr->axis[0][12];
	gamma->axis[0][13]=tab_ptr->axis[0][13];
	gamma->axis[0][14]=tab_ptr->axis[0][14];
	gamma->axis[0][15]=tab_ptr->axis[0][15];
	gamma->axis[0][16]=tab_ptr->axis[0][16];
	gamma->axis[0][17]=tab_ptr->axis[0][17];
	gamma->axis[0][18]=tab_ptr->axis[0][18];
	gamma->axis[0][19]=tab_ptr->axis[0][19];
	gamma->axis[0][20]=tab_ptr->axis[0][20];
	gamma->axis[0][21]=tab_ptr->axis[0][21];
	gamma->axis[0][22]=tab_ptr->axis[0][22];
	gamma->axis[0][23]=tab_ptr->axis[0][23];
	gamma->axis[0][24]=tab_ptr->axis[0][24];
	gamma->axis[0][25]=tab_ptr->axis[0][25];

	gamma->axis[1][0]=tab_ptr->axis[1][0];
	gamma->axis[1][1]=tab_ptr->axis[1][1];
	gamma->axis[1][2]=tab_ptr->axis[1][2];
	gamma->axis[1][3]=tab_ptr->axis[1][3];
	gamma->axis[1][4]=tab_ptr->axis[1][4];
	gamma->axis[1][5]=tab_ptr->axis[1][5];
	gamma->axis[1][6]=tab_ptr->axis[1][6];
	gamma->axis[1][7]=tab_ptr->axis[1][7];
	gamma->axis[1][8]=tab_ptr->axis[1][8];
	gamma->axis[1][9]=tab_ptr->axis[1][9];
	gamma->axis[1][10]=tab_ptr->axis[1][10];
	gamma->axis[1][11]=tab_ptr->axis[1][11];
	gamma->axis[1][12]=tab_ptr->axis[1][12];
	gamma->axis[1][13]=tab_ptr->axis[1][13];
	gamma->axis[1][14]=tab_ptr->axis[1][14];
	gamma->axis[1][15]=tab_ptr->axis[1][15];
	gamma->axis[1][16]=tab_ptr->axis[1][16];
	gamma->axis[1][17]=tab_ptr->axis[1][17];
	gamma->axis[1][18]=tab_ptr->axis[1][18];
	gamma->axis[1][19]=tab_ptr->axis[1][19];
	gamma->axis[1][20]=tab_ptr->axis[1][20];
	gamma->axis[1][21]=tab_ptr->axis[1][21];
	gamma->axis[1][22]=tab_ptr->axis[1][22];
	gamma->axis[1][23]=tab_ptr->axis[1][23];
	gamma->axis[1][24]=tab_ptr->axis[1][24];
	gamma->axis[1][25]=tab_ptr->axis[1][25];

	gamma->index[0]=_ispLog2n(gamma->axis[0][1]-gamma->axis[0][0]);
	gamma->index[1]=_ispLog2n(gamma->axis[0][2]-gamma->axis[0][1]);
	gamma->index[2]=_ispLog2n(gamma->axis[0][3]-gamma->axis[0][2]);
	gamma->index[3]=_ispLog2n(gamma->axis[0][4]-gamma->axis[0][3]);
	gamma->index[4]=_ispLog2n(gamma->axis[0][5]-gamma->axis[0][4]);
	gamma->index[5]=_ispLog2n(gamma->axis[0][6]-gamma->axis[0][5]);
	gamma->index[6]=_ispLog2n(gamma->axis[0][7]-gamma->axis[0][6]);
	gamma->index[7]=_ispLog2n(gamma->axis[0][8]-gamma->axis[0][7]);
	gamma->index[8]=_ispLog2n(gamma->axis[0][9]-gamma->axis[0][8]);
	gamma->index[9]=_ispLog2n(gamma->axis[0][10]-gamma->axis[0][9]);
	gamma->index[10]=_ispLog2n(gamma->axis[0][11]-gamma->axis[0][10]);
	gamma->index[11]=_ispLog2n(gamma->axis[0][12]-gamma->axis[0][11]);
	gamma->index[12]=_ispLog2n(gamma->axis[0][13]-gamma->axis[0][12]);
	gamma->index[13]=_ispLog2n(gamma->axis[0][14]-gamma->axis[0][13]);
	gamma->index[14]=_ispLog2n(gamma->axis[0][15]-gamma->axis[0][14]);
	gamma->index[15]=_ispLog2n(gamma->axis[0][16]-gamma->axis[0][15]);
	gamma->index[16]=_ispLog2n(gamma->axis[0][17]-gamma->axis[0][16]);
	gamma->index[17]=_ispLog2n(gamma->axis[0][18]-gamma->axis[0][17]);
	gamma->index[18]=_ispLog2n(gamma->axis[0][19]-gamma->axis[0][18]);
	gamma->index[19]=_ispLog2n(gamma->axis[0][20]-gamma->axis[0][19]);
	gamma->index[20]=_ispLog2n(gamma->axis[0][21]-gamma->axis[0][20]);
	gamma->index[21]=_ispLog2n(gamma->axis[0][22]-gamma->axis[0][21]);
	gamma->index[22]=_ispLog2n(gamma->axis[0][23]-gamma->axis[0][22]);
	gamma->index[23]=_ispLog2n(gamma->axis[0][24]-gamma->axis[0][23]);
	gamma->index[24]=_ispLog2n(gamma->axis[0][25]-gamma->axis[0][24]+1);

	return rtn;
}

/* _ispSetCceMatrix --
*@
*@
*@ return:
*/
static int32_t _ispSetCceMatrix(struct isp_cce_matrix* cce, struct isp_cce_matrix* cce_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	cce->matrix[0]=cce_ptr->matrix[0];
	cce->matrix[1]=cce_ptr->matrix[1];
	cce->matrix[2]=cce_ptr->matrix[2];
	cce->matrix[3]=cce_ptr->matrix[3];
	cce->matrix[4]=cce_ptr->matrix[4];
	cce->matrix[5]=cce_ptr->matrix[5];
	cce->matrix[6]=cce_ptr->matrix[6];
	cce->matrix[7]=cce_ptr->matrix[7];
	cce->matrix[8]=cce_ptr->matrix[8];

	cce->y_shift=cce_ptr->y_shift;
	cce->u_shift=cce_ptr->u_shift;
	cce->v_shift=cce_ptr->v_shift;

	return rtn;
}

/* _ispSetDenoise --
*@
*@
*@ return:
*/
static int32_t _ispSetDenoise(struct isp_denoise_param* denoise_ptr, struct isp_denoise_param_tab* denoise_tab_ptr)
{
	int32_t rtn = ISP_SUCCESS;

	memcpy((void*)denoise_ptr->diswei, (void*)denoise_tab_ptr->diswei, sizeof(denoise_tab_ptr->diswei));
	memcpy((void*)denoise_ptr->ranwei, (void*)denoise_tab_ptr->ranwei, sizeof(denoise_tab_ptr->ranwei));

	return rtn;
}

/* _ispSetSmartParam
*@
*@
*@ return:
*/
static int32_t _ispSetSmartParam(struct isp_ae_param *ae_param_ptr, struct isp_smart_ae_param *smart_ae_ptr )
{
	int32_t rtn = ISP_SUCCESS;

	ae_param_ptr->smart = smart_ae_ptr->smart;
	ae_param_ptr->smart_mode = smart_ae_ptr->smart_mode;
	ae_param_ptr->smart_base_gain = smart_ae_ptr->smart_base_gain;
	ae_param_ptr->smart_wave_min = smart_ae_ptr->smart_wave_min;
	ae_param_ptr->smart_wave_max = smart_ae_ptr->smart_wave_max;
	ae_param_ptr->smart_pref_min = smart_ae_ptr->smart_pref_min;
	ae_param_ptr->smart_pref_max = smart_ae_ptr->smart_pref_max;
	ae_param_ptr->smart_denoise_min_index = smart_ae_ptr->smart_denoise_min_index;
	ae_param_ptr->smart_denoise_max_index = smart_ae_ptr->smart_denoise_max_index;
	ae_param_ptr->smart_edge_min_index = smart_ae_ptr->smart_edge_min_index;
	ae_param_ptr->smart_edge_max_index = smart_ae_ptr->smart_edge_max_index;
	ae_param_ptr->smart_sta_low_thr = smart_ae_ptr->smart_sta_low_thr;
	ae_param_ptr->smart_sta_ratio1 = smart_ae_ptr->smart_sta_ratio1;
	ae_param_ptr->smart_sta_ratio = smart_ae_ptr->smart_sta_ratio;

	return rtn;
}

/* _ispSetLncParam --
*@
*@
*@ return:
*/
static int32_t _ispSetLncParam(struct isp_lnc_param* param_ptr, struct isp_context* isp_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	uint32_t handler_id = ISP_ID_INVALID;

	if(ISP_ZERO==param_ptr->cur_use_buf)
	{
		param_ptr->load_buf=ISP_ONE;
	} else {
		param_ptr->load_buf=ISP_ZERO;
	}

	param_ptr->map.param_addr=isp_ptr->isp_lnc_addr;
	param_ptr->map.grid_pitch=isp_ptr->lnc_map_tab[isp_ptr->param_index-ISP_ONE][isp_ptr->awb.cur_index].grid_pitch;
	param_ptr->map.grid_mode=isp_ptr->lnc_map_tab[isp_ptr->param_index-ISP_ONE][isp_ptr->awb.cur_index].grid_mode;

	rtn=_ispGetLncAddr(param_ptr, &isp_ptr->slice, isp_ptr->src.w);
	ISP_RETURN_IF_FAIL(rtn, ("get lnc addr error"));

	return rtn;
}

/* _ispCfgFeatchData --
*@
*@
*@ return:
*/
static int32_t _ispCfgFeatchData(uint32_t handler_id, struct isp_fetch_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispFetchSubtract(handler_id, param_ptr->sub_stract);
	ispSetFetchYAddr(handler_id, param_ptr->addr.chn0);
	ispSetFetchYPitch(handler_id, param_ptr->pitch.chn0);
	ispSetFetchUAddr(handler_id, param_ptr->addr.chn1);
	ispSetFetchUPitch(handler_id, param_ptr->pitch.chn1);
	ispSetFetchMipiWordInfo(handler_id, param_ptr->mipi_word_num);
	ispSetFetchMipiByteInfo(handler_id, param_ptr->mipi_byte_rel_pos);
	ispSetFetchVAddr(handler_id, param_ptr->addr.chn2);
	ispSetFetchVPitch(handler_id, param_ptr->pitch.chn2);
	ispFetchBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgStoreData --
*@
*@
*@ return:
*/
static int32_t _ispCfgStoreData(uint32_t handler_id, struct isp_store_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispStoreSubtract(handler_id, param_ptr->sub_stract);
	ispSetStoreYAddr(handler_id, param_ptr->addr.chn0);
	ispSetStoreYPitch(handler_id, param_ptr->pitch.chn0);
	ispSetStoreUAddr(handler_id, param_ptr->addr.chn1);
	ispSetStoreUPitch(handler_id, param_ptr->pitch.chn1);
	ispSetStoreVAddr(handler_id, param_ptr->addr.chn2);
	ispSetStoreVPitch(handler_id, param_ptr->pitch.chn2);
	ispStoreBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgFeeder --
*@
*@
*@ return:
*/
static int32_t _ispCfgFeeder(uint32_t handler_id, struct isp_feeder_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetFeederDataType(handler_id, param_ptr->data_type);

	return rtn;
}

/* _ispCfgComData --
*@
*@
*@ return:
*/
static int32_t _ispCfgComData(uint32_t handler_id, struct isp_com_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	uint32_t nlc_bayer;
	uint32_t awbc_bayer;
	uint32_t wave_bayer;
	uint32_t cfa_bayer;
	uint32_t gain_bayer;

	ispInMode(handler_id, param_ptr->in_mode);
	ispFetchEdian(handler_id, param_ptr->fetch_endian, param_ptr->fetch_bit_reorder);
	ispFetchDataFormat(handler_id, param_ptr->fetch_color_format);
	ispBPCEdian(handler_id, param_ptr->bpc_endian);

	ispOutMode(handler_id, param_ptr->in_mode);
	ispStoreEdian(handler_id, param_ptr->store_endian);
	ispStoreFormat(handler_id, param_ptr->store_yuv_format);

	_ispChangeByerPattern(handler_id, & nlc_bayer, & awbc_bayer,& wave_bayer,& cfa_bayer,& gain_bayer);
	ispByerMode(handler_id, nlc_bayer, awbc_bayer, wave_bayer, cfa_bayer, gain_bayer);

	return rtn;
}

/* _ispCfgBlc --
*@
*@
*@ return:
*/
static int32_t _ispCfgBlc(uint32_t handler_id, struct isp_blc_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetBlcMode(handler_id, param_ptr->mode);
	ispSetBlcCalibration(handler_id, param_ptr->r, param_ptr->b, param_ptr->gr, param_ptr->gb);
	ispBlcBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgNlc --
*@
*@
*@ return:
*/
static int32_t _ispCfgNlc(uint32_t handler_id, struct isp_nlc_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetNlcRNode(handler_id, param_ptr->r_node);
	ispSetNlcGNode(handler_id, param_ptr->g_node);
	ispSetNlcBNode(handler_id, param_ptr->b_node);
	ispSetNlcLNode(handler_id, param_ptr->l_node);
	ispNlcBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgLnc --
*@
*@
*@ return:
*/
static int32_t _ispCfgLnc(uint32_t handler_id, struct isp_lnc_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetLensEndian(handler_id, ISP_ONE);
	ispLensBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispLncParamSet --
*@
*@
*@ return:
*/
static int32_t _ispLncParamSet(uint32_t handler_id, struct isp_lnc_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	//ispLensParamAddr((uint32_t)__pa(param_ptr->map.param_addr));
	ispSetLensGridPitch(handler_id, param_ptr->map.grid_pitch);
	ispSetLensGridMode(handler_id, param_ptr->map.grid_mode);
	ispSetLensBuf(handler_id, param_ptr->load_buf);

	return rtn;
}

/* _ispLncParamLoad --
*@
*@
*@ return:
*/
static int32_t _ispLncParamLoad(uint32_t handler_id, struct isp_lnc_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetLensLoaderEnable(handler_id, param_ptr->map.param_addr);

	return rtn;
}

/* _ispLncParamValid --
*@
*@
*@ return:
*/
static int32_t _ispLncParamValid(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_lnc_param* lnc_param_ptr = &isp_context_ptr->lnc;

	lnc_param_ptr->cur_use_buf=lnc_param_ptr->load_buf;
	ispLensBufSel(handler_id, lnc_param_ptr->cur_use_buf);

	return rtn;
}

/* _ispAfmEb --
*@
*@
*@ return:
*/
static int32_t _ispAfmEb(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_af_param* af_param_ptr = &isp_context_ptr->af;
	uint32_t cur_point=0x00;

	//ISP_LOG("af ----_ispAfmEb----- init:%d, bypass%d",af_param_ptr->init, af_param_ptr->monitor_bypass);

	if((ISP_UEB == af_param_ptr->monitor_bypass)
		&&(ISP_EB == af_param_ptr->init))
	{
		if(ISP_UEB == af_param_ptr->monitor_bypass)
		{
			ispAFMbypass(handler_id, ISP_UEB);
			af_param_ptr->monitor_bypass = ISP_EB;

		}
	}

	return rtn;
}

/* _ispAwbmEb --
*@
*@
*@ return:
*/
static int32_t _ispAwbmEb(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_awb_param* awb_param_ptr = &isp_context_ptr->awb;
	uint32_t isp_id = IspGetId();

	if(SC8830_ISP_ID==isp_id)
	{
		if((ISP_UEB == awb_param_ptr->bypass)&&(ISP_EB == awb_param_ptr->init))
		{
			if (ISP_UEB==awb_param_ptr->monitor_bypass) {
				ispAwbmSkip(handler_id, 2);//ISP_ZERO);
				ispAwbmBypass(handler_id, ISP_UEB);
				awb_param_ptr->monitor_bypass = ISP_UEB;
			}
		}
	}

	return rtn;
}

/* _ispAemEb --
*@
*@
*@ return:
*/
static int32_t _ispAemEb(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_ae_param* ae_param_ptr = &isp_context_ptr->ae;
	uint32_t skip_frame = ae_param_ptr->skip_frame;
	uint32_t isp_id=IspGetId();

	if(SC8830_ISP_ID==isp_id)
	{
		if(ISP_EB == ae_param_ptr->init)
		{
			if(ISP_UEB==ae_param_ptr->monitor_bypass) {
				if (ISP_ZERO < skip_frame) {
					skip_frame -= ISP_ZERO;
				}
				ispAemSkipNum(handler_id, skip_frame);
				ispAembypass(handler_id, ISP_UEB);
				ae_param_ptr->monitor_bypass = ISP_EB;
				isp_context_ptr->ae.monitor_conter = ISP_ZERO;
			}
		}
	}

	return rtn;
}

/* _ispAeAwbmEb --
*@
*@
*@ return:
*/
static int32_t _ispAeAwbmEb(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_ae_param* ae_param_ptr = &isp_context_ptr->ae;
	struct isp_awb_param* awb_param_ptr = &isp_context_ptr->awb;
	uint32_t isp_id = IspGetId();

	if(SC8825_ISP_ID==isp_id)
	{
		//ISP_LOG("_ispAeAwbmEb skip:%d, ae bypass:%d, awb bypass:%d", ae_param_ptr->cur_skip_num, ae_param_ptr->monitor_bypass, awb_param_ptr->monitor_bypass);

		if((ISP_EB == ae_param_ptr->init)
			||(ISP_EB == awb_param_ptr->init))
		{
			if(ISP_AE_SKIP_FOREVER!=ae_param_ptr->cur_skip_num)
			{
				if(ISP_ZERO == ae_param_ptr->cur_skip_num)
				{
					ispAwbmBypass(handler_id, ISP_EB);
					ae_param_ptr->cur_skip_num = ISP_AE_SKIP_FOREVER;
				}
				else
				{
					ae_param_ptr->cur_skip_num--;
				}
			}
		}

		if(((ISP_UEB == ae_param_ptr->bypass)&&(ISP_EB == ae_param_ptr->init))
			||((ISP_UEB == awb_param_ptr->bypass)&&(ISP_EB == awb_param_ptr->init)))
		{
			if((ISP_UEB==ae_param_ptr->monitor_bypass)
				||(ISP_UEB==awb_param_ptr->monitor_bypass))
			{
				ispAwbmBypass(handler_id, ISP_UEB);
				ae_param_ptr->monitor_bypass = ISP_EB;
				awb_param_ptr->monitor_bypass = ISP_EB;
				ae_param_ptr->cur_skip_num = ae_param_ptr->monitor_conter+ISP_TWO;
			}
		}

	}
	else if(SC8830_ISP_ID==isp_id)
	{
		if((ISP_EB == ae_param_ptr->bypass)
			&&(ISP_EB == ae_param_ptr->init))
			//&&(ISP_EB == ae_param_ptr->flash.eb))
		{
			if ((ISP_ZERO == ae_param_ptr->cur_skip_num)
				||(ISP_AE_SKIP_FOREVER == ae_param_ptr->cur_skip_num)) {
				ISP_LOG("callback ISP_AE_BAPASS_CALLBACK");
				isp_context_ptr->cfg.callback(handler_id, ISP_CALLBACK_EVT|ISP_AE_BAPASS_CALLBACK, (void*)&rtn, sizeof(uint32_t));
				ae_param_ptr->cur_skip_num = ISP_AE_SKIP_LOCK;
			} else if (ISP_AE_SKIP_LOCK == ae_param_ptr->cur_skip_num){

			} else if (ISP_AE_SKIP_FOREVER != ae_param_ptr->cur_skip_num){
				ae_param_ptr->cur_skip_num--;
			}
		}
		else if(((ISP_UEB == ae_param_ptr->bypass)&&(ISP_EB == ae_param_ptr->init))
			||((ISP_UEB == isp_context_ptr->awb.bypass)&&(ISP_EB == isp_context_ptr->awb.init)))
		{
			//ISP_LOG("skip:%d, ae bypass:%d, awb bypass:%d", ae_param_ptr->cur_skip_num, ae_param_ptr->monitor_bypass, awb_param_ptr->monitor_bypass);

			if((ISP_ZERO != ae_param_ptr->cur_skip_num)
				&&(ISP_AE_SKIP_FOREVER != ae_param_ptr->cur_skip_num)) {
				ae_param_ptr->cur_skip_num--;
			}

			if(ISP_UEB==ae_param_ptr->monitor_bypass)
			{
				ispAwbmBypass(handler_id, ISP_UEB);
				ispAwbmSkip(handler_id, ae_param_ptr->skip_frame);
				ae_param_ptr->monitor_bypass = ISP_EB;
				ae_param_ptr->cur_skip_num=ae_param_ptr->skip_frame+ISP_TWO;
			}
			else if((ISP_UEB==awb_param_ptr->monitor_bypass)
				&&(ISP_EB == ae_param_ptr->bypass))
			{
				ispAwbmBypass(handler_id, ISP_UEB);
				ispAwbmSkip(handler_id, 0);
				awb_param_ptr->monitor_bypass = ISP_EB;
			}

		} else {
			ae_param_ptr->cur_skip_num= ISP_ZERO;
		}

	}

	return rtn;
}

/* _ispCfgAwbm --
*@
*@
*@ return:
*/
static int32_t _ispCfgAwbm(uint32_t handler_id, struct isp_awbm_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	uint32_t isp_id=IspGetId();

	ispSetAwbmWinStart(handler_id, param_ptr->win_start.x, param_ptr->win_start.y);
	ispSetAwbmWinSize(handler_id, param_ptr->win_size.w, param_ptr->win_size.h);
	ispSetAwbmShift(handler_id, 0);
	if(SC8825_ISP_ID==isp_id)
	{
		ispAwbmMode(handler_id, 1);
	}
	else if(SC8830_ISP_ID==isp_id)
	{
		ispAwbmMode(handler_id, 0);
	}
	ispAwbmSkip(handler_id, 1);
	ispAwbmBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgAwbmWin --
*@
*@
*@ return:
*/
static int32_t _ispCfgAwbWin(uint32_t handler_id, struct isp_awbm_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetAwbmWinStart(handler_id, param_ptr->win_start.x, param_ptr->win_start.y);
	ispSetAwbmWinSize(handler_id, param_ptr->win_size.w, param_ptr->win_size.h);

	return rtn;
}

/* _ispCalcAwbWin --
*@
*@
*@ return:
*/
static int32_t _ispCalcAwbWin(uint32_t handler_id, struct isp_trim_size* param_ptr)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_awbm_param*awb_param_ptr = &isp_context_ptr->awbm;
	uint16_t h_num = 0x00;
	uint16_t v_num = 0x00;

	ispGetAwbWinNum(handler_id, &h_num, &v_num);

	if(((h_num*2)<=param_ptr->w)
		&&((v_num*2)<=param_ptr->h))
	{
		awb_param_ptr->win_size.w=(((param_ptr->w/h_num)>>ISP_ONE)<<ISP_ONE);
		awb_param_ptr->win_size.h=(((param_ptr->h/v_num)>>ISP_ONE)<<ISP_ONE);

		awb_param_ptr->win_start.x=param_ptr->x+((param_ptr->w-(awb_param_ptr->win_size.w*h_num))>>ISP_ONE);
		awb_param_ptr->win_start.y=param_ptr->y+((param_ptr->h-(awb_param_ptr->win_size.h*v_num))>>ISP_ONE);

		isp_context_ptr->wb_trim_conter=ISP_ZERO;
	}

	return rtn;
}

/* _ispCfgAwbc --
*@
*@
*@ return:
*/
static int32_t _ispCfgAwbc(uint32_t handler_id, struct isp_awbc_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetAwbGain(handler_id, param_ptr->r_gain, param_ptr->g_gain, param_ptr->b_gain);
	ispSetAwbGainOffset(handler_id, param_ptr->r_offset, param_ptr->g_offset, param_ptr->b_offset);
	ispSetAwbGainThrd(handler_id, 0x3ff, 0x3ff, 0x3ff);
	ispAwbcBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgAwbmInfo --
*@
*@
*@ return:
*/
static int32_t _ispCfgAwbmInfo(uint32_t handler_id, struct isp_awb_statistic_info* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispGetAWBMStatistic(handler_id, param_ptr->r_info, param_ptr->g_info, param_ptr->b_info);

	return rtn;
}

/* _ispCfgBPC --
*@
*@
*@ return:
*/
static int32_t _ispCfgBPC(uint32_t handler_id, struct isp_bpc_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispBpcMode(handler_id, param_ptr->mode);
	ispSetBpcThrd(handler_id, param_ptr->flat_thr, param_ptr->std_thr, param_ptr->texture_thr);
	ispBpcMapAddr(handler_id, param_ptr->map_addr);
	ispBpcBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgDenoise --
*@
*@
*@ return:
*/
static int32_t _ispCfgDenoise(uint32_t handler_id, struct isp_denoise_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispWDenoiseWriteBack(handler_id, param_ptr->write_back);
	ispWDenoiseThrd(handler_id, param_ptr->r_thr, param_ptr->g_thr, param_ptr->b_thr);
	ispWDenoiseDiswei(handler_id, param_ptr->diswei);
	ispWDenoiseRanwei(handler_id, param_ptr->ranwei);
	ispWDenoiseBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgGrGb --
*@
*@
*@ return:
*/
static int32_t _ispCfgGrGb(uint32_t handler_id, struct isp_grgb_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetGrGbThrd(handler_id, param_ptr->edge_thr, param_ptr->diff_thr);
	ispGrGbbypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgCfa --
*@
*@
*@ return:
*/
static int32_t _ispCfgCfa(uint32_t handler_id, struct isp_cfa_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetCFAThrd(handler_id, param_ptr->edge_thr, param_ptr->diff_thr);

	return rtn;
}

/* _ispCfgCmc --
*@
*@
*@ return:
*/
static int32_t _ispCfgCmc(uint32_t handler_id, struct isp_cmc_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetCMCMatrix(handler_id, param_ptr->matrix);
	ispCMCbypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgGamma --
*@
*@
*@ return:
*/
static int32_t _ispCfgGamma(uint32_t handler_id, struct isp_gamma_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetGammaXNode(handler_id, (uint16_t*)&param_ptr->axis[0]);
	ispSetGammaYNode(handler_id, (uint16_t*)&param_ptr->axis[1]);
	ispSetGammaRNode(handler_id, (uint16_t*)&param_ptr->axis[1]);
	ispSetGammaGNode(handler_id, (uint16_t*)&param_ptr->axis[1]);
	ispSetGammaBNode(handler_id, (uint16_t*)&param_ptr->axis[1]);
	ispSetGammaNodeIndex(handler_id, (uint8_t*)&param_ptr->index);
	ispGammabypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgCCEMatrix --
*@
*@
*@ return:
*/
static int32_t _ispCfgCCEMatrix(uint32_t handler_id, struct isp_cce_matrix* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetCCEMatrix(handler_id, param_ptr->matrix);
	ispSetCCEShift(handler_id, param_ptr->y_shift, param_ptr->u_shift, param_ptr->v_shift);

	return rtn;
}

/* _ispCfgUVDiv --
*@
*@
*@ return:
*/
static int32_t _ispCfgUVDiv(uint32_t handler_id, struct isp_cce_uvdiv* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetCCEUVDiv(handler_id, param_ptr->thrd);
	ispSetCCEUVC(handler_id, &param_ptr->t[0], &param_ptr->m[0]);
	ispCCEUVDivBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgPref --
*@
*@
*@ return:
*/
static int32_t _ispCfgPref(uint32_t handler_id, struct isp_pref_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispPrefWriteBack(handler_id, param_ptr->write_back);
	ispSetPrefThrd(handler_id, param_ptr->y_thr, param_ptr->u_thr, param_ptr->v_thr);
	ispPrefBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgBright --
*@
*@
*@ return:
*/
static int32_t _ispCfgBright(uint32_t handler_id, struct isp_bright_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetBrightFactor(handler_id, param_ptr->factor);
	ispBrightBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgContrast --
*@
*@
*@ return:
*/
static int32_t _ispCfgContrast(uint32_t handler_id, struct isp_contrast_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetContrastFactor(handler_id, param_ptr->factor);
	ispContrastbypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgHist --
*@
*@
*@ return:
*/
static int32_t _ispCfgHist(uint32_t handler_id, struct isp_hist_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispHistbypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgAutoContrast --
*@
*@
*@ return:
*/
static int32_t _ispCfgAutoContrast(uint32_t handler_id, struct isp_auto_contrast_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetAutoContrastMode(handler_id, param_ptr->mode);
	ispSetAutoContrastMaxMin(handler_id, param_ptr->in_min, param_ptr->in_max, param_ptr->out_min, param_ptr->out_max);
	ispAutoContrastbypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgSaturation --
*@
*@
*@ return:
*/
static int32_t _ispCfgSaturation(uint32_t handler_id, struct isp_saturation_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetSaturationFactor(handler_id, param_ptr->factor + param_ptr->offset);
	ispSaturationbypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgYGamma --
*@
*@
*@ return:
*/
static int32_t _ispCfgYGamma(uint32_t handler_id, struct isp_ygamma_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispYGammabypass(handler_id, param_ptr->bypass);
	ispSetYGammaXNode(handler_id, (uint8_t*)&param_ptr->gamma[0][0]);
	ispSetYGammaYNode(handler_id, (uint16_t*)&param_ptr->gamma[1][0]);

	return rtn;
}

/* _ispCfgAe --
*@
*@
*@ return:
*/
static int32_t _ispCfgAe(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;

	ispAemMode(handler_id, ISP_ZERO);
	ispAemSkipNum(handler_id, ISP_ZERO);
	ispAemSourceSel(handler_id, ISP_TWO);
	ispAembypass(handler_id, ISP_ONE);

	return rtn;
}

/* _ispCfgAemInfo --
*@
*@
*@ return:
*/
static int32_t _ispCfgAemInfo(uint32_t handler_id, struct isp_ae_statistic_info* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispGetAEMStatistic(handler_id, param_ptr->y);

	return rtn;
}

/* _ispCfgFlicker --
*@
*@
*@ return:
*/
static int32_t _ispCfgFlicker(uint32_t handler_id, struct isp_flicker_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispAutoFlickerbypass(handler_id, param_ptr->bypass);
	ispAutoFlickerMode(handler_id, 0);
	ispAutoFlickerVHeight(handler_id, param_ptr->flicker_v_height);
	ispAutoFlickerLineConter(handler_id, param_ptr->flicker_line_conter);
	ispAutoFlickerLineStep(handler_id, param_ptr->flicker_line_step);
	ispAutoFlickerLineStart(handler_id, param_ptr->flicker_line_start);

	return rtn;
}

/* _ispCfgHue --
*@
*@
*@ return:
*/
static int32_t _ispCfgHue(uint32_t handler_id, struct isp_saturation_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetHueFactor(handler_id, param_ptr->factor + param_ptr->offset);
	ispHuebypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgAf --
*@
*@
*@ return:
*/
static int32_t _ispCfgAf(uint32_t handler_id, struct isp_af_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_awbm_param awbm_param;
	uint32_t x_start,y_start,x_end,y_end;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	uint32_t temp;

	if((3 == param_ptr->alg_id) && (isp_context_ptr->af.init)){
		ispAwbmBypass(handler_id, ISP_EB);
		ispAFMbypass(handler_id, ISP_EB);
		x_start = param_ptr->win[0][0];
		y_start = param_ptr->win[0][1];
		x_end = param_ptr->win[0][2];
		y_end = param_ptr->win[0][3];
		awbm_param.win_start.x = x_start;
		awbm_param.win_start.y = y_start;
		awbm_param.win_size.w = (x_end + 31 - x_start)>>5;
		awbm_param.win_size.h = (y_end + 31 - y_start)>>5;
		if(awbm_param.win_size.w%2){
			awbm_param.win_size.w++;
		}
		if(awbm_param.win_size.h%2){
			awbm_param.win_size.h++;
		}
		temp = ((awbm_param.win_size.w<<5) - (x_end - x_start))>>1;
		awbm_param.win_start.x = awbm_param.win_start.x>temp?awbm_param.win_start.x-temp:0;
		temp = ((awbm_param.win_size.h<<5) - (y_end - y_start))>>1;
		awbm_param.win_start.y = awbm_param.win_start.y>temp?awbm_param.win_start.y-temp:0;

		if(awbm_param.win_start.x + awbm_param.win_size.w*32 > isp_context_ptr->src.w){
			awbm_param.win_start.x = isp_context_ptr->src.w - (awbm_param.win_size.w<<5);
		}
		if(awbm_param.win_start.y + awbm_param.win_size.h*32 > isp_context_ptr->src.h){
			awbm_param.win_start.y =isp_context_ptr->src.h - (awbm_param.win_size.h<<5);
		}

		awbm_param.bypass = ISP_UEB;
		isp_context_ptr->ae.monitor_conter = 1;
		_ispCfgAwbm(handler_id, &awbm_param);
		isp_context_ptr->af.awbm_win_w = awbm_param.win_size.w;
		isp_context_ptr->af.awbm_win_h = awbm_param.win_size.h;
		ispSetAFMShift(handler_id, 0x00);
		ispSetAFMWindow(handler_id, param_ptr->win);
		ispAFMMode(handler_id, 0);
		ispAFMSkipNum(handler_id, 0);
		ispAFMSkipClear(handler_id, 0);
		ispAFMbypass(handler_id, ISP_UEB);
		//ISP_LOG("P_w:%d  P_h:%d  x:%d  y:%d  w:%d  h:%d  ",isp_context_ptr->src.w,isp_context_ptr->src.h,awbm_param.win_start.x,awbm_param.win_start.y,awbm_param.win_size.w,awbm_param.win_size.h);
	}else{
		ispSetAFMShift(handler_id, 0x00);
		ispSetAFMWindow(handler_id, param_ptr->win);
		ispAFMMode(handler_id, 0);
		ispAFMSkipNum(handler_id, 0);
		ispAFMSkipClear(handler_id, 0);
		ispAFMbypass(handler_id, param_ptr->monitor_bypass);
	}
	param_ptr->monitor_bypass = ISP_EB;

	return rtn;
}

/* _ispCfgAfConStat --
*@
*@
*@ return:
*/
static int32_t _ispCfgAfConStat(uint32_t handler_id, struct isp_af_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	//ispSetAFMShift(0x00);
	//ispSetAFMWindow(param_ptr->win);
	//ispAFMMode(1);
	//ispAFMSkipNum(0);
	//ispAFMSkipClear(1);
	ispAFMbypass(handler_id, param_ptr->monitor_bypass);
	//param_ptr->monitor_bypass = ISP_EB;

	return rtn;
}

/* _ispGetAfInof --
*@
*@
*@ return:
*/
static int32_t _ispGetAfInof(uint32_t handler_id, struct isp_af_statistic_info* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispGetAFMStatistic(handler_id, param_ptr->info);

	return rtn;
}

/* _ispCfgEdge --
*@
*@
*@ return:
*/
static int32_t _ispCfgEdge(uint32_t handler_id, struct isp_edge_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetEdgeParam(handler_id, param_ptr->detail_thr, param_ptr->smooth_thr, param_ptr->strength);
	ispEdgeBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgEmboss --
*@
*@
*@ return:
*/
static int32_t _ispCfgEmboss(uint32_t handler_id, struct isp_emboss_param* param_ptr)
{
	uint32_t rtn=ISP_SUCCESS;

	ispSetEmbossParam(handler_id, param_ptr->step);
	ispEmbossypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgFalseColor --
*@
*@
*@ return:
*/
static int32_t _ispCfgFalseColor(uint32_t handler_id, struct isp_fcs_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispFalseColorMode(handler_id, ISP_ONE);
	ispFalseColorBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgSatursationSup --
*@
*@
*@ return:
*/
static int32_t _ispCfgSatursationSup(uint32_t handler_id, struct isp_css_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetColorSaturationSuppressThrd(handler_id, param_ptr->low_thr, param_ptr->low_sum_thr, param_ptr->lum_thr, param_ptr->chr_thr);
	ispColorSaturationSuppressBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgHdr --
*@
*@
*@ return:
*/
static int32_t _ispCfgHdr(uint32_t handler_id, struct isp_hdr_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetHDRLevel(handler_id, param_ptr->level);
	ispHDRBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgHDRIndexTab --
*@
*@
*@ return:
*/
static int32_t _ispCfgHDRIndexTab(uint32_t handler_id, struct isp_hdr_index* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetHDRIndex(handler_id, param_ptr->r_index, param_ptr->g_index, param_ptr->b_index);
	ispSetHDRIndexTab(handler_id, param_ptr->com_ptr, param_ptr->p2e_ptr, param_ptr->e2p_ptr);

	return rtn;
}

/* _ispCfgGlobalGain --
*@
*@
*@ return:
*/
static int32_t _ispCfgGlobalGain(uint32_t handler_id, struct isp_global_gain_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetGlbGain(handler_id, param_ptr->gain);
	ispGlbGainBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _ispCfgChnGain --
*@
*@
*@ return:
*/
static int32_t _ispCfgChnGain(uint32_t handler_id, struct isp_chn_gain_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	ispSetChnGain(handler_id, param_ptr->r_gain, param_ptr->g_gain, param_ptr->b_gain);
	ispSetChnGainOffset(handler_id, param_ptr->r_offset, param_ptr->g_offset, param_ptr->g_offset);
	ispChnGainBypass(handler_id, param_ptr->bypass);

	return rtn;
}

/* _isp_clear_slice_info --
*@
*@
*@ return:
*/
int _isp_clear_slice_info(uint32_t handler_id)
{
	int rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_slice_param* slice_param_ptr = &isp_context_ptr->slice;

	memset((void*)slice_param_ptr, ISP_ZERO, sizeof(struct isp_slice_param));

	return rtn;
}

/* _ispSetInterfaceParam --
*@
*@
*@ return:
*/
static int32_t _ispSetInterfaceParam(uint32_t handler_id, struct isp_cfg_param* param_ptr)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	uint32_t i = 0x00;
	uint16_t max_slice_width = 0x00;
	uint16_t max_slice_height = 0x00;

	memcpy((void*)&isp_context_ptr->cfg, (void*)param_ptr, sizeof(struct isp_cfg_param));
	rtn=_isp_clear_slice_info(handler_id);

	rtn=ispGetContinueSize(handler_id, &max_slice_width, &max_slice_height);

	if(param_ptr->data.input_size.w<max_slice_width) {
		isp_context_ptr->slice.max_size.w=param_ptr->data.input_size.w;
	} else {
		isp_context_ptr->slice.max_size.w=max_slice_width;
	}

	if(param_ptr->data.slice_height<max_slice_height) {
		isp_context_ptr->slice.max_size.h=param_ptr->data.slice_height;
	} else {
		isp_context_ptr->slice.max_size.h=max_slice_height;
	}

	isp_context_ptr->slice.all_line=param_ptr->data.input_size.h;
	isp_context_ptr->slice.complete_line=ISP_ZERO;

	_ispGetSliceWidthNum(&isp_context_ptr->src, &isp_context_ptr->slice);
	_ispGetSliceHeightNum(&isp_context_ptr->src, &isp_context_ptr->slice);

	// common
	isp_context_ptr->com.in_mode=param_ptr->data.input;
	isp_context_ptr->com.fetch_bit_reorder=ISP_LSB;
	if(ISP_EMC_MODE == param_ptr->data.input) {
		isp_context_ptr->com.fetch_endian=ISP_ENDIAN_LITTLE;
	} else {
		isp_context_ptr->com.fetch_endian=ISP_ENDIAN_BIG;
	}

	isp_context_ptr->com.bpc_endian=ISP_ENDIAN_BIG;

	isp_context_ptr->com.out_mode=param_ptr->data.output;
	if(ISP_EMC_MODE == param_ptr->data.output) {
		isp_context_ptr->com.store_endian=ISP_ENDIAN_LITTLE;
	} else {
		isp_context_ptr->com.store_endian=ISP_ENDIAN_BIG;
	}

	isp_context_ptr->com.store_yuv_format=_ispStoreFormat(param_ptr->data.output_format);
	isp_context_ptr->com.fetch_color_format=_ispFetchFormat(param_ptr->data.input_format);
	isp_context_ptr->com.bayer_pattern=param_ptr->data.format_pattern;

	// featch
	isp_context_ptr->featch.bypass=ISP_ZERO;
	isp_context_ptr->featch.sub_stract=ISP_ZERO;
	isp_context_ptr->featch.addr.chn0=param_ptr->data.input_addr.chn0;
	isp_context_ptr->featch.addr.chn1=param_ptr->data.input_addr.chn1;
	isp_context_ptr->featch.addr.chn2=param_ptr->data.input_addr.chn2;
	_ispGetFetchPitch(&(isp_context_ptr->featch.pitch), param_ptr->data.input_size.w, param_ptr->data.input_format);
	// feeder
	isp_context_ptr->feeder.data_type=_ispFeederDataType(param_ptr->data.input_format);
	// store
	isp_context_ptr->store.bypass=ISP_ZERO;
	isp_context_ptr->store.sub_stract=ISP_ONE;
	isp_context_ptr->store.addr.chn0=param_ptr->data.output_addr.chn0;
	isp_context_ptr->store.addr.chn1=param_ptr->data.output_addr.chn1;
	isp_context_ptr->store.addr.chn2=param_ptr->data.output_addr.chn2;
	_ispGetStorePitch(&(isp_context_ptr->store.pitch), param_ptr->data.input_size.w, param_ptr->data.output_format);

	return rtn;
}

/* _ispSetParam --
*@
*@
*@ return:
*/
static int32_t _ispSetParam(uint32_t handler_id, struct isp_cfg_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct sensor_raw_info* raw_info_ptr = (struct sensor_raw_info*)param_ptr->sensor_info_ptr;
	struct sensor_raw_tune_info* raw_tune_ptr = (struct sensor_raw_tune_info*)raw_info_ptr->tune_ptr;
	struct sensor_raw_fix_info* raw_fix_ptr = (struct sensor_raw_fix_info*)raw_info_ptr->fix_ptr;
	struct sensor_raw_cali_info* cali_ptr = (struct sensor_raw_cali_info*)raw_info_ptr->cali_ptr;
	struct sensor_raw_resolution_info_tab* resolution_ptr = (struct sensor_raw_resolution_info_tab*)raw_info_ptr->resolution_info_ptr;
	struct sensor_raw_ioctrl* ioctrl_ptr = (struct sensor_raw_ioctrl*)raw_info_ptr->ioctrl_ptr;
	//SENSOR_TRIM_T* resolution_trim_ptr = NULL;
	uint32_t i = 0x00;
	uint32_t j = 0x00;
	uint32_t max_param_index = 0x00;

	isp_context_ptr->src.w=param_ptr->data.input_size.w;
	isp_context_ptr->src.h=param_ptr->data.input_size.h;

	for(i=0x00; i<ISP_INPUT_SIZE_NUM_MAX; i++) {
		isp_context_ptr->input_size_trim[i].trim_start_x = resolution_ptr->tab[i].start_x;
		isp_context_ptr->input_size_trim[i].trim_start_y = resolution_ptr->tab[i].start_y;
		isp_context_ptr->input_size_trim[i].trim_width = resolution_ptr->tab[i].width;
		isp_context_ptr->input_size_trim[i].trim_height = resolution_ptr->tab[i].height;
		isp_context_ptr->input_size_trim[i].line_time = resolution_ptr->tab[i].line_time;
		isp_context_ptr->input_size_trim[i].frame_line = resolution_ptr->tab[i].frame_line;
	}

	isp_context_ptr->af.go_position = ioctrl_ptr->set_focus;
	isp_context_ptr->ae.write_exposure = ioctrl_ptr->set_exposure;
	isp_context_ptr->ae.write_gain = ioctrl_ptr->set_gain;

	// isp param index
	isp_context_ptr->param_index=_ispGetIspParamIndex(handler_id, &param_ptr->data.input_size);
	isp_context_ptr->awb.cur_index=ISP_AWB_INDEX0;

	ISP_LOG("param_index, 0x%x",isp_context_ptr->param_index);

	// isp tune param
	isp_context_ptr->blc.bypass=raw_tune_ptr->blc_bypass;
	isp_context_ptr->nlc.bypass=raw_tune_ptr->nlc_bypass;
	isp_context_ptr->lnc.bypass=raw_tune_ptr->lnc_bypass;
	isp_context_ptr->awbm.bypass=ISP_EB;
	isp_context_ptr->awbc.bypass=raw_tune_ptr->awb_bypass;
	isp_context_ptr->awb.bypass=raw_tune_ptr->awb_bypass;
	isp_context_ptr->awb.back_bypass=raw_tune_ptr->awb_bypass;
	isp_context_ptr->ae.bypass=raw_tune_ptr->ae_bypass;
	isp_context_ptr->ae.back_bypass=raw_tune_ptr->ae_bypass;
	isp_context_ptr->bpc.bypass=raw_tune_ptr->bpc_bypass;
	isp_context_ptr->denoise.bypass=raw_tune_ptr->denoise_bypass;
	isp_context_ptr->grgb.bypass=raw_tune_ptr->grgb_bypass;
	isp_context_ptr->cmc.bypass=raw_tune_ptr->cmc_bypass;
	isp_context_ptr->gamma.bypass=raw_tune_ptr->gamma_bypass;
	isp_context_ptr->uv_div.bypass=raw_tune_ptr->uvdiv_bypass;
	isp_context_ptr->pref.bypass=raw_tune_ptr->pref_bypass;
	isp_context_ptr->bright.bypass=raw_tune_ptr->bright_bypass;
	isp_context_ptr->contrast.bypass=raw_tune_ptr->contrast_bypass;
	isp_context_ptr->hist.bypass=raw_tune_ptr->hist_bypass;
	isp_context_ptr->auto_contrast.bypass=raw_tune_ptr->auto_contrast_bypass;
	isp_context_ptr->saturation.bypass=raw_tune_ptr->saturation_bypass;
	isp_context_ptr->af.bypass=raw_tune_ptr->af_bypass;
	isp_context_ptr->af.back_bypass=raw_tune_ptr->af_bypass;
	isp_context_ptr->af.monitor_bypass=ISP_EB;
	isp_context_ptr->edge.bypass=raw_tune_ptr->edge_bypass;
	isp_context_ptr->emboss.bypass=ISP_EB;
	isp_context_ptr->fcs.bypass=raw_tune_ptr->fcs_bypass;
	isp_context_ptr->css.bypass=raw_tune_ptr->css_bypass;
	isp_context_ptr->hdr.bypass=ISP_EB;
	isp_context_ptr->global.bypass=raw_tune_ptr->glb_gain_bypass;
	isp_context_ptr->chn.bypass=raw_tune_ptr->chn_gain_bypass;

	//blc
	isp_context_ptr->blc.mode=raw_tune_ptr->blc.mode;
	isp_context_ptr->blc.r=raw_tune_ptr->blc.offset[0].r;
	isp_context_ptr->blc.gr=raw_tune_ptr->blc.offset[0].gr;
	isp_context_ptr->blc.gb=raw_tune_ptr->blc.offset[0].gb;
	isp_context_ptr->blc.b=raw_tune_ptr->blc.offset[0].b;

	for(i=0x00; i<8; i++) {
		isp_context_ptr->blc_offset[i].r=raw_tune_ptr->blc.offset[i].r;
		isp_context_ptr->blc_offset[i].gr=raw_tune_ptr->blc.offset[i].gr;
		isp_context_ptr->blc_offset[i].gb=raw_tune_ptr->blc.offset[i].gb;
		isp_context_ptr->blc_offset[i].b=raw_tune_ptr->blc.offset[i].b;
	}

	//nlc
	//SCI_MEMCPY((void*)&isp_context_ptr->nlc.r_node, (void*)raw_tune_ptr->nlc.r_node_ptr, sizeof(uint16_t)*29);
	//SCI_MEMCPY((void*)&isp_context_ptr->nlc.g_node, (void*)raw_tune_ptr->nlc.g_node_ptr, sizeof(uint16_t)*29);
	//SCI_MEMCPY((void*)&isp_context_ptr->nlc.b_node, (void*)raw_tune_ptr->nlc.b_node_ptr, sizeof(uint16_t)*29);
	//SCI_MEMCPY((void*)&isp_context_ptr->nlc.l_node, (void*)raw_tune_ptr->nlc.l_node_ptr, sizeof(uint16_t)*29);
	isp_context_ptr->nlc.r_node[0]=raw_tune_ptr->nlc.r_node[0];
	isp_context_ptr->nlc.r_node[1]=raw_tune_ptr->nlc.r_node[1];
	isp_context_ptr->nlc.r_node[2]=raw_tune_ptr->nlc.r_node[2];
	isp_context_ptr->nlc.r_node[3]=raw_tune_ptr->nlc.r_node[3];
	isp_context_ptr->nlc.r_node[4]=raw_tune_ptr->nlc.r_node[4];
	isp_context_ptr->nlc.r_node[5]=raw_tune_ptr->nlc.r_node[5];
	isp_context_ptr->nlc.r_node[6]=raw_tune_ptr->nlc.r_node[6];
	isp_context_ptr->nlc.r_node[7]=raw_tune_ptr->nlc.r_node[7];
	isp_context_ptr->nlc.r_node[8]=raw_tune_ptr->nlc.r_node[8];
	isp_context_ptr->nlc.r_node[9]=raw_tune_ptr->nlc.r_node[9];
	isp_context_ptr->nlc.r_node[10]=raw_tune_ptr->nlc.r_node[10];
	isp_context_ptr->nlc.r_node[11]=raw_tune_ptr->nlc.r_node[11];
	isp_context_ptr->nlc.r_node[12]=raw_tune_ptr->nlc.r_node[12];
	isp_context_ptr->nlc.r_node[13]=raw_tune_ptr->nlc.r_node[13];
	isp_context_ptr->nlc.r_node[14]=raw_tune_ptr->nlc.r_node[14];
	isp_context_ptr->nlc.r_node[15]=raw_tune_ptr->nlc.r_node[15];
	isp_context_ptr->nlc.r_node[16]=raw_tune_ptr->nlc.r_node[16];
	isp_context_ptr->nlc.r_node[17]=raw_tune_ptr->nlc.r_node[17];
	isp_context_ptr->nlc.r_node[18]=raw_tune_ptr->nlc.r_node[18];
	isp_context_ptr->nlc.r_node[19]=raw_tune_ptr->nlc.r_node[19];
	isp_context_ptr->nlc.r_node[20]=raw_tune_ptr->nlc.r_node[20];
	isp_context_ptr->nlc.r_node[21]=raw_tune_ptr->nlc.r_node[21];
	isp_context_ptr->nlc.r_node[22]=raw_tune_ptr->nlc.r_node[22];
	isp_context_ptr->nlc.r_node[23]=raw_tune_ptr->nlc.r_node[23];
	isp_context_ptr->nlc.r_node[24]=raw_tune_ptr->nlc.r_node[24];
	isp_context_ptr->nlc.r_node[25]=raw_tune_ptr->nlc.r_node[25];
	isp_context_ptr->nlc.r_node[26]=raw_tune_ptr->nlc.r_node[26];
	isp_context_ptr->nlc.r_node[27]=raw_tune_ptr->nlc.r_node[27];
	isp_context_ptr->nlc.r_node[28]=raw_tune_ptr->nlc.r_node[28];

	isp_context_ptr->nlc.g_node[0]=raw_tune_ptr->nlc.g_node[0];
	isp_context_ptr->nlc.g_node[1]=raw_tune_ptr->nlc.g_node[1];
	isp_context_ptr->nlc.g_node[2]=raw_tune_ptr->nlc.g_node[2];
	isp_context_ptr->nlc.g_node[3]=raw_tune_ptr->nlc.g_node[3];
	isp_context_ptr->nlc.g_node[4]=raw_tune_ptr->nlc.g_node[4];
	isp_context_ptr->nlc.g_node[5]=raw_tune_ptr->nlc.g_node[5];
	isp_context_ptr->nlc.g_node[6]=raw_tune_ptr->nlc.g_node[6];
	isp_context_ptr->nlc.g_node[7]=raw_tune_ptr->nlc.g_node[7];
	isp_context_ptr->nlc.g_node[8]=raw_tune_ptr->nlc.g_node[8];
	isp_context_ptr->nlc.g_node[9]=raw_tune_ptr->nlc.g_node[9];
	isp_context_ptr->nlc.g_node[10]=raw_tune_ptr->nlc.g_node[10];
	isp_context_ptr->nlc.g_node[11]=raw_tune_ptr->nlc.g_node[11];
	isp_context_ptr->nlc.g_node[12]=raw_tune_ptr->nlc.g_node[12];
	isp_context_ptr->nlc.g_node[13]=raw_tune_ptr->nlc.g_node[13];
	isp_context_ptr->nlc.g_node[14]=raw_tune_ptr->nlc.g_node[14];
	isp_context_ptr->nlc.g_node[15]=raw_tune_ptr->nlc.g_node[15];
	isp_context_ptr->nlc.g_node[16]=raw_tune_ptr->nlc.g_node[16];
	isp_context_ptr->nlc.g_node[17]=raw_tune_ptr->nlc.g_node[17];
	isp_context_ptr->nlc.g_node[18]=raw_tune_ptr->nlc.g_node[18];
	isp_context_ptr->nlc.g_node[19]=raw_tune_ptr->nlc.g_node[19];
	isp_context_ptr->nlc.g_node[20]=raw_tune_ptr->nlc.g_node[20];
	isp_context_ptr->nlc.g_node[21]=raw_tune_ptr->nlc.g_node[21];
	isp_context_ptr->nlc.g_node[22]=raw_tune_ptr->nlc.g_node[22];
	isp_context_ptr->nlc.g_node[23]=raw_tune_ptr->nlc.g_node[23];
	isp_context_ptr->nlc.g_node[24]=raw_tune_ptr->nlc.g_node[24];
	isp_context_ptr->nlc.g_node[25]=raw_tune_ptr->nlc.g_node[25];
	isp_context_ptr->nlc.g_node[26]=raw_tune_ptr->nlc.g_node[26];
	isp_context_ptr->nlc.g_node[27]=raw_tune_ptr->nlc.g_node[27];
	isp_context_ptr->nlc.g_node[28]=raw_tune_ptr->nlc.g_node[28];

	isp_context_ptr->nlc.b_node[0]=raw_tune_ptr->nlc.b_node[0];
	isp_context_ptr->nlc.b_node[1]=raw_tune_ptr->nlc.b_node[1];
	isp_context_ptr->nlc.b_node[2]=raw_tune_ptr->nlc.b_node[2];
	isp_context_ptr->nlc.b_node[3]=raw_tune_ptr->nlc.b_node[3];
	isp_context_ptr->nlc.b_node[4]=raw_tune_ptr->nlc.b_node[4];
	isp_context_ptr->nlc.b_node[5]=raw_tune_ptr->nlc.b_node[5];
	isp_context_ptr->nlc.b_node[6]=raw_tune_ptr->nlc.b_node[6];
	isp_context_ptr->nlc.b_node[7]=raw_tune_ptr->nlc.b_node[7];
	isp_context_ptr->nlc.b_node[8]=raw_tune_ptr->nlc.b_node[8];
	isp_context_ptr->nlc.b_node[9]=raw_tune_ptr->nlc.b_node[9];
	isp_context_ptr->nlc.b_node[10]=raw_tune_ptr->nlc.b_node[10];
	isp_context_ptr->nlc.b_node[11]=raw_tune_ptr->nlc.b_node[11];
	isp_context_ptr->nlc.b_node[12]=raw_tune_ptr->nlc.b_node[12];
	isp_context_ptr->nlc.b_node[13]=raw_tune_ptr->nlc.b_node[13];
	isp_context_ptr->nlc.b_node[14]=raw_tune_ptr->nlc.b_node[14];
	isp_context_ptr->nlc.b_node[15]=raw_tune_ptr->nlc.b_node[15];
	isp_context_ptr->nlc.b_node[16]=raw_tune_ptr->nlc.b_node[16];
	isp_context_ptr->nlc.b_node[17]=raw_tune_ptr->nlc.b_node[17];
	isp_context_ptr->nlc.b_node[18]=raw_tune_ptr->nlc.b_node[18];
	isp_context_ptr->nlc.b_node[19]=raw_tune_ptr->nlc.b_node[19];
	isp_context_ptr->nlc.b_node[20]=raw_tune_ptr->nlc.b_node[20];
	isp_context_ptr->nlc.b_node[21]=raw_tune_ptr->nlc.b_node[21];
	isp_context_ptr->nlc.b_node[22]=raw_tune_ptr->nlc.b_node[22];
	isp_context_ptr->nlc.b_node[23]=raw_tune_ptr->nlc.b_node[23];
	isp_context_ptr->nlc.b_node[24]=raw_tune_ptr->nlc.b_node[24];
	isp_context_ptr->nlc.b_node[25]=raw_tune_ptr->nlc.b_node[25];
	isp_context_ptr->nlc.b_node[26]=raw_tune_ptr->nlc.b_node[26];
	isp_context_ptr->nlc.b_node[27]=raw_tune_ptr->nlc.b_node[27];
	isp_context_ptr->nlc.b_node[28]=raw_tune_ptr->nlc.b_node[28];

	isp_context_ptr->nlc.l_node[0]=raw_tune_ptr->nlc.l_node[0];
	isp_context_ptr->nlc.l_node[1]=raw_tune_ptr->nlc.l_node[1];
	isp_context_ptr->nlc.l_node[2]=raw_tune_ptr->nlc.l_node[2];
	isp_context_ptr->nlc.l_node[3]=raw_tune_ptr->nlc.l_node[3];
	isp_context_ptr->nlc.l_node[4]=raw_tune_ptr->nlc.l_node[4];
	isp_context_ptr->nlc.l_node[5]=raw_tune_ptr->nlc.l_node[5];
	isp_context_ptr->nlc.l_node[6]=raw_tune_ptr->nlc.l_node[6];
	isp_context_ptr->nlc.l_node[7]=raw_tune_ptr->nlc.l_node[7];
	isp_context_ptr->nlc.l_node[8]=raw_tune_ptr->nlc.l_node[8];
	isp_context_ptr->nlc.l_node[9]=raw_tune_ptr->nlc.l_node[9];
	isp_context_ptr->nlc.l_node[10]=raw_tune_ptr->nlc.l_node[10];
	isp_context_ptr->nlc.l_node[11]=raw_tune_ptr->nlc.l_node[11];
	isp_context_ptr->nlc.l_node[12]=raw_tune_ptr->nlc.l_node[12];
	isp_context_ptr->nlc.l_node[13]=raw_tune_ptr->nlc.l_node[13];
	isp_context_ptr->nlc.l_node[14]=raw_tune_ptr->nlc.l_node[14];
	isp_context_ptr->nlc.l_node[15]=raw_tune_ptr->nlc.l_node[15];
	isp_context_ptr->nlc.l_node[16]=raw_tune_ptr->nlc.l_node[16];
	isp_context_ptr->nlc.l_node[17]=raw_tune_ptr->nlc.l_node[17];
	isp_context_ptr->nlc.l_node[18]=raw_tune_ptr->nlc.l_node[18];
	isp_context_ptr->nlc.l_node[19]=raw_tune_ptr->nlc.l_node[19];
	isp_context_ptr->nlc.l_node[20]=raw_tune_ptr->nlc.l_node[20];
	isp_context_ptr->nlc.l_node[21]=raw_tune_ptr->nlc.l_node[21];
	isp_context_ptr->nlc.l_node[22]=raw_tune_ptr->nlc.l_node[22];
	isp_context_ptr->nlc.l_node[23]=raw_tune_ptr->nlc.l_node[23];
	isp_context_ptr->nlc.l_node[24]=raw_tune_ptr->nlc.l_node[24];
	isp_context_ptr->nlc.l_node[25]=raw_tune_ptr->nlc.l_node[25];
	isp_context_ptr->nlc.l_node[26]=raw_tune_ptr->nlc.l_node[26];
	isp_context_ptr->nlc.l_node[27]=raw_tune_ptr->nlc.l_node[27];
	isp_context_ptr->nlc.l_node[28]=raw_tune_ptr->nlc.l_node[28];

	/*lnc*/
	/*isp_context_ptr->lnc.param_addr[0]=(uint32_t)raw_fix_ptr->lnc.map[isp_context_ptr->param_index][0].param_addr;*/
	isp_context_ptr->lnc_map_tab[0][0].param_addr=(uint32_t)raw_fix_ptr->lnc.map[0][0].param_addr;
	isp_context_ptr->lnc_map_tab[0][1].param_addr=(uint32_t)raw_fix_ptr->lnc.map[0][1].param_addr;
	isp_context_ptr->lnc_map_tab[0][2].param_addr=(uint32_t)raw_fix_ptr->lnc.map[0][2].param_addr;
	isp_context_ptr->lnc_map_tab[0][3].param_addr=(uint32_t)raw_fix_ptr->lnc.map[0][3].param_addr;
	isp_context_ptr->lnc_map_tab[0][4].param_addr=(uint32_t)raw_fix_ptr->lnc.map[0][4].param_addr;
	isp_context_ptr->lnc_map_tab[0][5].param_addr=(uint32_t)raw_fix_ptr->lnc.map[0][5].param_addr;
	isp_context_ptr->lnc_map_tab[0][6].param_addr=(uint32_t)raw_fix_ptr->lnc.map[0][6].param_addr;
	isp_context_ptr->lnc_map_tab[0][7].param_addr=(uint32_t)raw_fix_ptr->lnc.map[0][7].param_addr;
	isp_context_ptr->lnc_map_tab[0][8].param_addr=(uint32_t)raw_fix_ptr->lnc.map[0][8].param_addr;

	isp_context_ptr->lnc_map_tab[1][0].param_addr=(uint32_t)raw_fix_ptr->lnc.map[1][0].param_addr;
	isp_context_ptr->lnc_map_tab[1][1].param_addr=(uint32_t)raw_fix_ptr->lnc.map[1][1].param_addr;
	isp_context_ptr->lnc_map_tab[1][2].param_addr=(uint32_t)raw_fix_ptr->lnc.map[1][2].param_addr;
	isp_context_ptr->lnc_map_tab[1][3].param_addr=(uint32_t)raw_fix_ptr->lnc.map[1][3].param_addr;
	isp_context_ptr->lnc_map_tab[1][4].param_addr=(uint32_t)raw_fix_ptr->lnc.map[1][4].param_addr;
	isp_context_ptr->lnc_map_tab[1][5].param_addr=(uint32_t)raw_fix_ptr->lnc.map[1][5].param_addr;
	isp_context_ptr->lnc_map_tab[1][6].param_addr=(uint32_t)raw_fix_ptr->lnc.map[1][6].param_addr;
	isp_context_ptr->lnc_map_tab[1][7].param_addr=(uint32_t)raw_fix_ptr->lnc.map[1][7].param_addr;
	isp_context_ptr->lnc_map_tab[1][8].param_addr=(uint32_t)raw_fix_ptr->lnc.map[1][8].param_addr;

	isp_context_ptr->lnc_map_tab[2][0].param_addr=(uint32_t)raw_fix_ptr->lnc.map[2][0].param_addr;
	isp_context_ptr->lnc_map_tab[2][1].param_addr=(uint32_t)raw_fix_ptr->lnc.map[2][1].param_addr;
	isp_context_ptr->lnc_map_tab[2][2].param_addr=(uint32_t)raw_fix_ptr->lnc.map[2][2].param_addr;
	isp_context_ptr->lnc_map_tab[2][3].param_addr=(uint32_t)raw_fix_ptr->lnc.map[2][3].param_addr;
	isp_context_ptr->lnc_map_tab[2][4].param_addr=(uint32_t)raw_fix_ptr->lnc.map[2][4].param_addr;
	isp_context_ptr->lnc_map_tab[2][5].param_addr=(uint32_t)raw_fix_ptr->lnc.map[2][5].param_addr;
	isp_context_ptr->lnc_map_tab[2][6].param_addr=(uint32_t)raw_fix_ptr->lnc.map[2][6].param_addr;
	isp_context_ptr->lnc_map_tab[2][7].param_addr=(uint32_t)raw_fix_ptr->lnc.map[2][7].param_addr;
	isp_context_ptr->lnc_map_tab[2][8].param_addr=(uint32_t)raw_fix_ptr->lnc.map[2][8].param_addr;

	isp_context_ptr->lnc_map_tab[3][0].param_addr=(uint32_t)raw_fix_ptr->lnc.map[3][0].param_addr;
	isp_context_ptr->lnc_map_tab[3][1].param_addr=(uint32_t)raw_fix_ptr->lnc.map[3][1].param_addr;
	isp_context_ptr->lnc_map_tab[3][2].param_addr=(uint32_t)raw_fix_ptr->lnc.map[3][2].param_addr;
	isp_context_ptr->lnc_map_tab[3][3].param_addr=(uint32_t)raw_fix_ptr->lnc.map[3][3].param_addr;
	isp_context_ptr->lnc_map_tab[3][4].param_addr=(uint32_t)raw_fix_ptr->lnc.map[3][4].param_addr;
	isp_context_ptr->lnc_map_tab[3][5].param_addr=(uint32_t)raw_fix_ptr->lnc.map[3][5].param_addr;
	isp_context_ptr->lnc_map_tab[3][6].param_addr=(uint32_t)raw_fix_ptr->lnc.map[3][6].param_addr;
	isp_context_ptr->lnc_map_tab[3][7].param_addr=(uint32_t)raw_fix_ptr->lnc.map[3][7].param_addr;
	isp_context_ptr->lnc_map_tab[3][8].param_addr=(uint32_t)raw_fix_ptr->lnc.map[3][8].param_addr;

	isp_context_ptr->lnc_map_tab[4][0].param_addr=(uint32_t)raw_fix_ptr->lnc.map[4][0].param_addr;
	isp_context_ptr->lnc_map_tab[4][1].param_addr=(uint32_t)raw_fix_ptr->lnc.map[4][1].param_addr;
	isp_context_ptr->lnc_map_tab[4][2].param_addr=(uint32_t)raw_fix_ptr->lnc.map[4][2].param_addr;
	isp_context_ptr->lnc_map_tab[4][3].param_addr=(uint32_t)raw_fix_ptr->lnc.map[4][3].param_addr;
	isp_context_ptr->lnc_map_tab[4][4].param_addr=(uint32_t)raw_fix_ptr->lnc.map[4][4].param_addr;
	isp_context_ptr->lnc_map_tab[4][5].param_addr=(uint32_t)raw_fix_ptr->lnc.map[4][5].param_addr;
	isp_context_ptr->lnc_map_tab[4][6].param_addr=(uint32_t)raw_fix_ptr->lnc.map[4][6].param_addr;
	isp_context_ptr->lnc_map_tab[4][7].param_addr=(uint32_t)raw_fix_ptr->lnc.map[4][7].param_addr;
	isp_context_ptr->lnc_map_tab[4][8].param_addr=(uint32_t)raw_fix_ptr->lnc.map[4][8].param_addr;

	isp_context_ptr->lnc_map_tab[5][0].param_addr=(uint32_t)raw_fix_ptr->lnc.map[5][0].param_addr;
	isp_context_ptr->lnc_map_tab[5][1].param_addr=(uint32_t)raw_fix_ptr->lnc.map[5][1].param_addr;
	isp_context_ptr->lnc_map_tab[5][2].param_addr=(uint32_t)raw_fix_ptr->lnc.map[5][2].param_addr;
	isp_context_ptr->lnc_map_tab[5][3].param_addr=(uint32_t)raw_fix_ptr->lnc.map[5][3].param_addr;
	isp_context_ptr->lnc_map_tab[5][4].param_addr=(uint32_t)raw_fix_ptr->lnc.map[5][4].param_addr;
	isp_context_ptr->lnc_map_tab[5][5].param_addr=(uint32_t)raw_fix_ptr->lnc.map[5][5].param_addr;
	isp_context_ptr->lnc_map_tab[5][6].param_addr=(uint32_t)raw_fix_ptr->lnc.map[5][6].param_addr;
	isp_context_ptr->lnc_map_tab[5][7].param_addr=(uint32_t)raw_fix_ptr->lnc.map[5][7].param_addr;
	isp_context_ptr->lnc_map_tab[5][8].param_addr=(uint32_t)raw_fix_ptr->lnc.map[5][8].param_addr;

	isp_context_ptr->lnc_map_tab[6][0].param_addr=(uint32_t)raw_fix_ptr->lnc.map[6][0].param_addr;
	isp_context_ptr->lnc_map_tab[6][1].param_addr=(uint32_t)raw_fix_ptr->lnc.map[6][1].param_addr;
	isp_context_ptr->lnc_map_tab[6][2].param_addr=(uint32_t)raw_fix_ptr->lnc.map[6][2].param_addr;
	isp_context_ptr->lnc_map_tab[6][3].param_addr=(uint32_t)raw_fix_ptr->lnc.map[6][3].param_addr;
	isp_context_ptr->lnc_map_tab[6][4].param_addr=(uint32_t)raw_fix_ptr->lnc.map[6][4].param_addr;
	isp_context_ptr->lnc_map_tab[6][5].param_addr=(uint32_t)raw_fix_ptr->lnc.map[6][5].param_addr;
	isp_context_ptr->lnc_map_tab[6][6].param_addr=(uint32_t)raw_fix_ptr->lnc.map[6][6].param_addr;
	isp_context_ptr->lnc_map_tab[6][7].param_addr=(uint32_t)raw_fix_ptr->lnc.map[6][7].param_addr;
	isp_context_ptr->lnc_map_tab[6][8].param_addr=(uint32_t)raw_fix_ptr->lnc.map[6][8].param_addr;

	isp_context_ptr->lnc_map_tab[7][0].param_addr=(uint32_t)raw_fix_ptr->lnc.map[7][0].param_addr;
	isp_context_ptr->lnc_map_tab[7][1].param_addr=(uint32_t)raw_fix_ptr->lnc.map[7][1].param_addr;
	isp_context_ptr->lnc_map_tab[7][2].param_addr=(uint32_t)raw_fix_ptr->lnc.map[7][2].param_addr;
	isp_context_ptr->lnc_map_tab[7][3].param_addr=(uint32_t)raw_fix_ptr->lnc.map[7][3].param_addr;
	isp_context_ptr->lnc_map_tab[7][4].param_addr=(uint32_t)raw_fix_ptr->lnc.map[7][4].param_addr;
	isp_context_ptr->lnc_map_tab[7][5].param_addr=(uint32_t)raw_fix_ptr->lnc.map[7][5].param_addr;
	isp_context_ptr->lnc_map_tab[7][6].param_addr=(uint32_t)raw_fix_ptr->lnc.map[7][6].param_addr;
	isp_context_ptr->lnc_map_tab[7][7].param_addr=(uint32_t)raw_fix_ptr->lnc.map[7][7].param_addr;
	isp_context_ptr->lnc_map_tab[7][8].param_addr=(uint32_t)raw_fix_ptr->lnc.map[7][8].param_addr;

	isp_context_ptr->lnc_map_tab[0][0].len=raw_fix_ptr->lnc.map[0][0].len;
	isp_context_ptr->lnc_map_tab[0][1].len=raw_fix_ptr->lnc.map[0][1].len;
	isp_context_ptr->lnc_map_tab[0][2].len=raw_fix_ptr->lnc.map[0][2].len;
	isp_context_ptr->lnc_map_tab[0][3].len=raw_fix_ptr->lnc.map[0][3].len;
	isp_context_ptr->lnc_map_tab[0][4].len=raw_fix_ptr->lnc.map[0][4].len;
	isp_context_ptr->lnc_map_tab[0][5].len=raw_fix_ptr->lnc.map[0][5].len;
	isp_context_ptr->lnc_map_tab[0][6].len=raw_fix_ptr->lnc.map[0][6].len;
	isp_context_ptr->lnc_map_tab[0][7].len=raw_fix_ptr->lnc.map[0][7].len;
	isp_context_ptr->lnc_map_tab[0][8].len=raw_fix_ptr->lnc.map[0][8].len;

	isp_context_ptr->lnc_map_tab[1][0].len=raw_fix_ptr->lnc.map[1][0].len;
	isp_context_ptr->lnc_map_tab[1][1].len=raw_fix_ptr->lnc.map[1][1].len;
	isp_context_ptr->lnc_map_tab[1][2].len=raw_fix_ptr->lnc.map[1][2].len;
	isp_context_ptr->lnc_map_tab[1][3].len=raw_fix_ptr->lnc.map[1][3].len;
	isp_context_ptr->lnc_map_tab[1][4].len=raw_fix_ptr->lnc.map[1][4].len;
	isp_context_ptr->lnc_map_tab[1][5].len=raw_fix_ptr->lnc.map[1][5].len;
	isp_context_ptr->lnc_map_tab[1][6].len=raw_fix_ptr->lnc.map[1][6].len;
	isp_context_ptr->lnc_map_tab[1][7].len=raw_fix_ptr->lnc.map[1][7].len;
	isp_context_ptr->lnc_map_tab[1][8].len=raw_fix_ptr->lnc.map[1][8].len;

	isp_context_ptr->lnc_map_tab[2][0].len=raw_fix_ptr->lnc.map[2][0].len;
	isp_context_ptr->lnc_map_tab[2][1].len=raw_fix_ptr->lnc.map[2][1].len;
	isp_context_ptr->lnc_map_tab[2][2].len=raw_fix_ptr->lnc.map[2][2].len;
	isp_context_ptr->lnc_map_tab[2][3].len=raw_fix_ptr->lnc.map[2][3].len;
	isp_context_ptr->lnc_map_tab[2][4].len=raw_fix_ptr->lnc.map[2][4].len;
	isp_context_ptr->lnc_map_tab[2][5].len=raw_fix_ptr->lnc.map[2][5].len;
	isp_context_ptr->lnc_map_tab[2][6].len=raw_fix_ptr->lnc.map[2][6].len;
	isp_context_ptr->lnc_map_tab[2][7].len=raw_fix_ptr->lnc.map[2][7].len;
	isp_context_ptr->lnc_map_tab[2][8].len=raw_fix_ptr->lnc.map[2][8].len;

	isp_context_ptr->lnc_map_tab[3][0].len=raw_fix_ptr->lnc.map[3][0].len;
	isp_context_ptr->lnc_map_tab[3][1].len=raw_fix_ptr->lnc.map[3][1].len;
	isp_context_ptr->lnc_map_tab[3][2].len=raw_fix_ptr->lnc.map[3][2].len;
	isp_context_ptr->lnc_map_tab[3][3].len=raw_fix_ptr->lnc.map[3][3].len;
	isp_context_ptr->lnc_map_tab[3][4].len=raw_fix_ptr->lnc.map[3][4].len;
	isp_context_ptr->lnc_map_tab[3][5].len=raw_fix_ptr->lnc.map[3][5].len;
	isp_context_ptr->lnc_map_tab[3][6].len=raw_fix_ptr->lnc.map[3][6].len;
	isp_context_ptr->lnc_map_tab[3][7].len=raw_fix_ptr->lnc.map[3][7].len;
	isp_context_ptr->lnc_map_tab[3][8].len=raw_fix_ptr->lnc.map[3][8].len;

	isp_context_ptr->lnc_map_tab[4][0].len=raw_fix_ptr->lnc.map[4][0].len;
	isp_context_ptr->lnc_map_tab[4][1].len=raw_fix_ptr->lnc.map[4][1].len;
	isp_context_ptr->lnc_map_tab[4][2].len=raw_fix_ptr->lnc.map[4][2].len;
	isp_context_ptr->lnc_map_tab[4][3].len=raw_fix_ptr->lnc.map[4][3].len;
	isp_context_ptr->lnc_map_tab[4][4].len=raw_fix_ptr->lnc.map[4][4].len;
	isp_context_ptr->lnc_map_tab[4][5].len=raw_fix_ptr->lnc.map[4][5].len;
	isp_context_ptr->lnc_map_tab[4][6].len=raw_fix_ptr->lnc.map[4][6].len;
	isp_context_ptr->lnc_map_tab[4][7].len=raw_fix_ptr->lnc.map[4][7].len;
	isp_context_ptr->lnc_map_tab[4][8].len=raw_fix_ptr->lnc.map[4][8].len;

	isp_context_ptr->lnc_map_tab[5][0].len=raw_fix_ptr->lnc.map[5][0].len;
	isp_context_ptr->lnc_map_tab[5][1].len=raw_fix_ptr->lnc.map[5][1].len;
	isp_context_ptr->lnc_map_tab[5][2].len=raw_fix_ptr->lnc.map[5][2].len;
	isp_context_ptr->lnc_map_tab[5][3].len=raw_fix_ptr->lnc.map[5][3].len;
	isp_context_ptr->lnc_map_tab[5][4].len=raw_fix_ptr->lnc.map[5][4].len;
	isp_context_ptr->lnc_map_tab[5][5].len=raw_fix_ptr->lnc.map[5][5].len;
	isp_context_ptr->lnc_map_tab[5][6].len=raw_fix_ptr->lnc.map[5][6].len;
	isp_context_ptr->lnc_map_tab[5][7].len=raw_fix_ptr->lnc.map[5][7].len;
	isp_context_ptr->lnc_map_tab[5][8].len=raw_fix_ptr->lnc.map[5][8].len;

	isp_context_ptr->lnc_map_tab[6][0].len=raw_fix_ptr->lnc.map[6][0].len;
	isp_context_ptr->lnc_map_tab[6][1].len=raw_fix_ptr->lnc.map[6][1].len;
	isp_context_ptr->lnc_map_tab[6][2].len=raw_fix_ptr->lnc.map[6][2].len;
	isp_context_ptr->lnc_map_tab[6][3].len=raw_fix_ptr->lnc.map[6][3].len;
	isp_context_ptr->lnc_map_tab[6][4].len=raw_fix_ptr->lnc.map[6][4].len;
	isp_context_ptr->lnc_map_tab[6][5].len=raw_fix_ptr->lnc.map[6][5].len;
	isp_context_ptr->lnc_map_tab[6][6].len=raw_fix_ptr->lnc.map[6][6].len;
	isp_context_ptr->lnc_map_tab[6][7].len=raw_fix_ptr->lnc.map[6][7].len;
	isp_context_ptr->lnc_map_tab[6][8].len=raw_fix_ptr->lnc.map[6][8].len;

	isp_context_ptr->lnc_map_tab[7][0].len=raw_fix_ptr->lnc.map[7][0].len;
	isp_context_ptr->lnc_map_tab[7][1].len=raw_fix_ptr->lnc.map[7][1].len;
	isp_context_ptr->lnc_map_tab[7][2].len=raw_fix_ptr->lnc.map[7][2].len;
	isp_context_ptr->lnc_map_tab[7][3].len=raw_fix_ptr->lnc.map[7][3].len;
	isp_context_ptr->lnc_map_tab[7][4].len=raw_fix_ptr->lnc.map[7][4].len;
	isp_context_ptr->lnc_map_tab[7][5].len=raw_fix_ptr->lnc.map[7][5].len;
	isp_context_ptr->lnc_map_tab[7][6].len=raw_fix_ptr->lnc.map[7][6].len;
	isp_context_ptr->lnc_map_tab[7][7].len=raw_fix_ptr->lnc.map[7][7].len;
	isp_context_ptr->lnc_map_tab[7][8].len=raw_fix_ptr->lnc.map[7][8].len;

	isp_context_ptr->lnc_map_tab[0][0].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[0][0].grid);
	isp_context_ptr->lnc_map_tab[0][1].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[0][1].grid);
	isp_context_ptr->lnc_map_tab[0][2].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[0][2].grid);
	isp_context_ptr->lnc_map_tab[0][3].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[0][3].grid);
	isp_context_ptr->lnc_map_tab[0][4].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[0][4].grid);
	isp_context_ptr->lnc_map_tab[0][5].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[0][5].grid);
	isp_context_ptr->lnc_map_tab[0][6].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[0][6].grid);
	isp_context_ptr->lnc_map_tab[0][7].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[0][7].grid);
	isp_context_ptr->lnc_map_tab[0][8].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[0][8].grid);

	isp_context_ptr->lnc_map_tab[1][0].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[1][0].grid);
	isp_context_ptr->lnc_map_tab[1][1].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[1][1].grid);
	isp_context_ptr->lnc_map_tab[1][2].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[1][2].grid);
	isp_context_ptr->lnc_map_tab[1][3].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[1][3].grid);
	isp_context_ptr->lnc_map_tab[1][4].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[1][4].grid);
	isp_context_ptr->lnc_map_tab[1][5].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[1][5].grid);
	isp_context_ptr->lnc_map_tab[1][6].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[1][6].grid);
	isp_context_ptr->lnc_map_tab[1][7].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[1][7].grid);
	isp_context_ptr->lnc_map_tab[1][8].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[1][8].grid);

	isp_context_ptr->lnc_map_tab[2][0].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[2][0].grid);
	isp_context_ptr->lnc_map_tab[2][1].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[2][1].grid);
	isp_context_ptr->lnc_map_tab[2][2].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[2][2].grid);
	isp_context_ptr->lnc_map_tab[2][3].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[2][3].grid);
	isp_context_ptr->lnc_map_tab[2][4].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[2][4].grid);
	isp_context_ptr->lnc_map_tab[2][5].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[2][5].grid);
	isp_context_ptr->lnc_map_tab[2][6].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[2][6].grid);
	isp_context_ptr->lnc_map_tab[2][7].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[2][7].grid);
	isp_context_ptr->lnc_map_tab[2][8].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[2][8].grid);

	isp_context_ptr->lnc_map_tab[3][0].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[3][0].grid);
	isp_context_ptr->lnc_map_tab[3][1].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[3][1].grid);
	isp_context_ptr->lnc_map_tab[3][2].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[3][2].grid);
	isp_context_ptr->lnc_map_tab[3][3].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[3][3].grid);
	isp_context_ptr->lnc_map_tab[3][4].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[3][4].grid);
	isp_context_ptr->lnc_map_tab[3][5].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[3][5].grid);
	isp_context_ptr->lnc_map_tab[3][6].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[3][6].grid);
	isp_context_ptr->lnc_map_tab[3][7].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[3][7].grid);
	isp_context_ptr->lnc_map_tab[3][8].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[3][8].grid);

	isp_context_ptr->lnc_map_tab[4][0].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[4][0].grid);
	isp_context_ptr->lnc_map_tab[4][1].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[4][1].grid);
	isp_context_ptr->lnc_map_tab[4][2].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[4][2].grid);
	isp_context_ptr->lnc_map_tab[4][3].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[4][3].grid);
	isp_context_ptr->lnc_map_tab[4][4].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[4][4].grid);
	isp_context_ptr->lnc_map_tab[4][5].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[4][5].grid);
	isp_context_ptr->lnc_map_tab[4][6].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[4][6].grid);
	isp_context_ptr->lnc_map_tab[4][7].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[4][7].grid);
	isp_context_ptr->lnc_map_tab[4][8].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[4][8].grid);

	isp_context_ptr->lnc_map_tab[5][0].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[5][0].grid);
	isp_context_ptr->lnc_map_tab[5][1].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[5][1].grid);
	isp_context_ptr->lnc_map_tab[5][2].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[5][2].grid);
	isp_context_ptr->lnc_map_tab[5][3].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[5][3].grid);
	isp_context_ptr->lnc_map_tab[5][4].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[5][4].grid);
	isp_context_ptr->lnc_map_tab[5][5].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[5][5].grid);
	isp_context_ptr->lnc_map_tab[5][6].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[5][6].grid);
	isp_context_ptr->lnc_map_tab[5][7].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[5][7].grid);
	isp_context_ptr->lnc_map_tab[5][8].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[5][8].grid);

	isp_context_ptr->lnc_map_tab[6][0].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[6][0].grid);
	isp_context_ptr->lnc_map_tab[6][1].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[6][1].grid);
	isp_context_ptr->lnc_map_tab[6][2].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[6][2].grid);
	isp_context_ptr->lnc_map_tab[6][3].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[6][3].grid);
	isp_context_ptr->lnc_map_tab[6][4].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[6][4].grid);
	isp_context_ptr->lnc_map_tab[6][5].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[6][5].grid);
	isp_context_ptr->lnc_map_tab[6][6].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[6][6].grid);
	isp_context_ptr->lnc_map_tab[6][7].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[6][7].grid);
	isp_context_ptr->lnc_map_tab[6][8].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[6][8].grid);

	isp_context_ptr->lnc_map_tab[7][0].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[7][0].grid);
	isp_context_ptr->lnc_map_tab[7][1].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[7][1].grid);
	isp_context_ptr->lnc_map_tab[7][2].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[7][2].grid);
	isp_context_ptr->lnc_map_tab[7][3].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[7][3].grid);
	isp_context_ptr->lnc_map_tab[7][4].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[7][4].grid);
	isp_context_ptr->lnc_map_tab[7][5].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[7][5].grid);
	isp_context_ptr->lnc_map_tab[7][6].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[7][6].grid);
	isp_context_ptr->lnc_map_tab[7][7].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[7][7].grid);
	isp_context_ptr->lnc_map_tab[7][8].grid_mode=_ispGetLensGridMode(raw_fix_ptr->lnc.map[7][8].grid);

	isp_context_ptr->lnc_map_tab[0][0].grid_pitch=raw_fix_ptr->lnc.map[0][0].grid;
	isp_context_ptr->lnc_map_tab[0][1].grid_pitch=raw_fix_ptr->lnc.map[0][1].grid;
	isp_context_ptr->lnc_map_tab[0][2].grid_pitch=raw_fix_ptr->lnc.map[0][2].grid;
	isp_context_ptr->lnc_map_tab[0][3].grid_pitch=raw_fix_ptr->lnc.map[0][3].grid;
	isp_context_ptr->lnc_map_tab[0][4].grid_pitch=raw_fix_ptr->lnc.map[0][4].grid;
	isp_context_ptr->lnc_map_tab[0][5].grid_pitch=raw_fix_ptr->lnc.map[0][5].grid;
	isp_context_ptr->lnc_map_tab[0][6].grid_pitch=raw_fix_ptr->lnc.map[0][6].grid;
	isp_context_ptr->lnc_map_tab[0][7].grid_pitch=raw_fix_ptr->lnc.map[0][7].grid;
	isp_context_ptr->lnc_map_tab[0][8].grid_pitch=raw_fix_ptr->lnc.map[0][8].grid;

	isp_context_ptr->lnc_map_tab[1][0].grid_pitch=raw_fix_ptr->lnc.map[1][0].grid;
	isp_context_ptr->lnc_map_tab[1][1].grid_pitch=raw_fix_ptr->lnc.map[1][1].grid;
	isp_context_ptr->lnc_map_tab[1][2].grid_pitch=raw_fix_ptr->lnc.map[1][2].grid;
	isp_context_ptr->lnc_map_tab[1][3].grid_pitch=raw_fix_ptr->lnc.map[1][3].grid;
	isp_context_ptr->lnc_map_tab[1][4].grid_pitch=raw_fix_ptr->lnc.map[1][4].grid;
	isp_context_ptr->lnc_map_tab[1][5].grid_pitch=raw_fix_ptr->lnc.map[1][5].grid;
	isp_context_ptr->lnc_map_tab[1][6].grid_pitch=raw_fix_ptr->lnc.map[1][6].grid;
	isp_context_ptr->lnc_map_tab[1][7].grid_pitch=raw_fix_ptr->lnc.map[1][7].grid;
	isp_context_ptr->lnc_map_tab[1][8].grid_pitch=raw_fix_ptr->lnc.map[1][8].grid;

	isp_context_ptr->lnc_map_tab[2][0].grid_pitch=raw_fix_ptr->lnc.map[2][0].grid;
	isp_context_ptr->lnc_map_tab[2][1].grid_pitch=raw_fix_ptr->lnc.map[2][1].grid;
	isp_context_ptr->lnc_map_tab[2][2].grid_pitch=raw_fix_ptr->lnc.map[2][2].grid;
	isp_context_ptr->lnc_map_tab[2][3].grid_pitch=raw_fix_ptr->lnc.map[2][3].grid;
	isp_context_ptr->lnc_map_tab[2][4].grid_pitch=raw_fix_ptr->lnc.map[2][4].grid;
	isp_context_ptr->lnc_map_tab[2][5].grid_pitch=raw_fix_ptr->lnc.map[2][5].grid;
	isp_context_ptr->lnc_map_tab[2][6].grid_pitch=raw_fix_ptr->lnc.map[2][6].grid;
	isp_context_ptr->lnc_map_tab[2][7].grid_pitch=raw_fix_ptr->lnc.map[2][7].grid;
	isp_context_ptr->lnc_map_tab[2][8].grid_pitch=raw_fix_ptr->lnc.map[2][8].grid;

	isp_context_ptr->lnc_map_tab[3][0].grid_pitch=raw_fix_ptr->lnc.map[3][0].grid;
	isp_context_ptr->lnc_map_tab[3][1].grid_pitch=raw_fix_ptr->lnc.map[3][1].grid;
	isp_context_ptr->lnc_map_tab[3][2].grid_pitch=raw_fix_ptr->lnc.map[3][2].grid;
	isp_context_ptr->lnc_map_tab[3][3].grid_pitch=raw_fix_ptr->lnc.map[3][3].grid;
	isp_context_ptr->lnc_map_tab[3][4].grid_pitch=raw_fix_ptr->lnc.map[3][4].grid;
	isp_context_ptr->lnc_map_tab[3][5].grid_pitch=raw_fix_ptr->lnc.map[3][5].grid;
	isp_context_ptr->lnc_map_tab[3][6].grid_pitch=raw_fix_ptr->lnc.map[3][6].grid;
	isp_context_ptr->lnc_map_tab[3][7].grid_pitch=raw_fix_ptr->lnc.map[3][7].grid;
	isp_context_ptr->lnc_map_tab[3][8].grid_pitch=raw_fix_ptr->lnc.map[3][8].grid;

	isp_context_ptr->lnc_map_tab[4][0].grid_pitch=raw_fix_ptr->lnc.map[4][0].grid;
	isp_context_ptr->lnc_map_tab[4][1].grid_pitch=raw_fix_ptr->lnc.map[4][1].grid;
	isp_context_ptr->lnc_map_tab[4][2].grid_pitch=raw_fix_ptr->lnc.map[4][2].grid;
	isp_context_ptr->lnc_map_tab[4][3].grid_pitch=raw_fix_ptr->lnc.map[4][3].grid;
	isp_context_ptr->lnc_map_tab[4][4].grid_pitch=raw_fix_ptr->lnc.map[4][4].grid;
	isp_context_ptr->lnc_map_tab[4][5].grid_pitch=raw_fix_ptr->lnc.map[4][5].grid;
	isp_context_ptr->lnc_map_tab[4][6].grid_pitch=raw_fix_ptr->lnc.map[4][6].grid;
	isp_context_ptr->lnc_map_tab[4][7].grid_pitch=raw_fix_ptr->lnc.map[4][7].grid;
	isp_context_ptr->lnc_map_tab[4][8].grid_pitch=raw_fix_ptr->lnc.map[4][8].grid;

	isp_context_ptr->lnc_map_tab[5][0].grid_pitch=raw_fix_ptr->lnc.map[5][0].grid;
	isp_context_ptr->lnc_map_tab[5][1].grid_pitch=raw_fix_ptr->lnc.map[5][1].grid;
	isp_context_ptr->lnc_map_tab[5][2].grid_pitch=raw_fix_ptr->lnc.map[5][2].grid;
	isp_context_ptr->lnc_map_tab[5][3].grid_pitch=raw_fix_ptr->lnc.map[5][3].grid;
	isp_context_ptr->lnc_map_tab[5][4].grid_pitch=raw_fix_ptr->lnc.map[5][4].grid;
	isp_context_ptr->lnc_map_tab[5][5].grid_pitch=raw_fix_ptr->lnc.map[5][5].grid;
	isp_context_ptr->lnc_map_tab[5][6].grid_pitch=raw_fix_ptr->lnc.map[5][6].grid;
	isp_context_ptr->lnc_map_tab[5][7].grid_pitch=raw_fix_ptr->lnc.map[5][7].grid;
	isp_context_ptr->lnc_map_tab[5][8].grid_pitch=raw_fix_ptr->lnc.map[5][8].grid;

	isp_context_ptr->lnc_map_tab[6][0].grid_pitch=raw_fix_ptr->lnc.map[6][0].grid;
	isp_context_ptr->lnc_map_tab[6][1].grid_pitch=raw_fix_ptr->lnc.map[6][1].grid;
	isp_context_ptr->lnc_map_tab[6][2].grid_pitch=raw_fix_ptr->lnc.map[6][2].grid;
	isp_context_ptr->lnc_map_tab[6][3].grid_pitch=raw_fix_ptr->lnc.map[6][3].grid;
	isp_context_ptr->lnc_map_tab[6][4].grid_pitch=raw_fix_ptr->lnc.map[6][4].grid;
	isp_context_ptr->lnc_map_tab[6][5].grid_pitch=raw_fix_ptr->lnc.map[6][5].grid;
	isp_context_ptr->lnc_map_tab[6][6].grid_pitch=raw_fix_ptr->lnc.map[6][6].grid;
	isp_context_ptr->lnc_map_tab[6][7].grid_pitch=raw_fix_ptr->lnc.map[6][7].grid;
	isp_context_ptr->lnc_map_tab[6][8].grid_pitch=raw_fix_ptr->lnc.map[6][8].grid;

	isp_context_ptr->lnc_map_tab[7][0].grid_pitch=raw_fix_ptr->lnc.map[7][0].grid;
	isp_context_ptr->lnc_map_tab[7][1].grid_pitch=raw_fix_ptr->lnc.map[7][1].grid;
	isp_context_ptr->lnc_map_tab[7][2].grid_pitch=raw_fix_ptr->lnc.map[7][2].grid;
	isp_context_ptr->lnc_map_tab[7][3].grid_pitch=raw_fix_ptr->lnc.map[7][3].grid;
	isp_context_ptr->lnc_map_tab[7][4].grid_pitch=raw_fix_ptr->lnc.map[7][4].grid;
	isp_context_ptr->lnc_map_tab[7][5].grid_pitch=raw_fix_ptr->lnc.map[7][5].grid;
	isp_context_ptr->lnc_map_tab[7][6].grid_pitch=raw_fix_ptr->lnc.map[7][6].grid;
	isp_context_ptr->lnc_map_tab[7][7].grid_pitch=raw_fix_ptr->lnc.map[7][7].grid;
	isp_context_ptr->lnc_map_tab[7][8].grid_pitch=raw_fix_ptr->lnc.map[7][8].grid;

	isp_context_ptr->lnc.load_buf=ISP_ONE;
	isp_context_ptr->lnc.cur_use_buf=ISP_ONE;

	max_param_index=_ispGetIspParamMaxIndex(handler_id, raw_info_ptr);
	isp_context_ptr->isp_lnc_addr=ispAlloc(handler_id, raw_fix_ptr->lnc.map[max_param_index-ISP_ONE][0].len);

	/*ae*/
	/*isp_context_ptr->ae.frame_mode=raw_tune_ptr->ae.frame_mode;
	isp_context_ptr->ae.tab_type=raw_tune_ptr->ae.tab_type;
	isp_context_ptr->ae.weight=raw_tune_ptr->ae.weight;*/
	isp_context_ptr->ae.tab_type=ISP_NORMAL_50HZ;
	isp_context_ptr->ae.frame_mode=ISP_AE_AUTO;
	isp_context_ptr->ae.iso=ISP_ISO_100;
	isp_context_ptr->ae.cur_iso=ISP_ISO_100;
	isp_context_ptr->ae.back_iso=ISP_ISO_100;
	isp_context_ptr->ae.real_iso=ISP_ISO_100;
	isp_context_ptr->ae.skip_frame=raw_tune_ptr->ae.skip_frame;
	isp_context_ptr->ae.fix_fps=raw_tune_ptr->ae.normal_fix_fps;
	isp_context_ptr->ae.normal_fps=raw_tune_ptr->ae.normal_fix_fps;
	isp_context_ptr->ae.night_fps=raw_tune_ptr->ae.night_fix_fps;
	isp_context_ptr->ae.sport_fps=raw_tune_ptr->ae.video_fps;
	isp_context_ptr->ae.line_time=_ispGetLineTime((struct isp_resolution_info*)&isp_context_ptr->input_size_trim, isp_context_ptr->param_index);
	isp_context_ptr->ae.frame_line=_ispGetFrameLine(handler_id, isp_context_ptr->ae.line_time, isp_context_ptr->ae.fix_fps);
	isp_context_ptr->ae.target_lum=raw_tune_ptr->ae.target_lum;
	isp_context_ptr->ae.target_zone=raw_tune_ptr->ae.target_zone;
	isp_context_ptr->ae.quick_mode=raw_tune_ptr->ae.quick_mode;
	if (ISP_ZERO == raw_tune_ptr->ae.min_exposure) {
		isp_context_ptr->ae.min_exposure=ISP_ONE;
	} else {
		isp_context_ptr->ae.min_exposure=raw_tune_ptr->ae.min_exposure;
	}

	isp_context_ptr->ae.monitor.w=raw_tune_ptr->awb.win_size.w;
	isp_context_ptr->ae.monitor.h=raw_tune_ptr->awb.win_size.h;

	isp_context_ptr->ae.smart=raw_tune_ptr->ae.smart;
	isp_context_ptr->ae.smart_mode=raw_tune_ptr->ae.smart_mode;
	isp_context_ptr->ae.smart_rotio=raw_tune_ptr->ae.smart_rotio;
	isp_context_ptr->ae.smart_base_gain=raw_tune_ptr->ae.smart_base_gain;
	isp_context_ptr->ae.smart_wave_min=raw_tune_ptr->ae.smart_wave_min;
	isp_context_ptr->ae.smart_wave_max=raw_tune_ptr->ae.smart_wave_max;
	isp_context_ptr->ae.smart_pref_min=raw_tune_ptr->ae.smart_pref_min;
	isp_context_ptr->ae.smart_pref_max=raw_tune_ptr->ae.smart_pref_max;
	isp_context_ptr->ae.smart_denoise_min_index=raw_tune_ptr->ae.smart_denoise_min_index;
	isp_context_ptr->ae.smart_denoise_max_index=raw_tune_ptr->ae.smart_denoise_max_index;
	isp_context_ptr->ae.smart_edge_min_index=raw_tune_ptr->ae.smart_edge_min_index;
	isp_context_ptr->ae.smart_edge_max_index=raw_tune_ptr->ae.smart_edge_max_index;
	isp_context_ptr->ae.smart_sta_low_thr=raw_tune_ptr->ae.smart_sta_low_thr=60;
	isp_context_ptr->ae.smart_sta_ratio1=raw_tune_ptr->ae.smart_sta_ratio1=120;
	isp_context_ptr->ae.smart_sta_ratio=raw_tune_ptr->ae.smart_sta_ratio=138;
	isp_context_ptr->ae.smart_sta_start_index=raw_tune_ptr->ae.smart_sta_start_index=170;
	isp_context_ptr->ae.again_skip=raw_tune_ptr->ae.again_skip;
	isp_context_ptr->ae.dgain_skip=raw_tune_ptr->ae.dgain_skip;
	isp_context_ptr->ae.lux_500_index=raw_tune_ptr->ae.lux_500_index;

	isp_context_ptr->gamma_index=raw_tune_ptr->ae.gamma_start;
	isp_context_ptr->ae.gamma_num=raw_tune_ptr->ae.gamma_num;
	isp_context_ptr->ae.gamma_zone=raw_tune_ptr->ae.gamma_zone;
	isp_context_ptr->ae.gamma_thr[0]=raw_tune_ptr->ae.gamma_thr[0];
	isp_context_ptr->ae.gamma_thr[1]=raw_tune_ptr->ae.gamma_thr[1];
	isp_context_ptr->ae.gamma_thr[2]=raw_tune_ptr->ae.gamma_thr[2];
	isp_context_ptr->ae.gamma_thr[3]=raw_tune_ptr->ae.gamma_thr[3];

	isp_context_ptr->ae.ev=raw_tune_ptr->ae.ev[3];
	isp_ae_set_ev(handler_id, raw_tune_ptr->ae.ev[3]);
	isp_context_ptr->ev_tab[0]=raw_tune_ptr->ae.ev[0];
	isp_context_ptr->ev_tab[1]=raw_tune_ptr->ae.ev[1];
	isp_context_ptr->ev_tab[2]=raw_tune_ptr->ae.ev[2];
	isp_context_ptr->ev_tab[3]=raw_tune_ptr->ae.ev[3];
	isp_context_ptr->ev_tab[4]=raw_tune_ptr->ae.ev[4];
	isp_context_ptr->ev_tab[5]=raw_tune_ptr->ae.ev[5];
	isp_context_ptr->ev_tab[6]=raw_tune_ptr->ae.ev[6];
	isp_context_ptr->ev_tab[7]=raw_tune_ptr->ae.ev[7];
	isp_context_ptr->ev_tab[8]=raw_tune_ptr->ae.ev[8];
	isp_context_ptr->ev_tab[9]=raw_tune_ptr->ae.ev[9];
	isp_context_ptr->ev_tab[10]=raw_tune_ptr->ae.ev[10];
	isp_context_ptr->ev_tab[11]=raw_tune_ptr->ae.ev[11];
	isp_context_ptr->ev_tab[12]=raw_tune_ptr->ae.ev[12];
	isp_context_ptr->ev_tab[13]=raw_tune_ptr->ae.ev[13];
	isp_context_ptr->ev_tab[14]=raw_tune_ptr->ae.ev[14];
	isp_context_ptr->ev_tab[15]=raw_tune_ptr->ae.ev[15];

	isp_context_ptr->ae.stat_r_ptr=(uint32_t*)&isp_context_ptr->awb_stat.r_info;
	isp_context_ptr->ae.stat_g_ptr=(uint32_t*)&isp_context_ptr->awb_stat.g_info;
	isp_context_ptr->ae.stat_b_ptr=(uint32_t*)&isp_context_ptr->awb_stat.b_info;
	isp_context_ptr->ae.stat_y_ptr=(uint32_t*)&isp_context_ptr->ae_stat.y;

	isp_context_ptr->ae.weight_ptr[0]=(uint8_t*)ISP_AEAWB_weight_avrg;
	isp_context_ptr->ae.weight_ptr[1]=(uint8_t*)ISP_AEAWB_weight_center;
	isp_context_ptr->ae.weight_ptr[2]=raw_fix_ptr->ae.weight_tab;
	isp_context_ptr->ae.weight_id=ISP_ONE;
	_ispAeMeasureLumSet(handler_id, ISP_AE_WEIGHT_AVG);

	isp_context_ptr->ae.tab[0].e_ptr=raw_fix_ptr->ae.tab[0].e_ptr;
	isp_context_ptr->ae.tab[0].g_ptr=raw_fix_ptr->ae.tab[0].g_ptr;
	_ispSetAeTabMaxIndex(handler_id, ISP_NORMAL_50HZ, ISP_ISO_AUTO, raw_tune_ptr->ae.normal_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NORMAL_50HZ, ISP_ISO_100, raw_tune_ptr->ae.normal_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NORMAL_50HZ, ISP_ISO_200, raw_tune_ptr->ae.normal_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NORMAL_50HZ, ISP_ISO_400, raw_tune_ptr->ae.normal_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NORMAL_50HZ, ISP_ISO_800, raw_tune_ptr->ae.normal_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NORMAL_50HZ, ISP_ISO_1600, raw_tune_ptr->ae.normal_fix_fps);

	isp_context_ptr->ae.tab[1].e_ptr=raw_fix_ptr->ae.tab[1].e_ptr;
	isp_context_ptr->ae.tab[1].g_ptr=raw_fix_ptr->ae.tab[1].g_ptr;
	_ispSetAeTabMaxIndex(handler_id, ISP_NORMAL_60HZ, ISP_ISO_AUTO, raw_tune_ptr->ae.normal_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NORMAL_60HZ, ISP_ISO_100, raw_tune_ptr->ae.normal_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NORMAL_60HZ, ISP_ISO_200, raw_tune_ptr->ae.normal_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NORMAL_60HZ, ISP_ISO_400, raw_tune_ptr->ae.normal_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NORMAL_60HZ, ISP_ISO_800, raw_tune_ptr->ae.normal_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NORMAL_60HZ, ISP_ISO_1600, raw_tune_ptr->ae.normal_fix_fps);

	isp_context_ptr->ae.tab[2].e_ptr=raw_fix_ptr->ae.tab[2].e_ptr;
	isp_context_ptr->ae.tab[2].g_ptr=raw_fix_ptr->ae.tab[2].g_ptr;
	_ispSetAeTabMaxIndex(handler_id, ISP_NIGHT_50HZ, ISP_ISO_AUTO, raw_tune_ptr->ae.night_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NIGHT_50HZ, ISP_ISO_100, raw_tune_ptr->ae.night_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NIGHT_50HZ, ISP_ISO_200, raw_tune_ptr->ae.night_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NIGHT_50HZ, ISP_ISO_400, raw_tune_ptr->ae.night_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NIGHT_50HZ, ISP_ISO_800, raw_tune_ptr->ae.night_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NIGHT_50HZ, ISP_ISO_1600, raw_tune_ptr->ae.night_fix_fps);

	isp_context_ptr->ae.tab[3].e_ptr=raw_fix_ptr->ae.tab[3].e_ptr;
	isp_context_ptr->ae.tab[3].g_ptr=raw_fix_ptr->ae.tab[3].g_ptr;
	_ispSetAeTabMaxIndex(handler_id, ISP_NIGHT_60HZ, ISP_ISO_AUTO, raw_tune_ptr->ae.night_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NIGHT_60HZ, ISP_ISO_100, raw_tune_ptr->ae.night_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NIGHT_60HZ, ISP_ISO_200, raw_tune_ptr->ae.night_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NIGHT_60HZ, ISP_ISO_400, raw_tune_ptr->ae.night_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NIGHT_60HZ, ISP_ISO_800, raw_tune_ptr->ae.night_fix_fps);
	_ispSetAeTabMaxIndex(handler_id, ISP_NIGHT_60HZ, ISP_ISO_1600, raw_tune_ptr->ae.night_fix_fps);

	isp_context_ptr->ae.cur_index=raw_fix_ptr->ae.tab[0].index[0].start;
	isp_context_ptr->ae.max_index=raw_fix_ptr->ae.tab[0].index[0].max;

	isp_context_ptr->ae.speed_bright_to_dark = cali_ptr->ae.speed_bright_to_dark;
	isp_context_ptr->ae.step_bright_to_dark = cali_ptr->ae.step_bright_to_dark;
	isp_context_ptr->ae.speed_dark_to_bright = cali_ptr->ae.speed_dark_to_bright;
	isp_context_ptr->ae.step_dark_to_bright = cali_ptr->ae.step_dark_to_bright;
	isp_context_ptr->ae.histogram_segment = cali_ptr->ae.histogram_segment;

	isp_context_ptr->ae.histogram_segment_num = cali_ptr->ae.histogram_segment_num;
	isp_context_ptr->ae.flash_cali.gldn_cali.r_sum = cali_ptr->flashlight.golden_cali_info.r_sum;
	isp_context_ptr->ae.flash_cali.gldn_cali.b_sum = cali_ptr->flashlight.golden_cali_info.b_sum;
	isp_context_ptr->ae.flash_cali.gldn_cali.gr_sum = cali_ptr->flashlight.golden_cali_info.gr_sum;
	isp_context_ptr->ae.flash_cali.gldn_cali.gb_sum = cali_ptr->flashlight.golden_cali_info.gb_sum;
	isp_context_ptr->ae.flash_cali.rdm_cali.r_sum = cali_ptr->flashlight.cali_info.r_sum;
	isp_context_ptr->ae.flash_cali.rdm_cali.b_sum = cali_ptr->flashlight.cali_info.b_sum;
	isp_context_ptr->ae.flash_cali.rdm_cali.gr_sum = cali_ptr->flashlight.cali_info.gr_sum;
	isp_context_ptr->ae.flash_cali.rdm_cali.gb_sum = cali_ptr->flashlight.cali_info.gb_sum;

	isp_context_ptr->ae.callback = param_ptr->callback;
	isp_context_ptr->ae.self_callback = param_ptr->self_callback;
	isp_context_ptr->ae.awbm_skip = ispAwbmSkip;
	isp_context_ptr->ae.awbm_bypass = ispAwbcBypass;
	isp_context_ptr->ae.ae_set_eb=ISP_UEB;

	/*flash*/
	isp_context_ptr->flash.lum_ratio=raw_tune_ptr->flash.lum_ratio;
	isp_context_ptr->flash.r_ratio=raw_tune_ptr->flash.r_ratio;
	isp_context_ptr->flash.g_ratio=raw_tune_ptr->flash.g_ratio;
	isp_context_ptr->flash.b_ratio=raw_tune_ptr->flash.b_ratio;

	/*awb*/
	isp_context_ptr->awbm.win_start.x=raw_tune_ptr->awb.win_start.x;
	isp_context_ptr->awbm.win_start.y=raw_tune_ptr->awb.win_start.y;
	isp_context_ptr->awbm.win_size.w=raw_tune_ptr->awb.win_size.w;
	isp_context_ptr->awbm.win_size.h=raw_tune_ptr->awb.win_size.h;

	isp_context_ptr->awb.back_monitor_pos.x=raw_tune_ptr->awb.win_start.x;
	isp_context_ptr->awb.back_monitor_pos.y=raw_tune_ptr->awb.win_start.y;
	isp_context_ptr->awb.back_monitor_size.w=raw_tune_ptr->awb.win_size.w;
	isp_context_ptr->awb.back_monitor_size.h=raw_tune_ptr->awb.win_size.h;

	isp_context_ptr->awb_r_gain[0]=raw_tune_ptr->awb.r_gain[0];
	isp_context_ptr->awb_g_gain[0]=raw_tune_ptr->awb.g_gain[0];
	isp_context_ptr->awb_b_gain[0]=raw_tune_ptr->awb.b_gain[0];
	isp_context_ptr->awb_r_gain[1]=raw_tune_ptr->awb.r_gain[1];
	isp_context_ptr->awb_g_gain[1]=raw_tune_ptr->awb.g_gain[1];
	isp_context_ptr->awb_b_gain[1]=raw_tune_ptr->awb.b_gain[1];
	isp_context_ptr->awb_r_gain[2]=raw_tune_ptr->awb.r_gain[2];
	isp_context_ptr->awb_g_gain[2]=raw_tune_ptr->awb.g_gain[2];
	isp_context_ptr->awb_b_gain[2]=raw_tune_ptr->awb.b_gain[2];
	isp_context_ptr->awb_r_gain[3]=raw_tune_ptr->awb.r_gain[3];
	isp_context_ptr->awb_g_gain[3]=raw_tune_ptr->awb.g_gain[3];
	isp_context_ptr->awb_b_gain[3]=raw_tune_ptr->awb.b_gain[3];
	isp_context_ptr->awb_r_gain[4]=raw_tune_ptr->awb.r_gain[4];
	isp_context_ptr->awb_g_gain[4]=raw_tune_ptr->awb.g_gain[4];
	isp_context_ptr->awb_b_gain[4]=raw_tune_ptr->awb.b_gain[4];
	isp_context_ptr->awb_r_gain[5]=raw_tune_ptr->awb.r_gain[5];
	isp_context_ptr->awb_g_gain[5]=raw_tune_ptr->awb.g_gain[5];
	isp_context_ptr->awb_b_gain[5]=raw_tune_ptr->awb.b_gain[5];
	isp_context_ptr->awb_r_gain[6]=raw_tune_ptr->awb.r_gain[6];
	isp_context_ptr->awb_g_gain[6]=raw_tune_ptr->awb.g_gain[6];
	isp_context_ptr->awb_b_gain[6]=raw_tune_ptr->awb.b_gain[6];
	isp_context_ptr->awb_r_gain[7]=raw_tune_ptr->awb.r_gain[7];
	isp_context_ptr->awb_g_gain[7]=raw_tune_ptr->awb.g_gain[7];
	isp_context_ptr->awb_b_gain[7]=raw_tune_ptr->awb.b_gain[7];
	isp_context_ptr->awb_r_gain[8]=raw_tune_ptr->awb.r_gain[8];
	isp_context_ptr->awb_g_gain[8]=raw_tune_ptr->awb.g_gain[8];
	isp_context_ptr->awb_b_gain[8]=raw_tune_ptr->awb.b_gain[8];

	for(i=0x00; i<8; i++) {
		isp_context_ptr->awb.gain_convert[i].r=raw_tune_ptr->awb.gain_convert[i].r;
		isp_context_ptr->awb.gain_convert[i].g=raw_tune_ptr->awb.gain_convert[i].g;
		isp_context_ptr->awb.gain_convert[i].b=raw_tune_ptr->awb.gain_convert[i].b;
	}

	/*isp_context_ptr->awb.mode=raw_tune_ptr->awb.mode;
	isp_context_ptr->awb.weight=raw_tune_ptr->awb.weight;*/
	isp_context_ptr->awb.weight_ptr[0]=(uint8_t*)ISP_AEAWB_weight_avrg;
	isp_context_ptr->awb.weight_ptr[1]=(uint8_t*)ISP_AEAWB_weight_center;
	isp_context_ptr->awb.weight_ptr[2]=NULL;
	for(i=0x00; i<20; i++) {
		isp_context_ptr->awb.win[i].x=raw_tune_ptr->awb.win[i].x;
		isp_context_ptr->awb.win[i].yb=raw_tune_ptr->awb.win[i].yb;
		isp_context_ptr->awb.win[i].yt=raw_tune_ptr->awb.win[i].yt;
	}

	isp_context_ptr->awb.cali_info.b_sum = cali_ptr->awb.cali_info.b_sum;
	isp_context_ptr->awb.cali_info.r_sum = cali_ptr->awb.cali_info.r_sum;
	isp_context_ptr->awb.cali_info.gr_sum = cali_ptr->awb.cali_info.gr_sum;
	isp_context_ptr->awb.cali_info.gb_sum = cali_ptr->awb.cali_info.gb_sum;

	isp_context_ptr->awb.golden_info.b_sum= cali_ptr->awb.golden_cali_info.b_sum;
	isp_context_ptr->awb.golden_info.r_sum = cali_ptr->awb.golden_cali_info.r_sum;
	isp_context_ptr->awb.golden_info.gr_sum = cali_ptr->awb.golden_cali_info.gr_sum;
	isp_context_ptr->awb.golden_info.gb_sum = cali_ptr->awb.golden_cali_info.gb_sum;

	isp_context_ptr->awb.alg_id=raw_tune_ptr->awb.alg_id;
	isp_context_ptr->awb.scanline_map.addr=raw_fix_ptr->awb.addr;
	isp_context_ptr->awb.scanline_map.len=raw_fix_ptr->awb.len;
	isp_context_ptr->awb.gain_index=raw_tune_ptr->awb.gain_index;
	isp_context_ptr->awb.target_zone=raw_tune_ptr->awb.target_zone;
	isp_context_ptr->awb.quick_mode=raw_tune_ptr->awb.quick_mode;
	isp_context_ptr->awb.smart=raw_tune_ptr->awb.smart;
	isp_context_ptr->awb.cur_index=raw_tune_ptr->awb.smart_index;
	isp_context_ptr->awb.prv_index=isp_context_ptr->awb.cur_index;
	isp_context_ptr->awb.cur_gain.r=isp_context_ptr->awb_r_gain[isp_context_ptr->awb.gain_index];
	isp_context_ptr->awb.cur_gain.g=isp_context_ptr->awb_g_gain[isp_context_ptr->awb.gain_index];
	isp_context_ptr->awb.cur_gain.b=isp_context_ptr->awb_b_gain[isp_context_ptr->awb.gain_index];
	isp_context_ptr->awb.cur_T = 4800;
	isp_context_ptr->awb.matrix_index=ISP_ZERO;
	isp_context_ptr->cmc_index=ISP_ZERO;
	//isp_context_ptr->awb.cur_color_temperature=0x00;
	isp_context_ptr->awb.set_eb=ISP_EB;
	isp_context_ptr->awb.continue_focus_stat=_isp_ContinueFocusInforCallback;
	isp_context_ptr->awb.set_saturation_offset = _ispCfgSaturationoffset;
	isp_context_ptr->awb.set_hue_offset = _ispCfgHueoffset;
	isp_context_ptr->awb.get_ev_lux= _ispGetEvLum;
	isp_context_ptr->awb.GetDefaultGain = ispGetAwbDefaultGain;
	isp_context_ptr->awb.change_param = isp_change_param;

	isp_context_ptr->awbc.r_gain=isp_context_ptr->awb.cur_gain.r;
	isp_context_ptr->awbc.g_gain=isp_context_ptr->awb.cur_gain.g;
	isp_context_ptr->awbc.b_gain=isp_context_ptr->awb.cur_gain.b;

	isp_context_ptr->awb.target_gain.r=isp_context_ptr->awb.cur_gain.r;
	isp_context_ptr->awb.target_gain.g=isp_context_ptr->awb.cur_gain.g;
	isp_context_ptr->awb.target_gain.b=isp_context_ptr->awb.cur_gain.b;

	isp_context_ptr->awb.g_estimate.num=raw_tune_ptr->awb.g_estimate.num;
	for (i=0; i<isp_context_ptr->awb.g_estimate.num; i++)
	{
		isp_context_ptr->awb.g_estimate.t_thr[i] = raw_tune_ptr->awb.g_estimate.t_thr[i];
		isp_context_ptr->awb.g_estimate.g_thr[i][0] = raw_tune_ptr->awb.g_estimate.g_thr[i][0];
		isp_context_ptr->awb.g_estimate.g_thr[i][1] = raw_tune_ptr->awb.g_estimate.g_thr[i][1];
		isp_context_ptr->awb.g_estimate.w_thr[i][0] = raw_tune_ptr->awb.g_estimate.w_thr[i][0];
		isp_context_ptr->awb.g_estimate.w_thr[i][1] = raw_tune_ptr->awb.g_estimate.w_thr[i][1];
	}
	isp_context_ptr->awb.gain_adjust.num=raw_tune_ptr->awb.gain_adjust.num;
	for (i=0; i<isp_context_ptr->awb.gain_adjust.num; i++)
	{
		isp_context_ptr->awb.gain_adjust.t_thr[i] = raw_tune_ptr->awb.gain_adjust.t_thr[i];
		isp_context_ptr->awb.gain_adjust.w_thr[i] = raw_tune_ptr->awb.gain_adjust.w_thr[i];
	}
	isp_context_ptr->awb.t_func.a=raw_tune_ptr->awb.t_func.a;
	isp_context_ptr->awb.t_func.b=raw_tune_ptr->awb.t_func.b;
	isp_context_ptr->awb.t_func.shift=raw_tune_ptr->awb.t_func.shift;
	isp_context_ptr->awb.wp_count_range.max_proportion=raw_tune_ptr->awb.wp_count_range.max_proportion;
	isp_context_ptr->awb.wp_count_range.min_proportion=raw_tune_ptr->awb.wp_count_range.min_proportion;
	isp_context_ptr->awb.debug_level=raw_tune_ptr->awb.debug_level;

	isp_context_ptr->awb.light.num = raw_tune_ptr->awb.light.num;
	for (i=0; i<isp_context_ptr->awb.light.num; i++)
	{
		isp_context_ptr->awb.light.t_thr[i] = raw_tune_ptr->awb.light.t_thr[i];
		isp_context_ptr->awb.light.w_thr[i] = raw_tune_ptr->awb.light.w_thr[i];
	}

	isp_context_ptr->awb.steady_speed = raw_tune_ptr->awb.steady_speed;
	isp_context_ptr->awb.mointor_info=_ispAwbGetMonitorInfo;
	isp_context_ptr->awb.set_monitor_win=_ispAwbSetMonitorWin;
	isp_context_ptr->awb.recover_monitor_wn=_ispAwbSetMonitorWinRecover;

	/*bpc*/
	isp_context_ptr->bpc.mode=ISP_ZERO;
	isp_context_ptr->bpc.flat_thr=raw_tune_ptr->bpc.flat_thr;
	isp_context_ptr->bpc.std_thr=raw_tune_ptr->bpc.std_thr;
	isp_context_ptr->bpc.texture_thr=raw_tune_ptr->bpc.texture_thr;

	/*denoise*/
	isp_context_ptr->denoise.write_back=raw_tune_ptr->denoise.write_back;
	memcpy((void*)isp_context_ptr->denoise_tab[0].diswei, (void*)raw_tune_ptr->denoise.diswei, 19);
	memcpy((void*)isp_context_ptr->denoise_tab[0].ranwei, (void*)raw_tune_ptr->denoise.ranwei, 31);
	memcpy((void*)isp_context_ptr->denoise_tab[1].diswei, (void*)raw_tune_ptr->denoise.tab[0].diswei, 19);
	memcpy((void*)isp_context_ptr->denoise_tab[1].ranwei, (void*)raw_tune_ptr->denoise.tab[0].ranwei, 31);
	isp_context_ptr->denoise.r_thr=raw_tune_ptr->denoise.r_thr;
	isp_context_ptr->denoise.g_thr=raw_tune_ptr->denoise.g_thr;
	isp_context_ptr->denoise.b_thr=raw_tune_ptr->denoise.b_thr;
	memcpy((void*)&isp_context_ptr->denoise.diswei, (void*)&raw_tune_ptr->denoise.diswei, 19);
	memcpy((void*)&isp_context_ptr->denoise.ranwei, (void*)&raw_tune_ptr->denoise.ranwei, 31);
	isp_ae_set_denosie_level(handler_id, 0);

	/*grgb*/
	isp_context_ptr->grgb.edge_thr=raw_tune_ptr->grgb.edge_thr;
	isp_context_ptr->grgb.diff_thr=raw_tune_ptr->grgb.diff_thr;

	/*cfa*/
	isp_context_ptr->cfa.edge_thr=raw_tune_ptr->cfa.edge_thr;
	isp_context_ptr->cfa.diff_thr=raw_tune_ptr->cfa.diff_thr;

	/*cmc*/
	/*SCI_MEMCPY((void*)&isp_context_ptr->cmc.matrix, (void*)&raw_tune_ptr->cmc.matrix, 18);*/
	isp_context_ptr->cmc.matrix[0]=raw_tune_ptr->cmc.matrix[0][0];
	isp_context_ptr->cmc.matrix[1]=raw_tune_ptr->cmc.matrix[0][1];
	isp_context_ptr->cmc.matrix[2]=raw_tune_ptr->cmc.matrix[0][2];
	isp_context_ptr->cmc.matrix[3]=raw_tune_ptr->cmc.matrix[0][3];
	isp_context_ptr->cmc.matrix[4]=raw_tune_ptr->cmc.matrix[0][4];
	isp_context_ptr->cmc.matrix[5]=raw_tune_ptr->cmc.matrix[0][5];
	isp_context_ptr->cmc.matrix[6]=raw_tune_ptr->cmc.matrix[0][6];
	isp_context_ptr->cmc.matrix[7]=raw_tune_ptr->cmc.matrix[0][7];
	isp_context_ptr->cmc.matrix[8]=raw_tune_ptr->cmc.matrix[0][8];

	isp_context_ptr->cmc_tab[0][0]=raw_tune_ptr->cmc.matrix[0][0];
	isp_context_ptr->cmc_tab[0][1]=raw_tune_ptr->cmc.matrix[0][1];
	isp_context_ptr->cmc_tab[0][2]=raw_tune_ptr->cmc.matrix[0][2];
	isp_context_ptr->cmc_tab[0][3]=raw_tune_ptr->cmc.matrix[0][3];
	isp_context_ptr->cmc_tab[0][4]=raw_tune_ptr->cmc.matrix[0][4];
	isp_context_ptr->cmc_tab[0][5]=raw_tune_ptr->cmc.matrix[0][5];
	isp_context_ptr->cmc_tab[0][6]=raw_tune_ptr->cmc.matrix[0][6];
	isp_context_ptr->cmc_tab[0][7]=raw_tune_ptr->cmc.matrix[0][7];
	isp_context_ptr->cmc_tab[0][8]=raw_tune_ptr->cmc.matrix[0][8];

	isp_context_ptr->cmc_tab[1][0]=raw_tune_ptr->cmc.matrix[1][0];
	isp_context_ptr->cmc_tab[1][1]=raw_tune_ptr->cmc.matrix[1][1];
	isp_context_ptr->cmc_tab[1][2]=raw_tune_ptr->cmc.matrix[1][2];
	isp_context_ptr->cmc_tab[1][3]=raw_tune_ptr->cmc.matrix[1][3];
	isp_context_ptr->cmc_tab[1][4]=raw_tune_ptr->cmc.matrix[1][4];
	isp_context_ptr->cmc_tab[1][5]=raw_tune_ptr->cmc.matrix[1][5];
	isp_context_ptr->cmc_tab[1][6]=raw_tune_ptr->cmc.matrix[1][6];
	isp_context_ptr->cmc_tab[1][7]=raw_tune_ptr->cmc.matrix[1][7];
	isp_context_ptr->cmc_tab[1][8]=raw_tune_ptr->cmc.matrix[1][8];

	isp_context_ptr->cmc_tab[2][0]=raw_tune_ptr->cmc.matrix[2][0];
	isp_context_ptr->cmc_tab[2][1]=raw_tune_ptr->cmc.matrix[2][1];
	isp_context_ptr->cmc_tab[2][2]=raw_tune_ptr->cmc.matrix[2][2];
	isp_context_ptr->cmc_tab[2][3]=raw_tune_ptr->cmc.matrix[2][3];
	isp_context_ptr->cmc_tab[2][4]=raw_tune_ptr->cmc.matrix[2][4];
	isp_context_ptr->cmc_tab[2][5]=raw_tune_ptr->cmc.matrix[2][5];
	isp_context_ptr->cmc_tab[2][6]=raw_tune_ptr->cmc.matrix[2][6];
	isp_context_ptr->cmc_tab[2][7]=raw_tune_ptr->cmc.matrix[2][7];
	isp_context_ptr->cmc_tab[2][8]=raw_tune_ptr->cmc.matrix[2][8];

	isp_context_ptr->cmc_tab[3][0]=raw_tune_ptr->cmc.matrix[3][0];
	isp_context_ptr->cmc_tab[3][1]=raw_tune_ptr->cmc.matrix[3][1];
	isp_context_ptr->cmc_tab[3][2]=raw_tune_ptr->cmc.matrix[3][2];
	isp_context_ptr->cmc_tab[3][3]=raw_tune_ptr->cmc.matrix[3][3];
	isp_context_ptr->cmc_tab[3][4]=raw_tune_ptr->cmc.matrix[3][4];
	isp_context_ptr->cmc_tab[3][5]=raw_tune_ptr->cmc.matrix[3][5];
	isp_context_ptr->cmc_tab[3][6]=raw_tune_ptr->cmc.matrix[3][6];
	isp_context_ptr->cmc_tab[3][7]=raw_tune_ptr->cmc.matrix[3][7];
	isp_context_ptr->cmc_tab[3][8]=raw_tune_ptr->cmc.matrix[3][8];

	isp_context_ptr->cmc_tab[4][0]=raw_tune_ptr->cmc.matrix[4][0];
	isp_context_ptr->cmc_tab[4][1]=raw_tune_ptr->cmc.matrix[4][1];
	isp_context_ptr->cmc_tab[4][2]=raw_tune_ptr->cmc.matrix[4][2];
	isp_context_ptr->cmc_tab[4][3]=raw_tune_ptr->cmc.matrix[4][3];
	isp_context_ptr->cmc_tab[4][4]=raw_tune_ptr->cmc.matrix[4][4];
	isp_context_ptr->cmc_tab[4][5]=raw_tune_ptr->cmc.matrix[4][5];
	isp_context_ptr->cmc_tab[4][6]=raw_tune_ptr->cmc.matrix[4][6];
	isp_context_ptr->cmc_tab[4][7]=raw_tune_ptr->cmc.matrix[4][7];
	isp_context_ptr->cmc_tab[4][8]=raw_tune_ptr->cmc.matrix[4][8];

	isp_context_ptr->cmc_tab[5][0]=raw_tune_ptr->cmc.matrix[5][0];
	isp_context_ptr->cmc_tab[5][1]=raw_tune_ptr->cmc.matrix[5][1];
	isp_context_ptr->cmc_tab[5][2]=raw_tune_ptr->cmc.matrix[5][2];
	isp_context_ptr->cmc_tab[5][3]=raw_tune_ptr->cmc.matrix[5][3];
	isp_context_ptr->cmc_tab[5][4]=raw_tune_ptr->cmc.matrix[5][4];
	isp_context_ptr->cmc_tab[5][5]=raw_tune_ptr->cmc.matrix[5][5];
	isp_context_ptr->cmc_tab[5][6]=raw_tune_ptr->cmc.matrix[5][6];
	isp_context_ptr->cmc_tab[5][7]=raw_tune_ptr->cmc.matrix[5][7];
	isp_context_ptr->cmc_tab[5][8]=raw_tune_ptr->cmc.matrix[5][8];

	isp_context_ptr->cmc_tab[6][0]=raw_tune_ptr->cmc.matrix[6][0];
	isp_context_ptr->cmc_tab[6][1]=raw_tune_ptr->cmc.matrix[6][1];
	isp_context_ptr->cmc_tab[6][2]=raw_tune_ptr->cmc.matrix[6][2];
	isp_context_ptr->cmc_tab[6][3]=raw_tune_ptr->cmc.matrix[6][3];
	isp_context_ptr->cmc_tab[6][4]=raw_tune_ptr->cmc.matrix[6][4];
	isp_context_ptr->cmc_tab[6][5]=raw_tune_ptr->cmc.matrix[6][5];
	isp_context_ptr->cmc_tab[6][6]=raw_tune_ptr->cmc.matrix[6][6];
	isp_context_ptr->cmc_tab[6][7]=raw_tune_ptr->cmc.matrix[6][7];
	isp_context_ptr->cmc_tab[6][8]=raw_tune_ptr->cmc.matrix[6][8];

	isp_context_ptr->cmc_tab[7][0]=raw_tune_ptr->cmc.matrix[7][0];
	isp_context_ptr->cmc_tab[7][1]=raw_tune_ptr->cmc.matrix[7][1];
	isp_context_ptr->cmc_tab[7][2]=raw_tune_ptr->cmc.matrix[7][2];
	isp_context_ptr->cmc_tab[7][3]=raw_tune_ptr->cmc.matrix[7][3];
	isp_context_ptr->cmc_tab[7][4]=raw_tune_ptr->cmc.matrix[7][4];
	isp_context_ptr->cmc_tab[7][5]=raw_tune_ptr->cmc.matrix[7][5];
	isp_context_ptr->cmc_tab[7][6]=raw_tune_ptr->cmc.matrix[7][6];
	isp_context_ptr->cmc_tab[7][7]=raw_tune_ptr->cmc.matrix[7][7];
	isp_context_ptr->cmc_tab[7][8]=raw_tune_ptr->cmc.matrix[7][8];

	isp_context_ptr->cmc_tab[8][0]=raw_tune_ptr->cmc.matrix[8][0];
	isp_context_ptr->cmc_tab[8][1]=raw_tune_ptr->cmc.matrix[8][1];
	isp_context_ptr->cmc_tab[8][2]=raw_tune_ptr->cmc.matrix[8][2];
	isp_context_ptr->cmc_tab[8][3]=raw_tune_ptr->cmc.matrix[8][3];
	isp_context_ptr->cmc_tab[8][4]=raw_tune_ptr->cmc.matrix[8][4];
	isp_context_ptr->cmc_tab[8][5]=raw_tune_ptr->cmc.matrix[8][5];
	isp_context_ptr->cmc_tab[8][6]=raw_tune_ptr->cmc.matrix[8][6];
	isp_context_ptr->cmc_tab[8][7]=raw_tune_ptr->cmc.matrix[8][7];
	isp_context_ptr->cmc_tab[8][8]=raw_tune_ptr->cmc.matrix[8][8];

	/*yiq*/
	isp_context_ptr->ygamma.bypass = ISP_ONE;

	/*gamma*/
	/*SCI_MEMCPY((void*)&isp_context_ptr->gamma.axis, (void*)&raw_tune_ptr->gamma.axis, 104);*/
	for(i=0; i<26; i++) {
		isp_context_ptr->gamma_tab[0].axis[0][i]=raw_tune_ptr->gamma.axis[0][i];
		isp_context_ptr->gamma_tab[0].axis[1][i]=raw_tune_ptr->gamma.axis[1][i];
		isp_context_ptr->gamma_tab[1].axis[0][i]=raw_tune_ptr->gamma.tab[0].axis[0][i];
		isp_context_ptr->gamma_tab[1].axis[1][i]=raw_tune_ptr->gamma.tab[0].axis[1][i];
		isp_context_ptr->gamma_tab[2].axis[0][i]=raw_tune_ptr->gamma.tab[1].axis[0][i];
		isp_context_ptr->gamma_tab[2].axis[1][i]=raw_tune_ptr->gamma.tab[1].axis[1][i];
		isp_context_ptr->gamma_tab[3].axis[0][i]=raw_tune_ptr->gamma.tab[2].axis[0][i];
		isp_context_ptr->gamma_tab[3].axis[1][i]=raw_tune_ptr->gamma.tab[2].axis[1][i];
		isp_context_ptr->gamma_tab[4].axis[0][i]=raw_tune_ptr->gamma.tab[3].axis[0][i];
		isp_context_ptr->gamma_tab[4].axis[1][i]=raw_tune_ptr->gamma.tab[3].axis[1][i];
		isp_context_ptr->gamma_tab[5].axis[0][i]=raw_tune_ptr->gamma.tab[4].axis[0][i];
		isp_context_ptr->gamma_tab[5].axis[1][i]=raw_tune_ptr->gamma.tab[4].axis[1][i];
	}

	isp_set_gamma(&isp_context_ptr->gamma, &isp_context_ptr->gamma_tab[isp_context_ptr->gamma_index]);

	/*cce matrix*/
	if(ISP_ZERO!=raw_tune_ptr->special_effect[0].matrix[0])
	{
		for(i=0; i<16; i++) {
			isp_context_ptr->cce_tab[i].matrix[0]=raw_tune_ptr->special_effect[i].matrix[0];
			isp_context_ptr->cce_tab[i].matrix[1]=raw_tune_ptr->special_effect[i].matrix[1];
			isp_context_ptr->cce_tab[i].matrix[2]=raw_tune_ptr->special_effect[i].matrix[2];
			isp_context_ptr->cce_tab[i].matrix[3]=raw_tune_ptr->special_effect[i].matrix[3];
			isp_context_ptr->cce_tab[i].matrix[4]=raw_tune_ptr->special_effect[i].matrix[4];
			isp_context_ptr->cce_tab[i].matrix[5]=raw_tune_ptr->special_effect[i].matrix[5];
			isp_context_ptr->cce_tab[i].matrix[6]=raw_tune_ptr->special_effect[i].matrix[6];
			isp_context_ptr->cce_tab[i].matrix[7]=raw_tune_ptr->special_effect[i].matrix[7];
			isp_context_ptr->cce_tab[i].matrix[8]=raw_tune_ptr->special_effect[i].matrix[8];
			isp_context_ptr->cce_tab[i].y_shift=raw_tune_ptr->special_effect[i].y_shift;
			isp_context_ptr->cce_tab[i].u_shift=raw_tune_ptr->special_effect[i].u_shift;
			isp_context_ptr->cce_tab[i].v_shift=raw_tune_ptr->special_effect[i].v_shift;
		}
	} else {
		for(i=0; i<8; i++) {
			isp_context_ptr->cce_tab[i].matrix[0]=cce_matrix[i][0];
			isp_context_ptr->cce_tab[i].matrix[1]=cce_matrix[i][1];
			isp_context_ptr->cce_tab[i].matrix[2]=cce_matrix[i][2];
			isp_context_ptr->cce_tab[i].matrix[3]=cce_matrix[i][3];
			isp_context_ptr->cce_tab[i].matrix[4]=cce_matrix[i][4];
			isp_context_ptr->cce_tab[i].matrix[5]=cce_matrix[i][5];
			isp_context_ptr->cce_tab[i].matrix[6]=cce_matrix[i][6];
			isp_context_ptr->cce_tab[i].matrix[7]=cce_matrix[i][7];
			isp_context_ptr->cce_tab[i].matrix[8]=cce_matrix[i][8];
			isp_context_ptr->cce_tab[i].y_shift=cce_matrix[i][9];
			isp_context_ptr->cce_tab[i].u_shift=cce_matrix[i][10];
			isp_context_ptr->cce_tab[i].v_shift=cce_matrix[i][11];
		}
	}
	_ispSetCceMatrix(&isp_context_ptr->cce_matrix, &isp_context_ptr->cce_tab[0]);

	/*uv div*/
	/*SCI_MEMCPY((void*)&isp_context_ptr->uv_div.thrd, (void*)&raw_info_ptr->uv_div.thrd, 7);*/
	isp_context_ptr->uv_div.thrd[0]=raw_tune_ptr->uv_div.thrd[0];
	isp_context_ptr->uv_div.thrd[1]=raw_tune_ptr->uv_div.thrd[1];
	isp_context_ptr->uv_div.thrd[2]=raw_tune_ptr->uv_div.thrd[2];
	isp_context_ptr->uv_div.thrd[3]=raw_tune_ptr->uv_div.thrd[3];
	isp_context_ptr->uv_div.thrd[4]=raw_tune_ptr->uv_div.thrd[4];
	isp_context_ptr->uv_div.thrd[5]=raw_tune_ptr->uv_div.thrd[5];
	isp_context_ptr->uv_div.thrd[6]=raw_tune_ptr->uv_div.thrd[6];
	isp_context_ptr->uv_div.t[0]=raw_tune_ptr->uv_div.t[0];
	isp_context_ptr->uv_div.t[1]=raw_tune_ptr->uv_div.t[1];
	isp_context_ptr->uv_div.m[0]=raw_tune_ptr->uv_div.m[0];
	isp_context_ptr->uv_div.m[1]=raw_tune_ptr->uv_div.m[1];
	isp_context_ptr->uv_div.m[2]=raw_tune_ptr->uv_div.m[2];


	/*pref*/
	isp_context_ptr->pref.write_back=raw_tune_ptr->pref.write_back;
	isp_context_ptr->pref.y_thr=raw_tune_ptr->pref.y_thr;
	isp_context_ptr->pref.u_thr=raw_tune_ptr->pref.u_thr;
	isp_context_ptr->pref.v_thr=raw_tune_ptr->pref.v_thr;

	/*bright*/
	isp_context_ptr->bright.factor=raw_tune_ptr->bright.factor[3];
	isp_context_ptr->bright_tab[0]=raw_tune_ptr->bright.factor[0];
	isp_context_ptr->bright_tab[1]=raw_tune_ptr->bright.factor[1];
	isp_context_ptr->bright_tab[2]=raw_tune_ptr->bright.factor[2];
	isp_context_ptr->bright_tab[3]=raw_tune_ptr->bright.factor[3];
	isp_context_ptr->bright_tab[4]=raw_tune_ptr->bright.factor[4];
	isp_context_ptr->bright_tab[5]=raw_tune_ptr->bright.factor[5];
	isp_context_ptr->bright_tab[6]=raw_tune_ptr->bright.factor[6];
	isp_context_ptr->bright_tab[7]=raw_tune_ptr->bright.factor[7];
	isp_context_ptr->bright_tab[8]=raw_tune_ptr->bright.factor[8];
	isp_context_ptr->bright_tab[9]=raw_tune_ptr->bright.factor[9];
	isp_context_ptr->bright_tab[10]=raw_tune_ptr->bright.factor[10];
	isp_context_ptr->bright_tab[11]=raw_tune_ptr->bright.factor[11];
	isp_context_ptr->bright_tab[12]=raw_tune_ptr->bright.factor[12];
	isp_context_ptr->bright_tab[13]=raw_tune_ptr->bright.factor[13];
	isp_context_ptr->bright_tab[14]=raw_tune_ptr->bright.factor[14];
	isp_context_ptr->bright_tab[15]=raw_tune_ptr->bright.factor[15];
	/*contrast*/
	isp_context_ptr->contrast.factor=raw_tune_ptr->contrast.factor[3];
	isp_context_ptr->contrast_tab[0]=raw_tune_ptr->contrast.factor[0];
	isp_context_ptr->contrast_tab[1]=raw_tune_ptr->contrast.factor[1];
	isp_context_ptr->contrast_tab[2]=raw_tune_ptr->contrast.factor[2];
	isp_context_ptr->contrast_tab[3]=raw_tune_ptr->contrast.factor[3];
	isp_context_ptr->contrast_tab[4]=raw_tune_ptr->contrast.factor[4];
	isp_context_ptr->contrast_tab[5]=raw_tune_ptr->contrast.factor[5];
	isp_context_ptr->contrast_tab[6]=raw_tune_ptr->contrast.factor[6];
	isp_context_ptr->contrast_tab[7]=raw_tune_ptr->contrast.factor[7];
	isp_context_ptr->contrast_tab[8]=raw_tune_ptr->contrast.factor[8];
	isp_context_ptr->contrast_tab[9]=raw_tune_ptr->contrast.factor[9];
	isp_context_ptr->contrast_tab[10]=raw_tune_ptr->contrast.factor[10];
	isp_context_ptr->contrast_tab[11]=raw_tune_ptr->contrast.factor[11];
	isp_context_ptr->contrast_tab[12]=raw_tune_ptr->contrast.factor[12];
	isp_context_ptr->contrast_tab[13]=raw_tune_ptr->contrast.factor[13];
	isp_context_ptr->contrast_tab[14]=raw_tune_ptr->contrast.factor[14];
	isp_context_ptr->contrast_tab[15]=raw_tune_ptr->contrast.factor[15];
	/*hist*/
	isp_context_ptr->hist.mode=raw_tune_ptr->hist.mode;
	isp_context_ptr->hist.low_ratio=(raw_tune_ptr->hist.low_ratio*isp_context_ptr->src.w*isp_context_ptr->src.h)>>0x10;
	isp_context_ptr->hist.high_ratio=(raw_tune_ptr->hist.high_ratio*isp_context_ptr->src.w*isp_context_ptr->src.h)>>0x10;

	isp_context_ptr->hist.in_min=0x01;
	isp_context_ptr->hist.in_max=0xff;
	isp_context_ptr->hist.out_min=0x01;
	isp_context_ptr->hist.out_max=0xff;

	/*auto contrast*/
	isp_context_ptr->auto_contrast.mode=raw_tune_ptr->auto_contrast.mode;
	isp_context_ptr->auto_contrast.in_min=0x01;
	isp_context_ptr->auto_contrast.in_max=0xff;
	isp_context_ptr->auto_contrast.out_min=0x01;
	isp_context_ptr->auto_contrast.out_max=0xff;

	/*saturation*/
	isp_context_ptr->saturation.offset = ISP_ZERO;
	isp_context_ptr->saturation.factor=raw_tune_ptr->saturation.factor[3];
	isp_context_ptr->saturation_tab[0]=raw_tune_ptr->saturation.factor[0];
	isp_context_ptr->saturation_tab[1]=raw_tune_ptr->saturation.factor[1];
	isp_context_ptr->saturation_tab[2]=raw_tune_ptr->saturation.factor[2];
	isp_context_ptr->saturation_tab[3]=raw_tune_ptr->saturation.factor[3];
	isp_context_ptr->saturation_tab[4]=raw_tune_ptr->saturation.factor[4];
	isp_context_ptr->saturation_tab[5]=raw_tune_ptr->saturation.factor[5];
	isp_context_ptr->saturation_tab[6]=raw_tune_ptr->saturation.factor[6];
	isp_context_ptr->saturation_tab[7]=raw_tune_ptr->saturation.factor[7];
	isp_context_ptr->saturation_tab[8]=raw_tune_ptr->saturation.factor[8];
	isp_context_ptr->saturation_tab[9]=raw_tune_ptr->saturation.factor[9];
	isp_context_ptr->saturation_tab[10]=raw_tune_ptr->saturation.factor[10];
	isp_context_ptr->saturation_tab[11]=raw_tune_ptr->saturation.factor[11];
	isp_context_ptr->saturation_tab[12]=raw_tune_ptr->saturation.factor[12];
	isp_context_ptr->saturation_tab[13]=raw_tune_ptr->saturation.factor[13];
	isp_context_ptr->saturation_tab[14]=raw_tune_ptr->saturation.factor[14];
	isp_context_ptr->saturation_tab[15]=raw_tune_ptr->saturation.factor[15];

	/*hue*/
	isp_context_ptr->hue.offset = ISP_ZERO;
	isp_context_ptr->hue.factor = ISP_ZERO;

	/*af*/
	isp_context_ptr->af.continue_status=ISP_END_FLAG;
	isp_context_ptr->af.status = ISP_AF_STOP;
	isp_context_ptr->af.have_success_record = ISP_UEB;
	isp_context_ptr->af.continue_focus_stat =_isp_ContinueFocusInforCallback;
	isp_context_ptr->af.max_step=raw_tune_ptr->af.max_step;
	isp_context_ptr->af.min_step=raw_tune_ptr->af.min_step;
	isp_context_ptr->af.stab_period=raw_tune_ptr->af.stab_period;
	isp_context_ptr->af.alg_id=raw_tune_ptr->af.alg_id;
	isp_context_ptr->af.max_tune_step=raw_tune_ptr->af.max_tune_step;
	isp_context_ptr->af.rough_count=raw_tune_ptr->af.rough_count;
	isp_context_ptr->af.fine_count=raw_tune_ptr->af.fine_count;
	for(i=0;i<32;i++){
		isp_context_ptr->af.af_rough_step[i]=raw_tune_ptr->af.af_rough_step[i];
		isp_context_ptr->af.af_fine_step[i]=raw_tune_ptr->af.af_fine_step[i];
	}
	isp_context_ptr->af.default_rough_step_len = raw_tune_ptr->af.default_step_len;
	isp_context_ptr->af.peak_thr_0 = raw_tune_ptr->af.peak_thr_0;
	isp_context_ptr->af.peak_thr_1 = raw_tune_ptr->af.peak_thr_1;
	isp_context_ptr->af.peak_thr_2 = raw_tune_ptr->af.peak_thr_2;
	isp_context_ptr->af.detect_thr = raw_tune_ptr->af.detect_thr;
	isp_context_ptr->af.detect_step_mum = raw_tune_ptr->af.detect_step_mum;
	isp_context_ptr->af.start_area_range = raw_tune_ptr->af.start_area_range;
	isp_context_ptr->af.end_area_range = raw_tune_ptr->af.end_area_range;
	isp_context_ptr->af.noise_thr = raw_tune_ptr->af.noise_thr;
	isp_context_ptr->af.AfmEb = ispAfmEb;
	isp_context_ptr->af.AwbmEb_immediately = ispAwbmEb_immediately;
	isp_context_ptr->af.CfgAwbm = ispCfgAwbm;

	/*emboss*/
	isp_context_ptr->emboss.step=raw_tune_ptr->emboss.step;

	/*edge*/
	isp_context_ptr->edge.detail_thr=raw_tune_ptr->edge.info[0].detail_thr;
	isp_context_ptr->edge.smooth_thr=raw_tune_ptr->edge.info[0].smooth_thr;
	isp_context_ptr->edge.strength=raw_tune_ptr->edge.info[0].strength;

	for(i=0x00; i<16; i++) {
		isp_context_ptr->edge_tab[i].detail_thr=raw_tune_ptr->edge.info[i].detail_thr;
		isp_context_ptr->edge_tab[i].smooth_thr=raw_tune_ptr->edge.info[i].smooth_thr;
		isp_context_ptr->edge_tab[i].strength=raw_tune_ptr->edge.info[i].strength;
	}

	/*css*/
	//SCI_MEMCPY((void*)&isp_context_ptr->css.low_thr, (void*)&raw_tune_ptr->css.low_thr, 7);
	//SCI_MEMCPY((void*)&isp_context_ptr->css.low_sum_thr, (void*)&raw_tune_ptr->css.low_sum_thr, 7);
	isp_context_ptr->css.low_thr[0]=raw_tune_ptr->css.low_thr[0];
	isp_context_ptr->css.low_thr[1]=raw_tune_ptr->css.low_thr[1];
	isp_context_ptr->css.low_thr[2]=raw_tune_ptr->css.low_thr[2];
	isp_context_ptr->css.low_thr[3]=raw_tune_ptr->css.low_thr[3];
	isp_context_ptr->css.low_thr[4]=raw_tune_ptr->css.low_thr[4];
	isp_context_ptr->css.low_thr[5]=raw_tune_ptr->css.low_thr[5];
	isp_context_ptr->css.low_thr[6]=raw_tune_ptr->css.low_thr[6];
	isp_context_ptr->css.lum_thr=raw_tune_ptr->css.lum_thr;
	isp_context_ptr->css.low_sum_thr[0]=raw_tune_ptr->css.low_sum_thr[0];
	isp_context_ptr->css.low_sum_thr[1]=raw_tune_ptr->css.low_sum_thr[1];
	isp_context_ptr->css.low_sum_thr[2]=raw_tune_ptr->css.low_sum_thr[2];
	isp_context_ptr->css.low_sum_thr[3]=raw_tune_ptr->css.low_sum_thr[3];
	isp_context_ptr->css.low_sum_thr[4]=raw_tune_ptr->css.low_sum_thr[4];
	isp_context_ptr->css.low_sum_thr[5]=raw_tune_ptr->css.low_sum_thr[5];
	isp_context_ptr->css.low_sum_thr[6]=raw_tune_ptr->css.low_sum_thr[6];
	isp_context_ptr->css.chr_thr=raw_tune_ptr->css.chr_thr;

	/*hdr*/
	isp_context_ptr->hdr_index.r_index=0x4d;
	isp_context_ptr->hdr_index.g_index=0x96;
	isp_context_ptr->hdr_index.b_index=0x1d;
	isp_context_ptr->hdr_index.com_ptr=(uint8_t*)com_ptr;
	isp_context_ptr->hdr_index.p2e_ptr=(uint8_t*)p2e_ptr;
	isp_context_ptr->hdr_index.e2p_ptr=(uint8_t*)e2p_ptr;

	/*global gain*/
	isp_context_ptr->global.gain=raw_tune_ptr->global.gain;

	/*chn gain*/
	isp_context_ptr->chn.r_gain=raw_tune_ptr->chn.r_gain;
	isp_context_ptr->chn.g_gain=raw_tune_ptr->chn.g_gain;
	isp_context_ptr->chn.b_gain=raw_tune_ptr->chn.b_gain;
	isp_context_ptr->chn.r_offset=raw_tune_ptr->chn.r_offset;
	isp_context_ptr->chn.g_offset=raw_tune_ptr->chn.g_offset;
	isp_context_ptr->chn.b_offset=raw_tune_ptr->chn.b_offset;

	return rtn;
}

/* _ispUpdateParam --
*@
*@
*@ return:
*/
static int32_t _ispUpdateParam(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);

	rtn=_ispSetParam(0, (struct isp_cfg_param*)&isp_context_ptr->cfg);

	return rtn;
}

/* _ispAeCorrect --
*@
*@
*@ return:
*/
static int32_t _ispAeCorrect(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;

	isp_ae_set_exposure_gain(handler_id);

	return rtn;
}

/* _ispAwbCorrect --
*@
*@
*@ return:
*/
static int32_t _ispAwbCorrect(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_awb_param* awb_param_ptr = &isp_context_ptr->awb;

	if (ISP_EB == awb_param_ptr->set_eb)
	{
		isp_context_ptr->awbc.r_gain=isp_context_ptr->awb.cur_gain.r;
		isp_context_ptr->awbc.g_gain=isp_context_ptr->awb.cur_gain.g;
		isp_context_ptr->awbc.b_gain=isp_context_ptr->awb.cur_gain.b;
		_ispCfgAwbc(handler_id, &isp_context_ptr->awbc);
		awb_param_ptr->monitor_bypass=ISP_UEB;
	}

	awb_param_ptr->set_eb=ISP_UEB;

	return rtn;
}

/* _ispUncfg --
*@
*@
*@ return:
*/
static int32_t _ispUncfg(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;

	rtn=ispOpenClk(handler_id, ISP_ZERO);
	ISP_RETURN_IF_FAIL(rtn, ("open clk error"));

	rtn=ispReset(handler_id, ISP_ZERO);
	ISP_RETURN_IF_FAIL(rtn, ("isp reset error"));

	return rtn;
}

/* _ispChangeVideoCfg --
*@
*@
*@ return:
*/
static int32_t _ispChangeVideoCfg(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_awb_param* awb_param_ptr = &isp_context_ptr->awb;
	struct isp_ae_param* ae_param_ptr = &isp_context_ptr->ae;
	struct isp_af_param* af_param_ptr = &isp_context_ptr->af;
	uint32_t awb_index = isp_context_ptr->awb.cur_index;

	if(ISP_VIDEO_MODE_SINGLE==_ispGetVideoMode(handler_id))
	{/*capture use video mode need bypass ae awb*/
		ae_param_ptr->bypass=ISP_EB;
		awb_param_ptr->bypass=ISP_EB;
	} else {
		ae_param_ptr->monitor.w=isp_context_ptr->awbm.win_size.w;
		ae_param_ptr->monitor.h=isp_context_ptr->awbm.win_size.h;
	}

	/*flash on need modify awb gan*/
	isp_awb_set_flash_gain();

	/* isp param index */
	isp_context_ptr->video_param_index=_ispGetIspParamIndex(handler_id, &isp_context_ptr->src);
	isp_context_ptr->param_index=isp_context_ptr->video_param_index;
	isp_ae_set_param_index(0, isp_context_ptr->video_param_index);
	ISP_LOG("video param index :0x%x", isp_context_ptr->param_index);

	/* isp ae exposure line time */
	isp_context_ptr->ae.line_time=_ispGetLineTime((struct isp_resolution_info*)&isp_context_ptr->input_size_trim, isp_context_ptr->param_index);

	/* change lnc param*/
	ispSetLncParam(handler_id, isp_context_ptr->lnc_map_tab[isp_context_ptr->param_index-ISP_ONE][awb_index].param_addr, isp_context_ptr->lnc_map_tab[isp_context_ptr->param_index-ISP_ONE][awb_index].len);

	return rtn;
}

/* _ispChangeProcBLC --
*@
*@
*@ return:
*/
static int32_t _ispChangeProcBLC(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_blc_param* blc_ptr = (struct isp_blc_param*)&isp_context_ptr->blc;
	uint32_t param_index = isp_context_ptr->proc_param_index-ISP_ONE;

	if(isp_context_ptr->video_param_index!=isp_context_ptr->proc_param_index)
	{
		blc_ptr->r=isp_context_ptr->blc_offset[param_index].r;
		blc_ptr->gr=isp_context_ptr->blc_offset[param_index].gr;
		blc_ptr->gb=isp_context_ptr->blc_offset[param_index].gb;
		blc_ptr->b=isp_context_ptr->blc_offset[param_index].b;
	}

	return rtn;
}

/* _ispChangeProcAwbGain --
*@
*@
*@ return:
*/
static int32_t _ispChangeProcAwbGain(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_awbc_param* awbc_ptr=(struct isp_awbc_param*)&isp_context_ptr->awbc;
	struct isp_awb_param* awb_ptr=(struct isp_awb_param*)&isp_context_ptr->awb;
	uint32_t param_index=isp_context_ptr->proc_param_index-ISP_ONE;

	/*flash on need modify awb gan*/
	isp_awb_set_flash_gain();

	if(isp_context_ptr->video_param_index!=isp_context_ptr->proc_param_index)
	{
		awbc_ptr->r_gain=(awbc_ptr->r_gain*awb_ptr->gain_convert[param_index].r)>>0x08;
		awbc_ptr->g_gain=(awbc_ptr->g_gain*awb_ptr->gain_convert[param_index].g)>>0x08;
		awbc_ptr->b_gain=(awbc_ptr->b_gain*awb_ptr->gain_convert[param_index].b)>>0x08;
	}

	return rtn;
}

/* _ispChangeProcCfg --
*@
*@
*@ return:
*/
static int32_t _ispChangeProcCfg(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	uint32_t awb_index=isp_context_ptr->awb.cur_index;

	/* isp param index */
	isp_context_ptr->proc_param_index=_ispGetIspParamIndex(handler_id, &isp_context_ptr->src);
	isp_context_ptr->param_index=isp_context_ptr->proc_param_index;
	ISP_LOG("proc param index :0x%x", isp_context_ptr->param_index);
	/* change lnc param*/
	ispSetLncParam(handler_id, isp_context_ptr->lnc_map_tab[isp_context_ptr->param_index-ISP_ONE][awb_index].param_addr, isp_context_ptr->lnc_map_tab[isp_context_ptr->param_index-ISP_ONE][awb_index].len);

	_ispChangeProcBLC(handler_id);
	_ispChangeProcAwbGain(handler_id);

	return rtn;
}

/* _ispCfgInt --
*@
*@
*@ return:
*/
static int32_t _ispCfgInt(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;

	if(ISP_VIDEO_MODE_CONTINUE==_ispGetVideoMode(handler_id))
	{
		rtn=ispRegIRQ(handler_id, ISP_MONITOR_EVT_AWB|ISP_MONITOR_EVT_AF|ISP_MONITOR_EVT_SOF);
		ISP_RETURN_IF_FAIL(rtn, ("reg irq error"));

		rtn=ispCfgDcamIRQ(handler_id, ISP_EB);
		ISP_RETURN_IF_FAIL(rtn, ("cfg dcam irq error"));
	}

	return rtn;
}

/* _ispCfg --
*@
*@
*@ return:
*/
static int32_t _ispCfg(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);

	// open isp
	rtn=ispOpenClk(handler_id, ISP_ONE);
	rtn=ispReset(handler_id, ISP_ONE);
	// config isp paramter
	rtn=_ispCfgBlc(handler_id, &isp_context_ptr->blc);
	rtn=_ispCfgNlc(handler_id, &isp_context_ptr->nlc);
	rtn=_ispCfgLnc(handler_id, &isp_context_ptr->lnc);
	rtn=_ispCfgAwbm(handler_id, &isp_context_ptr->awbm);
	rtn=_ispCfgAwbc(handler_id, &isp_context_ptr->awbc);
	rtn=_ispCfgBPC(handler_id, &isp_context_ptr->bpc);
	rtn=_ispCfgDenoise(handler_id, &isp_context_ptr->denoise);
	rtn=_ispCfgGrGb(handler_id, &isp_context_ptr->grgb);
	rtn=_ispCfgCfa(handler_id, &isp_context_ptr->cfa);
	rtn=_ispCfgCmc(handler_id, &isp_context_ptr->cmc);
	rtn=_ispCfgGamma(handler_id, &isp_context_ptr->gamma);
	rtn=_ispCfgAe(handler_id);
	rtn=_ispCfgYGamma(handler_id, &isp_context_ptr->ygamma);
	rtn=_ispCfgFlicker(handler_id, &isp_context_ptr->flicker);
	rtn=_ispCfgCCEMatrix(handler_id, &isp_context_ptr->cce_matrix);
	rtn=_ispCfgUVDiv(handler_id, &isp_context_ptr->uv_div);
	rtn=_ispCfgPref(handler_id, &isp_context_ptr->pref);
	rtn=_ispCfgBright(handler_id, &isp_context_ptr->bright);
	rtn=_ispCfgContrast(handler_id, &isp_context_ptr->contrast);
	rtn=_ispCfgHist(handler_id, &isp_context_ptr->hist);
	rtn=_ispCfgAutoContrast(handler_id, &isp_context_ptr->auto_contrast);
	rtn=_ispCfgSaturation(handler_id, &isp_context_ptr->saturation);
	rtn=_ispCfgAf(handler_id, &isp_context_ptr->af);
	rtn=_ispCfgEdge(handler_id, &isp_context_ptr->edge);
	rtn=_ispCfgEmboss(handler_id, &isp_context_ptr->emboss);
	rtn=_ispCfgFalseColor(handler_id, &isp_context_ptr->fcs);
	rtn=_ispCfgSatursationSup(handler_id, &isp_context_ptr->css);
	rtn=_ispCfgHdr(handler_id, &isp_context_ptr->hdr);
	rtn=_ispCfgHDRIndexTab(handler_id, &isp_context_ptr->hdr_index);
	rtn=_ispCfgGlobalGain(handler_id, &isp_context_ptr->global);
	rtn=_ispCfgChnGain(handler_id, &isp_context_ptr->chn);
	rtn=_ispCfgHue(handler_id, (struct isp_saturation_param*)&isp_context_ptr->hue);

	return rtn;
}

/* _ispSetSlice --
*@
*@
*@ return:
*/
int32_t _ispSetSlice(uint32_t handler_id, struct isp_slice_param* slice_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	uint8_t i=0x00;

	ispSetFetchSliceSize(handler_id, slice_ptr->size[ISP_FETCH].w, slice_ptr->size[ISP_FETCH].h);
	ispSetBNLCSliceSize(handler_id, slice_ptr->size[ISP_BNLC].w, slice_ptr->size[ISP_BNLC].h);
	ispSetBNLCSliceInfo(handler_id, slice_ptr->edge_info);
	ispSetLensSliceStart(handler_id, slice_ptr->size[ISP_LENS].x, slice_ptr->size[ISP_LENS].y);
	ispSetLensGridSize(handler_id, slice_ptr->size[ISP_LENS].w, slice_ptr->size[ISP_LENS].h);
	ispWDenoiseSliceSize(handler_id, slice_ptr->size[ISP_WAVE].w, slice_ptr->size[ISP_WAVE].h);
	ispWDenoiseSliceInfo(handler_id, slice_ptr->edge_info&(ISP_SLICE_LEFT|ISP_SLICE_UP));
	ispCFASliceSize(handler_id, slice_ptr->size[ISP_CFA].w, slice_ptr->size[ISP_CFA].h);
	ispCFASliceInfo(handler_id, slice_ptr->edge_info);
	ispPrefSliceSize(handler_id, slice_ptr->size[ISP_PREF].w, slice_ptr->size[ISP_PREF].h);
	ispPrefSliceInfo(handler_id, slice_ptr->edge_info&(ISP_SLICE_LEFT|ISP_SLICE_UP));
	ispBrightSliceSize(handler_id, slice_ptr->size[ISP_BRIGHT].w, slice_ptr->size[ISP_BRIGHT].h);
	ispBrightSliceInfo(handler_id, slice_ptr->edge_info);
	ispColorSaturationSuppressSliceSize(handler_id, slice_ptr->size[ISP_CSS].w, slice_ptr->size[ISP_CSS].h);
	ispSetStoreSliceSize(handler_id, slice_ptr->size[ISP_STORE].w, slice_ptr->size[ISP_STORE].h);
	ispSetFeederSliceSize(handler_id, slice_ptr->size[ISP_FEEDER].w, slice_ptr->size[ISP_FEEDER].h);
	ispGlbGainSliceSize(handler_id, slice_ptr->size[ISP_GLB_GAIN].w, slice_ptr->size[ISP_GLB_GAIN].h);

	return rtn;
}

/* _ispStart --
*@
*@
*@ return:
*/
static int32_t _ispStart(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_fetch_param fetch_param;
	struct isp_store_param store_param;

	rtn=_ispGetSliceEdgeInfo(&isp_context_ptr->slice);

	rtn=_ispGetSliceSize(isp_context_ptr->com.proc_type, &isp_context_ptr->src, &isp_context_ptr->slice);

	rtn=_ispAddSliceBorder(ISP_WAVE, isp_context_ptr->com.proc_type, &isp_context_ptr->slice);
	rtn=_ispAddSliceBorder(ISP_CFA, isp_context_ptr->com.proc_type, &isp_context_ptr->slice);
	rtn=_ispAddSliceBorder(ISP_PREF, isp_context_ptr->com.proc_type, &isp_context_ptr->slice);
	rtn=_ispAddSliceBorder(ISP_BRIGHT, isp_context_ptr->com.proc_type, &isp_context_ptr->slice);
	rtn=_ispAddSliceBorder(ISP_CSS, isp_context_ptr->com.proc_type, &isp_context_ptr->slice);
	rtn=_ispAddSliceBorder(ISP_GLB_GAIN, isp_context_ptr->com.proc_type, &isp_context_ptr->slice);
	rtn=_ispSetLncParam(&isp_context_ptr->lnc, isp_context_ptr);

	rtn=_ispSetSlice(handler_id, &isp_context_ptr->slice);
	rtn=_ispLncParamSet(handler_id, &isp_context_ptr->lnc);

	rtn=_ispGetFetchAddr(handler_id, &fetch_param);
	rtn=_ispGetStoreAddr(handler_id, &store_param);
	rtn=_ispCfgFeatchData(handler_id, &fetch_param);
	rtn=_ispCfgStoreData(handler_id, &store_param);
	rtn=_ispCfgFeeder(handler_id, &isp_context_ptr->feeder);
	rtn=_ispCfgComData(handler_id, &isp_context_ptr->com);

	rtn=ispShadow(handler_id, ISP_ONE);

	rtn=_ispLncParamLoad(handler_id, &isp_context_ptr->lnc);
	rtn=_ispLncParamValid(handler_id);

	if(ISP_CAP_MODE!=isp_context_ptr->com.in_mode) {
		rtn=isp_Start(handler_id, ISP_ONE);
	}

	return rtn;
}

/* _ispProcessEndHandle --
*@
*@
*@ return:
*/
static int32_t _ispProcessEndHandle(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_size* cur_slice_ptr=&isp_context_ptr->slice.cur_slice_num;
	struct isp_size* all_slice_ptr=&isp_context_ptr->slice.all_slice_num;
	struct ips_out_param callback_param={0x00};

	if(all_slice_ptr->w==(cur_slice_ptr->w+ISP_ONE)) {
		isp_system_ptr->isp_status=ISP_IDLE;
		isp_context_ptr->slice.complete_line+=isp_context_ptr->slice.max_size.h;
		ISP_LOG("complete line:%d , slice max line:%d\n", isp_context_ptr->slice.complete_line, isp_context_ptr->slice.max_size.h);
		if(PNULL!=isp_context_ptr->cfg.callback) {
			callback_param.output_height=isp_context_ptr->slice.max_size.h;
			//ISP_LOG("callback ISP_PROC_CALLBACK");
			ISP_LOG("output height:%d \n", callback_param.output_height);
			isp_context_ptr->cfg.callback(handler_id, ISP_CALLBACK_EVT|ISP_PROC_CALLBACK, (void*)&callback_param, sizeof(struct ips_out_param));
		}

		return rtn;
	}
	else
	{
		cur_slice_ptr->w++;
		_ispStart(handler_id);
	}

	return rtn;
}

/* _ispSetTuneParam --
*@
*@
*@ return:
*/
int32_t _ispSetTuneParam(uint32_t handler_id)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetAlgContext(handler_id);

	if(0x00)//!=isp_context_ptr->ae.flash.eb)
	{
		ISP_LOG("sensor sof");
	}

	_ispWbTrimParamValid(handler_id);

	_ispSofConterHandler(handler_id);

	usleep(1000);

	if(ISP_EB==isp_context_ptr->tune.af_stat_continue) {
		if ((ISP_EB==isp_context_ptr->af_get_stat)
			||(ISP_END_FLAG!=isp_context_ptr->af.continue_status))
		{
			_ispCfgAfConStat(handler_id, &isp_context_ptr->af);
		}else{
			ispAFMbypass(handler_id, ISP_EB);
		}
		isp_context_ptr->tune.af_stat_continue=ISP_UEB;
	}

	if(ISP_EB==isp_context_ptr->tune.af) {
		_ispCfgAf(handler_id, &isp_context_ptr->af);
		isp_context_ptr->af.status=ISP_AF_START;
		isp_context_ptr->af.suc_win=ISP_ZERO;
		isp_context_ptr->tune.af=ISP_UEB;
	} else {
		//_ispAfmEb();
	}
	if(ISP_EB==isp_context_ptr->tune.ae) {
		_ispAeInfoSet(handler_id);
		_ispAeCorrect(handler_id);
		isp_context_ptr->tune.ae=ISP_UEB;
	} else {
		_ispAeCorrect(handler_id);
	}
	if(ISP_EB==isp_context_ptr->tune.awb) {
		_ispCfgAwbc(handler_id, &isp_context_ptr->awbc);
		isp_context_ptr->awb.cur_gain.r = isp_context_ptr->awbc.r_gain;
		isp_context_ptr->awb.cur_gain.g = isp_context_ptr->awbc.g_gain;
		isp_context_ptr->awb.cur_gain.b = isp_context_ptr->awbc.b_gain;
		isp_context_ptr->tune.awb=ISP_UEB;
	} else {
		_ispAwbCorrect(handler_id);
	}
	//_ispAemEb(handler_id);
	//_ispAwbmEb(handler_id);
	_ispAeAwbmEb(handler_id);

	if(ISP_EB==isp_context_ptr->tune.wb_trim) {
		_ispCfgAwbWin(handler_id, &isp_context_ptr->awbm);
		isp_context_ptr->wb_trim_valid=ISP_EB;
		isp_context_ptr->tune.wb_trim=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.special_effect) {
		_ispCfgCCEMatrix(handler_id, &isp_context_ptr->cce_matrix);
		_ispCfgEmboss(handler_id, &isp_context_ptr->emboss);
		isp_context_ptr->tune.special_effect=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.bright) {
		_ispCfgBright(handler_id, &isp_context_ptr->bright);
		isp_context_ptr->tune.bright=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.contrast) {
		_ispCfgContrast(handler_id, &isp_context_ptr->contrast);
		isp_context_ptr->tune.contrast=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.hist) {
		isp_context_ptr->tune.hist=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.auto_contrast) {
		isp_context_ptr->tune.auto_contrast=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.saturation) {
		_ispCfgSaturation(handler_id, &isp_context_ptr->saturation);
		isp_context_ptr->tune.saturation=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.hue) {
		_ispCfgHue(handler_id, (struct isp_saturation_param*)&isp_context_ptr->hue);
		isp_context_ptr->tune.hue=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.css) {
		isp_context_ptr->tune.css=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.hdr) {
		_ispCfgHdr(handler_id, &isp_context_ptr->hdr);
		isp_context_ptr->tune.hdr=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.global_gain) {
		isp_context_ptr->tune.global_gain=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.chn_gain) {
		isp_context_ptr->tune.chn_gain=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.denoise) {
		_ispCfgPref(handler_id, &isp_context_ptr->pref);
		_ispCfgDenoise(handler_id, &isp_context_ptr->denoise);
		isp_context_ptr->tune.denoise=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.edge) {
		_ispCfgEdge(handler_id, &isp_context_ptr->edge);
		isp_context_ptr->tune.edge=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.cmc) {
		_ispCfgCmc(handler_id, &isp_context_ptr->cmc);
		isp_context_ptr->tune.cmc=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.lnc) {
		_ispCfgLnc(handler_id, &isp_context_ptr->lnc);
		isp_context_ptr->tune.lnc=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.lnc_load) {
		_ispLncParamValid(handler_id);
		isp_context_ptr->tune.lnc_load=ISP_UEB;
	}
	if(ISP_EB==isp_context_ptr->tune.gamma) {
		_ispCfgGamma(handler_id, &isp_context_ptr->gamma);
		isp_context_ptr->tune.gamma=ISP_UEB;
	}

	rtn=ispShadow(handler_id, ISP_ONE);

	//_isp_SofHandler(handler_id);

	if ( PNULL != isp_context_ptr->cfg.self_callback) {
		isp_context_ptr->cfg.self_callback(handler_id, ISP_SOF_CALLBACK, NULL, ISP_ZERO);
	}

	return rtn;
}

/* _ispGetExifInfo --
*@
*@
*@ return:
*/
static int32_t _ispGetExifInfo(uint32_t handler_id, void *exif_info_ptr)
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct exif_isp_info* exif_isp_info_ptr=(struct exif_isp_info*)exif_info_ptr;
	uint32_t i = 0;
	uint32_t j = 0;

	if((PNULL == exif_info_ptr) || (PNULL == isp_context_ptr))
	{
		ISP_LOG("ISP_RAW:exif_isp_info: 0x%x, context_ptr: 0x%x\n", (uint32_t)exif_info_ptr, (uint32_t)isp_context_ptr);
		rtn = ISP_PARAM_NULL;
		return rtn;
	}

	exif_isp_info_ptr->tool_version = 0x0001;
	exif_isp_info_ptr->is_exif_validate = 1;
	exif_isp_info_ptr->version_id = ISP_SOFT_ID;
	exif_isp_info_ptr->info_len = sizeof(struct exif_isp_info);
	exif_isp_info_ptr->blc_bypass = isp_context_ptr->blc.bypass;
	exif_isp_info_ptr->nlc_bypass = isp_context_ptr->nlc.bypass;
	exif_isp_info_ptr->lnc_bypass = isp_context_ptr->lnc.bypass;
	exif_isp_info_ptr->ae_bypass = isp_context_ptr->ae.back_bypass;
	exif_isp_info_ptr->awb_bypass = isp_context_ptr->awb.back_bypass;
	exif_isp_info_ptr->bpc_bypass = isp_context_ptr->bpc.bypass;
	exif_isp_info_ptr->denoise_bypass = isp_context_ptr->denoise.bypass;
	exif_isp_info_ptr->grgb_bypass = isp_context_ptr->grgb.bypass;
	exif_isp_info_ptr->cmc_bypass = isp_context_ptr->cmc.bypass;
	exif_isp_info_ptr->gamma_bypass = isp_context_ptr->gamma.bypass;
	exif_isp_info_ptr->uvdiv_bypass = isp_context_ptr->uv_div.bypass;
	exif_isp_info_ptr->pref_bypass = isp_context_ptr->pref.bypass;
	exif_isp_info_ptr->bright_bypass = isp_context_ptr->bright.bypass;
	exif_isp_info_ptr->contrast_bypass = isp_context_ptr->contrast.bypass;
	exif_isp_info_ptr->hist_bypass = isp_context_ptr->hist.bypass;
	exif_isp_info_ptr->auto_contrast_bypass = isp_context_ptr->auto_contrast.bypass;
	exif_isp_info_ptr->af_bypass = isp_context_ptr->af.bypass;
	exif_isp_info_ptr->edge_bypass = isp_context_ptr->edge.bypass;
	exif_isp_info_ptr->fcs_bypass = isp_context_ptr->fcs.bypass;
	exif_isp_info_ptr->css_bypass = isp_context_ptr->css.bypass;
	exif_isp_info_ptr->saturation_bypass = isp_context_ptr->saturation.bypass;
	exif_isp_info_ptr->hdr_bypass = isp_context_ptr->hdr.bypass;
	exif_isp_info_ptr->glb_gain_bypass = isp_context_ptr->global.bypass;
	exif_isp_info_ptr->chn_gain_bypass = isp_context_ptr->chn.bypass;

	exif_isp_info_ptr->blc.mode = isp_context_ptr->blc.mode;
	exif_isp_info_ptr->blc.r = isp_context_ptr->blc.r;
	exif_isp_info_ptr->blc.gr = isp_context_ptr->blc.gr;
	exif_isp_info_ptr->blc.gb = isp_context_ptr->blc.gb;
	exif_isp_info_ptr->blc.b = isp_context_ptr->blc.b;

	for(i=0; i<29; i++)
	{
		exif_isp_info_ptr->nlc.r_node[i] = isp_context_ptr->nlc.r_node[i];
		exif_isp_info_ptr->nlc.g_node[i] = isp_context_ptr->nlc.g_node[i];
		exif_isp_info_ptr->nlc.b_node[i] = isp_context_ptr->nlc.b_node[i];
		exif_isp_info_ptr->nlc.l_node[i] = isp_context_ptr->nlc.l_node[i];
	}

	exif_isp_info_ptr->lnc.grid = isp_context_ptr->lnc.map.grid_mode;
	exif_isp_info_ptr->lnc.r_pec = 0;
	exif_isp_info_ptr->lnc.g_pec = 0;
	exif_isp_info_ptr->lnc.b_pec = 0;

	exif_isp_info_ptr->ae.iso = isp_context_ptr->ae.cur_iso;
	exif_isp_info_ptr->ae.exposure = isp_context_ptr->ae.cur_exposure;
	exif_isp_info_ptr->ae.gain = isp_context_ptr->ae.cur_gain;

	exif_isp_info_ptr->awb.alg_id = isp_context_ptr->awb.alg_id;
	exif_isp_info_ptr->awb.r_gain = isp_context_ptr->awb.cur_rgb.r;
	exif_isp_info_ptr->awb.g_gain = isp_context_ptr->awb.cur_rgb.g;
	exif_isp_info_ptr->awb.b_gain = isp_context_ptr->awb.cur_rgb.b;

	exif_isp_info_ptr->bpc.flat_thr = isp_context_ptr->bpc.flat_thr;
	exif_isp_info_ptr->bpc.std_thr = isp_context_ptr->bpc.std_thr;
	exif_isp_info_ptr->bpc.texture_thr = isp_context_ptr->bpc.texture_thr;
	exif_isp_info_ptr->bpc.reserved = isp_context_ptr->bpc.reserved;

	exif_isp_info_ptr->denoise.write_back = isp_context_ptr->denoise.write_back;
	exif_isp_info_ptr->denoise.r_thr = isp_context_ptr->denoise.r_thr;
	exif_isp_info_ptr->denoise.g_thr = isp_context_ptr->denoise.g_thr;
	exif_isp_info_ptr->denoise.b_thr = isp_context_ptr->denoise.b_thr;
	for(i=0; i<19; i++)
	{
		exif_isp_info_ptr->denoise.diswei[i] = isp_context_ptr->denoise.diswei[i] ;
	}
	for(i=0; i<31; i++)
	{
		exif_isp_info_ptr->denoise.ranwei[i] = isp_context_ptr->denoise.ranwei[i];
	}
	exif_isp_info_ptr->denoise.reserved1 = isp_context_ptr->denoise.reserved1;
	exif_isp_info_ptr->denoise.reserved0 = isp_context_ptr->denoise.reserved0;

	exif_isp_info_ptr->grgb.edge_thr = isp_context_ptr->grgb.edge_thr;
	exif_isp_info_ptr->grgb.diff_thr = isp_context_ptr->grgb.diff_thr;

	exif_isp_info_ptr->cfa.edge_thr = isp_context_ptr->cfa.edge_thr;
	exif_isp_info_ptr->cfa.diff_thr = isp_context_ptr->cfa.diff_thr;

	for(i=0; i<9; i++)
	{
		exif_isp_info_ptr->cmc.matrix[i] = isp_context_ptr->cmc.matrix[i];
	}
	exif_isp_info_ptr->cmc.reserved = isp_context_ptr->cmc.reserved;

	for(i=0; i<2; i++)
	{
		for(j=0; j<26; j++)
		{
			exif_isp_info_ptr->gamma.axis[i][j] = isp_context_ptr->gamma.axis[i][j];
		}
	}

	for(i=0; i<9; i++)
	{
		exif_isp_info_ptr->cce.matrix[i] = isp_context_ptr->cce_matrix.matrix[i];
	}
	exif_isp_info_ptr->cce.y_shift = isp_context_ptr->cce_matrix.y_shift;
	exif_isp_info_ptr->cce.u_shift = isp_context_ptr->cce_matrix.u_shift;
	exif_isp_info_ptr->cce.v_shift = isp_context_ptr->cce_matrix.v_shift;

	for(i=0; i<7; i++)
	{
		exif_isp_info_ptr->uv_div.thrd[i] = isp_context_ptr->uv_div.thrd[i];
	}

	exif_isp_info_ptr->uv_div.t[0] = isp_context_ptr->uv_div.t[0];
	exif_isp_info_ptr->uv_div.t[1] = isp_context_ptr->uv_div.t[1];
	exif_isp_info_ptr->uv_div.m[0] = isp_context_ptr->uv_div.m[0];
	exif_isp_info_ptr->uv_div.m[1] = isp_context_ptr->uv_div.m[1];
	exif_isp_info_ptr->uv_div.m[2] = isp_context_ptr->uv_div.m[2];

	exif_isp_info_ptr->pref.write_back = isp_context_ptr->pref.write_back;
	exif_isp_info_ptr->pref.y_thr = isp_context_ptr->pref.y_thr;
	exif_isp_info_ptr->pref.u_thr = isp_context_ptr->pref.u_thr;
	exif_isp_info_ptr->pref.v_thr = isp_context_ptr->pref.v_thr;

	exif_isp_info_ptr->bright.factor = isp_context_ptr->bright.factor;

	exif_isp_info_ptr->contrast.factor = isp_context_ptr->contrast.factor;

	exif_isp_info_ptr->hist.low_ratio = isp_context_ptr->hist.low_ratio;
	exif_isp_info_ptr->hist.high_ratio = isp_context_ptr->hist.high_ratio;
	exif_isp_info_ptr->hist.mode = isp_context_ptr->hist.mode;

	exif_isp_info_ptr->hist.reserved2 = 0;
	exif_isp_info_ptr->hist.reserved1 = 0;
	exif_isp_info_ptr->hist.reserved0 = 0;

	exif_isp_info_ptr->auto_contrast.mode = isp_context_ptr->auto_contrast.mode;
	exif_isp_info_ptr->auto_contrast.reserved2 = 0;
	exif_isp_info_ptr->auto_contrast.reserved1 = 0;
	exif_isp_info_ptr->auto_contrast.reserved0 = 0;

	exif_isp_info_ptr->saturation.factor = isp_context_ptr->saturation.factor;

	for(i=0; i<7; i++)
	{
		exif_isp_info_ptr->css.low_thr[i] = isp_context_ptr->css.low_thr[i];
	}
	exif_isp_info_ptr->css.lum_thr = isp_context_ptr->css.lum_thr;
	for(i=0; i<7; i++)
	{
		exif_isp_info_ptr->css.low_sum_thr[i] = isp_context_ptr->css.low_sum_thr[i];
	}
	exif_isp_info_ptr->css.chr_thr = isp_context_ptr->css.chr_thr;

	exif_isp_info_ptr->af.alg_id = 0;
	exif_isp_info_ptr->af.cur_step = 0;
	for(i=0; i<32; i++)
	{
		exif_isp_info_ptr->af.edge_info[i] = 0;
	}

	exif_isp_info_ptr->edge.detail_thr = isp_context_ptr->edge.detail_thr;
	exif_isp_info_ptr->edge.smooth_thr = isp_context_ptr->edge.smooth_thr;
	exif_isp_info_ptr->edge.strength = isp_context_ptr->edge.strength;
	exif_isp_info_ptr->edge.reserved = isp_context_ptr->edge.reserved;

	exif_isp_info_ptr->emboss.step = isp_context_ptr->emboss.step;
	exif_isp_info_ptr->emboss.reserved2 = 0;
	exif_isp_info_ptr->emboss.reserved1 = 0;
	exif_isp_info_ptr->emboss.reserved0 = 0;

	exif_isp_info_ptr->global.gain= isp_context_ptr->global.gain;

	exif_isp_info_ptr->chn.r_gain= isp_context_ptr->chn.r_gain;
	exif_isp_info_ptr->chn.g_gain= isp_context_ptr->chn.g_gain;
	exif_isp_info_ptr->chn.b_gain= isp_context_ptr->chn.b_gain;
	exif_isp_info_ptr->chn.reserved0= isp_context_ptr->chn.reserved0;
	exif_isp_info_ptr->chn.r_offset= isp_context_ptr->chn.r_offset;
	exif_isp_info_ptr->chn.g_offset= isp_context_ptr->chn.g_offset;
	exif_isp_info_ptr->chn.b_offset= isp_context_ptr->chn.b_offset;
	exif_isp_info_ptr->chn.reserved1= isp_context_ptr->chn.reserved1;

	exif_isp_info_ptr->flash.effect= isp_context_ptr->ae.flash.effect;
	exif_isp_info_ptr->flash.lum_ratio= isp_context_ptr->flash.lum_ratio;
	exif_isp_info_ptr->flash.r_ratio= isp_context_ptr->flash.r_ratio;
	exif_isp_info_ptr->flash.g_ratio= isp_context_ptr->flash.g_ratio;
	exif_isp_info_ptr->flash.b_ratio= isp_context_ptr->flash.b_ratio;

	return rtn;
}

/* _ispGetExifInfoIOCtrl --
*@
*@
*@ return:
*/
static int32_t _ispGetExifInfoIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn=ISP_SUCCESS;

	ISP_LOG("ISP_RAW:--IOCtrl--GET_EXIF_INFO--:0x%x", *(uint32_t*)param_ptr);
	rtn = _ispGetExifInfo(handler_id, param_ptr);

	return rtn;
}

/* _ispAfStopIOCtrl --
*@
*@
*@ return:
*/
static int32_t _ispAfStopIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn=ISP_SUCCESS;
	ISP_MSG_INIT(isp_proc_msg);

	ISP_LOG("--IOCtrl--AF_STOP--");
	isp_proc_msg.handler_id = handler_id;
	isp_proc_msg.msg_type = ISP_PROC_EVT_AF_STOP;
	rtn = _isp_proc_msg_post(&isp_proc_msg);

	return rtn;
}

/* _ispAeMeasureLumIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispAeMeasureLumIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn = ISP_SUCCESS;
	enum isp_ae_weight weight = *(uint32_t*)param_ptr;

	ISP_LOG("--IOCtrl--AE_MEASURE_LUM--:0x%x",*(uint32_t*)param_ptr);
	_ispAeMeasureLumSet(handler_id, weight);

	return rtn;
}

/* _ispAeModeIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispAeModeIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetAlgContext(handler_id);

	ISP_LOG("--IOCtrl--AE_MODE--:0x%x",*(uint32_t*)param_ptr);
	if(ISP_UEB == isp_context_ptr->ae.back_bypass)
	{
		isp_context_ptr->ae.mode=*(uint32_t*)param_ptr;
		isp_ae_set_iso(handler_id, isp_ae_get_save_iso(handler_id));
		isp_context_ptr->ae.frame_mode=ISP_AE_AUTO;
		isp_context_ptr->ae.bypass = ISP_UEB;

		switch(isp_context_ptr->ae.mode)
		{
			case ISP_AUTO:
			{
				break;
			}
			case ISP_NIGHT:
			{
				break;
			}
			case ISP_SPORT:
			{
				isp_ae_set_iso(handler_id, ISP_ISO_1600);
				isp_context_ptr->ae.frame_mode=ISP_AE_FIX;
				break;
			}
			case ISP_PORTRAIT:
			{
				_ispAeMeasureLumSet(handler_id, ISP_AE_WEIGHT_CENTER);
				break;
			}
			case ISP_LANDSCAPE:
			{
				_ispAeMeasureLumSet(handler_id, ISP_AE_WEIGHT_AVG);
				break;
			}
			default :
				break;
		}
		isp_context_ptr->tune.ae = ISP_EB;
	} else {
		isp_context_ptr->ae.bypass = ISP_EB;
	}

	return rtn;
}

/* _ispSpecialEffectIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispSpecialEffectIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetAlgContext(handler_id);
	uint32_t cce_matrix_mode = *(uint32_t*)param_ptr;

	ISP_LOG("--IOCtrl--SPECIAL_EFFECT--:0x%x", cce_matrix_mode);
	if(ISP_EFFECT_EMBOSS==(*(uint32_t*)param_ptr)) {
		cce_matrix_mode=ISP_EFFECT_NORMAL;
		isp_context_ptr->emboss.bypass=ISP_UEB;
	} else {
		isp_context_ptr->emboss.bypass=ISP_EB;
	}
	_ispSetCceMatrix(&isp_context_ptr->cce_matrix, &isp_context_ptr->cce_tab[cce_matrix_mode]);
	isp_context_ptr->tune.special_effect=ISP_EB;

	return rtn;
}

/* _ispAfIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispAfIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetAlgContext(handler_id);
	struct isp_af_win* af_ptr = (struct isp_af_win*)param_ptr;

	ISP_LOG("--IOCtrl--AF--win_num:0x%x, mode:0x%x bypass:%d",
			af_ptr->valid_win, af_ptr->mode,isp_context_ptr->af.bypass);

	isp_context_ptr->af.monitor_bypass=ISP_EB;
	if((ISP_UEB == isp_context_ptr->af.bypass)
		&&(ISP_AF_CONTINUE != isp_context_ptr->af.status))
	{
		isp_context_ptr->af.mode = af_ptr->mode;
		isp_context_ptr->awb_win_conter = ISP_AWB_SKIP_FOREVER;
		_ispCfgAfWin(handler_id, &isp_context_ptr->af, af_ptr);

		if(ISP_FOCUS_WIN==isp_context_ptr->af.mode)
		{

		}
		else
		{
			if (ISP_EB == isp_context_ptr->af.end_handler_flag) {
				isp_context_ptr->af.ae_status=isp_context_ptr->ae.bypass;
				isp_context_ptr->af.awb_status=isp_context_ptr->awb.bypass;
				ISP_LOG("backup ae bypass %d",isp_context_ptr->af.ae_status);
			}
			isp_context_ptr->ae.bypass=ISP_EB;
			isp_context_ptr->awb.bypass=ISP_EB;
			isp_context_ptr->ae.monitor_bypass=ISP_EB;
			isp_context_ptr->awb.monitor_bypass=ISP_EB;
			isp_context_ptr->ae.cur_skip_num = ISP_AE_SKIP_FOREVER;
			isp_context_ptr->af.monitor_bypass=ISP_UEB;
			isp_context_ptr->tune.af=ISP_EB;
		}
	}

	return rtn;
}


/* _ispWbTrimIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispWbTrimIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn=ISP_SUCCESS;

	ISP_LOG("--IOCtrl--WB_TRIM--");
	_ispCalcAwbWin(handler_id, (struct isp_trim_size*)param_ptr);

	return rtn;
}

/* _ispParamUpdateIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispParamUpdateIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn=ISP_SUCCESS;

	ISP_LOG("--IOCtrl--PARAM_UPDATE--");
	_ispUpdateParam(handler_id);

	return rtn;
}


/* _ispAeTouchIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispAeTouchIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn=ISP_SUCCESS;

	ISP_LOG("--IOCtrl--AE_TOUCH--");
	rtn=_ispAeTouchZone(handler_id, (struct isp_pos_rect*) param_ptr);

	return rtn;
}

/* _ispAeInfoIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispAeInfoIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn=ISP_SUCCESS;

	ISP_LOG("--IOCtrl--AE_INFO--");
	_ispAeInfo(handler_id, (struct isp_ae_info*) param_ptr);

	return rtn;
}

/* _ispAfIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispAfModeIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetAlgContext(handler_id);

	ISP_LOG("--IOCtrl--AF_MODE--:0x%x", *(uint32_t*)param_ptr);

	if((ISP_FOCUS_CONTINUE == isp_context_ptr->af.mode)&&(ISP_FOCUS_CONTINUE != *(uint32_t*)param_ptr)){
		isp_context_ptr->af.continue_status=ISP_END_FLAG;
		if(ISP_AF_STOP != isp_context_ptr->af.status){
			_ispAfStopIOCtrl(handler_id, NULL, NULL);
		}
	}

	isp_context_ptr->af.mode = *(uint32_t*)param_ptr;

	if (ISP_FOCUS_CONTINUE == isp_context_ptr->af.mode){
		_isp_ContinueFocusStartInternal(handler_id);
	}

	return rtn;
}

/* _ispGammaIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispGammaIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetAlgContext(handler_id);

	ISP_LOG("--IOCtrl--ISP_CTRL_GAMMA--:%d", *(uint32_t*)param_ptr);
	isp_set_gamma(&isp_context_ptr->gamma, &isp_context_ptr->gamma_tab[*(uint32_t*)param_ptr]);
	isp_context_ptr->tune.gamma=ISP_EB;

	return rtn;
}

/* _ispDenoiseIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispDenoiseIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetAlgContext(handler_id);

	ISP_LOG("--IOCtrl--ISP_CTRL_DENOISE--:%d", *(uint32_t*)param_ptr);
	_ispSetDenoise(&isp_context_ptr->denoise, &isp_context_ptr->denoise_tab[*(uint32_t*)param_ptr]);
	isp_ae_set_denosie_level(handler_id, *(uint32_t*)param_ptr);
	isp_context_ptr->tune.denoise=ISP_EB;

	return rtn;
}

/* _ispSmartAeIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispSmartAeIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetAlgContext(handler_id);
	struct isp_smart_ae_param *smart_ae_ptr = (struct isp_smart_ae_param*)param_ptr;
	struct isp_ae_param *ae_param_ptr = (struct isp_ae_param*)&(isp_context_ptr->ae);

	ISP_LOG("--IOCtrl--SMART AE PARAM--");
	_ispSetSmartParam(ae_param_ptr, smart_ae_ptr);

	return rtn;
}



/* _ispAfIOCtrl --
*@
*@
*@ return:
*/
static int32_t _ispAfInfoIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_af_ctrl* af_ctrl_ptr = (struct isp_af_ctrl*)param_ptr;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);

	ISP_LOG("--IOCtrl--AF_CTRL--mode:0x%x", af_ctrl_ptr->mode);

	if (ISP_CTRL_SET==af_ctrl_ptr->mode) {
		isp_af_set_postion(handler_id, af_ctrl_ptr->step);
		ispAFMSkipNum(handler_id, 2);
		ispAFMbypass(handler_id, ISP_UEB);
	} else {
		af_ctrl_ptr->step = isp_context_ptr->af.cur_step;
		af_ctrl_ptr->num = 9;
		isp_af_get_stat_value(handler_id, (void*)af_ctrl_ptr->stat_value);
	}

	return rtn;
}

/* _ispRegIOCtrl --
*@
*@
*@ return:
*/
int32_t _ispRegIOCtrl(uint32_t handler_id, void* param_ptr, int(*call_back)())
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_reg_ctrl* reg_ctrl_ptr = (struct isp_reg_ctrl*)param_ptr;

	ISP_LOG("--IOCtrl--REG_CTRL--mode:0x%x", reg_ctrl_ptr->mode);

	if (ISP_CTRL_SET==reg_ctrl_ptr->mode) {
		ispRegWrite(handler_id, reg_ctrl_ptr->num, reg_ctrl_ptr->reg_tab);
	} else {
		ispRegRead(handler_id, reg_ctrl_ptr->num, reg_ctrl_ptr->reg_tab);
	}

	return rtn;
}

struct isp_io_ctrl_fun _s_isp_io_ctrl_fun_tab[]=
{
	{ISP_CTRL_AWB_MODE,              _ispAwbModeIOCtrl},
	{ISP_CTRL_AE_MODE,                 _ispAeModeIOCtrl},
	{ISP_CTRL_AE_MEASURE_LUM,    _ispAeMeasureLumIOCtrl},
	{ISP_CTRL_EV,                           _ispEVIOCtrl},
	{ISP_CTRL_FLICKER,                  _ispFlickerIOCtrl},
	{ISP_CTRL_ALG,                         _ispAlgIOCtrl},
	{ISP_CTRL_SPECIAL_EFFECT,      _ispSpecialEffectIOCtrl},
	{ISP_CTRL_BRIGHTNESS,            _ispBrightnessIOCtrl},
	{ISP_CTRL_CONTRAST,              _ispContrastIOCtrl},
	{ISP_CTRL_HIST,                       _ispHistIOCtrl},
	{ISP_CTRL_AUTO_CONTRAST,    _ispAutoContrastIOCtrl},
	{ISP_CTRL_SATURATION,           _ispSaturationIOCtrl},
	{ISP_CTRL_AF,                           _ispAfIOCtrl},
	{ISP_CTRL_AF_MODE,                 _ispAfModeIOCtrl},
	{ISP_CTRL_CSS,                         _ispCSSIOCtrl},
	{ISP_CTRL_HDR,                         _ispHDRIOCtrl},
	{ISP_CTRL_GLOBAL_GAIN,           _ispGlobalGainIOCtrl},
	{ISP_CTRL_CHN_GAIN,                _ispChnGainIOCtrl},
	{ISP_CTRL_GET_EXIF_INFO,        _ispGetExifInfoIOCtrl},
	{ISP_CTRL_ISO,                          _ispIsoIOCtrl},
	{ISP_CTRL_WB_TRIM,                 _ispWbTrimIOCtrl},
	{ISP_CTRL_PARAM_UPDATE,       _ispParamUpdateIOCtrl},
	{ISP_CTRL_FLASH_EG,                _ispFlashEGIOCtrl},
	{ISP_CTRL_VIDEO_MODE,            _ispVideoModeIOCtrl},
	{ISP_CTRL_AF_STOP,                  _ispAfStopIOCtrl},
	{ISP_CTRL_AE_TOUCH,                _ispAeTouchIOCtrl},
	{ISP_CTRL_AE_INFO,                   _ispAeInfoIOCtrl},
	{ISP_CTRL_SHARPNESS,              _ispSharpnessIOCtrl},
	{ISP_CTRL_GET_FAST_AE_STAB, _ispGetFastAeStabIOCtrl},
	{ISP_CTRL_GET_AE_STAB,          _ispGetAeStabIOCtrl},
	{ISP_CTRL_GET_AE_CHG,            _ispGetAeChangeIOCtrl},
	{ISP_CTRL_GET_AWB_STAT,       _ispGetAwbStatIOCtrl},
	{ISP_CTRL_GET_AF_STAT,          _ispGetAfStatIOCtrl},
	{ISP_CTRL_GAMMA,                    _ispGammaIOCtrl},
	{ISP_CTRL_DENOISE,                  _ispDenoiseIOCtrl},
	{ISP_CTRL_SMART_AE,               _ispSmartAeIOCtrl},
	{ISP_CTRL_CONTINUE_AF,          _ispContinueAfIOCtrl},
	{ISP_CTRL_AF_DENOISE,            _ispAfDenoiseIOCtrl},
	{ISP_CTRL_FLASH_CTRL,            PNULL}, // for tool cali
	{ISP_CTRL_AE_CTRL,                  _ispAeIOCtrl}, // for tool cali
	{ISP_CTRL_AF_CTRL,                  _ispAfInfoIOCtrl}, // for tool cali
	{ISP_CTRL_REG_CTRL,                _ispRegIOCtrl}, // for tool cali
	{ISP_CTRL_AF_END_INFO,           _ispRegIOCtrl}, // for tool cali
	{ISP_CTRL_MAX, PNULL}
};

/* _ispGetIOCtrlFun --
*@
*@
*@ return:
*/
static io_fun _ispGetIOCtrlFun(enum isp_ctrl_cmd cmd)
{
	io_fun io_ctrl=PNULL;
	uint32_t i=0x00;

	for (i=0x00; i < ISP_CTRL_MAX; i++) {
		if (cmd == _s_isp_io_ctrl_fun_tab[i].cmd) {
			io_ctrl=_s_isp_io_ctrl_fun_tab[i].io_ctrl;
			break ;
		}
	}

	return io_ctrl;
}

/* ispTuneIOCtrl --
*@
*@
*@ return:
*/
static int32_t _ispTuneIOCtrl(uint32_t handler_id, enum isp_ctrl_cmd io_cmd, void* param_ptr, int(*call_back)())
{
	int32_t rtn = ISP_SUCCESS;
	enum isp_ctrl_cmd cmd = io_cmd&0x7fffffff;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	io_fun io_ctrl = PNULL;

	isp_context_ptr->isp_callback_bypass = io_cmd&0x80000000;

	io_ctrl=_ispGetIOCtrlFun(cmd);

	if (PNULL != io_ctrl) {
		io_ctrl(handler_id, param_ptr, call_back);
	} else {
		ISP_LOG("io ctrl fun is null error");
	}

	return rtn;
}

/* _isp_check_init_param --
*@
*@
*@ return:
*/
static int32_t _isp_check_init_param(uint32_t handler_id, struct isp_init_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	struct sensor_raw_info* raw_info_ptr=NULL;
	struct sensor_version_info* version_info_ptr=NULL;
	struct sensor_raw_tune_info* raw_tune_ptr=NULL;

	if(NULL==param_ptr){
		rtn=ISP_PARAM_NULL;
		ISP_RETURN_IF_FAIL(rtn, ("init param ptr null error"));
	}

	if(NULL==param_ptr->setting_param_ptr){
		rtn=ISP_PARAM_NULL;
		ISP_RETURN_IF_FAIL(rtn, ("sensor param ptr null error"));
	}
	raw_info_ptr=(struct sensor_raw_info*)param_ptr->setting_param_ptr;
	version_info_ptr=(struct sensor_version_info*)raw_info_ptr->version_info;
	raw_tune_ptr=(struct sensor_raw_tune_info*)raw_info_ptr->tune_ptr;

	if(version_info_ptr->version_id!=param_ptr->isp_id){
		rtn=ISP_PARAM_ERROR;
		ISP_RETURN_IF_FAIL(rtn, ("check isp id chip_id:0x%08x, isp_id:0x%08x  error", param_ptr->isp_id, version_info_ptr->version_id));
	}

	if(raw_tune_ptr->version_id!=param_ptr->isp_id){
		rtn=ISP_PARAM_ERROR;
		ISP_RETURN_IF_FAIL(rtn, ("check isp param id chip_id:0x%08x, param_id:0x%08x error", param_ptr->isp_id, raw_tune_ptr->version_id));
	}

	if((ISP_ZERO!=(param_ptr->size.w&ISP_ONE))
		||(ISP_ZERO!=(param_ptr->size.h&ISP_ONE))) {
		rtn=ISP_PARAM_ERROR;
		ISP_RETURN_IF_FAIL(rtn, ("input size: w:%d, h:%d error", param_ptr->size.w, param_ptr->size.h));
	}

	if(PNULL==param_ptr->ctrl_callback){
		rtn=ISP_CALLBACK_NULL;
		ISP_RETURN_IF_FAIL(rtn, ("ctrl callback null error"));
	}

	return rtn;
}

/* _isp_set_init_param --
*@
*@
*@ return:
*/
static int32_t _isp_set_init_param(uint32_t handler_id, struct isp_init_param* ptr, struct isp_cfg_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_system* isp_system_ptr = ispGetSystem();

	memset((void*)&isp_context_ptr->cfg, ISP_ZERO, sizeof(struct isp_cfg_param));

	isp_system_ptr->isp_id=ptr->isp_id;
	//isp_SetIspId(isp_system_ptr->isp_id);
	if (ISP_ONE == isp_system_ptr->handler_num) {
		ispRegInit(handler_id, isp_system_ptr->isp_id);
	}

	isp_context_ptr->cfg.sensor_info_ptr=ptr->setting_param_ptr;
	isp_context_ptr->cfg.data.input_size.w=ptr->size.w;
	isp_context_ptr->cfg.data.input_size.h=ptr->size.h;
	isp_context_ptr->cfg.callback=ptr->ctrl_callback;
	isp_context_ptr->cfg.self_callback=ptr->self_callback;

	param_ptr->sensor_info_ptr=ptr->setting_param_ptr;
	param_ptr->data.input_size.w=ptr->size.w;
	param_ptr->data.input_size.h=ptr->size.h;
	param_ptr->callback=ptr->ctrl_callback;
	param_ptr->self_callback=ptr->self_callback;

	return rtn;
}

/* _isp_check_video_param --
*@
*@
*@ return:
*/
static int32_t _isp_check_video_param(uint32_t handler_id, struct isp_video_start* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	if((ISP_ZERO!=(param_ptr->size.w&ISP_ONE))
		||(ISP_ZERO!=(param_ptr->size.h&ISP_ONE))){
		rtn=ISP_PARAM_ERROR;
		ISP_RETURN_IF_FAIL(rtn, ("input size: w:%d, h:%d error", param_ptr->size.w, param_ptr->size.h));
	}
/*
	if(!((ISP_DATA_NORMAL_RAW10==param_ptr->format)
		||(ISP_DATA_CSI2_RAW10==param_ptr->format))){
		rtn=ISP_PARAM_ERROR;
		ISP_RETURN_IF_FAIL(rtn, ("input format: %d error", param_ptr->format));
	}
*/
	return rtn;
}

/* _isp_get_video_param --
*@
*@
*@ return:
*/
static int32_t _isp_get_video_param(uint32_t handler_id, struct isp_video_start* param_ptr, struct isp_cfg_param* rtn_param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct sensor_raw_info* sensor_info_ptr = (struct sensor_raw_info*)isp_context_ptr->cfg.sensor_info_ptr;

	isp_context_ptr->src.w=param_ptr->size.w;
	isp_context_ptr->src.h=param_ptr->size.h;

	rtn_param_ptr->video_mode=param_ptr->mode;
	rtn_param_ptr->data.work_mode=ISP_CONTINUE_MODE;
	rtn_param_ptr->data.input=ISP_CAP_MODE;
	rtn_param_ptr->data.input_format=param_ptr->format;
	rtn_param_ptr->data.format_pattern=sensor_info_ptr->resolution_info_ptr->image_pattern;
	rtn_param_ptr->data.input_size.w=param_ptr->size.w;
	rtn_param_ptr->data.input_size.h=param_ptr->size.h;
	rtn_param_ptr->data.output_format=ISP_DATA_UYVY;
	rtn_param_ptr->data.output=ISP_DCAM_MODE;
	rtn_param_ptr->data.slice_height=param_ptr->size.h;
	rtn_param_ptr->callback=isp_context_ptr->cfg.callback;
	rtn_param_ptr->self_callback=isp_context_ptr->cfg.self_callback;
	rtn_param_ptr->sensor_info_ptr=isp_context_ptr->cfg.sensor_info_ptr;

	return rtn;
}

/* _isp_set_video_param --
*@
*@
*@ return:
*/
static int32_t _isp_set_video_param(uint32_t handler_id, struct isp_cfg_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);

	rtn=_ispSetInterfaceParam(handler_id, param_ptr);
	ISP_RETURN_IF_FAIL(rtn, ("set param error"));

	isp_context_ptr->slice.pos_info=ISP_SLICE_ALL;

	return rtn;
}

/* _ispCheckProcParam --
*@
*@
*@ return:
*/
static int32_t _isp_check_proc_param(uint32_t handler_id, struct ips_in_param* in_param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	if((ISP_ZERO!=(in_param_ptr->src_frame.img_size.w&ISP_ONE))
		||(ISP_ZERO!=(in_param_ptr->src_frame.img_size.h&ISP_ONE))){
		rtn=ISP_PARAM_ERROR;
		ISP_RETURN_IF_FAIL(rtn, ("input size: w:%d, h:%d error", in_param_ptr->src_frame.img_size.w, in_param_ptr->src_frame.img_size.h));
	}

	return rtn;
}

/* _isp_check_proc_next_param --
*@
*@
*@ return:
*/
static int32_t _isp_check_proc_next_param(uint32_t handler_id, struct ipn_in_param* in_param_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	if((ISP_ZERO!=(in_param_ptr->src_slice_height&ISP_ONE))
		||(ISP_ZERO!=(in_param_ptr->src_avail_height&ISP_ONE))){
		rtn=ISP_PARAM_ERROR;
		ISP_RETURN_IF_FAIL(rtn, ("input size:src_slice_h:%d,src_avail_h:%d error", in_param_ptr->src_slice_height, in_param_ptr->src_avail_height));
	}

	return rtn;
}

/* _isp_get_proc_param --
*@
*@
*@ return:
*/
static int32_t _isp_get_proc_param(uint32_t handler_id, struct ips_in_param* in_param_ptr, struct isp_cfg_param* rtn_param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct sensor_raw_info* sensor_info_ptr = (struct sensor_raw_info*)isp_context_ptr->cfg.sensor_info_ptr;

	isp_context_ptr->src.w=in_param_ptr->src_frame.img_size.w;
	isp_context_ptr->src.h=in_param_ptr->src_frame.img_size.h;

	rtn_param_ptr->data.work_mode=ISP_SINGLE_MODE;
	rtn_param_ptr->data.input=ISP_EMC_MODE;
	rtn_param_ptr->data.input_format=in_param_ptr->src_frame.img_fmt;
	rtn_param_ptr->data.format_pattern=sensor_info_ptr->resolution_info_ptr->image_pattern;
	rtn_param_ptr->data.input_size.w=in_param_ptr->src_frame.img_size.w;
	rtn_param_ptr->data.input_size.h=in_param_ptr->src_frame.img_size.h;
	rtn_param_ptr->data.input_addr.chn0=in_param_ptr->src_frame.img_addr_phy.chn0;
	rtn_param_ptr->data.input_addr.chn1=in_param_ptr->src_frame.img_addr_phy.chn1;
	rtn_param_ptr->data.input_addr.chn2=in_param_ptr->src_frame.img_addr_phy.chn2;
	rtn_param_ptr->data.slice_height=in_param_ptr->src_slice_height;

	rtn_param_ptr->data.output_format=in_param_ptr->dst_frame.img_fmt;
	rtn_param_ptr->data.output=ISP_EMC_MODE;
	rtn_param_ptr->data.output_addr.chn0=in_param_ptr->dst_frame.img_addr_phy.chn0;
	rtn_param_ptr->data.output_addr.chn1=in_param_ptr->dst_frame.img_addr_phy.chn1;
	rtn_param_ptr->data.output_addr.chn2=in_param_ptr->dst_frame.img_addr_phy.chn2;
	rtn_param_ptr->callback=isp_context_ptr->cfg.callback;
	rtn_param_ptr->self_callback=isp_context_ptr->cfg.self_callback;
	rtn_param_ptr->sensor_info_ptr=isp_context_ptr->cfg.sensor_info_ptr;

	return rtn;
}

/* _isp_set_proc_param --
*@
*@
*@ return:
*/
static int32_t _isp_set_proc_param(uint32_t handler_id, struct isp_cfg_param* param_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);

	memset((void*)&isp_context_ptr->slice, ISP_ZERO, sizeof(struct isp_slice_param));

	rtn=_ispSetInterfaceParam(handler_id, param_ptr);
	ISP_RETURN_IF_FAIL(rtn, ("set param error"));

	rtn=_ispSetSlicePosInfo(&isp_context_ptr->slice);

	return rtn;
}

/* _isp_set_proc_next_param --
*@
*@
*@ return:
*/
static int32_t _isp_set_proc_next_param(uint32_t handler_id, struct ipn_in_param* in_ptr)
{
	int32_t rtn=ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);

	isp_context_ptr->slice.cur_slice_num.w=ISP_ZERO;
	isp_context_ptr->cfg.data.slice_height=in_ptr->src_slice_height;
	isp_context_ptr->slice.max_size.h=in_ptr->src_slice_height;

	rtn=_ispSetSlicePosInfo(&isp_context_ptr->slice);

	// store
	isp_context_ptr->store.addr.chn0=in_ptr->dst_addr_phy.chn0;
	isp_context_ptr->store.addr.chn1=in_ptr->dst_addr_phy.chn1;
	isp_context_ptr->store.addr.chn2=in_ptr->dst_addr_phy.chn2;

	return rtn;
}

/* _isp_init --
*@
*@
*@ return:
*/
void check_param(struct isp_init_param* ptr)
{
	struct sensor_raw_info*  raw_info_ptr = (struct sensor_raw_info*)ptr->setting_param_ptr;
	struct sensor_raw_tune_info* tune_ptr = raw_info_ptr->tune_ptr;
	struct sensor_raw_fix_info* fix_ptr = raw_info_ptr->fix_ptr;
	uint32_t tune_info_offset = sizeof(struct sensor_version_info);
	uint32_t fix_info_offset = tune_info_offset+sizeof(struct sensor_raw_tune_info);
}

static int _isp_init(uint32_t handler_id, struct isp_init_param* ptr)
{
	int rtn=0x00;
	struct isp_system* isp_system_ptr = ispGetSystem();
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);
	struct isp_cfg_param cfg_param;

	isp_ae_init_context(handler_id,(void*)isp_context_ptr);

	rtn=_isp_set_init_param(handler_id, ptr, &cfg_param);
	ISP_RETURN_IF_FAIL(rtn, ("set init param error"));

	check_param(ptr);

	rtn=_ispSetParam(handler_id, &cfg_param);
	ISP_RETURN_IF_FAIL(rtn, ("set param error"));

	isp_system_ptr->isp_status=ISP_IDLE;

	return rtn;
}

/* _isp_deinit --
*@
*@
*@ return:
*/
static int _isp_deinit(uint32_t handler_id)
{
	int rtn=0x00;

	rtn=_ispUncfg(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("isp uncfg error"));

	rtn = ispStop(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("isp stop error"));

	return rtn;
}

/* _isp_video_start --
*@
*@
*@ return:
*/
static int _isp_video_start(uint32_t handler_id, struct isp_video_start* param_ptr)
{
	int rtn=0x00;
	struct isp_system* isp_system_ptr = ispGetSystem();
	struct isp_cfg_param cfg_param;

	rtn=_ispIoCtrlInit(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("isp IO ctrl Init error"));

	rtn=_isp_get_video_param(handler_id, param_ptr ,&cfg_param);
	ISP_RETURN_IF_FAIL(rtn, ("get video param error"));

	rtn=_isp_set_video_param(handler_id, &cfg_param);
	ISP_RETURN_IF_FAIL(rtn, ("set video param error"));

	rtn=_ispChangeVideoCfg(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("vodeo change cfg error"));

	rtn=_ispCfg(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("isp cfg error"));

	rtn=_ispCfgInt(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("isp cfg int error"));

	rtn=_ispStart(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("video isp start error"));

	isp_system_ptr->isp_status=ISP_CONTINUE;

	return rtn;
}

/* _isp_video_stop --
*@
*@
*@ return:
*/
static int _isp_video_stop(uint32_t handler_id)
{
	int rtn=0x00;
	struct isp_system* isp_system_ptr = ispGetSystem();

	rtn=ispRegIRQ(handler_id, ISP_ZERO);
	ISP_RETURN_IF_FAIL(rtn, ("reg irq error"));

	rtn=ispCfgDcamIRQ(handler_id, ISP_UEB);
	ISP_RETURN_IF_FAIL(rtn, ("cfg dcam irq error"));

	rtn=_isp3ADeInit(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("deinit3a error"));

	rtn=_ispUncfg(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("isp cfg error"));

	isp_system_ptr->isp_status=ISP_IDLE;

	return rtn;
}

/* _isp_proc_start --
*@
*@
*@ return:
*/
static int _isp_proc_start(uint32_t handler_id, struct ips_in_param* in_param_ptr)
{
	int rtn=0x00;
	struct isp_system* isp_system_ptr = ispGetSystem();
	struct isp_cfg_param cfg_param;

	rtn=_isp_get_proc_param(handler_id, in_param_ptr ,&cfg_param);
	ISP_RETURN_IF_FAIL(rtn, ("get proc param error"));

	rtn=_ispChangeProcCfg(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("proc change cfg error"));

	rtn=_ispCfg(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("isp cfg error"));

	rtn=_isp_set_proc_param(handler_id, &cfg_param);
	ISP_RETURN_IF_FAIL(rtn, ("set proc param error"));

	rtn=ispRegIRQ(handler_id, ISP_MONITOR_EVT_TX);
	ISP_RETURN_IF_FAIL(rtn, ("reg irq error"));

	rtn=_ispStart(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("proc isp start error"));

	isp_system_ptr->isp_status=ISP_SIGNAL;

	return rtn;
}

/* _isp_proc_next --
*@
*@
*@ return:
*/
static int _isp_proc_next(uint32_t handler_id, struct ipn_in_param* in_ptr)
{
	int rtn=0x00;
	struct isp_system* isp_system_ptr = ispGetSystem();

	rtn=_isp_set_proc_next_param(handler_id, in_ptr);
	ISP_RETURN_IF_FAIL(rtn, ("set proc next param error"));

	rtn=_ispStart(handler_id);
	ISP_RETURN_IF_FAIL(rtn, ("proc next isp start error"));

	isp_system_ptr->isp_status=ISP_SIGNAL;

	return rtn;
}

/* _isp_msg_queue_create --
*@
*@
*@ return:
*/
static int _isp_msg_queue_create(uint32_t count, uint32_t *queue_handle)
{
	int   rtn=ISP_SUCCESS;

	rtn=isp_msg_queue_create( count, queue_handle);

	return rtn;
}

/* _isp_msg_queue_destroy --
*@
*@
*@ return:
*/
static int _isp_msg_queue_destroy(uint32_t queue_handle)
{
	int   rtn=ISP_SUCCESS;

	rtn=isp_msg_queue_destroy(queue_handle);

	return rtn;
}

/* _isp_cond_wait --
*@
*@
*@ return:
*/
static int _isp_cond_wait(pthread_cond_t* cond_ptr, pthread_mutex_t* mutex_ptr)
{
	int rtn = ISP_SUCCESS;
	uint32_t handler_id = ISP_ZERO;

	rtn = pthread_mutex_lock(mutex_ptr);
	ISP_RETURN_IF_FAIL(rtn, ("lock cond mutex %d error", rtn));
	rtn = pthread_cond_wait(cond_ptr, mutex_ptr);
	ISP_RETURN_IF_FAIL(rtn, ("cond wait %d error", rtn));
	rtn = pthread_mutex_unlock(mutex_ptr);
	ISP_RETURN_IF_FAIL(rtn, ("unlock cond mutex %d error", rtn));

	return rtn;
}

/* _isp_cond_signal --
*@
*@
*@ return:
*/
static int _isp_cond_signal(pthread_cond_t* cond_ptr, pthread_mutex_t* mutex_ptr)
{
	int rtn=ISP_SUCCESS;
	uint32_t handler_id = ISP_ZERO;
	rtn = pthread_mutex_lock(mutex_ptr);
	ISP_RETURN_IF_FAIL(rtn, ("lock cond mutex %d error", rtn));
	rtn=pthread_cond_signal(cond_ptr);
	ISP_TRACE_IF_FAIL(rtn, ("cond signal %d error", rtn));
	rtn = pthread_mutex_unlock(mutex_ptr);
	ISP_RETURN_IF_FAIL(rtn, ("unlock cond mutex %d error", rtn));
	return rtn;
}

/* _isp_monitor_msg_get --
*@
*@
*@ return:
*/
static int _isp_monitor_msg_get(struct isp_msg *message)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();

	rtn=isp_msg_get( isp_system_ptr->monitor_queue, message);

	return rtn;
}

/* _isp_monitor_msg_post --
*@
*@
*@ return:
*/
static int _isp_monitor_msg_post(struct isp_msg *message)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();

	rtn=isp_msg_post( isp_system_ptr->monitor_queue, message);

	return rtn;
}


/* _isp_ctrl_msg_get --
*@
*@
*@ return:
*/
static int _isp_ctrl_msg_get(struct isp_msg *message)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();

	rtn=isp_msg_get( isp_system_ptr->ctrl_queue, message);

	return rtn;
}

/* _isp_ctrl_msg_post --
*@
*@
*@ return:
*/
static int _isp_ctrl_msg_post(struct isp_msg *message)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();
	uint32_t           handler_id = 0;

/*	if (message) {
		ISP_LOG("isp test,0x%x,0x%x",message->msg_type,
				message->sub_msg_type);
	}*/
	rtn=isp_msg_post( isp_system_ptr->ctrl_queue, message);

	return rtn;
}

/* _isp_proc_msg_get --
*@
*@
*@ return:
*/
static int _isp_proc_msg_get(struct isp_msg *message)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();

	rtn=isp_msg_get( isp_system_ptr->proc_queue, message);

	return rtn;
}

/* _isp_proc_msg_post --
*@
*@
*@ return:
*/
static int _isp_proc_msg_post(struct isp_msg *message)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();

	rtn=isp_msg_post( isp_system_ptr->proc_queue, message);

	return rtn;
}

/* _isp_monitor_routine --
*@
*@
*@ return:
*/
static void *_isp_monitor_routine(void *client_data)
{
	int rtn=ISP_SUCCESS;
	uint32_t i = 0;
	struct isp_context* isp_context_ptr=NULL;
	struct isp_system* isp_system_ptr = ispGetSystem();
	ISP_MSG_INIT(isp_ctrl_msg);
	struct isp_irq_param evt;
	uint32_t handler_id=0x00;
	uint32_t int_val = 0;
	uint32_t sof_cont=0x00;
	uint32_t sp_param=0x00;
	uint32_t isp_id=IspGetId();

	ISP_LOG("enter isp monitor routine.");

	while (1) {
		if(ISP_SUCCESS != ispGetIRQ((uint32_t*)&evt)) {
			ISP_LOG("wait int 0x%08x error ", evt.irq_val);
			break;
		} else {

			handler_id = 0x00;
			isp_system_ptr->monitor_status=ISP_RUN;
			isp_context_ptr = ispGetContext(handler_id);

			if(ISP_ZERO != (evt.irq_val & ISP_MONITOR_EVT_STOP))
			{
				ISP_LOG("isp monitor routine stop evt");
				isp_system_ptr->monitor_status=ISP_CLOSE;
			}
			if(ISP_CLOSE == isp_system_ptr->monitor_status) {
				isp_ctrl_msg.handler_id = handler_id;
				isp_ctrl_msg.msg_type = ISP_CTRL_EVT_MONITOR_STOP;
				rtn = _isp_ctrl_msg_post(&isp_ctrl_msg);
				break;
			}
			if(ISP_ZERO != (evt.irq_val & ISP_MONITOR_EVT_AF))
			{
//				ISP_LOG("af isp monitor routine af");
				isp_ctrl_msg.handler_id = handler_id;
				isp_ctrl_msg.msg_type = ISP_CTRL_EVT_AF;
				rtn = _isp_ctrl_msg_post(&isp_ctrl_msg);
			}
			if(ISP_ZERO != (evt.irq_val & ISP_MONITOR_EVT_AWB))
			{
				isp_id=IspGetId();
				//ISP_LOG("isp monitor routine conter :%d", isp_context_ptr->ae.monitor_conter);
				if (SC8825_ISP_ID==isp_id)
				{
					if((isp_context_ptr->ae.skip_frame+ISP_ONE)==isp_context_ptr->ae.monitor_conter)
					{
						isp_context_ptr->ae.cur_skip_num = isp_context_ptr->ae.skip_frame;
					}

					if(ISP_ZERO!=isp_context_ptr->ae.monitor_conter)
					{
						isp_context_ptr->ae.monitor_conter--;
					}
					else
					{
						isp_context_ptr->ae.monitor_conter=isp_context_ptr->ae.skip_frame+ISP_ONE;
						isp_ctrl_msg.handler_id = handler_id;
						isp_ctrl_msg.msg_type = ISP_CTRL_EVT_AWB;
						//ISP_LOG("isp monitor routine awb msg");
						rtn = _isp_ctrl_msg_post(&isp_ctrl_msg);
					}
				}
				else if (SC8830_ISP_ID==isp_id)
				{
					isp_ctrl_msg.handler_id = handler_id;
					isp_ctrl_msg.msg_type = ISP_CTRL_EVT_AWB;
					rtn = _isp_ctrl_msg_post(&isp_ctrl_msg);
				}
			}
			if(ISP_ZERO != (evt.irq_val & ISP_MONITOR_EVT_AE))
			{
				isp_id=IspGetId();
				if (SC8830_ISP_ID==isp_id)
				{
					if ( ISP_ONE == isp_context_ptr->ae.monitor_conter) {
						//ISP_LOG("tim_ae isp monitor routine ae");
						isp_ctrl_msg.handler_id = handler_id;
						isp_ctrl_msg.msg_type = ISP_CTRL_EVT_AE;
						rtn = _isp_ctrl_msg_post(&isp_ctrl_msg);
					} else {
						isp_context_ptr->ae.monitor_conter ++;
					}
				}
			}
			if(ISP_ZERO != (evt.irq_val & ISP_MONITOR_EVT_EOF))
			{
				//ISP_LOG("isp monitor routine eof");
				isp_ctrl_msg.handler_id = handler_id;
				isp_ctrl_msg.msg_type = ISP_CTRL_EVT_EOF;
				rtn = _isp_ctrl_msg_post(&isp_ctrl_msg);
			}
			if(ISP_ZERO != (evt.irq_val & ISP_MONITOR_EVT_SOF))
			{
				//ISP_LOG("isp monitor routine sof");
				isp_ctrl_msg.handler_id = handler_id;
				isp_ctrl_msg.msg_type = ISP_CTRL_EVT_SOF;
				rtn = _isp_ctrl_msg_post(&isp_ctrl_msg);
			}
			if(ISP_ZERO != (evt.irq_val & ISP_MONITOR_EVT_TX))
			{
				ISP_LOG("isp monitor routine tx");
				isp_ctrl_msg.handler_id = handler_id;
				isp_ctrl_msg.msg_type = ISP_CTRL_EVT_TX;
				rtn = _isp_ctrl_msg_post(&isp_ctrl_msg);
			}
			if((ISP_ZERO != (evt.irq_val & ISP_MONITOR_EVT_FETCH))
				||(ISP_ZERO != (evt.irq_val & ISP_MONITOR_EVT_DCAM))
				||(ISP_ZERO != (evt.irq_val & ISP_MONITOR_EVT_STORE)))
			{
				ISP_LOG("isp monitor routine error");
			}
		}
		isp_system_ptr->monitor_status = ISP_IDLE;
	}

	ISP_LOG("exit isp monitor routine.");

	return NULL;

}

/* _isp_create_monitor_thread --
*@
*@
*@ return:
*/
static int _isp_create_monitor_thread(void)
{
	int rtn = ISP_SUCCESS;
	uint32_t handler_id = ISP_ZERO;
	struct isp_system* isp_system_ptr = ispGetSystem();
	pthread_attr_t attr;

	rtn = _isp_msg_queue_create(ISP_THREAD_QUEUE_NUM, &isp_system_ptr->monitor_queue);
	ISP_RETURN_IF_FAIL(rtn, ("careate monitor queue error"));

	pthread_attr_init (&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	rtn = pthread_create(&isp_system_ptr->monitor_thr, &attr, _isp_monitor_routine, NULL);
	ISP_RETURN_IF_FAIL(rtn, ("careate monitor thread error"));
	pthread_attr_destroy(&attr);

	isp_system_ptr->monitor_status=ISP_IDLE;

	return rtn;
}

/* _isp_destory_monitor_thread --
*@
*@
*@ return:
*/
static int _isp_destory_monitor_thread(void)
{
	int rtn = ISP_SUCCESS;
	uint32_t handler_id = ISP_ZERO;
	struct isp_system* isp_system_ptr = ispGetSystem();

	rtn=_isp_msg_queue_destroy(isp_system_ptr->monitor_queue);
	ISP_RETURN_IF_FAIL(rtn, ("destroy ctrl queue error"));

	return rtn;
}

/* _isp_ctrl_routine --
*@
*@
*@ return:
*/
static void *_isp_ctrl_routine(void *client_data)
{
	int rtn=ISP_SUCCESS;
	struct isp_respond* map_res_ptr;
	struct isp_respond* res_ptr;
	struct isp_system* isp_system_ptr = ispGetSystem();
	struct isp_context* isp_context_ptr = NULL;
	ISP_MSG_INIT(isp_ctrl_msg);
	ISP_MSG_INIT(isp_ctrl_self_msg);
	ISP_MSG_INIT(isp_proc_msg);
	uint32_t handler_id=0x00;
	uint32_t evt=0x00;
	uint32_t sub_type=0x00;
	void* param_ptr=NULL;

	ISP_LOG("enter isp ctrl routine.");

	while (1) {
		rtn = _isp_ctrl_msg_get(&isp_ctrl_msg);
		if (rtn)
		{
			ISP_LOG("msg queue error");
			break;
		}

		isp_system_ptr->ctrl_status=ISP_RUN;
		handler_id = isp_ctrl_msg.handler_id;
		evt = (uint32_t)(isp_ctrl_msg.msg_type & ISP_CTRL_EVT_MASK);
		sub_type = isp_ctrl_msg.sub_msg_type;
		param_ptr = (void*)isp_ctrl_msg.data;
		map_res_ptr = (void*)isp_ctrl_msg.respond;

		isp_context_ptr=ispGetContext(handler_id);

//		ISP_LOG("ctrl handler_id: %d", handler_id);

		switch (evt) {

			case ISP_CTRL_EVT_START:
				isp_system_ptr->ctrl_status=ISP_IDLE;
				rtn=ispOpenDev(handler_id, ISP_ONE);
				pthread_mutex_lock(&isp_system_ptr->cond_mutex);
				rtn=pthread_cond_signal(&isp_system_ptr->thread_common_cond);
				pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
				break;

			case ISP_CTRL_EVT_STOP:
				isp_system_ptr->ctrl_status=ISP_CLOSE;
				ispOpenDev(handler_id, ISP_ZERO);
				pthread_mutex_lock(&isp_system_ptr->cond_mutex);
				rtn=pthread_cond_signal(&isp_system_ptr->thread_common_cond);
				pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
				break;

			case ISP_CTRL_EVT_INIT:
				rtn=_isp_init(handler_id, (struct isp_init_param*)param_ptr);
				pthread_mutex_lock(&isp_system_ptr->cond_mutex);
				rtn=pthread_cond_signal(&isp_system_ptr->init_cond);
				pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
				break;

			case ISP_CTRL_EVT_DEINIT:
				rtn=_isp_deinit(handler_id);
				break;

			case ISP_CTRL_EVT_MONITOR_STOP:
				pthread_mutex_lock(&isp_system_ptr->cond_mutex);
				rtn=pthread_cond_signal(&isp_system_ptr->deinit_cond);
				pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
				break;

			case ISP_CTRL_EVT_CONTINUE:
				rtn = _isp_video_start(handler_id, (struct isp_video_start*)param_ptr);
				ISP_TRACE_IF_FAIL(rtn, ("_isp_video_start error"));
				rtn = _isp3AInit(handler_id);
				ISP_TRACE_IF_FAIL(rtn, ("_isp3AInit error"));

				pthread_mutex_lock(&isp_system_ptr->cond_mutex);
				rtn=pthread_cond_signal(&isp_system_ptr->continue_cond);
				pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
				break;

			case ISP_CTRL_EVT_CONTINUE_STOP:
				rtn = _isp_video_stop(handler_id);
				isp_proc_msg.handler_id = handler_id;
				isp_proc_msg.msg_type = ISP_PROC_EVT_STOP_HANDLER;
				rtn = _isp_proc_msg_post(&isp_proc_msg);

				pthread_mutex_lock(&isp_system_ptr->cond_mutex);
				rtn=pthread_cond_signal(&isp_system_ptr->continue_stop_cond);
				pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
				break;

			case ISP_CTRL_EVT_SIGNAL:
			{
				rtn = _isp_proc_start(handler_id, (struct ips_in_param*)param_ptr);
				break;
			}
			case ISP_CTRL_EVT_SIGNAL_NEXT:
				rtn = _isp_proc_next(handler_id, (struct ipn_in_param*)param_ptr);
				break;

			case ISP_CTRL_EVT_IOCTRL:
				res_ptr=map_res_ptr;
				rtn = _ispTuneIOCtrl(handler_id, sub_type, param_ptr,NULL);
				res_ptr->rtn = rtn;
					pthread_mutex_lock(&isp_system_ptr->cond_mutex);
					rtn=pthread_cond_signal(&isp_system_ptr->ioctrl_cond);
					pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
				break;

			case ISP_CTRL_EVT_CTRL_SYNC:
				res_ptr=map_res_ptr;
				res_ptr->rtn=sub_type;
				pthread_mutex_lock(&isp_system_ptr->cond_mutex);
				rtn=pthread_cond_signal(&isp_system_ptr->ioctrl_cond);
				pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
				break;

			case ISP_CTRL_EVT_TX:
				ISP_LOG("ISP_CTRL_EVT_TX");
				rtn = _ispProcessEndHandle(handler_id);
				break;

			case ISP_CTRL_EVT_SOF:
				//ISP_LOG("ae awb af _isp_ctrl_routine sof ");
				rtn = _ispSetTuneParam(handler_id);
				break;

			case ISP_CTRL_EVT_EOF:
				break;

			case ISP_CTRL_EVT_AE:
				//ISP_LOG("tim_ae _isp_ctrl_routine --- ae");
				rtn=_ispCfgAemInfo(handler_id, &isp_context_ptr->ae_stat);
				isp_proc_msg.handler_id = handler_id;
				isp_proc_msg.msg_type = ISP_PROC_EVT_AE;
				rtn = _isp_proc_msg_post(&isp_proc_msg);
				break;

			case ISP_CTRL_EVT_AWB:
			//	ISP_LOG("ae _isp_ctrl_routine awb");
				rtn=_ispCfgAwbmInfo(handler_id, &isp_context_ptr->awb_stat);
				if(isp_context_ptr->af.init && \
					(3==isp_context_ptr->af.alg_id) &&\
					((ISP_AF_START == isp_context_ptr->af.status) || (ISP_AF_CONTINUE== isp_context_ptr->af.status))){
			//		ISP_LOG("ae _isp_ctrl_routine awb -- af");
					isp_context_ptr->af.awbm_flag = ISP_EB;
					isp_proc_msg.handler_id = handler_id;
					isp_proc_msg.msg_type = ISP_PROC_EVT_AF;
					rtn = _isp_proc_msg_post(&isp_proc_msg);
				}else{
					isp_proc_msg.handler_id = handler_id;
					isp_proc_msg.msg_type = ISP_PROC_EVT_AWB;
					rtn = _isp_proc_msg_post(&isp_proc_msg);
				}
				break;

			case ISP_CTRL_EVT_AF:
		//		ISP_LOG("af _isp_ctrl_routine af");
				rtn = _ispGetAfInof(handler_id, &isp_context_ptr->af_stat);
				isp_context_ptr->af.afm_flag = ISP_EB;
				isp_proc_msg.handler_id = handler_id;
				isp_proc_msg.msg_type = ISP_PROC_EVT_AF;
				rtn = _isp_proc_msg_post(&isp_proc_msg);
				break;
			case ISP_CTRL_EVT_CONTINUE_AF:
				_isp_ContinueFocusTrigeAF(handler_id);
				break;
			default:
				break;
		}

		if (0x01==isp_ctrl_msg.alloc_flag) {
			free(isp_ctrl_msg.data);
		}
		if (ISP_CLOSE == isp_system_ptr->ctrl_status) {
			break;
		}
		isp_system_ptr->ctrl_status = ISP_IDLE;

	}

	ISP_LOG("exit isp ctrl routine.");

	return NULL;

}

/* _isp_create_ctrl_thread --
*@
*@
*@ return:
*/
static int _isp_create_ctrl_thread(void)
{
	int rtn = ISP_SUCCESS;
	uint32_t handler_id = ISP_ZERO;
	struct isp_system* isp_system_ptr = ispGetSystem();
	pthread_attr_t attr;
	ISP_MSG_INIT(isp_msg);

	rtn = _isp_msg_queue_create(ISP_THREAD_QUEUE_NUM, &isp_system_ptr->ctrl_queue);
	ISP_RETURN_IF_FAIL(rtn, ("careate ctrl queue error"));

	pthread_attr_init (&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	rtn = pthread_create(&isp_system_ptr->ctrl_thr, &attr, _isp_ctrl_routine, NULL);
	ISP_RETURN_IF_FAIL(rtn, ("careate ctrl thread error"));
	pthread_attr_destroy(&attr);

	isp_msg.handler_id = ISP_ZERO;
	isp_msg.msg_type = ISP_CTRL_EVT_START;

	pthread_mutex_lock(&isp_system_ptr->cond_mutex);
	rtn = _isp_ctrl_msg_post(&isp_msg);
	ISP_RETURN_IF_FAIL(rtn, ("send msg to ctrl thread error"));

	rtn = pthread_cond_wait(&isp_system_ptr->thread_common_cond, &isp_system_ptr->cond_mutex);
	pthread_mutex_unlock(&isp_system_ptr->cond_mutex);

	return rtn;
}

/* _isp_destory_ctrl_thread --
*@
*@
*@ return:
*/
static int _isp_destory_ctrl_thread(void)
{
	int rtn = ISP_SUCCESS;
	uint32_t handler_id = ISP_ZERO;
	struct isp_system* isp_system_ptr = ispGetSystem();
	ISP_MSG_INIT(isp_msg);

	isp_msg.handler_id = ISP_ZERO;
	isp_msg.msg_type = ISP_CTRL_EVT_STOP;

	pthread_mutex_lock(&isp_system_ptr->cond_mutex);
	rtn = _isp_ctrl_msg_post(&isp_msg);

	rtn=pthread_cond_wait(&isp_system_ptr->thread_common_cond,&isp_system_ptr->cond_mutex);
	pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
	rtn=_isp_msg_queue_destroy(isp_system_ptr->ctrl_queue);
	ISP_RETURN_IF_FAIL(rtn, ("destroy ctrl queue error"));

	return rtn;
}

/* _isp_proc_routine --
*@
*@
*@ return:
*/
static void *_isp_proc_routine(void *client_data)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();
	struct isp_context* isp_context_ptr=NULL;
	ISP_MSG_INIT(isp_proc_msg);
	ISP_MSG_INIT(isp_proc_self_msg);
	ISP_MSG_INIT(isp_ctrl_msg);
	uint32_t evt=0X00;
	uint32_t handler_id=0X00;

	ISP_LOG("enter isp proc routine.");

	while (1) 	{
		rtn = _isp_proc_msg_get( &isp_proc_msg);
		if (rtn)
		{
			ISP_LOG("msg queue destroied error");
			break;
		}

		isp_system_ptr->proc_status = ISP_RUN;
		handler_id = isp_proc_msg.handler_id;
		evt = (uint32_t)(isp_proc_msg.msg_type & ISP_PROC_EVT_MASK);
		isp_context_ptr = ispGetContext(handler_id);

//		ISP_LOG("proc handler_id: %d", handler_id);

		switch (evt) {
			case ISP_PROC_EVT_START:
				isp_system_ptr->proc_status = ISP_IDLE;
				pthread_mutex_lock(&isp_system_ptr->cond_mutex);
				rtn=pthread_cond_signal(&isp_system_ptr->thread_common_cond);
				pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
				break;

			case ISP_PROC_EVT_STOP:
				isp_system_ptr->proc_status = ISP_CLOSE;
				pthread_mutex_lock(&isp_system_ptr->cond_mutex);
				rtn=pthread_cond_signal(&isp_system_ptr->thread_common_cond);
				pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
				break;

			case ISP_PROC_EVT_AE:
				//ISP_LOG("tim_ae ae calc-------");
				rtn=_ispAeCalculation(handler_id);
				break;

			case ISP_PROC_EVT_AWB:
				//rtn=_ispAwbCalculation(handler_id);
				rtn=_ispAeAwbCorrect(handler_id);
				ISP_TRACE_IF_FAIL(rtn, ("_ispAwbCalculation error"));
				if (ISP_FOCUS_CONTINUE == isp_context_ptr->af.mode) {
					rtn=_isp_ContinueFocusHandler(handler_id);
					if(ISP_SUCCESS==rtn)
					{
						isp_proc_self_msg.handler_id = handler_id;
						isp_proc_self_msg.msg_type = ISP_PROC_EVT_CONTINUE_AF;
						rtn = _isp_proc_msg_post(&isp_proc_self_msg);
					}
				}
				break;

			case ISP_PROC_EVT_AF:
				rtn=isp_af_calculation(handler_id);
				if(ISP_SUCCESS!=(rtn&ISP_AF_END_FLAG))
				{
					isp_af_end(handler_id, ISP_ZERO);

					if (ISP_FOCUS_CONTINUE == isp_context_ptr->af.mode) {
						_isp_ContinueFocusStartInternal(handler_id);
					}
				}

				if(ISP_FOCUS_CONTINUE == isp_context_ptr->af.mode){
					rtn=_isp_ContinueFocusHandler(handler_id);
					if( ISP_SUCCESS==rtn) {
						isp_proc_self_msg.handler_id = handler_id;
						isp_proc_self_msg.msg_type = ISP_PROC_EVT_CONTINUE_AF;
						rtn = _isp_proc_msg_post(&isp_proc_self_msg);
					}
				}
				break;
			case ISP_PROC_EVT_AF_STOP:
				ISP_LOG("--ISP_PROC_EVT_AF_STOP--");
				isp_af_end(handler_id, ISP_ONE);
				ISP_LOG("--ISP_PROC_EVT_AF_STOP--end");
				break;
			case ISP_PROC_EVT_CONTINUE_AF:
				rtn=isp_continue_af_calc(handler_id);
				if(rtn){
					isp_ctrl_msg.handler_id = handler_id;
					isp_ctrl_msg.msg_type = ISP_CTRL_EVT_CONTINUE_AF;
					rtn = _isp_ctrl_msg_post(&isp_ctrl_msg);
				}
				break;
			case ISP_PROC_EVT_CONTINUE_AF_STOP:

				break;
			case ISP_PROC_EVT_STOP_HANDLER:
				rtn=_isp_StopCallbackHandler(handler_id);
				break;
			default:
				break;
		}

		if (0x01 == isp_proc_msg.alloc_flag) {
			free(isp_proc_msg.data);
		}
		if (ISP_CLOSE == isp_system_ptr->proc_status) {
			break;
		}
		isp_system_ptr->proc_status = ISP_IDLE;

	}

	ISP_LOG("exit isp proc routine.");
	return NULL;

}

/* _isp_create_proc_thread --
*@
*@
*@ return:
*/
static int _isp_create_proc_thread(void)
{
	int rtn = ISP_SUCCESS;
	uint32_t handler_id = ISP_ZERO;
	struct isp_system* isp_system_ptr = ispGetSystem();
	pthread_attr_t attr;
	ISP_MSG_INIT(isp_msg);

	rtn=_isp_msg_queue_create(ISP_THREAD_QUEUE_NUM, &isp_system_ptr->proc_queue);
	ISP_RETURN_IF_FAIL(rtn, ("careate proc queue error"));

	pthread_attr_init (&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	rtn=pthread_create(&isp_system_ptr->proc_thr, &attr, _isp_proc_routine, NULL);
	ISP_RETURN_IF_FAIL(rtn, ("careate proc thread error"));
	pthread_attr_destroy(&attr);

	isp_msg.msg_type = ISP_PROC_EVT_START;

	pthread_mutex_lock(&isp_system_ptr->cond_mutex);
	rtn = _isp_proc_msg_post(&isp_msg);
	ISP_RETURN_IF_FAIL(rtn, ("send msg to proc thread error"));

	rtn = pthread_cond_wait(&isp_system_ptr->thread_common_cond, &isp_system_ptr->cond_mutex);
	pthread_mutex_unlock(&isp_system_ptr->cond_mutex);

	return rtn;
}

/* _isp_destory_proc_thread --
*@
*@
*@ return:
*/
static int _isp_destory_proc_thread(void)
{
	int rtn=ISP_SUCCESS;
	uint32_t handler_id = ISP_ZERO;
	struct isp_system* isp_system_ptr = ispGetSystem();
	ISP_MSG_INIT(isp_msg);

	isp_msg.msg_type = ISP_PROC_EVT_STOP;
	pthread_mutex_lock(&isp_system_ptr->cond_mutex);
	rtn = _isp_proc_msg_post(&isp_msg);
	ISP_RETURN_IF_FAIL(rtn, ("send msg to proc thread error"));

	rtn=pthread_cond_wait(&isp_system_ptr->thread_common_cond,&isp_system_ptr->cond_mutex);
	pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
	rtn=_isp_msg_queue_destroy(isp_system_ptr->proc_queue);
	ISP_RETURN_IF_FAIL(rtn, ("destroy proc queue error"));

	return rtn;
}

/* _isp_create_Resource --
*@
*@
*@ return:
*/
int _isp_create_Resource(void)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();

	//pthread_mutex_init (&isp_system_ptr->ctrl_mutex, NULL);
	//pthread_mutex_init (&isp_system_ptr->proc_mutex, NULL);
	pthread_mutex_init (&isp_system_ptr->cond_mutex, NULL);

	pthread_cond_init(&isp_system_ptr->init_cond, NULL);
	pthread_cond_init(&isp_system_ptr->deinit_cond, NULL);
	pthread_cond_init(&isp_system_ptr->continue_cond, NULL);
	pthread_cond_init(&isp_system_ptr->continue_stop_cond, NULL);
	pthread_cond_init(&isp_system_ptr->signal_cond, NULL);
	pthread_cond_init(&isp_system_ptr->ioctrl_cond, NULL);
	pthread_cond_init(&isp_system_ptr->thread_common_cond, NULL);

	_isp_create_ctrl_thread();
	_isp_create_proc_thread();
	_isp_create_monitor_thread();

	return rtn;
}

/* _isp_DeInitResource --
*@
*@
*@ return:
*/
int _isp_release_resource(void)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();

	_isp_destory_monitor_thread();
	_isp_destory_proc_thread();
	_isp_destory_ctrl_thread();

	//pthread_mutex_destroy(&isp_system_ptr->ctrl_mutex);
	//pthread_mutex_destroy(&isp_system_ptr->proc_mutex);
	pthread_mutex_destroy(&isp_system_ptr->cond_mutex);

	pthread_cond_destroy(&isp_system_ptr->init_cond);
	pthread_cond_destroy(&isp_system_ptr->deinit_cond);
	pthread_cond_destroy(&isp_system_ptr->continue_cond);
	pthread_cond_destroy(&isp_system_ptr->continue_stop_cond);
	pthread_cond_destroy(&isp_system_ptr->signal_cond);
	pthread_cond_destroy(&isp_system_ptr->ioctrl_cond);
	pthread_cond_destroy(&isp_system_ptr->thread_common_cond);

	return rtn;
}

/* IspGetId --
*@
*@
*@ return:
*/
uint32_t IspGetId(void)
{
	uint32_t handler_id = 0x00;
	struct isp_system* isp_system_ptr = ispGetSystem();

	return isp_system_ptr->isp_id;
}

// public

/* isp_init --
*@
*@
*@ return:
*/

int isp_init_param_trace(uint32_t handler_id, struct isp_init_param* ptr)
{
	int rtn=ISP_SUCCESS;
	struct sensor_raw_info* raw_info_ptr=(struct sensor_raw_info*)ptr->setting_param_ptr;
	struct sensor_raw_tune_info* raw_tune_ptr=(struct sensor_raw_tune_info*)raw_info_ptr->tune_ptr;
	struct sensor_raw_fix_info* raw_fix_ptr=(struct sensor_raw_fix_info*)raw_info_ptr->fix_ptr;
	struct sensor_raw_resolution_info_tab* resolution_ptr=(struct sensor_raw_resolution_info_tab*)raw_info_ptr->resolution_info_ptr;

	//ISP_LOG("raw_info_ptr 0x%08x", raw_info_ptr);
	//ISP_LOG("raw_tune_ptr 0x%08x", raw_tune_ptr);
	//ISP_LOG("raw_fix_ptr 0x%08x", raw_fix_ptr);
	ISP_LOG("param version_id 0x%08x", raw_info_ptr->version_info->version_id);
	ISP_LOG("param_id 0x%08x", raw_info_ptr->tune_ptr->version_id);
	ISP_LOG("image_pattern 0x%02x", resolution_ptr->image_pattern);

	return rtn;
}

int isp_ctrl_init(uint32_t handler_id, struct isp_init_param* ptr)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = NULL;
	ISP_MSG_INIT(isp_msg);

	ISP_LOG("--isp_init--chip id:0x%08x, soft id:0x%08x", ptr->isp_id, ISP_SOFT_ID);

	isp_init_param_trace(handler_id, ptr);

	rtn = _isp_CtrlLock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl lock error"));

	rtn = ispInitContext();
	ISP_RETURN_IF_FAIL(rtn, ("init isp context error"));

	isp_system_ptr = ispGetSystem();

	if(ISP_MAX_HANDLE_NUM <= isp_system_ptr->handler_num) {
		ISP_LOG("handler num: 0x%x error", isp_system_ptr->handler_num);
		rtn = ISP_ERROR;
		goto EXIT;
	}

	isp_system_ptr->handler_num++;

	rtn = _isp_check_init_param(handler_id, ptr);
	ISP_RETURN_IF_FAIL(rtn, ("check init param error"));

	if(ISP_ONE == isp_system_ptr->handler_num) {
		rtn = _isp_create_Resource();
		ISP_RETURN_IF_FAIL(rtn, ("create resource error"));
	}

	isp_msg.data = malloc(sizeof(struct isp_init_param));
	memcpy(isp_msg.data, ptr, sizeof(struct isp_init_param));
	isp_msg.alloc_flag = 1;
	isp_msg.handler_id = handler_id;
	isp_msg.msg_type = ISP_CTRL_EVT_INIT;

	pthread_mutex_lock(&isp_system_ptr->cond_mutex);

	rtn = _isp_ctrl_msg_post(&isp_msg);
	ISP_RETURN_IF_FAIL(rtn, ("send msg to ctrl thread error"));

	rtn = pthread_cond_wait(&isp_system_ptr->init_cond, &isp_system_ptr->cond_mutex);
	pthread_mutex_unlock(&isp_system_ptr->cond_mutex);

	ISP_RETURN_IF_FAIL(rtn, ("pthread_cond_wait error"));

	EXIT :

	rtn = _isp_CtrlUnlock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl unlock error"));

	ISP_LOG("---isp_init-- end");

	return rtn;
}

/* isp_deinit --
*@
*@
*@ return:
*/
int isp_ctrl_deinit(uint32_t handler_id)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();
	ISP_MSG_INIT(isp_msg);

	ISP_LOG("--isp_deinit--");

	// get mutex
	rtn = _isp_CtrlLock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl lock error"));

	if (ISP_ONE > isp_system_ptr->handler_num) {
		ISP_LOG("handler num: 0x%x error", isp_system_ptr->handler_num);
		rtn = ISP_ERROR;
		goto EXIT;
	}

	isp_system_ptr->handler_num--;

	isp_msg.handler_id = handler_id;
	isp_msg.msg_type = ISP_CTRL_EVT_DEINIT;
	isp_msg.alloc_flag = 0x00;

	// close hw isp
	pthread_mutex_lock(&isp_system_ptr->cond_mutex);
	rtn = _isp_ctrl_msg_post(&isp_msg);
	ISP_RETURN_IF_FAIL(rtn, ("send msg to ctrl thread error"));

	rtn = pthread_cond_wait(&isp_system_ptr->deinit_cond, &isp_system_ptr->cond_mutex);
	pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
	ISP_RETURN_IF_FAIL(rtn, ("pthread_cond_wait error"));

	if (ISP_ZERO== isp_system_ptr->handler_num) {
		rtn = _isp_release_resource();
		ISP_RETURN_IF_FAIL(rtn, ("_isp_release_resource error"));
	}

	//rtn = ispDeinitContext();
	//ISP_RETURN_IF_FAIL(rtn, ("denit isp context error"));

	EXIT:

	rtn = _isp_CtrlUnlock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl unlock error"));

	ISP_LOG("---isp_deinit------- end");

	return rtn;
}

/* isp_capability --
*@
*@
*@ return:
*/
int isp_ctrl_capability(uint32_t handler_id, enum isp_capbility_cmd cmd, void* param_ptr)
{
	int rtn=ISP_SUCCESS;

	switch(cmd)
	{
		case ISP_VIDEO_SIZE:
		{
			struct isp_video_limit* size_ptr=param_ptr;
			rtn = ispGetContinueSize(handler_id, &size_ptr->width, &size_ptr->height);
			break;
		}
		case ISP_CAPTURE_SIZE:
		{
			struct isp_video_limit* size_ptr=param_ptr;
			rtn = ispGetSignalSize(handler_id, &size_ptr->width, &size_ptr->height);
			break;
		}
		case ISP_LOW_LUX_EB:
		{
			rtn = _ispGetAeIndexIsMax(handler_id, (uint32_t*)param_ptr);
			break;
		}
		case ISP_CUR_ISO:
		{
			rtn = isp_ae_get_iso(handler_id, (uint32_t*)param_ptr);
			break;
		}
		case ISP_DENOISE_LEVEL:
		{
			rtn = isp_ae_get_denosie_level(handler_id, (uint32_t*)param_ptr);
			break;
		}
		default :
		{
			break;
		}
	}

	return rtn;
}

/* isp_ioctl --
*@
*@
*@ return:
*/
int isp_ctrl_ioctl(uint32_t handler_id, enum isp_ctrl_cmd cmd, void* param_ptr)
{
	int rtn=ISP_SUCCESS;
	struct isp_respond respond;
	struct isp_context* isp_context_ptr = ispGetContext(handler_id);
	struct isp_system* isp_system_ptr = ispGetSystem();
	ISP_MSG_INIT(isp_msg);
	uint32_t callback_flag=cmd&0x80000000;

	//ISP_LOG("--isp_ioctl--cmd:0x%x", cmd);

	rtn = _isp_CtrlLock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl lock error"));

	respond.rtn=ISP_SUCCESS;
	isp_msg.handler_id = handler_id;
	isp_msg.msg_type = ISP_CTRL_EVT_IOCTRL;
	isp_msg.sub_msg_type = cmd;
	isp_msg.data=(void*)param_ptr;
	isp_msg.alloc_flag=0x00;
	isp_msg.respond=(void*)&respond;

	pthread_mutex_lock(&isp_system_ptr->cond_mutex);
	rtn = _isp_ctrl_msg_post(&isp_msg);
	ISP_RETURN_IF_FAIL(rtn, ("send msg to ctrl thread error"));

	rtn = pthread_cond_wait(&isp_system_ptr->ioctrl_cond, &isp_system_ptr->cond_mutex);
	pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
	ISP_RETURN_IF_FAIL(rtn, ("pthread_cond_wait error"));

	rtn=respond.rtn;

	if((0x00==callback_flag)
		&&(PNULL!=isp_context_ptr->cfg.callback))
	{
		isp_context_ptr->cfg.callback(handler_id, ISP_CALLBACK_EVT|ISP_CTRL_CALLBAC|cmd, NULL, ISP_ZERO);
	}
	else
	{/*isp tuning tool callback*/
		rtn=respond.rtn;
	}

	rtn = _isp_CtrlUnlock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl unlock error"));

	return rtn;
}

int isp_video_start_param_trace(uint32_t handler_id, struct isp_video_start* ptr)
{
	int rtn=ISP_SUCCESS;

	ISP_LOG("isp_video_start w:%d, h:%d", ptr->size.w, ptr->size.h);
	ISP_LOG("isp_video_start format:0x%x", ptr->format);

	return rtn;
}

/* isp_video_start --
*@
*@
*@ return:
*/
int isp_ctrl_video_start(uint32_t handler_id, struct isp_video_start* param_ptr)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();
	ISP_MSG_INIT(isp_msg);

	ISP_LOG("--isp_video_start--");

	rtn = _isp_CtrlLock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl lock error"));

	isp_video_start_param_trace(handler_id, param_ptr);

	rtn=_isp_check_video_param(handler_id, param_ptr);
	ISP_RETURN_IF_FAIL(rtn, ("check param error"));

	isp_msg.data = malloc(sizeof(struct isp_video_start));
	memcpy(isp_msg.data, param_ptr, sizeof(struct isp_video_start));
	isp_msg.alloc_flag=0x01;
	isp_msg.handler_id = handler_id;
	isp_msg.msg_type = ISP_CTRL_EVT_CONTINUE;
	isp_msg.sub_msg_type;
	//isp_msg.data=(void*)param_ptr;

	pthread_mutex_lock(&isp_system_ptr->cond_mutex);

	rtn = _isp_ctrl_msg_post(&isp_msg);
	ISP_RETURN_IF_FAIL(rtn, ("send msg to ctrl thread error"));

	rtn = pthread_cond_wait(&isp_system_ptr->continue_cond, &isp_system_ptr->cond_mutex);
	pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
	ISP_RETURN_IF_FAIL(rtn, ("pthread_cond_wait error"));

	rtn = _isp_CtrlUnlock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl unlock error"));

	ISP_LOG("---isp_video_start-- end");

	return rtn;
}

/* isp_video_start --
*@
*@
*@ return:
*/
int isp_ctrl_video_stop(uint32_t handler_id)
{
	int rtn=ISP_SUCCESS;
	struct isp_system* isp_system_ptr = ispGetSystem();
	ISP_MSG_INIT(isp_msg);

	ISP_LOG("--isp_video_stop--");

	rtn = _isp_CtrlLock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl lock error"));

	isp_msg.handler_id = handler_id;
	isp_msg.msg_type = ISP_CTRL_EVT_CONTINUE_STOP;
	isp_msg.sub_msg_type;
	isp_msg.data=NULL;
	isp_msg.alloc_flag=0x00;

	pthread_mutex_lock(&isp_system_ptr->cond_mutex);
	rtn = _isp_ctrl_msg_post(&isp_msg);
	ISP_RETURN_IF_FAIL(rtn, ("send msg to ctrl thread error"));

	rtn = pthread_cond_wait(&isp_system_ptr->continue_stop_cond, &isp_system_ptr->cond_mutex);
	pthread_mutex_unlock(&isp_system_ptr->cond_mutex);
	ISP_RETURN_IF_FAIL(rtn, ("pthread_cond_wait error"));

	rtn = _isp_CtrlUnlock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl unlock error"));

	ISP_LOG("--isp_video_stop--end");

	return rtn;
}

int isp_proc_param_trace(uint32_t handler_id, struct ips_in_param* ptr)
{
	int rtn=ISP_SUCCESS;

	ISP_LOG("src image_format 0x%x", ptr->src_frame.img_fmt);
	ISP_LOG("src img_size: %d, %d", ptr->src_frame.img_size.w, ptr->src_frame.img_size.h);
	ISP_LOG("src addr:0x%x", ptr->src_frame.img_addr_phy.chn0);

	ISP_LOG("dst image_format 0x%x", ptr->dst_frame.img_fmt);
	ISP_LOG("dst img_size: %d, %d", ptr->dst_frame.img_size.w, ptr->dst_frame.img_size.h);
	ISP_LOG("dst addr:y=0x%x, uv=0x%x", ptr->dst_frame.img_addr_phy.chn0, ptr->dst_frame.img_addr_phy.chn1);

	ISP_LOG("src_avail_height:%d", ptr->src_avail_height);
	ISP_LOG("src_slice_height:%d", ptr->src_slice_height);
	ISP_LOG("dst_slice_height:%d", ptr->dst_slice_height);

	return rtn;
}

/* isp_proc_start --
*@
*@
*@ return:
*/
int isp_ctrl_proc_start(uint32_t handler_id, struct ips_in_param* in_param_ptr, struct ips_out_param* out_param_ptr)
{
	int rtn=ISP_SUCCESS;
	ISP_MSG_INIT(isp_msg);

	ISP_LOG("--isp_proc_start--");

	rtn = _isp_CtrlLock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl lock error"));

	isp_proc_param_trace(handler_id, in_param_ptr);

	rtn=_isp_check_proc_param(handler_id, in_param_ptr);
	ISP_RETURN_IF_FAIL(rtn, ("check init param error"));

	isp_msg.handler_id = handler_id;
	isp_msg.msg_type = ISP_CTRL_EVT_SIGNAL;
	isp_msg.sub_msg_type;
	isp_msg.data = malloc(sizeof(struct ips_in_param));
	memcpy(isp_msg.data, in_param_ptr, sizeof(struct ips_in_param));
	isp_msg.alloc_flag=0x01;

	rtn = _isp_ctrl_msg_post(&isp_msg);
	ISP_RETURN_IF_FAIL(rtn, ("send msg to ctrl thread error"));

	rtn = _isp_CtrlUnlock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl unlock error"));

	ISP_LOG("--isp_proc_start--end");

	return rtn;
}

/* isp_proc_next --
*@
*@
*@ return:
*/
int isp_ctrl_proc_next(uint32_t handler_id, struct ipn_in_param* in_ptr, struct ips_out_param *out_ptr)
{
	int rtn = ISP_SUCCESS;
	ISP_MSG_INIT(isp_msg);

	ISP_LOG("--isp_proc_next--");

	rtn = _isp_CtrlLock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl lock error"));

	rtn=_isp_check_proc_next_param(handler_id, in_ptr);
	ISP_RETURN_IF_FAIL(rtn, ("check init param error"));

	isp_msg.handler_id = handler_id;
	isp_msg.msg_type = ISP_CTRL_EVT_SIGNAL_NEXT;
	isp_msg.sub_msg_type;
	isp_msg.data = malloc(sizeof(struct ipn_in_param));
	memcpy(isp_msg.data, in_ptr, sizeof(struct ipn_in_param));
	isp_msg.alloc_flag=0x01;

	rtn = _isp_ctrl_msg_post(&isp_msg);
	ISP_RETURN_IF_FAIL(rtn, ("send msg to ctrl thread error"));

	rtn = _isp_CtrlUnlock();
	ISP_RETURN_IF_FAIL(rtn, ("ctrl unlock error"));

	ISP_LOG("--isp_proc_next--end");

	return rtn;
}

/* isp_change_param --
*@
*@
*@ return:
*/
int isp_change_param(uint32_t handler_id, enum isp_change_cmd cmd)
{
	int rtn = ISP_SUCCESS;
	struct isp_context* isp_context_ptr = ispGetAlgContext(handler_id);
	uint32_t param_index=isp_context_ptr->param_index-ISP_ONE;
	uint32_t awb_index=isp_context_ptr->awb.cur_index;

	switch(cmd)
	{
		case ISP_CHANGE_LNC:
		{
			ISP_LOG("--isp_change_param--p%d, a:%d", param_index, awb_index);
			rtn=ispSetLncParam(handler_id, isp_context_ptr->lnc_map_tab[param_index][awb_index].param_addr, isp_context_ptr->lnc_map_tab[param_index][awb_index].len);
			rtn=_ispSetLncParam(&isp_context_ptr->lnc, isp_context_ptr);
			rtn=_ispLncParamSet(handler_id, &isp_context_ptr->lnc);
			rtn=_ispLncParamLoad(handler_id, &isp_context_ptr->lnc);
			isp_context_ptr->tune.lnc_load=ISP_EB;
			break ;
		}
		case ISP_CHANGE_CMC:
		{
			isp_context_ptr->cmc.matrix[0]=isp_context_ptr->cmc_tab[awb_index][0];
			isp_context_ptr->cmc.matrix[1]=isp_context_ptr->cmc_tab[awb_index][1];
			isp_context_ptr->cmc.matrix[2]=isp_context_ptr->cmc_tab[awb_index][2];
			isp_context_ptr->cmc.matrix[3]=isp_context_ptr->cmc_tab[awb_index][3];
			isp_context_ptr->cmc.matrix[4]=isp_context_ptr->cmc_tab[awb_index][4];
			isp_context_ptr->cmc.matrix[5]=isp_context_ptr->cmc_tab[awb_index][5];
			isp_context_ptr->cmc.matrix[6]=isp_context_ptr->cmc_tab[awb_index][6];
			isp_context_ptr->cmc.matrix[7]=isp_context_ptr->cmc_tab[awb_index][7];
			isp_context_ptr->cmc.matrix[8]=isp_context_ptr->cmc_tab[awb_index][8];
			isp_context_ptr->tune.cmc=ISP_EB;
			break ;
		}
		default:
		{
			break ;
		}

	}

	return rtn;
}

/* isp_set_gamma --
*@
*@
*@ return:
*/
int32_t isp_set_gamma(struct isp_gamma_param* gamma, struct isp_gamma_tab* tab_ptr)
{
	int32_t rtn=ISP_SUCCESS;

	gamma->axis[0][0]=tab_ptr->axis[0][0];
	gamma->axis[0][1]=tab_ptr->axis[0][1];
	gamma->axis[0][2]=tab_ptr->axis[0][2];
	gamma->axis[0][3]=tab_ptr->axis[0][3];
	gamma->axis[0][4]=tab_ptr->axis[0][4];
	gamma->axis[0][5]=tab_ptr->axis[0][5];
	gamma->axis[0][6]=tab_ptr->axis[0][6];
	gamma->axis[0][7]=tab_ptr->axis[0][7];
	gamma->axis[0][8]=tab_ptr->axis[0][8];
	gamma->axis[0][9]=tab_ptr->axis[0][9];
	gamma->axis[0][10]=tab_ptr->axis[0][10];
	gamma->axis[0][11]=tab_ptr->axis[0][11];
	gamma->axis[0][12]=tab_ptr->axis[0][12];
	gamma->axis[0][13]=tab_ptr->axis[0][13];
	gamma->axis[0][14]=tab_ptr->axis[0][14];
	gamma->axis[0][15]=tab_ptr->axis[0][15];
	gamma->axis[0][16]=tab_ptr->axis[0][16];
	gamma->axis[0][17]=tab_ptr->axis[0][17];
	gamma->axis[0][18]=tab_ptr->axis[0][18];
	gamma->axis[0][19]=tab_ptr->axis[0][19];
	gamma->axis[0][20]=tab_ptr->axis[0][20];
	gamma->axis[0][21]=tab_ptr->axis[0][21];
	gamma->axis[0][22]=tab_ptr->axis[0][22];
	gamma->axis[0][23]=tab_ptr->axis[0][23];
	gamma->axis[0][24]=tab_ptr->axis[0][24];
	gamma->axis[0][25]=tab_ptr->axis[0][25];

	gamma->axis[1][0]=tab_ptr->axis[1][0];
	gamma->axis[1][1]=tab_ptr->axis[1][1];
	gamma->axis[1][2]=tab_ptr->axis[1][2];
	gamma->axis[1][3]=tab_ptr->axis[1][3];
	gamma->axis[1][4]=tab_ptr->axis[1][4];
	gamma->axis[1][5]=tab_ptr->axis[1][5];
	gamma->axis[1][6]=tab_ptr->axis[1][6];
	gamma->axis[1][7]=tab_ptr->axis[1][7];
	gamma->axis[1][8]=tab_ptr->axis[1][8];
	gamma->axis[1][9]=tab_ptr->axis[1][9];
	gamma->axis[1][10]=tab_ptr->axis[1][10];
	gamma->axis[1][11]=tab_ptr->axis[1][11];
	gamma->axis[1][12]=tab_ptr->axis[1][12];
	gamma->axis[1][13]=tab_ptr->axis[1][13];
	gamma->axis[1][14]=tab_ptr->axis[1][14];
	gamma->axis[1][15]=tab_ptr->axis[1][15];
	gamma->axis[1][16]=tab_ptr->axis[1][16];
	gamma->axis[1][17]=tab_ptr->axis[1][17];
	gamma->axis[1][18]=tab_ptr->axis[1][18];
	gamma->axis[1][19]=tab_ptr->axis[1][19];
	gamma->axis[1][20]=tab_ptr->axis[1][20];
	gamma->axis[1][21]=tab_ptr->axis[1][21];
	gamma->axis[1][22]=tab_ptr->axis[1][22];
	gamma->axis[1][23]=tab_ptr->axis[1][23];
	gamma->axis[1][24]=tab_ptr->axis[1][24];
	gamma->axis[1][25]=tab_ptr->axis[1][25];

	gamma->index[0]=_ispLog2n(gamma->axis[0][1]-gamma->axis[0][0]);
	gamma->index[1]=_ispLog2n(gamma->axis[0][2]-gamma->axis[0][1]);
	gamma->index[2]=_ispLog2n(gamma->axis[0][3]-gamma->axis[0][2]);
	gamma->index[3]=_ispLog2n(gamma->axis[0][4]-gamma->axis[0][3]);
	gamma->index[4]=_ispLog2n(gamma->axis[0][5]-gamma->axis[0][4]);
	gamma->index[5]=_ispLog2n(gamma->axis[0][6]-gamma->axis[0][5]);
	gamma->index[6]=_ispLog2n(gamma->axis[0][7]-gamma->axis[0][6]);
	gamma->index[7]=_ispLog2n(gamma->axis[0][8]-gamma->axis[0][7]);
	gamma->index[8]=_ispLog2n(gamma->axis[0][9]-gamma->axis[0][8]);
	gamma->index[9]=_ispLog2n(gamma->axis[0][10]-gamma->axis[0][9]);
	gamma->index[10]=_ispLog2n(gamma->axis[0][11]-gamma->axis[0][10]);
	gamma->index[11]=_ispLog2n(gamma->axis[0][12]-gamma->axis[0][11]);
	gamma->index[12]=_ispLog2n(gamma->axis[0][13]-gamma->axis[0][12]);
	gamma->index[13]=_ispLog2n(gamma->axis[0][14]-gamma->axis[0][13]);
	gamma->index[14]=_ispLog2n(gamma->axis[0][15]-gamma->axis[0][14]);
	gamma->index[15]=_ispLog2n(gamma->axis[0][16]-gamma->axis[0][15]);
	gamma->index[16]=_ispLog2n(gamma->axis[0][17]-gamma->axis[0][16]);
	gamma->index[17]=_ispLog2n(gamma->axis[0][18]-gamma->axis[0][17]);
	gamma->index[18]=_ispLog2n(gamma->axis[0][19]-gamma->axis[0][18]);
	gamma->index[19]=_ispLog2n(gamma->axis[0][20]-gamma->axis[0][19]);
	gamma->index[20]=_ispLog2n(gamma->axis[0][21]-gamma->axis[0][20]);
	gamma->index[21]=_ispLog2n(gamma->axis[0][22]-gamma->axis[0][21]);
	gamma->index[22]=_ispLog2n(gamma->axis[0][23]-gamma->axis[0][22]);
	gamma->index[23]=_ispLog2n(gamma->axis[0][24]-gamma->axis[0][23]);
	gamma->index[24]=_ispLog2n(gamma->axis[0][25]-gamma->axis[0][24]+1);

	return rtn;
}


int32_t ispAfmEb(uint32_t handler_id)
{

	return _ispAfmEb(handler_id);
}

int32_t ispCfgAwbm(uint32_t handler_id, struct isp_awbm_param* param_ptr)
{

	return _ispCfgAwbm(handler_id, param_ptr);
}

int32_t ispAwbmEb_immediately(uint32_t handler_id)
{
	int32_t rtn=ISP_SUCCESS;
	uint32_t isp_id=IspGetId();
	struct isp_context* isp_context_ptr=ispGetContext(handler_id);

	if(SC8825_ISP_ID==isp_id){
		isp_context_ptr->ae.monitor_conter = 0;
	}else if(SC8830_ISP_ID==isp_id){
		ispAwbmBypass(handler_id, ISP_UEB);
		ispAwbmSkip(handler_id, 0);
	}
	return rtn;
}

/**---------------------------------------------------------------------------*/
