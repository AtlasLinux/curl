all:
	mkdir -p build
	gcc -L../../lib src/main.c -o build/curl -lssl -lcrypto

clean:
	rm -rf build