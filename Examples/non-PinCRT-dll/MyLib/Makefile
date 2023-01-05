libmydll.so:mydll.c
	gcc -g -c -Wall -Werror -fpic mydll.c 
	gcc -g -shared mydll.o -o libmydll.so
clean:
	rm -f *.o *.so
