sequential-compile:
	gcc -Wall -o target/sequential sequential.c  -lm -lcrypto -lgomp 

sequential-sample: sequential-compile
	./target/sequential in/sample_inputs_hex.txt 7 4

pthreads-compile:
	gcc -pthread -Wall -O2 -o target/p-main pthreads.c -lm -lcrypto -lgomp 

pthreads-sample: pthreads-compile
	./target/p-main in/sample_inputs_hex.txt 7 4 4

pthreads-benchmark-all: pthreads-compile
	./target/p-main in/benchmark_merged.txt 7 16 4

mpi-compile:
	mpiCC -o target/mpi-main mpi-main.cpp  -lm -lcrypto

mpi-test:
	mpiexec ./target/mpi-main in/sample_inputs_hex.txt 4

mpi:
	make mpi-compile && make mpi-test

omp-compile:
	g++ –o target/omp-main omp.cpp -lcrypto –fopenmp

mpi-test:
	mpiexec ./target/omp-main in/sample_inputs_hex.txt 4

omp:
	make omp-compile && make omp-test
