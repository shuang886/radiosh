# rslight
# Makefile
#
# Quentin D. Carnicelli - qdc@rogueamoeba.com

rslight:
	cc rslight.c -Wall -W -Os -framework CoreFoundation -framework IOKit -o rslight

all: rslight

install:
	install -c -d /usr/local/bin/
	install -c -m 755 rslight /usr/local/bin/

clean:
	rm -f rslight

