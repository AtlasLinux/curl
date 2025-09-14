all:
	mkdir -p build
	gcc src/main.c -o build/curl -lssl -lcrypto

clean:
	rm -rf build