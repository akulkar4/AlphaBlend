CC = gcc
CFLAGS = -c -Wall -march=armv7-a -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a8 -mtune=cortex-a8 -O3 -ffast-math -fsingle-precision-constant -ftree-loop-vectorize -ftree-slp-vectorize -ftree-vectorize -ftree-vect-loop-version -fprefetch-loop-arrays -fopt-info-vec

#fprefetch-loop-arrays

alpha_time: alpha_time.o
	$(CC) alpha_time.o -pg -g -static -lrt -lm -o $@

original:
	gcc -o test_original original.c

alpha_time.s: alpha_time.c
	$(CC) $(CFLAGS) -Wa,-adhln -g alpha_time.c -c > alpha_time.s

clean:
	rm -f alpha_time test *.o  *.s *.out out.rgba perf.data

