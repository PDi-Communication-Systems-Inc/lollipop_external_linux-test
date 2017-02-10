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

int g_alpha = 0;
int g_colorkey = 0;

int fb0_display_setup(int status_alpha, int status_colorkey)
{
	int fd_fb_bg = 0;
	struct mxcfb_gbl_alpha alpha;
	struct mxcfb_color_key key;
#ifdef BUILD_FOR_ANDROID
	char fb_device_0[100] = "/dev/graphics/fb0";
#else
	char fb_device_0[100] = "/dev/fb0";
#endif

	if ((fd_fb_bg = open(fb_device_0, O_RDWR )) < 0) {
		printf("Unable to open bg frame buffer\n");
		return TFAIL;
	}

	/* Overlay setting */
	alpha.alpha = g_alpha;
	alpha.enable = status_alpha;
	if (ioctl(fd_fb_bg, MXCFB_SET_GBL_ALPHA, &alpha) < 0) {
		printf("Set global alpha failed\n");
		close(fd_fb_bg);
		return TFAIL;
	}

	/* Colorkey setting */
	key.enable = status_colorkey;
	key.color_key = g_colorkey;

	if (ioctl(fd_fb_bg, MXCFB_SET_CLR_KEY, &key)<0) {
		printf("Set color key failed\n");
		close(fd_fb_bg);
		return TFAIL;
	}

	close(fd_fb_bg);
	return TPASS;
}

int main(int argc, char **argv)
{
	if (argc != 5) {
		printf("Usage:\n\n%s <alpha_enable> <alpha_value> <colorkey_enable> <colorkey_value>\n\n", argv[0]);
		return -1;
	}
	g_alpha = atoi(argv[2]);
	g_colorkey = strtol(argv[4], NULL, 16);
	printf("\n Alpha : %d Colorkey : %x \n",g_alpha,g_colorkey);

	fb0_display_setup(atoi(argv[1]),atoi(argv[3]));

    return 0;
}
