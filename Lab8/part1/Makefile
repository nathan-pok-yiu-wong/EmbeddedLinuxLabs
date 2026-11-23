INCLUDES=-I../include
FLAGS=-Wall

part1:	part1.c audio.c
	gcc $(FLAGS) $(INCLUDES) -o part1 part1.c audio.c ../physical.c -lm

.PHONY:	clean

clean:
	rm -f part1
