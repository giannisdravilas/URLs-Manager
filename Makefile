ALL: sniffer

sniffer: manager.o worker.o
	g++ manager.o worker.o -o sniffer -lpthread

manager.o: manager.cpp
	g++ -g -Wall -c manager.cpp

worker.o: worker.cpp
	g++ -g -Wall -c worker.cpp

run: sniffer
	./sniffer

clean:
	rm -f sniffer manager.o worker.o