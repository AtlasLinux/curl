all:
	mkdir -p build
	gcc -L../../lib/libcurl/build src/main.c -o build/curl -lcurl -L../../lib -lssl

clean:
	rm -rf build