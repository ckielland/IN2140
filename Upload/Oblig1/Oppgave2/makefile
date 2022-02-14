all: oppgave1 oblig1_tests

oppgave1: oppgave1.c
	gcc -g oppgave1.c -o oppgave1

oblig1_tests: oblig1_tests.c o2.o
	gcc oblig1_tests.c o2.o -o oppgave2

o2: oppgave2.c
	gcc oppgave2.c -o o2

clean:
	rm -f oppgave1 oppgave2 o2
