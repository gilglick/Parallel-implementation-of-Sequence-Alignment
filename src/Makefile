build:
	mpicxx -fopenmp -c funcs.c -o funcs.o
	mpicxx -fopenmp -c main.c -o main.o
	nvcc -I./inc -Xcompiler -lineinfo -c cudaFunctions.cu -o cudaFunctions.o
	mpicxx -fopenmp -o mpiCudaOpemMP funcs.o main.o cudaFunctions.o  /usr/local/cuda/lib64/libcudart_static.a -ldl -lrt

all: build

clean:
	rm -f *.o ./mpiCudaOpemMP

run:
	mpiexec -np 2 ./mpiCudaOpemMP

runOn2:
	mpiexec -np 2 -machinefile  mf  -map-by  node  ./mpiCudaOpemMP
