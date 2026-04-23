#include "accelerator.h"
#include <stdint.h>

// TODO: Fill in BASE here
#define BASE 0xD0000000

struct Accelerator {
    // TODO: match your memory map here
  volatile uint32_t A[4]; // 0x00 - 0x0C 4 rows
  volatile uint32_t B[4]; // 0x10 - 0x1C 4 rows
  volatile uint32_t C[8]; // 0x20 - 0x3C 8 reads (2 elements per read)
  volatile uint32_t start;
  volatile uint32_t done;
};

Accelerator *initAccelerator() { return (Accelerator *)BASE; }

// TODO: Implement your functions here
void matmul_load(Accelerator *acc, uint8_t A[4][4], uint8_t B[4][4])
{
  for(int i = 0; i < 4; i++) {
    // write row i of A and B as one 32-bit value in MMIO bus with shift operations and OR
    acc->A[i] = (A[i][0] << 24) | (A[i][1] << 16) | (A[i][2] << 8) | A[i][3]; // shove 4 values into 32 bit mmio bus
    acc->B[i] = (B[i][0] << 24) | (B[i][1] << 16) | (B[i][2] << 8) | B[i][3]; // shove 4 values into 32 bit mmio bus
  }
}
void matmul_read(Accelerator *acc, uint16_t C[4][4])
{
  // wait for accelerator to signal done
  while(acc->done == 0);
  
  // read row by row
  for(int row = 0; row < 4; row++) {
    // first read gives columns 0 and 1
    uint32_t first_half = acc->C[row * 2]; // read in first half of current row
    C[row][0] = (uint16_t)(first_half >> 16) ; // only transfer upper 16 bits to 0th
    C[row][1] = (uint16_t)(first_half & 0x0000FFFF); // only transfer lower 16 bits to 1st
    
    // second read gives columns 2 and 3
    uint32_t second_half = acc->C[row*2 + 1]; // read in second half of current row
    C[row][2] = (uint16_t)(second_half >> 16); // only transfer upper 16 bits to 2nd
    C[row][3] = (uint16_t)(second_half & 0x0000FFFF); // only transfer lower 16 bits to 3rd
    
  }
  
}

void matmul_start(Accelerator *acc) {
    acc->start = 1;
}
