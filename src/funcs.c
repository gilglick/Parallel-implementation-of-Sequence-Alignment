#include <string.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include "funcs.h"

const int conservative_size = 9;
int conservatives_primed_group[conservative_size];
const int semi_conservative_size = 11;
int semi_conservatives_primed_group[semi_conservative_size];

const int PRIME_NUMBERS[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41,
		43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97 };

const char *string_conservative_group[9] = { "NDEQ", "NEQK", "STA", "MILV",
		"QHRK", "NHQK", "FYW", "HY", "MILF" };

const char *string_semi_conservative_group[] = { "SAG", "ATV", "CSA", "SGND",
		"STPA", "STNK", "NEQHRK", "NDEQHK", "SNDEQK", "HFY", "FVLIM" };

void from_char_to_prime(const char **group, int *primed_group, int length) {
	int offset = 'A';
#pragma omp parallel for
	for (int i = 0; i < length; i++) {
		primed_group[i] = 1;
		int len = strlen(group[i]);
		for (int j = 0; j < len; j++) {
			primed_group[i] *= PRIME_NUMBERS[toupper(group[i][j]) - offset];
		}
	}
}

/*init prime array*/
void init_prime_array(int **conservatives_primes, int *length,
		int **semi_conservatives_primes, int *semi_length) {
	*length = conservative_size;
	*semi_length = semi_conservative_size;
	from_char_to_prime(string_conservative_group, conservatives_primed_group,
			conservative_size);
	from_char_to_prime(string_semi_conservative_group,
			semi_conservatives_primed_group, semi_conservative_size);
	*conservatives_primes = conservatives_primed_group;
	*semi_conservatives_primes = semi_conservatives_primed_group;
}

/*read file to temp buffer and then to sequence1 and array of sequence 2*/
int read_file(char **sequence1, char ***sequence2, int *sequence2_length,
		weights_t *weights) {
	static char buffer[3000];
	FILE *file = fopen("input.txt", "r");
	if (!file)
		return 0;
	fscanf(file, "%f%f%f%f", &weights->w1, &weights->w2, &weights->w3,
			&weights->w4);
	fscanf(file, "%s", buffer);
	*sequence1 = (char*) malloc(strlen(buffer));
	strcpy(*sequence1, buffer);
	fscanf(file, "%d", sequence2_length);
	*sequence2 = (char**) malloc(sizeof(char*) * (*sequence2_length));
	for (int s = 0; s < *sequence2_length; s++) {
		fscanf(file, "%s", buffer);
		(*sequence2)[s] = (char*) malloc(strlen(buffer));
		strcpy((*sequence2)[s], buffer);
	}
	return 1;
}
/*write result to file*/
int write_results(result_t* results, int old_num_of_s2){
	FILE* file = fopen("output.txt", "w");
	if (!file)
		return 0;
	for (int i = 0; i < old_num_of_s2; i++)
		fprintf(file,"sequnce number %d: Best n:   %d, k:   %d\n", i + 1, results[i].n,
				results[i].k);
	fclose(file);
	return 1;
}
/*send sequence to another process*/
void send_sequence(const char *str, int tag, int proccess) {
	int length = strlen(str);
	MPI_Send(&length, 1, MPI_INT, proccess, tag,
	MPI_COMM_WORLD);
	MPI_Send(str, length, MPI_CHAR, proccess, tag,
	MPI_COMM_WORLD);
}
/*send every sequence to another process for every sequence in sequence array*/
void send_sequence_array(const char **str_array, int length, int tag,
		int proccess) {
	MPI_Send(&length, 1, MPI_INT, proccess, tag,
	MPI_COMM_WORLD);
	for (int i = 0; i < length; i++) {
		send_sequence(str_array[i], tag, proccess);
	}
}
/*send weights to another process*/
void send_weights(const weights_t *weights, int tag, int p) {
	float temp[] = { weights->w1, weights->w2, weights->w3, weights->w4 };
	for (int i = 1; i < p; i++) {
		MPI_Send(temp, 4, MPI_FLOAT, i, tag,
		MPI_COMM_WORLD);
	}
}
/*send sequence to slave*/
void send_sequence_to_all_slaves(const char *str, int tag, int p) {
	int length = strlen(str);
	for (int i = 1; i < p; i++) {
		send_sequence(str, tag, i);
	}
}
/*send part of sequence 2 array to every slave*/
void send_divide_sequence_array_to_all_slaves(const char **str_array,
		int length, int tag, int p) {
	int num_to_send = length / p;
	str_array += num_to_send + (length % p);
	for (int i = 1; i < p; i++) {
		send_sequence_array(str_array + (i - 1) * num_to_send, num_to_send, tag,
				i);
	}
}

void send_sequence_array_to_all(const char **str_array, int length, int tag,
		int p) {
	for (int i = 1; i < p; i++) {
		send_sequence_array(str_array, length, tag, i);
	}
}
/*send n and k to slave*/
void send_n_and_k(int k, int n, int tag, int to) {
	MPI_Send(&k, 1, MPI_INT, to, tag, MPI_COMM_WORLD);
	MPI_Send(&n, 1, MPI_INT, to, tag, MPI_COMM_WORLD);
}
/*receive all weights from master*/
void receive_weights(weights_t *weights, int tag, int from, MPI_Status *status) {
	float temp[4];
	MPI_Recv(temp, 4, MPI_FLOAT, from, tag,
	MPI_COMM_WORLD, status);
	weights->w1 = temp[0];
	weights->w2 = temp[1];
	weights->w3 = temp[2];
	weights->w4 = temp[3];
}
/*receive n and k from master*/
void receive_n_and_k(int *k, int *n, int tag, int from, MPI_Status *status) {
	MPI_Recv(k, 1, MPI_INT, from, tag, MPI_COMM_WORLD, status);
	MPI_Recv(n, 1, MPI_INT, from, tag, MPI_COMM_WORLD, status);
}
/*receive sequence from master*/
char* receive_sequence(int tag, int from, MPI_Status *status) {
	int length;
	MPI_Recv(&length, 1, MPI_INT, from, tag,
	MPI_COMM_WORLD, status);
	char *str = (char*) malloc(length + 1);
	MPI_Recv(str, length, MPI_CHAR, from, tag,
	MPI_COMM_WORLD, status);
	str[length] = 0;
	return str;
}
/*receive sequence array from master*/
char** receive_sequence_array(int tag, int from, MPI_Status *status,
		int *length) {
	MPI_Recv(length, 1, MPI_INT, from, tag,
	MPI_COMM_WORLD, status);
	char **result = (char**) malloc(sizeof(char*) * (*length));
	for (int i = 0; i < *length; i++) {
		result[i] = receive_sequence(tag, from, status);
	}
	return result;
}
/*send all to slave*/
void send_to_slaves(const char *s1, const char **s2, int num_of_s2,
		const weights_t *weights, int p) {
	send_weights(weights, TAG, p);
	send_sequence_to_all_slaves(s1, TAG, p);
	send_divide_sequence_array_to_all_slaves(s2, num_of_s2, TAG, p);

}

/*receive all from master*/
void receive_from_master(char **s1, char ***s2, MPI_Status *status,
		int *num_of_s2, weights_t *weights) {
	receive_weights(weights, TAG, 0, status);
	*s1 = receive_sequence(TAG, 0, status);
	*s2 = receive_sequence_array(TAG, 0, status, num_of_s2);
}
