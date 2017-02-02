/*
 * Copyright 2004-2013 Freescale Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*=======================================================================
                                        INCLUDE FILES
=======================================================================*/
/* Standard Include Files */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

/* Verification Test Environment Include Files */
#include <sys/ioctl.h>
//#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <linux/mxcfb.h>

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

int fd_fb = 0;



int
main(int argc, char **argv)
{
	int count;
	int i;

	int sleeptime;
	int retval = 0;
	unsigned int fps;
	unsigned int total_time;
	unsigned long long timestamp_prev = 0, timestamp_next = 0;
	unsigned long long start, total = 0;
	static unsigned long long diff[120] = {0};
	unsigned long long temp, calc_fps;
#ifdef BUILD_FOR_ANDROID
	char fbdev[] = "/dev/graphics/fb0";
#else
	char fbdev[] = "/dev/fb0";
#endif
	struct timeval tv_start, tv_current;

	if (argc != 4) {
		printf("Usage:\n\n%s <fb #> <count> <sleeptime>\n\n", argv[0]);
		return -1;
	}

	sleeptime = atoi(argv[3]);
	sleep(sleeptime);
	count = atoi(argv[2]);
#ifdef BUILD_FOR_ANDROID
	fbdev[16] = argv[1][0];
#else
	fbdev[7] = argv[1][0];
#endif

	printf(" <fb %s>, count:%d\n\n", fbdev, count);

	if ((fd_fb = open(fbdev, O_RDWR, 0)) < 0)
	{
		printf("Unable to open %s\n", fbdev);
		retval = -1;
		goto err0;
	}

	gettimeofday(&tv_start, 0);
	for (i = 0; i < count; i++) {
		retval = ioctl(fd_fb, MXCFB_WAIT_FOR_VSYNC, &timestamp_next);
		if (retval < 0)
		{
			goto err1;
		}

		if(i == 0)
			start = timestamp_next;

		if(i >= 1)
			diff[i] = timestamp_next - timestamp_prev;
		total = timestamp_next - start;
		timestamp_prev = timestamp_next;
	}
	gettimeofday(&tv_current, 0);
	//printf("count:%u, i:%u\n", count, i);

	total_time = (tv_current.tv_sec - tv_start.tv_sec) * 1000000L;
	total_time += tv_current.tv_usec - tv_start.tv_usec;
	fps = (i * 1000000ULL) / total_time;
	printf("Application :: total time for %d frames = %u us =  %lld fps\n", i, total_time, (i * 1000000ULL) / total_time);

	temp = count;
	temp *= 1000000000L;
	calc_fps = temp / total;

	printf(" Kernel time :: start to end time diff: %llu calculated fps = %llu temp : %llu\n", total, calc_fps, temp);
	if (fps > 45 && fps < 80)
		retval = 0;
	else
		retval = -1;

err1:
/*
	for(i = 0; i < count; i++)
		printf("diff = %llu\n",diff[i]);
*/
	close(fd_fb);
err0:
	return retval;
}

