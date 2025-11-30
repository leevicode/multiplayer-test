objects = main server
flags = -Iinclude -Llib -lm -lraylib -lenet -Wl,-rpath=lib -O0 -ggdb

main:
	gcc main.c -o main $(flags)

server:
	gcc server.c -o server $(flags)

clean:
	 rm $(objects)
