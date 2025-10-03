all:
	mkdir -p build
	gcc -L../../lib/libcurl/build src/main.c -o build/curl -lcurl

clean:
	rm -rf build