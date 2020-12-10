.PHONY:test

all2: server2 client2

all: server client

server:
	g++ .\source\lab3-1\server.cpp -o .\bin\server.exe -lwsock32

server2:
	g++ .\source\lab3-2\server.cpp -o .\bin\server2.exe -lwsock32

client:
	g++ .\source\lab3-1\client.cpp -o .\bin\client.exe -lwsock32

client2:
	g++ .\source\lab3-2\client.cpp -o .\bin\client2.exe -lwsock32

test:
	g++ .\source\test.cpp -o .\bin\test.exe
	.\bin\test.exe

clean:
	rm -rf .\bin\

runs:
	.\bin\server.exe

runs2:
	.\bin\server2.exe

runc:
	.\bin\client.exe

runc2:
	.\bin\client2.exe