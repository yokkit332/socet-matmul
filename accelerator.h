#ifndef ACCELERATOR_H
#define ACCELERATOR_H
#include <stdint.h>

typedef struct Accelerator Accelerator;

Accelerator *initAccelerator();

// TODO: Write all of your functions here!
void matmul_load(Accelerator *acc, uint8_t A[4][4], uint8_t B[4][4]);
void matmul_read(Accelerator *acc, uint16_t C[4][4]);
void matmul_start(Accelerator *acc);

#endif
