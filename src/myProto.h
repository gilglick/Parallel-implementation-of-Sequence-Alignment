#ifndef SRC_MYPROTO_H_
#define SRC_MYPROTO_H_

#include "structs.h"

void get_best_results_with_CUDA(const char* sequence1, int sequence1_length, char* sequence2, int sequence2_length,
		const weights_t* weights, int* results,
		int* conservative_group, int conservatives, int* semi_conservative_group,
		int semi_conservative);

#endif
