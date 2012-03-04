all: project

project: tftp_server.o
	gcc tftp_server.o -o project

tftp_server.o: tftp_server.c
	gcc -c tftp_server.c

clean: 
	rm -rf *o project
