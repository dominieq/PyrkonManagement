all: pyrkon

pyrkon: main.o init.o
	mpicc main.o init.o -o pyrkon.out

init.o: init.c
	mpicc init.c -c -Wall

main.o: main.c main.h
	mpicc main.c -c -Wall

clear:
	rm *.o pyrkon.out

run:
	mpirun -np 2 ./pyrkon.out
