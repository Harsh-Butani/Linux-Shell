all: compile

compile: 
		gcc -o shell shell.c

clean:
		rm -rf shell