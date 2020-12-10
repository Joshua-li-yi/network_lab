.PHONY:test

all: server client
server:
	g++ .\source\server.cpp -o .\bin\server.exe -lwsock32

client:
	g++ .\source\client.cpp -o .\bin\client.exe -lwsock32

test:
	g++ .\source\test.cpp -o .\test.exe
	.\test.exe

clean:
	rm -rf .\bin\

runs:
	.\bin\server.exe

runc:
	.\bin\client.exe