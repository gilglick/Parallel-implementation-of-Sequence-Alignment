#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "mpi.h"
#include "funcs.h"
#include "myProto.h"

/*conservative and semi conservative int arrays */
int *conservative_group, conservative_group_size, *semi_conservative_group,
		semi_conservative_group_size;

int main(int argc, char *argv[]) {
	int my_rank; /* rank of process */
	int p; /* number of processes */
	MPI_Status status; /* return status for receive */
	result_t *results;

	/*sequence 1 string and sequence 2 string array*/
	char *sequence1, **sequence2;
	/*number of sequences from type 2 for every process and total number of sequences*/
	int number_of_sequnce2_length, old_num_of_s2;
	/* struct weights_t array */
	weights_t weights = { 0, 0, 0, 0 };

	/* start up MPI */
	MPI_Init(&argc, &argv);

	/* find out process rank */
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	/* find out number of processes */
	MPI_Comm_size(MPI_COMM_WORLD, &p);

	/*init conservative and semi conservative int arrays - calculate every string multiplication*/
	init_prime_array(&conservative_group, &conservative_group_size,
			&semi_conservative_group, &semi_conservative_group_size);
	/*master read file, send it to slave and takes over the sequences2 remainder */
	if (my_rank == 0) {
		if (!read_file(&sequence1, &sequence2, &number_of_sequnce2_length, &weights)) {
			fprintf(stderr, "Could not open file!\n");
			MPI_Abort(MPI_COMM_WORLD, 1);
			return -1;
		}
		results = (result_t*) malloc(sizeof(result_t) * number_of_sequnce2_length);
		send_to_slaves((const char*) sequence1, (const char**) sequence2,
				number_of_sequnce2_length, &weights, p);
		old_num_of_s2 = number_of_sequnce2_length;
		number_of_sequnce2_length = number_of_sequnce2_length / p + (number_of_sequnce2_length % p);
		/*slave wait for his master to send him the data */
	} else {
		receive_from_master(&sequence1, &sequence2, &status, &number_of_sequnce2_length,
				&weights);
	}
/*every process runs cuda for every sequence 2 to get results*/
	for (int i = 0; i < number_of_sequnce2_length; i++) {
		int cuda_results[2];
		get_best_results_with_CUDA(sequence1, strlen(sequence1), sequence2[i],
				strlen(sequence2[i]), &weights, cuda_results,
				conservative_group, conservative_group_size,
				semi_conservative_group, semi_conservative_group_size);
		/*slave send the best n and k to his master*/
		if (my_rank != 0) {
			send_n_and_k(cuda_results[0], cuda_results[1], TAG, 0);
			/*master put his results in results struct*/
		} else {
			results[i].k = cuda_results[0];
			results[i].n = cuda_results[1];
		}
	}
	/*master get every slave results for every sequence from type 2 in results structs*/
	if (my_rank == 0) {
		int number_of_sequnce_per_p = old_num_of_s2 / p;
		for (int i = 1; i < p; i++) {
			for (int j = 0; j < number_of_sequnce_per_p; j++) {
				result_t *res = results + number_of_sequnce2_length + j + (i - 1) * number_of_sequnce_per_p;
				receive_n_and_k(&res->k, &res->n, TAG, i, &status);
			}
		}
		/*master writes result to "output.txt" */
		write_results(results,old_num_of_s2);
		free(results);
	}
	free(sequence1);
	for (int i = 0; i < number_of_sequnce2_length; i++)
		free(sequence2[i]);
	free(sequence2);
	/* shut down MPI */
	MPI_Finalize();
	return 0;
}
