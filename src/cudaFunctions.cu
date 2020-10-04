#include <cuda_runtime.h>
#include <helper_cuda.h>
#include <cuda.h>
#include "myProto.h"
#include "omp.h"

#define THREADS_PER_BLOCK 1024

int k,n;

__device__ const int PRIME_NUMBERS[] = {
	2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97
};

__device__ int gpu_pair(char sequence1_char, char sequence2_char, int* group, int group_size)
{
	int offset = 'A';
	int mul = PRIME_NUMBERS[sequence1_char - offset] * PRIME_NUMBERS[sequence2_char - offset];
	for(int i = 0 ; i < group_size ; i++){
		if(group[i]%mul == 0)
			return 1;
	}
	return 0;
}

__global__ void gpu_calc_str_weight(const char* sequence1, int sequence1_length, char* sequence2, int sequence2_length,
		int max_k, int max_n, float* results, const weights_t* weights, int* conservative_group,
		int conservative_group_size, int* semi_conservative_group, int semi_conservative_group_size, int max_threads)
{
	int threadId = threadIdx.x + blockIdx.x * blockDim.x;
	//kill over threads
	if(threadId > max_threads -1)
		return;

	int k = threadId/max_n;
	int n = threadId%max_n;
	if(k==0)
		return;
	for(int i = 0 ; i < sequence2_length ; i++){
		int j = 0;
		if(i>= k)
			j = 1;
		if(i == k){
			results[k + n*max_k] -= weights->w4;
		}
		if(sequence1[j+n+i] == sequence2[i]){
			results[k + n*max_k] += weights->w1;
		}else if(gpu_pair(sequence1[j+n+i],sequence2[i],conservative_group,conservative_group_size)){
			results[k + n*max_k] -= weights->w2;
		}else if(gpu_pair(sequence1[j+n+i],sequence2[i],semi_conservative_group,semi_conservative_group_size)){
			results[k + n*max_k] -= weights->w3;
		}else{
			results[k + n*max_k] -= weights->w4;
		}
	}
}

void find_best_k_n(float* results, int max_k, int max_n, int* best_k, int* best_n)
{
//The minimum value of float.
	float best_weight = -10e38;
#pragma omp parallel for collapse(2)
	for(int n = 0 ; n < max_n ; n++){
		for(int k = 0 ; k < max_k ; k++){
#pragma omp critical
{
	// k is the col n is the rows
			if(results[k + n*max_k] > best_weight){
				best_weight = results[k + n*max_k];
				*best_k = k;
				*best_n = n;
			}
}
		}
	}
}

void get_best_results_with_CUDA(const char* sequence1, int sequence1_length, char* sequence2, int sequence2_length,
		const weights_t* weights, int* results,
		int* conservative_group, int conservative_group_size, int* semi_conservative_group,
		int semi_conservative_group_size)
{
// Allocate space for sequence1 on GPU
	char* gpu_sequence1;
	cudaMalloc(&gpu_sequence1,sequence1_length+1);
	
// Allocate space for sequence2 on GPU
	char* gpu_sequence2;
	cudaMalloc(&gpu_sequence2,sequence2_length+1);

// Copy sequence1 to GPU
	cudaMemcpy(gpu_sequence1,sequence1,sequence1_length+1,cudaMemcpyHostToDevice);
	
// Copy sequence1 to GPU
	cudaMemcpy(gpu_sequence2,sequence2,sequence2_length+1,cudaMemcpyHostToDevice);

// Allocate space for 4 weights on GPU
	weights_t* gpu_weights;
	cudaMalloc(&gpu_weights,sizeof(weights_t));
	
	// Copy weights to GPU
	cudaMemcpy(gpu_weights,weights,sizeof(weights_t),cudaMemcpyHostToDevice);

// Allocate space for conservative group on GPU
	int* gpu_conservative_group;
	cudaMalloc(&gpu_conservative_group,sizeof(int)*conservative_group_size);
	// Copy conservative group to gpu
	cudaMemcpy(gpu_conservative_group,conservative_group,conservative_group_size * sizeof(int),cudaMemcpyHostToDevice);

// Allocate space for semi conservative group on GPU
	int* gpu_semi_conservative_group;
	cudaMalloc(&gpu_semi_conservative_group,semi_conservative_group_size*sizeof(int));
	// Copy semi conservative group to gpu
	cudaMemcpy(gpu_semi_conservative_group,semi_conservative_group,semi_conservative_group_size * sizeof(int),cudaMemcpyHostToDevice);

	dim3 grid, block;
	// Max size of hypen options
	int max_k = sequence2_length+1;
	// Max size of offset options
	int max_n = sequence1_length - sequence2_length;
//	int num_of_blocks = (sequence2_length*max_k*max_n/THREADS_PER_BLOCK)+1;

	// Max size of blocks options
	int num_of_blocks = (max_k*max_n/THREADS_PER_BLOCK)+1;
	// Set y grid axis
	grid.y = 1;
	// Set z grid axis 
	grid.z = 1;
	// Set x grid axis 
	grid.x = num_of_blocks;
	
	// Set y block axis to max
	block.y = 1;
	// Set z block axis to max
	block.z = 1;
	// Set x block axis to max
	block.x = THREADS_PER_BLOCK;
	// Allocate space for results on GPU and set to zero (why zero here ??? i did async)
	float* gpu_results;
	cudaMalloc(&gpu_results,max_k*max_n*sizeof(float));
	cudaMemsetAsync(gpu_results,0,max_k*max_n*sizeof(float));
	gpu_calc_str_weight<<<grid,block>>>(gpu_sequence1,sequence1_length,gpu_sequence2,sequence2_length,max_k,max_n,
			gpu_results,gpu_weights,gpu_conservative_group,conservative_group_size,gpu_semi_conservative_group,
			semi_conservative_group_size,max_k*max_n);
	//Cuda barrier
	cudaDeviceSynchronize();
	// Allocate space for results on CPU
	float* cpu_results = (float*)calloc(max_k*max_n,sizeof(float));
	// Copy result from gpu to cpu
	cudaMemcpy(cpu_results,gpu_results,max_k*max_n*sizeof(float),cudaMemcpyDeviceToHost);
	find_best_k_n(cpu_results,max_k,max_n,&k,&n);
	//Write best k in first cell in result CPU array
	results[0] = k;
	//Write best n in second cell in result CPU array
	results[1] = n;

	//Free GPU resource
	cudaFree(gpu_sequence1);
	cudaFree(gpu_sequence2);
	cudaFree(gpu_weights);
	cudaFree(gpu_conservative_group);
	cudaFree(gpu_semi_conservative_group);
	cudaFree(gpu_results);
	//Free CPU resource 
	free(cpu_results);
}
