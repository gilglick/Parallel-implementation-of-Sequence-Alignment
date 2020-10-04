#ifndef SRC_FUNCS_H_
#define SRC_FUNCS_H_

#define TAG 0

#include <mpi.h>
#include "structs.h"

extern const int PRIME_NUMBERS[];
void init_prime_array(int **conservatives_primes, int *length,
		int **semi_conservatives_primes, int *semi_length);

void from_char_to_prime(const char **group, int *primed_group, int length);
void init_prime_array(int **conservatives_primes, int *length,
		int **semi_conservatives_primes, int *semi_length);
int read_file(char **sequence1, char ***sequence2_array, int *sequence2_array_length, weights_t *weights);
int write_results(result_t* results, int old_num_of_s2);
void send_sequence(const char *sequence, int tag, int p);
void send_sequence_array(const char **sequence2_array, int length, int tag,
		int p);
void send_weights(const weights_t *weights, int tag, int p);
void send_sequence_to_all_slaves(const char *sequence, int tag, int p);
void send_divide_sequence_array_to_all_slaves(const char **sequence2_array, int length,
		int tag, int p);
void send_sequence_array_to_all(const char **sequence2_array, int length, int tag,
		int p);
void send_n_and_k(int k, int n, int tag, int dest);
void receive_weights(weights_t *weights, int tag, int source, MPI_Status *status);
void receive_n_and_k(int *k, int *n, int tag, int source, MPI_Status *status);
char* receive_sequence(int tag, int source, MPI_Status *status);
char** receive_sequence_array(int tag, int source, MPI_Status *status, int *length);
void send_to_slaves(const char *sequence1, const char **sequence2_array, int sequence2_array_length,
		const weights_t *weights, int p);
void receive_from_master(char **sequence1, char ***sequence2_array, MPI_Status *status,
		int *num_of_s2, weights_t *weights);

#endif /* SRC_GROUPS_H_ */
