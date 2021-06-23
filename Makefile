main : tfind.c
	gcc tfind.c -o tfind -pthread

clean :
	rm tfind
