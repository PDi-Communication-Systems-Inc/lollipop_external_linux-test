/*
 * Copyright 2007-2015 Freescale Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*
 * @file mxc_v4l2_tvin.c
 *
 * @brief Mxc TVIN For Linux 2 driver test application
 *
 */

#ifdef __cplusplus
extern "C"{
#endif

/*=======================================================================
										INCLUDE FILES
=======================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <math.h>
#include <string.h>
#include <malloc.h>
#include <sys/time.h>

#include <linux/mxcfb.h>
#include <linux/mxc_v4l2.h>
#include <linux/ipu.h>

#define TFAIL -1
#define TPASS 0
//#define DEBUG_CAM_APP   1
#ifdef DEBUG_CAM_APP
    #define PRDEBUG printf
#else
    #define PRDEBUG
#endif

#define NUMBER_BUFFERS    8

char v4l_capture_dev[100] = "/dev/video0";
#ifdef BUILD_FOR_ANDROID
char fb_display_dev[100] = "/dev/graphics/fb1";
char fb_display_bg_dev[100] = "/dev/graphics/fb0";
#else
char fb_display_dev[100] = "/dev/fb1";
char fb_display_bg_dev[100] = "/dev/fb0";
#endif
int fd_capture_v4l = 0;
int fd_fb_display = 0;
int fd_ipu = 0;
unsigned char * g_fb_display = NULL;
int g_input = 1;

//#define NUMBER_DISPLAY_BUFFERS 8
#define NUMBER_DISPLAY_BUFFERS 3

int g_display_num_buffers = NUMBER_DISPLAY_BUFFERS;
int g_capture_num_buffers = NUMBER_BUFFERS;
int g_camera_width = 0;
int g_camera_height = 0;
int g_in_fmt = V4L2_PIX_FMT_UYVY;
int g_display_width = 0;
int g_display_height = 0;
int g_display_top = 0;
int g_display_left = 0;
int g_display_fmt = V4L2_PIX_FMT_RGB565; //V4L2_PIX_FMT_UYVY;
int g_display_base_phy;;
int g_display_size;
int g_display_fg = 1;
int g_display_id = 1;
struct fb_var_screeninfo g_screen_info;
int g_frame_count = 0x7FFFFFFF;
int g_frame_size;
int g_mem_type = V4L2_MEMORY_MMAP;
int g_fb0_activate_force=0;
int g_delay = 0;
struct testbuffer
{
	unsigned char *start;
	size_t offset;
	unsigned int length;
};


struct testbuffer display_buffers[NUMBER_DISPLAY_BUFFERS];
struct testbuffer capture_buffers[NUMBER_BUFFERS];

#include <signal.h>
#include <pthread.h>
int g_rotate = 0;
int g_vflip = 0;
int g_hflip = 0;
int g_frame_period = 33333;
int g_alpha = 0;
int g_colorkey = 0;
v4l2_std_id g_current_std = V4L2_STD_NTSC;
int g_ctrl_c_flag = 0;
struct timeval g_start_time, g_end_time;
int g_do_print_end_time = 0;
int g_background=0;

int start_capturing(void)
{
	int i;
	struct v4l2_buffer buf;
	enum v4l2_buf_type type;

	for (i = 0; i < g_capture_num_buffers; i++) {
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = g_mem_type;
		buf.index = i;
		
		if (g_mem_type == V4L2_MEMORY_USERPTR) {
			buf.length = capture_buffers[i].length;
			buf.m.userptr = (unsigned long)capture_buffers[i].offset;
		}
		
	    if (ioctl(fd_capture_v4l, VIDIOC_QUERYBUF, &buf) < 0) {
            printf("VIDIOC_QUERYBUF error\n");
            return TFAIL;
	    }

		if (g_mem_type == V4L2_MEMORY_MMAP) {
			printf("%d) offset=%08x length=%d\n",i, buf.m.offset, buf.length);
            capture_buffers[i].length = buf.length;
            capture_buffers[i].offset = (size_t) buf.m.offset;
            capture_buffers[i].start = mmap(NULL, capture_buffers[i].length,
					                PROT_READ | PROT_WRITE, MAP_SHARED,
					                fd_capture_v4l, capture_buffers[i].offset);
			memset(capture_buffers[i].start, 0xFF, capture_buffers[i].length);
	    }
	}

	for (i = 0; i < g_capture_num_buffers; i++) {
		memset(&buf, 0, sizeof (buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = g_mem_type;
		buf.index = i;
		if (g_mem_type == V4L2_MEMORY_USERPTR)
			buf.m.offset = (unsigned int)capture_buffers[i].start;
		else
			buf.m.offset = capture_buffers[i].offset;
		buf.length = capture_buffers[i].length;
		if (ioctl(fd_capture_v4l, VIDIOC_QBUF, &buf) < 0) {
			printf("VIDIOC_QBUF error\n");
			return TFAIL;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl (fd_capture_v4l, VIDIOC_STREAMON, &type) < 0) {
		printf("VIDIOC_STREAMON error\n");
		return TFAIL;
	}

	return TPASS;
}



int prepare_display_buffers(void)
{
	int i;
	//int display_size = (1376*768*g_screen_info.bits_per_pixel / 8);
	//int display_size = ((1376*768*g_screen_info.bits_per_pixel / 8) *2);
	int display_size = (1376*768*2);
	//g_display_size = g_screen_info.xres * g_screen_info.yres * g_screen_info.bits_per_pixel / 8;
	//int display_size = g_display_size;

	g_fb_display = (unsigned char *)mmap(0, display_size * g_display_num_buffers, 
		PROT_READ | PROT_WRITE, MAP_SHARED, fd_fb_display, 0);
	if (g_fb_display == NULL) {
		printf("v4l2 tvin test: display mmap failed\n");
		return TFAIL;
	}

	for (i = 0; i < g_display_num_buffers; i++) {
		display_buffers[i].length = display_size;
		display_buffers[i].offset = g_display_base_phy + display_size * i;
		display_buffers[i].start = g_fb_display + (display_size * i);
	}

	return TPASS;
}

int v4l_capture_setup(void)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	struct v4l2_streamparm parm;
	unsigned int min;
	struct v4l2_control ctl;
	struct v4l2_dbg_chip_ident chip;
	v4l2_std_id id=0;

	if (ioctl (fd_capture_v4l, VIDIOC_QUERYCAP, &cap) < 0) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s is no V4L2 device\n",
					v4l_capture_dev);
			return TFAIL;
		} else {
			fprintf (stderr, "%s isn not V4L device,unknow error\n",
			v4l_capture_dev);
			return TFAIL;
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf (stderr, "%s is no video capture device\n",
			v4l_capture_dev);
		return TFAIL;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf (stderr, "%s does not support streaming i/o\n",
			v4l_capture_dev);
		return TFAIL;
	}

	if (ioctl(fd_capture_v4l, VIDIOC_DBG_G_CHIP_IDENT, &chip))
	{
		printf("VIDIOC_DBG_G_CHIP_IDENT failed.\n");
	}
	else
	{
		printf("TV decoder chip is %s\n", chip.match.name);
	}


	memset(&fmt, 0, sizeof(fmt));
	
	if (ioctl(fd_capture_v4l, VIDIOC_S_INPUT, &g_input) < 0) {
		printf("VIDIOC_S_INPUT failed\n");
		close(fd_capture_v4l);
		return TFAIL;
	}

	if (ioctl(fd_capture_v4l, VIDIOC_G_STD, &id) < 0)
	{
		printf("VIDIOC_G_STD failed\n");
	}
	else
	{
		/* Don't override default format */
		//id = g_current_std;
		g_current_std = id;
		printf("g_current_std = 0x%x.\r\n", g_current_std);

		if(id==V4L2_STD_ALL)
		{
			printf("\n Camera is not there\n");
		}
		else
		{
			printf("\n Camera is there \n");
		}

		if (ioctl(fd_capture_v4l, VIDIOC_S_STD, &id) < 0)
		{
			printf("VIDIOC_S_STD failed\n");
			close(fd_capture_v4l);
			return TFAIL;
		}
	}

	memset(&cropcap, 0, sizeof(cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl (fd_capture_v4l, VIDIOC_CROPCAP, &cropcap) < 0) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (ioctl (fd_capture_v4l, VIDIOC_S_CROP, &crop) < 0) {
			switch (errno) {
				case EINVAL:
					/* Cropping not supported. */
					fprintf (stderr, "%s  doesn't support crop\n",
						v4l_capture_dev);
					break;
				default:
					/* Errors ignored. */
					break;
			}
		}
	} else {
		/* Errors ignored. */
	}

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = 0;
	parm.parm.capture.capturemode = 0;
	if (ioctl(fd_capture_v4l, VIDIOC_S_PARM, &parm) < 0) {
		printf("VIDIOC_S_PARM failed\n");
		close(fd_capture_v4l);
		return TFAIL;
	}

	memset(&fmt, 0, sizeof(fmt));

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = 0;
	fmt.fmt.pix.height = 0;
    printf("Pixel Format %x for PAC \n", g_in_fmt);
	fmt.fmt.pix.pixelformat = g_in_fmt;
	fmt.fmt.pix.field = V4L2_FIELD_ANY;
	
	if (ioctl (fd_capture_v4l, VIDIOC_S_FMT, &fmt) < 0) {
		fprintf (stderr, "%s iformat not supported \n",
			v4l_capture_dev);
		return TFAIL;
	}

	/* Note VIDIOC_S_FMT may change width and height. */

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;

	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	if (ioctl(fd_capture_v4l, VIDIOC_G_FMT, &fmt) < 0) {
		printf("VIDIOC_G_FMT failed\n");
		close(fd_capture_v4l);
		return TFAIL;
	}

	g_camera_width = fmt.fmt.pix.width;
	g_camera_height = fmt.fmt.pix.height;


	printf("+++++g_camera_width = %d, g_camera_height = %d.\r\n", g_camera_width, g_camera_height);


	memset(&req, 0, sizeof (req));
	req.count = g_capture_num_buffers;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = g_mem_type;
	if (ioctl (fd_capture_v4l, VIDIOC_REQBUFS, &req) < 0) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s does not support "
					 "memory mapping\n", v4l_capture_dev);
			return TFAIL;
		} else {
			fprintf (stderr, "%s does not support "
					 "memory mapping, unknow error\n", v4l_capture_dev);
			return TFAIL;
		}
	}

	if (req.count < 2) {
		fprintf (stderr, "Insufficient buffer memory on %s\n",
			 v4l_capture_dev);
		return TFAIL;
	}

	return TPASS;
}

int display_bg_info(void)
{
	int fd_fb_display;
	struct fb_fix_screeninfo fb_fix;

	if ((fd_fb_display = open(fb_display_bg_dev, O_RDWR, 0)) < 0) {
		printf("Unable to open %s\n", fb_display_dev);
		close(fd_ipu);
		close(fd_capture_v4l);
		return TFAIL;
	}

	if (ioctl(fd_fb_display, FBIOGET_VSCREENINFO, &g_screen_info) < 0) {
		printf("fb_display_setup FBIOGET_VSCREENINFO failed\n");
		return TFAIL;
	}

	if (ioctl(fd_fb_display, FBIOGET_FSCREENINFO, &fb_fix) < 0) {
		printf("fb_display_setup FBIOGET_FSCREENINFO failed\n");
		return TFAIL;
	}

	printf("*** %s xres=%d yres=%d xvirtual=%d yvirtual=%d bits_per_pixel=%d fmt=%08x\n",
			fb_display_bg_dev,
			g_screen_info.xres,
			g_screen_info.yres,
			g_screen_info.xres_virtual,
			g_screen_info.yres_virtual,
			g_screen_info.bits_per_pixel,
			g_screen_info.nonstd);

	close(fd_fb_display);

	return 0;
}

int fb_display_setup(void)
{
	int fd_fb_bg = 0;
	struct mxcfb_gbl_alpha alpha;
	struct mxcfb_color_key key;
	char node[8];
	struct fb_fix_screeninfo fb_fix;
	struct mxcfb_pos pos;

	if (ioctl(fd_fb_display, FBIOGET_VSCREENINFO, &g_screen_info) < 0) {
		printf("fb_display_setup FBIOGET_VSCREENINFO failed\n");
		return TFAIL;
	}

	if (ioctl(fd_fb_display, FBIOGET_FSCREENINFO, &fb_fix) < 0) {
		printf("fb_display_setup FBIOGET_FSCREENINFO failed\n");
		return TFAIL;
	}

	printf("==== fb_fix.id = %s.\r\n", fb_fix.id);
	if ((strcmp(fb_fix.id, "DISP4 FG") == 0) || (strcmp(fb_fix.id, "DISP3 FG") == 0)) {
		g_display_fg = 1;
		pos.x = g_display_left;
		pos.y = g_display_top;
		if (ioctl(fd_fb_display, MXCFB_SET_OVERLAY_POS, &pos) < 0) {
			printf("fb_display_setup MXCFB_SET_OVERLAY_POS failed\n");
			return TFAIL;
		}

		g_screen_info.xres = g_display_width;
		g_screen_info.yres = g_display_height;
		g_screen_info.xres_virtual = g_display_width;
		g_screen_info.yres_virtual = g_screen_info.yres * g_display_num_buffers;
		g_screen_info.bits_per_pixel = 16;
		g_screen_info.nonstd = g_display_fmt;
		printf("++++++g_display_fmt=%08x g_screen_info.bits_per_pixel=%d",g_display_fmt,g_screen_info.bits_per_pixel);
		if (ioctl(fd_fb_display, FBIOPUT_VSCREENINFO, &g_screen_info) < 0) {
			printf("fb_display_setup FBIOPUET_VSCREENINFO failed\n");
			return TFAIL;
		}

		ioctl(fd_fb_display, FBIOGET_FSCREENINFO, &fb_fix);
		ioctl(fd_fb_display, FBIOGET_VSCREENINFO, &g_screen_info);

		sprintf(node, "%d", g_display_id - 1);	//for iMX6
#ifdef BUILD_FOR_ANDROID
		strcpy(fb_display_bg_dev, "/dev/graphics/fb");
#else
		strcpy(fb_display_bg_dev, "/dev/fb");
#endif
		strcat(fb_display_bg_dev, node);
		if ((fd_fb_bg = open(fb_display_bg_dev, O_RDWR )) < 0) {
			printf("Unable to open bg frame buffer\n");
			return TFAIL;
		}

		/* Overlay setting */
		alpha.alpha = g_alpha;
		alpha.enable = 1;
		if (ioctl(fd_fb_bg, MXCFB_SET_GBL_ALPHA, &alpha) < 0) {
			printf("Set global alpha failed\n");
			close(fd_fb_bg);
			return TFAIL;
		}

		/* Colorkey setting */
		key.enable = 1;
		key.color_key = g_colorkey;
			
		   if (ioctl(fd_fb_bg, MXCFB_SET_CLR_KEY, &key)<0){
			printf("Set color key failed\n");
			close(fd_fb_bg);
			return TFAIL;
		}

	}
	else
	{
		g_display_fg = 0;

		printf("It is background screen, only full screen default format was supported.\r\n");
		g_display_width = g_screen_info.xres;
		g_display_height = g_screen_info.yres;
		g_display_num_buffers = NUMBER_DISPLAY_BUFFERS;
		g_screen_info.yres_virtual = g_screen_info.yres * g_display_num_buffers;

		if (ioctl(fd_fb_display, FBIOPUT_VSCREENINFO, &g_screen_info) < 0) {
			printf("fb_display_setup FBIOPUET_VSCREENINFO failed\n");
			return TFAIL;
		}

		ioctl(fd_fb_display, FBIOGET_FSCREENINFO, &fb_fix);
		ioctl(fd_fb_display, FBIOGET_VSCREENINFO, &g_screen_info);

		if (g_screen_info.bits_per_pixel == 16)
			g_display_fmt = V4L2_PIX_FMT_RGB565;
		else if (g_screen_info.bits_per_pixel == 24)
			g_display_fmt = V4L2_PIX_FMT_RGB24;
		else
			g_display_fmt = V4L2_PIX_FMT_RGB32;

		printf("*** B) : g_display_fmt=%08x g_screen_info.bits_per_pixel=%d",g_display_fmt,g_screen_info.bits_per_pixel);
	}

	ioctl(fd_fb_display, FBIOBLANK, FB_BLANK_UNBLANK);

	g_display_base_phy = fb_fix.smem_start;
	printf("fb: smem_start = 0x%x, smem_len = 0x%x.\r\n", (unsigned int)fb_fix.smem_start, (unsigned int)fb_fix.smem_len);

	g_display_size = g_screen_info.xres * g_screen_info.yres * g_screen_info.bits_per_pixel / 8;
	printf("fb: frame buffer size = 0x%x bytes.\r\n", g_display_size);

	printf("fb: g_screen_info.xres = %d, g_screen_info.yres = %d.\r\n", g_screen_info.xres, g_screen_info.yres);
	printf("fb: g_display_left = %d.\r\n", g_display_left);
	printf("fb: g_display_top = %d.\r\n", g_display_top);
	printf("fb: g_display_width = %d.\r\n", g_display_width);
	printf("fb: g_display_height = %d.\r\n", g_display_height);

	return TPASS;
}

void dump_frame_old(char *file_name, void *data, int size)
{
	char *ptr = (char *)data;
	FILE *fp = fopen(file_name,"wb");

	printf("Dump 0x%p size=%d\n",data,size);

	if ( fp == NULL ) {
		printf("Failed to write to %s\n",file_name);
		return;
	} else {
		printf("Writing frame into file %s\n",file_name);
	}
	while(size > 0)
	{
		int ret, to_write;
		const int block_size = 8192;

		to_write = (size > block_size) ? block_size: size;

		ret = fwrite(ptr, 1, to_write, fp);
		if ( ret <= 0 )
			break;

		ptr += ret;
		size -= ret;
	}
	fflush(fp);
	fclose(fp);
}

void dump_frame(char *file_name, void *ptr, int size)
{
		char *p = ptr;
		FILE *fp = fopen(file_name, "wb");
		int offset = 0;

		if ( fp == NULL ) {
				printf("Failed to write to file %s\n",file_name);
				return;
		}

		while (size > 0 ) {
				int to_copy = 4096;
				if ( to_copy > size )
						to_copy = size;

				fwrite(p, 1, to_copy, fp);

				p += to_copy;
				size += to_copy;            
		}
		fflush(fp);

		fclose(fp);
}

int mxc_v4l_tvin_test(void)
{
	struct v4l2_buffer capture_buf;
	int i;
	struct ipu_task task;
	int total_time;
	struct timeval tv_start, tv_current;
	int display_buf_count = 0;

	printf("APP_BUILD: %s\n",__TIME__);
	if (prepare_display_buffers() < 0) {
			printf("prepare_display_buffers failed\n");
			return TFAIL;
	}

	if (start_capturing() < 0) {
		printf("start_capturing failed\n");
		return TFAIL;
	}

	memset(&task, 0, sizeof(struct ipu_task));
	task.output.width = 1376;//g_display_width;
	task.output.height = g_display_height;
	task.output.crop.w = 1376; //g_display_width;
	task.output.crop.h = g_display_height;
	task.output.format = g_display_fmt;

#if 1
	task.input.width  = g_camera_width;
	task.input.height = g_camera_height;
	task.input.crop.w = g_camera_width;
	task.input.crop.h = g_camera_height;
#else
	/* For testing we've received only half resolution image */
	task.input.width  = g_camera_width/2;
	task.input.height = g_camera_height/2;
	task.input.crop.w = g_camera_width/2;
	task.input.crop.h = g_camera_height/2;
#endif
	task.input.format = g_in_fmt;

	task.input.paddr = capture_buffers[0].offset;
	task.output.paddr = display_buffers[0].offset;

	if (ioctl(fd_ipu, IPU_CHECK_TASK, &task) != IPU_CHECK_OK) {
		printf("IPU_CHECK_TASK failed.\r\n");
		return TFAIL;
	}

	gettimeofday(&tv_start, 0);
	printf("start time = %d s, %d us\n", (unsigned int) tv_start.tv_sec,
		(unsigned int) tv_start.tv_usec);

	for (i = 0; i < g_frame_count; i++) {
		memset(&capture_buf, 0, sizeof(capture_buf));
		capture_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		capture_buf.memory = g_mem_type;
		if (ioctl(fd_capture_v4l, VIDIOC_DQBUF, &capture_buf) < 0) {
			printf("VIDIOC_DQBUF failed.\n");
			return TFAIL;
		}

		#if 0
		if ( i == 100 )
		{
			dump_frame("/mnt/udisk/NG/csi_frame500.yuv",
				 capture_buffers[capture_buf.index].start,
				1920*1080*2);
		}
		#endif

/*
		if (ioctl(fd_capture_v4l, VIDIOC_QBUF, &capture_buf) < 0) {
			printf("VIDIOC_QBUF failed\n");
			return TFAIL;
		}
*/
#if 0
		memcpy(display_buffers[display_buf_count].start, capture_buffers[capture_buf.index].start, g_display_size);
#else
		task.input.paddr = capture_buffers[capture_buf.index].offset;
		task.output.paddr = display_buffers[display_buf_count].offset;
		if ((task.input.paddr != 0) && (task.output.paddr != 0)) {
			if (ioctl(fd_ipu, IPU_QUEUE_TASK, &task) < 0) {
				printf("IPU_QUEUE_TASK failed\n");
				return TFAIL;
			}
		}
#endif
#if 0
		if (ioctl(fd_capture_v4l, VIDIOC_QBUF, &capture_buf) < 0) {
			printf("VIDIOC_QBUF failed\n");
			return TFAIL;
		}
#endif

		if(i==0)
			ioctl(fd_fb_display, FBIOBLANK, FB_BLANK_UNBLANK);
		g_screen_info.xoffset = 0;
		g_screen_info.yoffset = display_buf_count * g_display_height;
		if (ioctl(fd_fb_display, FBIOPAN_DISPLAY, &g_screen_info) < 0) {
			printf("FBIOPAN_DISPLAY failed, count = %d\n", i);
			break;
		}

#if 0
		/* Dump input and output of 10th frame */
		if ( i == 10 ) {
			dump_frame("/cache/dump/in-10.raw",capture_buffers[capture_buf.index].start, 1376*768*2);
			dump_frame("/cache/dump/out-10.raw",display_buffers[display_buf_count].start, 1376*768*4);
		} else if ( i == 11 ) {
			dump_frame("/cache/dump/in-11.raw",capture_buffers[capture_buf.index].start, 1376*768*2);
			dump_frame("/cache/dump/out-11.raw",display_buffers[display_buf_count].start, 1376*768*4);
		}
#endif

		if (ioctl(fd_capture_v4l, VIDIOC_QBUF, &capture_buf) < 0) {
			printf("VIDIOC_QBUF failed\n");
			return TFAIL;
		}

		display_buf_count ++;
		if (display_buf_count >= g_display_num_buffers)
			display_buf_count = 0;
		if ( g_delay != 0 ) {
			printf("Before sleep...");
			usleep(g_delay*1000);
			printf("After sleep\n");
		}
	}
	gettimeofday(&tv_current, 0);
	total_time = (tv_current.tv_sec - tv_start.tv_sec) * 1000000L;
	total_time += tv_current.tv_usec - tv_start.tv_usec;
	printf("total time for %u frames = %u us =  %lld fps\n", i, total_time, (i * 1000000ULL) / total_time);

	return TPASS;
}

int process_cmdline(int argc, char **argv)
{
	int i, val;
	char node[8];

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-res") == 0) {
			g_current_std = atoi(argv[++i]);
		}
		if (strcmp(argv[i], "-e") == 0) {
			g_fb0_activate_force = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-ow") == 0) {
			g_display_width = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-oh") == 0) {
			g_display_height = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-ot") == 0) {
			g_display_top = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-ol") == 0) {
			g_display_left = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-c") == 0) {
			g_frame_count = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-x") == 0) {
			val = atoi(argv[++i]);
			sprintf(node, "%d", val);
			strcpy(v4l_capture_dev, "/dev/video");
			strcat(v4l_capture_dev, node);
		}
		else if (strcmp(argv[i], "-q") == 0){
			val = atoi(argv[++i]);
			g_background = val;
		}
		else if (strcmp(argv[i], "-l") == 0){
			val = atoi(argv[++i]);
			g_delay = val;
		}	else if (strcmp(argv[i], "-d") == 0) {
			val = atoi(argv[++i]);
			g_display_id = val;
			sprintf(node, "%d", val);
#ifdef BUILD_FOR_ANDROID
			strcpy(fb_display_dev, "/dev/graphics/fb");
#else
			strcpy(fb_display_dev, "/dev/video");
#endif
			strcat(fb_display_dev, node);
		}
		else if (strcmp(argv[i], "-if") == 0) {
			i++;
			g_in_fmt = v4l2_fourcc(argv[i][0], argv[i][1],argv[i][2],argv[i][3]);
			if ((g_in_fmt != V4L2_PIX_FMT_NV12) &&
				(g_in_fmt != V4L2_PIX_FMT_RGB24) &&
				(g_display_fmt != V4L2_PIX_FMT_RGB32) &&
				(g_display_fmt != V4L2_PIX_FMT_BGR32) &&
				(g_in_fmt != V4L2_PIX_FMT_UYVY) &&
				(g_in_fmt != V4L2_PIX_FMT_YUYV) &&
				(g_in_fmt != V4L2_PIX_FMT_YUV420)) {
				printf("Default capture format is used: UYVY\n");
				g_in_fmt = V4L2_PIX_FMT_UYVY;
			}
		}
		else if (strcmp(argv[i], "-of") == 0) {
			i++;
			g_display_fmt = v4l2_fourcc(argv[i][0], argv[i][1],argv[i][2],argv[i][3]);
			if ((g_display_fmt != V4L2_PIX_FMT_RGB565) &&
				(g_display_fmt != V4L2_PIX_FMT_RGB24) &&
				(g_display_fmt != V4L2_PIX_FMT_RGB32) &&
				(g_display_fmt != V4L2_PIX_FMT_BGR32) &&
				(g_display_fmt != V4L2_PIX_FMT_UYVY) &&
				(g_display_fmt != V4L2_PIX_FMT_YUYV)) {
				printf("Default display format is used: UYVY\n");
				g_display_fmt = V4L2_PIX_FMT_UYVY;
			}
		}else if (strcmp(argv[i], "-av") == 0) {
			g_alpha = atoi(argv[++i]);
			printf("alpha value = %d\n", g_alpha);
		}
		else if (strcmp(argv[i], "-k") == 0) {
			g_colorkey = strtol(argv[++i], NULL, 16);
			printf("color key value = 0x%x\n", g_colorkey);
		}
		else if (strcmp(argv[i], "-help") == 0) {
			printf("MXC Video4Linux TVin Test\n\n" \
				   "Syntax: mxc_v4l2_tvin.out\n" \
				   " -e <1:activate 0:do nothing.. Forcefully activate fb0>\n" \
				   " -ow <capture display width>\n" \
				   " -oh <capture display height>\n" \
				   " -ot <display top>\n" \
				   " -ol <display left>\n" \
				   " -c <capture counter> \n" \
				   " -x <capture device> 0 = /dev/video0; 1 = /dev/video1 ...>\n" \
				   " -d <output frame buffer> 0 = /dev/fb0; 1 = /dev/fb1 ...>\n" \
				   " -if <capture format, only YU12, YUYV, UYVY and NV12 are supported>\n" \
				   " -of <display format, only RGBP, RGB3, RGB4, BGR4, YUYV, and UYVY are supported>\n" \
				   " -a <Camera input select> 0 = PAC Camera; 1 = WAM Camera.>\n" \
				   " -av <alpha value> 0 to 255 >\n" \
				   " -k <color key value> 000000 to FFFFFF >\n" \
				   " -f <format, only YU12, YUYV, UYVY and NV12 are supported> \n");
			return TFAIL;
		}
	}

	if ((g_display_width == 0) || (g_display_height == 0)) {
		printf("Zero display width or height\n");
		return TFAIL;
	}

	return TPASS;
}

void ctrl_c_handler(int signum, siginfo_t *info, void *myact)
{
    g_ctrl_c_flag = 3;
    return;
}


int main(int argc, char **argv)
{
	int i;
	enum v4l2_buf_type type;
    pthread_t thread_id;
    int test_id;
    int result;
    int next_camera_idx;
	int fd_fb_bg = 0;
	struct fb_var_screeninfo screen_info;

	printf(__FILE__);
	if (process_cmdline(argc, argv) < 0) {
		return TFAIL;
	}

    /* for ctrl-c */
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = ctrl_c_handler;

    if((sigaction(SIGINT, &act, NULL)) < 0)
    {
        printf("install sigal error\n");
        return TFAIL;
    }


	if ((fd_capture_v4l = open(v4l_capture_dev, O_RDWR, 0)) < 0) {
		printf("Unable to open %s\n", v4l_capture_dev);
		return TFAIL;
	}

	if (v4l_capture_setup() < 0) {
		printf("Setup v4l capture failed.\n");
		return TFAIL;
	}

	if ((fd_ipu = open("/dev/mxc_ipu", O_RDWR, 0)) < 0) {
		printf("open ipu dev fail\n");
		close(fd_capture_v4l);
		return TFAIL;
	}

/*
    if(g_fb0_activate_force){
        if ((fd_fb_bg = open(fb_display_bg_dev, O_RDWR )) < 0) {
	        printf("Unable to open bg frame buffer\n");
	        return TFAIL;
        }

        result = ioctl(fd_fb_bg, FBIOGET_VSCREENINFO, &screen_info);
        if (result < 0)
	        return TFAIL;
        screen_info.activate |= FB_ACTIVATE_FORCE;
        result = ioctl(fd_fb_bg, FBIOPUT_VSCREENINFO, &screen_info);
        if (result < 0)
	        return TFAIL;

        //        ioctl(fd_fb_bg, FBIOBLANK, FB_BLANK_NORMAL);
        //        ioctl(fd_fb_bg, FBIOBLANK, FB_BLANK_UNBLANK);
        close(fd_fb_bg);
        //printf("\n -----#### force ATP #####----- \n");
    }
*/
	/* Get information about bg panel */
	//display_bg_info();

	if ((fd_fb_display = open(fb_display_dev, O_RDWR, 0)) < 0) {
		printf("Unable to open %s\n", fb_display_dev);
		close(fd_ipu);
		close(fd_capture_v4l);
		return TFAIL;
	}

	if (fb_display_setup() < 0) {
		printf("Setup fb display failed.\n");
		close(fd_ipu);
		close(fd_capture_v4l);
		close(fd_fb_display);
		return TFAIL;
	}

	mxc_v4l_tvin_test();

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(fd_capture_v4l, VIDIOC_STREAMOFF, &type);

	if (g_display_fg)
		ioctl(fd_fb_display, FBIOBLANK, FB_BLANK_NORMAL);

	munmap(g_fb_display, g_display_size * g_display_num_buffers);

	for (i = 0; i < g_capture_num_buffers; i++) {
		munmap(capture_buffers[i].start, capture_buffers[i].length);
	}

	close(fd_ipu);
	close(fd_capture_v4l);
	close(fd_fb_display);
	return 0;
}


