all : pa3_server.out pa3_client.out

pa3_server.out : server.c
	gcc -o pa3_server.out server.c -lpthread

pa3_client.out : client.c
	gcc -o pa3_client.out client.c -lpthread

clean :
	rm -rf *.out *.o
