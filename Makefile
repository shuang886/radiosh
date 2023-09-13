default:
	@echo Do 'make radiosh32' for 32-bit or 'make radiosh64' for 64-bit.

radiosh32: clean
	gcc -arch ppc -arch i386 -o radiosh radiosh.c -Os -framework CoreFoundation -framework IOKit -isysroot /Developer/SDKs/MacOSX10.4u.sdk

radiosh64: clean
	cc -o radiosh radiosh.c -Os -framework CoreFoundation -framework IOKit

install:
	install -c -d /usr/local/bin/
	install -c -m 755 radiosh /usr/local/bin/

clean:
	rm -f radiosh

