default:
	@echo "Do 'make radiosh32' for 32-bit, 'make radiosh64' for 64-bit, or 'make universal' for a universal ARM/x86 binary."

radiosh32: clean
	gcc -arch ppc -arch i386 -o radiosh radiosh.c -Os -framework CoreFoundation -framework IOKit -isysroot /Developer/SDKs/MacOSX10.4u.sdk

radiosh64: clean
	cc -o radiosh radiosh.c -Os -framework CoreFoundation -framework IOKit

radio_x86_64: clean
	cc -o radiosh_x86 radiosh.c -Os -framework CoreFoundation -framework IOKit -target x86_64-apple-macos10.4

radio_arm64: clean
	cc -o radiosh_arm radiosh.c -Os -framework CoreFoundation -framework IOKit -target arm64-apple-macos10.4

universal: radio_x86_64 radio_arm64
	lipo -create -output radiosh radiosh_x86 radiosh_arm

install:
	install -c -d /usr/local/bin/
	install -c -m 755 radiosh /usr/local/bin/

clean:
	rm -f radiosh radiosh_x86 radiosh_arm
