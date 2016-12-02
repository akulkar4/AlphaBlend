// Team : akulkar4
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <arm_neon.h>

#define N_TESTS 100
#define GET_TIME_OF_DAY (1)
#define INTRINSICS (1)

#define A(x) (((x) & 0xff000000) >> 24)
#define R(x) (((x) & 0x00ff0000) >> 16)
#define G(x) (((x) & 0x0000ff00) >> 8)
#define B(x) ((x) & 0x000000ff)
#define RB(x) (((x) & 0x00ff00ff))
#define RGB(x) (((x) & 0x00ffffff))

void alphaBlend_c(int * __restrict__ fgImage, int * __restrict__ bgImage, int * __restrict__ dstImage);
int backImage[512 * 512]; 
int foreImage[512 * 512];
int newImage[512 * 512];

int main(int argc, char **argv)
{
	FILE *fgFile, *bgFile, *outFile;	
	#if GET_TIME_OF_DAY
		struct timeval start, end;
	#else
		struct timespec start,end;
	#endif
	int result, y;
	unsigned long diff, min = 123456789;

	if(argc != 4)
	{
		fprintf(stderr, "Usage:%s foreground background outFile\n",argv[0]);
		return 1;
	}
	fgFile = fopen(argv[1], "rb");
	bgFile = fopen(argv[2], "rb");
	outFile = fopen(argv[3], "wb");

	if(fgFile && bgFile && outFile)
	{
			result = fread(backImage, 512*sizeof(int), 512, bgFile);
			if(result != 512)
			{	
				fprintf(stderr, "Error with backImage\n");
				return 3;
			}
			result = fread(foreImage, 512*sizeof(int), 512, fgFile);
			if(result != 512)
			{
				fprintf(stderr, "Error with foreImage\n");
				return 4;
			}

		for(y=0; y< N_TESTS ; y++)
		{
			#if GET_TIME_OF_DAY 
				gettimeofday(&start, NULL);
				alphaBlend_c(&foreImage[0], &backImage[0], &newImage[0]);
				gettimeofday(&end, NULL);
				diff = (1000000 * (end.tv_sec - start.tv_sec)) + (end.tv_usec - start.tv_usec);
				if(diff < min)
					min = diff;
			#else
				clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
				alphaBlend_c(&foreImage[0], &backImage[0], &newImage[0]);
				clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end);
				diff = 1000000000 * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
				if(diff < min)
					min = diff;
			#endif
		}
		#if GET_TIME_OF_DAY
			fprintf(stdout, "Routine took %.3f microseconds\n", (double)(min));
		#else
			fprintf(stdout, "Routine took %.3f microseconds\n", (double)(min/1000.0));
		#endif
		fwrite(newImage, 512*sizeof(int),512,outFile);
		fclose(fgFile);
		fclose(bgFile);
		fclose(outFile);
		return 0;
	}
	fprintf(stderr, "Problem opening a file\n");
	return 2;
}

#if INTRINSICS
	/* Code modified using intrinsics*/
	void alphaBlend_c(int* __restrict__ fgImage, int * __restrict__ bgImage, int * __restrict__ dstImage)
	{
		int y;
		for(y=0; y< 262144 ; y+= 16)
		{
				uint8x8x4_t brgba;
				uint8x8x4_t frgba;
				uint8x8_t subafg;
				uint8x8x4_t dst;
				
				brgba = vld4_u8(&bgImage[y]);
				frgba = vld4_u8(&fgImage[y]);
				subafg = vmvn_u8(frgba.val[3]);
				dst.val[0] = vmovn_u16(vshrq_n_u16(vaddq_u16(vmull_u8(frgba.val[3], frgba.val[0]), 
                        	                           vmull_u8(brgba.val[0], subafg)), 8));
				dst.val[1] = vmovn_u16(vshrq_n_u16(vaddq_u16(vmull_u8(frgba.val[3], frgba.val[1]), 
                        	                           vmull_u8(brgba.val[1], subafg)), 8)); 
				dst.val[2] = vmovn_u16(vshrq_n_u16(vaddq_u16(vmull_u8(frgba.val[3], frgba.val[2]), 
                        	                           vmull_u8(brgba.val[2], subafg)), 8)); 
				dst.val[3] = vdup_n_u8(255);
				vst4_u8 (&dstImage[y], dst);
	
				brgba = vld4_u8(&bgImage[y+8]);
				frgba = vld4_u8(&fgImage[y+8]);
				subafg = vmvn_u8(frgba.val[3]);
				dst.val[0] = vmovn_u16(vshrq_n_u16(vaddq_u16(vmull_u8(frgba.val[3], frgba.val[0]), 
       		                                            vmull_u8(brgba.val[0], subafg)), 8));
				dst.val[1] = vmovn_u16(vshrq_n_u16(vaddq_u16(vmull_u8(frgba.val[3], frgba.val[1]), 
                	                                   vmull_u8(brgba.val[1], subafg)), 8)); 
				dst.val[2] = vmovn_u16(vshrq_n_u16(vaddq_u16(vmull_u8(frgba.val[3], frgba.val[2]), 
                	                                   vmull_u8(brgba.val[2], subafg)), 8)); 
				dst.val[3] = vdup_n_u8(255);
				vst4_u8 (&dstImage[y+8], dst);

				__builtin_prefetch(&bgImage[y+500], 0, 0);
				__builtin_prefetch(&fgImage[y+500], 0, 0);
		}
	}

#else
	/*
	 * Modified with R and B together
 	*/
	#if 1
		void alphaBlend_c(int * __restrict__ fgImage, int * __restrict__ bgImage, int * __restrict__ dstImage)
		{
			int y;
			for(y = 0; y < 512 * 512; y++)
			{
				uint8_t a_fg = A(fgImage[y]);
				int rb_fg = ((RB(fgImage[y]) * a_fg) + (RB(bgImage[y]) * ( 255 - a_fg )))/256;
				int g_fg = (((G(fgImage[y]) * a_fg) + (G(bgImage[y]) * ( 255 - a_fg )))/256) << 8;
				dstImage[y] = 0xff000000 | (0x00ff00ff & (rb_fg)) | (0x0000ff00 & (g_fg));
			}
		}	
	#endif
	
	#if 0
	/*
	 * Single Loop, Some more subexpressions taken outside loop
 	*/
	
		void alphaBlend_c(int * __restrict__ fgImage, int * __restrict__ bgImage, int * __restrict__ dstImage)
		{
			int y;
			for(y = 0; y < 512 * 512; y++)
			{
				int a_fg = A(fgImage[y]);
				int subAfg = 255 - a_fg;
				uint8_t r_fg = (((R(fgImage[y]) * a_fg) + (R(bgImage[y]) * ( subAfg )))/256) << 16;
				uint8_t g_fg = (((G(fgImage[y]) * a_fg) + (G(bgImage[y]) * ( subAfg )))/256) << 8;
				uint8_t b_fg = (((B(fgImage[y]) * a_fg) + (B(bgImage[y]) * ( subAfg )))/256);
				dstImage[y] =  0xff000000 | 
						(0x00ff0000 & (r_fg)) |
						(0x0000ff00 & (g_fg)) |
						(0x000000ff & (b_fg));
			}
		}

	#endif	

	#if 0
	/*
 	 * Manual CSE: Moving common elements outside the loop
 	 */
	
		void alphaBlend_c(int * __restrict__ fgImage, int * __restrict__ bgImage, int * __restrict__ dstImage)
		{
			int x, y;	
			for(y = 0; y < 512; y++)
			{
				int y_val = y*512;
				for(x = 0; x < 512; x++)
				{
	
					int xy_val = y_val+x;
					int a_fg = A(fgImage[xy_val]);
					int dst_r = ((R(fgImage[xy_val]) * a_fg) + (R(bgImage[xy_val]) * (255-a_fg)))/256;
					int dst_g = ((G(fgImage[xy_val]) * a_fg) + (G(bgImage[xy_val]) * (255-a_fg)))/256;
					int dst_b = ((B(fgImage[xy_val]) * a_fg) + (B(bgImage[xy_val]) * (255-a_fg)))/256;
					dstImage[xy_val] =  0xff000000 |
								(0x00ff0000 & (dst_r << 16)) |
								(0x0000ff00 & (dst_g << 8)) |
								(0x000000ff & (dst_b));
				}
			}		
		}
	
	#endif
	
	#if 0
	/*
	 * Original Code as it is
	 */
		void alphaBlend_c(int *fgImage, int *bgImage, int *dstImage)
		{
			int x, y;
			for(y = 0; y < 512; y++)
			{
				for(x = 0; x < 512; x++)
				{
					int a_fg = A(fgImage[(y*512)+x]);
					int dst_r = ((R(fgImage[(y*512)+x]) * a_fg) + (R(bgImage[(y*512)+x]) * (255-a_fg)))/256;
					int dst_g = ((G(fgImage[(y*512)+x]) * a_fg) + (G(bgImage[(y*512)+x]) * (255-a_fg)))/256;
					int dst_b = ((B(fgImage[(y*512)+x]) * a_fg) + (B(bgImage[(y*512)+x]) * (255-a_fg)))/256;
					dstImage[(y*512)+x] =  0xff000000 |
								(0x00ff0000 & (dst_r << 16)) |
								(0x0000ff00 & (dst_g << 8)) |
								(0x000000ff & (dst_b));
		
				}
			}
		}
	
	#endif
#endif
