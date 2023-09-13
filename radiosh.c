/*

radiosh -- OS X shell control for the Griffin radioSHARK v1/v2 with IOKit
1.0

Based on rslight by Quentin D. Carnicelli (qdc@rogueamoeba.com) and shark by
Michael Rolig (michael_rolig@alumni.macalester.edu) and Justin Yunke
(yunke@productivity.org) and shark2 by Hisaaki Shibata (shibata@luky.org)

Merged by Cameron Kaiser (ckaiser@floodgap.com), cleaned up, warnings
quashed, bugs fixed, and converted to my personal house style.

Copyright (C)2018 Contributors as enumerated. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   *    Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

   *    Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

   *    Neither the name of the product nor the names of its contributors
        may be used to endorse or promote products derived from this software
        without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  

*/

#include <stdio.h>
#include <getopt.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USBSpec.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>

enum {
	kRadioSharkVendorID  = 0x077D,
	kRadioSharkProductID = 0x627A,
	kRadioSharkv1Version = 0x0001,
	kRadioSharkv2Version = 0x0010,
};

typedef struct {
	char fBlueLightLevel;
	char fBlueLightPulse;
	char fRedLightLevel;
	char fRadioBand;
	unsigned char fRadioFreqHi;
	unsigned char fRadioFreqLo;
} rsSettings;

io_name_t deviceName; // "char[128]"
int mode = 0; // 0 = radioshark, non-zero = radioShark 2
int verbose = 0; // 0 = no

void _printUsage()
{
	FILE* out = stderr;

	fprintf(out, "Usage: radiosh [-v] [-b#] [-p#] [-r#] [-a#] [-f#]\n");
	fprintf(out, "    -v    Verbosity/display detected device version.\n");
	fprintf(out, "    -b    Set the blue light brightness, values are 0 (off) to 127.\n");
	fprintf(out, "    -p    Set the blue light pulse speed, values are 0 (off) to 127 (slow).\n");
	fprintf(out, "    -r    Set the red light brightness, values are 0 (off) to 127.\n");
	fprintf(out, "    -a    Set the radio to AM and tune to frequency in kHz (0=radio off).\n");
	fprintf(out, "    -f    Set the radio to FM and tune to frequency in MHz (0=radio off).\n");
	fprintf(out, "Any combination can be specified at once.\n");
	fprintf(out, "\n");
	fprintf(out, "Copyright (C) 2018 Cameron Kaiser, Quentin D. Carnicelli,\n");
	fprintf(out, "Michael Rolig, Justin Yunke and Hisaaki Shibata. All rights reserved.\n");
	fprintf(out, "http://www.floodgap.com/software/radiosh/ -- version 1.0\n");
	exit(1);
}

int _atoi(const char *s)
{
	if (!isdigit(s[0])) {
		_printUsage();
		return -1; // unreached
	}
	return atoi(s);
}

int _parseArguments(int argc, char **argv, rsSettings* settings)
{
	int opt;
	int arg;
	float freq;
	
	if(!settings)
		return 0;
	
	if(argc < 2)
		return 0;
			
	while((opt = getopt(argc, (char* const*)argv, "b:p:r:a:f:?hv")) != -1)
	{
		switch(opt)
		{
			case 'v':
				verbose = 1;
				fprintf(stderr, "radioSHARK version detected: %s\n",
					(mode) ? "v2" : "v1");
			break;
				
			case 'b':
				arg = _atoi(optarg);
				arg = arg < 0 ? 0 : arg > 127 ? 127 : arg;
				settings->fBlueLightLevel = arg;
			break;
			
			case 'p':
				arg = _atoi(optarg);
				arg = arg < 0 ? 0 : arg > 127 ? 127 : arg;
				settings->fBlueLightPulse = arg;
			break;
		
			case 'r':
				arg = _atoi(optarg);
				arg = arg < 0 ? 0 : arg > 127 ? 127 : arg;
				settings->fRedLightLevel = arg;
			break;
			
			case 'a':
				if (mode) {
					arg = (_atoi(optarg) * 4) + 16300;
					settings->fRadioBand = 0x24;
				} else {
					arg = _atoi(optarg) + 450;
					settings->fRadioBand = 0x12;
				}
				settings->fRadioFreqHi = (arg >> 8) & 0xff;
				settings->fRadioFreqLo = (arg & 0xff);
			break;
			
			case 'f':
				// This is a float, so we can't use our
				// convenience function.
				if (!isdigit(optarg[0])) {
					_printUsage();
					return 0; // unreached
				}
				freq = atof(optarg);
				if (mode) {
					arg = ((freq * 10 * 2) - 3);
					settings->fRadioBand = 0x28;
				} else {
					arg  = ((freq * 1000) + 10700) / 12.5;
					arg += 3;
					settings->fRadioBand = 0x00;
				}
				settings->fRadioFreqHi = (arg >> 8) & 0xff;
				settings->fRadioFreqLo = (arg & 0xff);
			break;
						
			case '?':
			case 'h':
				_printUsage();
				
			default:
				return 0;
		}
	}

	return 1;

}

CFMutableDictionaryRef _getMatchingDictionary(UInt16 version)
{
	CFMutableDictionaryRef matchingDict = NULL;
	int val;
	CFNumberRef valRef;

	matchingDict = IOServiceMatching(kIOHIDDeviceKey);
	if(matchingDict)
	{
		val = kRadioSharkVendorID;
		valRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &val);
		CFDictionarySetValue(matchingDict, CFSTR(kIOHIDVendorIDKey), valRef);
		CFRelease(valRef);

		val = kRadioSharkProductID;
		valRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &val);
		CFDictionarySetValue(matchingDict, CFSTR(kIOHIDProductIDKey), valRef);
		CFRelease(valRef);
		
		valRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt16Type, &version);
		CFDictionarySetValue(matchingDict, CFSTR(kIOHIDVersionNumberKey), valRef);
		CFRelease(valRef);
	}
	return matchingDict;

}

io_service_t _getIOService(CFMutableDictionaryRef matchingDict)
{
	if(!matchingDict) return (io_service_t)NULL;
	return IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);
}

IOHIDDeviceInterface** _getHIDInterface(io_service_t service)
{
	IOCFPlugInInterface** iodev = NULL;
	IOHIDDeviceInterface** hidInterface = NULL;
	kern_return_t result;
	SInt32 score = 0;

	if(!service) return NULL;

	result = IOCreatePlugInInterfaceForService(service, kIOHIDDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &iodev, &score);
	if (result == KERN_SUCCESS && iodev)
	{
		result = (*iodev)->QueryInterface(iodev, CFUUIDGetUUIDBytes(kIOHIDDeviceInterfaceID), (LPVOID) &hidInterface);
		if (result != KERN_SUCCESS)
			hidInterface = NULL;

		(*iodev)->Release(iodev);
	}
	
	return hidInterface;
}

/* The v1 "RadioSHARK" uses a 6-byte HID report packet.
   The v2 "radioSHARK" (note string) uses a 7-byte packet. */

kern_return_t _setBlueLight(IOHIDDeviceInterface** hidInterface, char level)
{
	if (mode) {
		char report[7] = { 0x83, level, 0, 0, 0, 0, 0 };
		if (level < 0) return KERN_SUCCESS;
		return (*hidInterface)->setReport(hidInterface, kIOHIDReportTypeOutput, 0, report, sizeof(report), 1000, NULL, NULL, NULL);
	}
	char report[6] = { 0xa0, level, 0, 0, 0, 0 };
	if (level < 0) return KERN_SUCCESS;
	return (*hidInterface)->setReport(hidInterface, kIOHIDReportTypeOutput, 0, report, sizeof(report), 1000, NULL, NULL, NULL);
}

kern_return_t _setBluePulse(IOHIDDeviceInterface** hidInterface, char level)
{
	if (mode) {
		if (level < 0) return KERN_SUCCESS;

		// Tried both 0xa1 and 0x82 and they don't do anything.
		fprintf(stderr, "Oops: pulsing not supported on v2 devices.\n");
		return KERN_INVALID_ARGUMENT;
	}
	char report[6] = { 0xa1, level, 0, 0, 0, 0 };
	if (level < 0) return KERN_SUCCESS;
	return (*hidInterface)->setReport(hidInterface, kIOHIDReportTypeOutput, 0, report, sizeof(report), 1000, NULL, NULL, NULL);
}

kern_return_t _setRedLight(IOHIDDeviceInterface** hidInterface, char level)
{
	if (mode) {
		char report[7] = { 0x84, level, 0, 0, 0, 0, 0 };
		if (level < 0) return KERN_SUCCESS;
		return (*hidInterface)->setReport(hidInterface, kIOHIDReportTypeOutput, 0, report, sizeof(report), 1000, NULL, NULL, NULL);
	}
	char report[6] = { (level > 0 ? 0xa9 : 0xa8), 0, 0, 0, 0, 0 };
	if (level < 0) return KERN_SUCCESS;
	return (*hidInterface)->setReport(hidInterface, kIOHIDReportTypeOutput, 0, report, sizeof(report), 1000, NULL, NULL, NULL);
}

kern_return_t _setRadio(IOHIDDeviceInterface** hidInterface, rsSettings* settings)
{
	if (mode) {
		char report[7] = { 0x81, settings->fRadioFreqHi, settings->fRadioFreqLo,
			(settings->fRadioBand == 0x24) ? 0xf3 : 0x33,
			(settings->fRadioBand == 0x24) ? 0x36 : 0x04,
			0x00,
			settings->fRadioBand };
		if (settings->fRadioBand < 0) return KERN_SUCCESS;
		return (*hidInterface)->setReport(hidInterface, kIOHIDReportTypeOutput, 0, report, sizeof(report), 1000, NULL, NULL, NULL);
	}
	char report[6] = { 0xc0, settings->fRadioBand, settings->fRadioFreqHi, settings->fRadioFreqLo, 0, 0 };
	if (settings->fRadioBand < 0) return KERN_SUCCESS;
	return (*hidInterface)->setReport(hidInterface, kIOHIDReportTypeOutput, 0, report, sizeof(report), 1000, NULL, NULL, NULL);
}	

/* Laziness */
#define DAMMIT(x, y, z) \
	x = y; if (!x) { fprintf(stderr, z); return -1; }
#define KDAMMIT(y, z, ...) \
	result = y; if (result != KERN_SUCCESS) { fprintf(stderr, z, __VA_ARGS__); return -1; }

int main(int argc, char **argv)
{
	rsSettings settings = { -1, -1, -1, -1, 0, 0 };
	CFMutableDictionaryRef matchingDict = NULL;
	io_service_t service = (io_service_t)NULL;
	IOHIDDeviceInterface** hidInterface = NULL;
	kern_return_t result;
	
	if (argc == 1 || (argc == 2 && argv[1][0] == '-' && argv[1][1] == 'v')) {
		_printUsage();
		return 1;
	}

	/* XXX: If you have multiple devices connected, one will be opened at random.
	   If one is a v1 and the other is a v2, the v1 will get precedence. */

	DAMMIT(matchingDict, _getMatchingDictionary(kRadioSharkv1Version),
		"InternalError: Could not create io service matching dictionary v1\n");
	if (!(service = _getIOService(matchingDict))) {
		DAMMIT(matchingDict, _getMatchingDictionary(kRadioSharkv2Version),
			"InternalError: Could not create io service matching dictionary v2\n");
		DAMMIT(service, _getIOService(matchingDict),
			"IOError: Could not find attached radioSHARK v1 or v2 device\n");
		mode = 1;
	}
	DAMMIT(hidInterface, _getHIDInterface(service),
		"IOError: Could find the HID interface of the radioSHARK device\n");
		
	if(!_parseArguments(argc, argv, &settings)) {
		_printUsage();
		return 1;
	}

	KDAMMIT((*hidInterface)->open(hidInterface, 0),
		"IOError: Could open the HID interface of the radioSHARK device (%d)\n", result);
	KDAMMIT(_setBlueLight(hidInterface, settings.fBlueLightLevel),
		"IOError: Setting the blue light failed (%d)\n", result);
	KDAMMIT(_setBluePulse(hidInterface, settings.fBlueLightPulse),
		"IOError: Setting the blue light pulse failed (%d)\n", result);
	KDAMMIT(_setRedLight(hidInterface, settings.fRedLightLevel),
		"IOError: Setting the red light failed (%d)\n", result);
	KDAMMIT(_setRadio(hidInterface, &settings),
		"IOError: Setting the radio failed (%d)\n", result);

	(*hidInterface)->close(hidInterface);
	(*hidInterface)->Release(hidInterface);
	IOObjectRelease(service);

	return 0;
}
