all:
	mkdir -p build
	gcc main.c -o build/curl

clean:
	rm -rf build