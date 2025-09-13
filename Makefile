all:
	mkdir -p build
	gcc -static main.c -o build/curl

clean:
	rm -rf build