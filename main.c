#include "accelerator.h"
#include <stdint.h>
#include <stdio.h>

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while(n--) *d++ = *s++;
    return dest;
}

#define POPCNT_ACCELERATOR 0


uint32_t get_mcycle() {
    uint32_t mcycle;
    asm volatile("csrr %0, mcycle" : "=r"(mcycle));
    return mcycle;
}

#define NUM_RUNS 10

uint32_t hw_times[NUM_RUNS];
uint32_t sw_times[NUM_RUNS];

#if POPCNT_ACCELERATOR
#define N 64
uint32_t time_variance();
uint32_t average();

uint32_t input[N] = {
    0x00000000, 0xFFFFFFFF, 0x00BA6A24, 0xF4A0DCF9, 0xBBD0D09A, 0x22E8AE33,
    0x15E2D9A5, 0x67F95E5C, 0xBBD0D09A, 0x22E8AE33, 0x15E2D9A5, 0x67F95E5C,
    0xEE92150C, 0xA3304C3D, 0x744477C5, 0x01B67AB8, 0x19018474, 0x8057564A,
    0x159D27CA, 0xF93EF0C3, 0x08DA476D, 0x322426FA, 0x5E072FFD, 0xD41543DE,
    0x0E44E744, 0x21F0EDCC, 0xB918FDBD, 0x441A692A, 0x9F67451A, 0xCB7763FB,
    0x0559C0F8, 0x2F504328, 0xC66BA44D, 0x86B7C770, 0x1D2CA59C, 0x1BB35723,
    0x8A6EF954, 0xC2ACDB61, 0x7C93AAFF, 0x0F201FFF, 0xEFB54BC9, 0xA7120239,
    0xF5E2BCF6, 0x28FB7BAA, 0xB53F17BF, 0x00000200, 0xC7B77096, 0x213F0640,
    0xB53F17BF, 0x5B9A9C3C, 0xC7B77096, 0x213F0640, 0x0B3390A8, 0x04786A3D,
    0x62EA482E, 0x4D865305, 0x8197C0A6, 0x58700621, 0x558FDBD0, 0xD531E786,
    0xAD9EF39F, 0x00004300, 0x67162F16, 0x00000001,
};

uint32_t expected[N] = {
    0,  32, 11, 18, 16, 15, 16, 20, 16, 15, 16, 20, 14, 14, 16, 15,
    10, 13, 16, 19, 15, 14, 20, 16, 13, 16, 20, 12, 17, 22, 13, 12,
    16, 17, 15, 17, 17, 16, 21, 18, 20, 12, 21, 19, 22, 1,  18, 11,
    22, 17, 18, 11, 12, 14, 14, 13, 13, 10, 18, 17, 22, 3,  16, 1,
};

uint32_t popcnt_time[N] = {};

extern uint8_t popcnt(uint32_t a);
extern uint8_t popcnt_secure(uint32_t a);





uint8_t testPopcnt(const char *name, uint8_t (*f)(uint32_t)) {
    uint8_t fails = 0;
    print("Testing %s:\n", name);
    for (int i = 0; i < N; i++) {
        uint32_t start_cycles = get_mcycle();
        uint8_t output = f(input[i]);
        popcnt_time[i] = get_mcycle() - start_cycles;
        if (output != expected[i]) {
            fails++;
            print("%s: Unexpected output for %x: Expected %d, got %d\n", name,
                  input[i], expected[i], output);
        }
    }
    if (fails) {
        print("Total %s failures: %d\n", name, fails);
    } else {
        print("All %s tests passed!\n", name);
        print("Variance of %s runtime cycles: %d\n", name, time_variance());
	print("Average of %s runtime cycles: %d\n", name, average());

        print("Number of cycles to execute %s:\n", name);
        for (int i = 0; i < N / 8; i++) {
            for (int j = 0; j < N / 8; j++) {
                print("%d, ", popcnt_time[i * 8 + j]);
            }
            print("\n");
        }
    }
    print("\n");
    return fails;
}


uint32_t average() {
  uint32_t mean = 0;
  for(int i = 0; i < N; i++) {
    mean += popcnt_time[i];
  }
  return mean / N;
}
  

uint32_t time_variance() {
    uint32_t mean = average();

    uint32_t variance = 0;
    for (int i = 0; i < N; i++) {
        uint32_t s = (popcnt_time[i] - mean);
        variance += s * s;
    }
    variance /= N;
    return variance;
}

#endif
void matmul_sw(uint8_t A[4][4], uint8_t B[4][4], uint16_t C[4][4]);
void matmul_hw(uint8_t A[4][4], uint8_t B[4][4], uint16_t C[4][4]);

uint8_t compare(uint16_t C_hw[4][4], uint16_t C_sw[4][4]) {
  uint8_t fails = 0;
  for(int i = 0; i < 4; i++) {
    for(int j = 0; j < 4; j++) {
      if(C_hw[i][j] != C_sw[i][j]) {
        print("FAIL at C[%d][%d]: hw=%d sw=%d\n", i, j, C_hw[i][j], C_sw[i][j]);
        fails++;
      }
    }
  }
  return fails;
}
void test_matmul(uint8_t A[4][4], uint8_t B[4][4], char* test_name, uint32_t *hw_out, uint32_t *sw_out) {
    uint16_t C_hw[4][4];
    uint16_t C_sw[4][4];
    
    for(int run = 0; run < NUM_RUNS; run++) {
        uint32_t hw_start = get_mcycle();
        matmul_hw(A, B, C_hw);
        hw_times[run] = get_mcycle() - hw_start;
        
        uint32_t sw_start = get_mcycle();
        matmul_sw(A, B, C_sw);
        sw_times[run] = get_mcycle() - sw_start;
    }
    
    // compute averages
    uint32_t hw_avg = 0, sw_avg = 0;
    for(int i = 0; i < NUM_RUNS; i++) {
        hw_avg += hw_times[i];
        sw_avg += sw_times[i];
    }
    hw_avg /= NUM_RUNS;
    sw_avg /= NUM_RUNS;
    
    // compute variances
    uint32_t hw_var = 0, sw_var = 0;
    for(int i = 0; i < NUM_RUNS; i++) {
        uint32_t hw_diff = hw_times[i] - hw_avg;
        uint32_t sw_diff = sw_times[i] - sw_avg;
        hw_var += hw_diff * hw_diff;
        sw_var += sw_diff * sw_diff;
    }
    hw_var /= NUM_RUNS;
    sw_var /= NUM_RUNS;
    
    // correctness check
    uint8_t fails = compare(C_hw, C_sw);
    
    if(!fails) {
        print("\n%s PASSED!\n", test_name);
        print("HW avg: %d cycles, variance: %d\n", hw_avg, hw_var);
        print("SW avg: %d cycles, variance: %d\n", sw_avg, sw_var);
        print("Speedup: %d.%dx\n", sw_avg/hw_avg, (sw_avg*10/hw_avg)%10);
    } else {
        print("%s FAILED! %d mismatches\n", test_name, fails);
    }
    *hw_out = hw_avg;
    *sw_out = sw_avg;
}
void matmul_sw(uint8_t A[4][4], uint8_t B[4][4], uint16_t C[4][4]) {
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            C[i][j] = 0;
            for(int k = 0; k < 4; k++)
                C[i][j] += A[i][k] * B[k][j];
        }
    }
}

void matmul_hw(uint8_t A[4][4], uint8_t B[4][4], uint16_t C[4][4]) {
    // TODO:
    Accelerator* acc = initAccelerator();
    matmul_load(acc, A, B);
    matmul_start(acc);
    matmul_read(acc, C);
}


int main() {
    // Enable interrupts, but disable pesky timer interrupts
    asm volatile("csrc mie, %0" : : "r"(0x80));
    interrupt_enable();
#if POPCNT_ACCELERATOR
    uint32_t popcnt_fails = 0;
    uint32_t popcnt_secure_fails = 0;
    uint32_t popcnt_hw_fails = 0;

    popcnt_fails = testPopcnt("popcnt", popcnt);

    popcnt_secure_fails = testPopcnt("popcnt_secure", popcnt_secure);

    popcnt_hw_fails = testPopcnt("popcnt_hw", popcnt_hw);

    return popcnt_fails + popcnt_secure_fails + popcnt_hw_fails;
#else
	uint32_t total_hw = 0, total_sw = 0;
	uint32_t hw_avg, sw_avg;
	// test 1: all ones - result should be all 4s
	uint8_t A1[4][4] = {{1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1}};
	uint8_t B1[4][4] = {{1,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1}};
	test_matmul(A1, B1, "test 1 - all ones", &hw_avg, &sw_avg);
	total_hw += hw_avg; total_sw += sw_avg;

	// test 2: sequential 1-16
	uint8_t A2[4][4] = {{1,2,3,4},{5,6,7,8},{9,10,11,12},{13,14,15,16}};
	uint8_t B2[4][4] = {{1,2,3,4},{5,6,7,8},{9,10,11,12},{13,14,15,16}};
	test_matmul(A2, B2, "test 2 - 1-16", &hw_avg, &sw_avg);
	total_hw += hw_avg; total_sw += sw_avg;

	// test 3: all zeros - result should be all 0s
	uint8_t A3[4][4] = {{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0}};
	uint8_t B3[4][4] = {{1,2,3,4},{5,6,7,8},{9,10,11,12},{13,14,15,16}};
	test_matmul(A3, B3, "test 3 - all zeros", &hw_avg, &sw_avg);
	total_hw += hw_avg; total_sw += sw_avg;

	// test 4: identity matrix A - result should equal B
	uint8_t A4[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
	uint8_t B4[4][4] = {{1,2,3,4},{5,6,7,8},{9,10,11,12},{13,14,15,16}};
	test_matmul(A4, B4, "test 4 - A is identity matrix", &hw_avg, &sw_avg);
	total_hw += hw_avg; total_sw += sw_avg;

	// test 5: identity matrix B - result should equal A
	uint8_t A5[4][4] = {{1,2,3,4},{5,6,7,8},{9,10,11,12},{13,14,15,16}};
	uint8_t B5[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
	test_matmul(A5, B5, "test 5 - B is identity matrix", &hw_avg, &sw_avg);
	total_hw += hw_avg; total_sw += sw_avg;

	// test 6: max safe values (127) - boundary test
	uint8_t A6[4][4] = {{127,127,127,127},{127,127,127,127},{127,127,127,127},{127,127,127,127}};
	uint8_t B6[4][4] = {{127,127,127,127},{127,127,127,127},{127,127,127,127},{127,127,127,127}};
	test_matmul(A6, B6, "test 6 - max safe values (127)", &hw_avg, &sw_avg);
	total_hw += hw_avg; total_sw += sw_avg;

	// test 7: diagonal matrix
	uint8_t A7[4][4] = {{2,0,0,0},{0,3,0,0},{0,0,4,0},{0,0,0,5}};
	uint8_t B7[4][4] = {{2,0,0,0},{0,3,0,0},{0,0,4,0},{0,0,0,5}};
	test_matmul(A7, B7, "test 7 - diagonal matrix", &hw_avg, &sw_avg);
	total_hw += hw_avg; total_sw += sw_avg;

	// test 8: transpose - A and B are transposes of each other
	uint8_t A8[4][4] = {{1,2,3,4},{5,6,7,8},{9,10,11,12},{13,14,15,16}};
	uint8_t B8[4][4] = {{1,5,9,13},{2,6,10,14},{3,7,11,15},{4,8,12,16}};
	test_matmul(A8, B8, "test 8 - A and B transposes", &hw_avg, &sw_avg);
	total_hw += hw_avg; total_sw += sw_avg;
	
	// test 9: random values within safe range
	uint8_t A9[4][4] = {{3,7,2,9},{1,5,8,4},{6,2,7,3},{9,4,1,6}};
	uint8_t B9[4][4] = {{2,8,1,5},{7,3,9,2},{4,6,3,8},{1,5,7,4}};
	test_matmul(A9, B9, "test 9 - random values within safe range", &hw_avg, &sw_avg);
	total_hw += hw_avg; total_sw += sw_avg;

	// test 10: mixed small and large safe values
	uint8_t A10[4][4] = {{127,1,64,32},{16,127,8,4},{2,32,127,16},{8,4,2,127}};
	uint8_t B10[4][4] = {{1,64,32,16},{127,8,4,2},{32,16,127,8},{4,2,16,127}};
	test_matmul(A10, B10, "test 10 - random values 2", &hw_avg, &sw_avg);
	total_hw += hw_avg; total_sw += sw_avg;

	uint32_t avg_hw = total_hw / 10;
	uint32_t avg_sw = total_sw / 10;
	print("\n\n--- Overall Summary ---\n");
	print("Avg HW cycles: %d\n", avg_hw);
	print("Avg SW cycles: %d\n", avg_sw);
	print("Avg speedup: %d.%dx\n", avg_sw/avg_hw, (avg_sw*10/avg_hw)%10);


    return 0;
#endif
}
