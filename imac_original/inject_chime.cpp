#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <stdint.h>

// By Doug Brown (a.k.a. dougg3)
// Public domain, do whatever you want with this. I wrote all the code, borrowing a few
// algorithms from different sources. Only thing I didn't write was md5.c and md5.h -- see
// those files for the terms and conditions of using the MD5 code.

#include "../util/md5.h"
#include "../util/ascii85.h"
#include "../util/adler32.h"
#include "../util/ima.h"

// TODO: Allow big or little endian raw data sound files (configured with a flag)
// TODO: Allow reading of .AIFF or .WAV files
// TODO: Allow saving the IMA-encoded chime as .AIFC so user can verify it sounds right

using namespace std;

// Used to ensure that the MD5 of the "iMac Firmware 3.0" file checks out
static const char *IMAC_FIRMWARE_30_MD5 = "702c51c05f59fb751e5dcfb5b194fba3";

// Location of the ROM image inside the firmware file
#define ROM_IMAGE_OFFSET					0x70192
#define ROM_IMAGE_END_OFFSET				0xDCC6F
#define ROM_IMAGE_ADLER_LENGTH				0x7FFFC
#define ROM_IMAGE_ADLER_POS					0xDCCCD
#define FIRMWARE_ADLER_POS_BACK				9
#define FIRMWARE_ADLER_END_POS_BACK			14

// Formatting for the outputted iMac Firmware 3.0 file
#define FIRMWARE_COLUMN_WIDTH	100

// Info about the sound stored in the ROM image
#define SOUND_ROM_IMAGE_OFFSET	0x43C50
#define NUM_SOUND_PACKETS		1722
#define BYTES_PER_PACKET		34
#define SAMPLES_PER_PACKET		64
#define BYTES_PER_SAMPLE		2 /* 16 bits */
#define SOUND_SAMPLES_MAX		(NUM_SOUND_PACKETS * SAMPLES_PER_PACKET)
#define SOUND_MAX_SIZE			(SOUND_SAMPLES_MAX * BYTES_PER_SAMPLE)
#define SOUND_COMPRESSED_SIZE	(NUM_SOUND_PACKETS * BYTES_PER_PACKET)

static string programName; // name of program as called
static string firmwareFileBuf; // the entire "iMac Firmware 3.0" file
static string soundFileBuf; // the provided sound file
static string compressedSoundBuf; // the compressed sound data
static string romDataBuf; // decoded ROM image
static ofstream outFile; // file we write the patched firmware to
static char tmpBuf[1024]; // temporary buffer for reading

// Declarations of functions
void loadFile(const char *filename, string &buf); // loads complete contents of file into the given buffer
void loadFirmwareFile(const char *filename); // loads the firmware file, verifies, and decodes it
void loadSoundFile(const char *filename); // loads the new sound chime and encodes it in IMA 4:1 format
void openOutputFile(const char *filename); // prepares for saving new firmware by opening output file
void injectChime(); // sticks the new sound in place, recalculates checksums, encodes, saves new firmware
void exitPrintUsage(); // exits with a message showing how to use the program

int main(int argc, char *argv[])
{
	programName = argv[0];

	// Need an exact number of arguments
	if (argc != 4)
	{
		exitPrintUsage();
	}
	
	// Make sure that the first file ("iMac Firmware 3.0") is the correct
	// file, and if it is, load it and decode it
	loadFirmwareFile(argv[1]);
	
	// Make sure the second file (raw audio) is not too big, and
	// convert it to IMA 4:1
	loadSoundFile(argv[2]);	
	
	// Open output file and make sure we're good to go
	openOutputFile(argv[3]);
	
	// Do the work -- inject sound, encode, fix checksums, save
	injectChime();
	
	// Success!
	cout << "Successfully injected new startup chime." << endl;
	
	return 0;
}

void loadFile(const char *filename, string &buf)
{
	// Open the file
	ifstream firmwareFile;
	firmwareFile.open(filename, ios::in | ios::binary);
	
	// Make sure it opened successfully
	if (!firmwareFile.is_open())
	{
		cerr << "Unable to open file \"" << filename << "\"" << endl;
		exitPrintUsage();
	}
	
	// Read the entire file into "buf"
	while (firmwareFile.good())
	{
		firmwareFile.read(tmpBuf, sizeof(tmpBuf));
		int numRead = firmwareFile.gcount();
		buf.append(tmpBuf, numRead);
	}
	
	if (!firmwareFile.eof())
	{
		// Some other error occurred...
		cerr << "Unable to read file \"" << filename << "\"" << endl;
		exitPrintUsage();
	}
	
	// Clear error flags
	firmwareFile.clear();
}

void loadFirmwareFile(const char *filename)
{
	loadFile(filename, firmwareFileBuf);

	// Verify the md5 of the entire file matches what we expect...
	if (md5(firmwareFileBuf) != IMAC_FIRMWARE_30_MD5)
	{
		cerr << "Error: iMac Firmware 3.0 file supplied is not the original iMac Firmware 3.0 file." << endl;
		exit(1);
	}
	
	// extract just the ROM image portion of the file out and decode the Ascii85
	size_t curPos = ROM_IMAGE_OFFSET;
	while (curPos < ROM_IMAGE_END_OFFSET)
	{
		// Skip past the "dc85 "
		size_t dc85Pos = firmwareFileBuf.find("dc85 ", curPos);
		if (dc85Pos == string::npos) break;
		curPos = dc85Pos + 5;
		
		// Find the carriage return
		size_t endLinePos = firmwareFileBuf.find("\r", curPos);
		if (endLinePos == string::npos) break;
		
		// Decode this line of the firmware file and add it to the buffer
		string this85 = firmwareFileBuf.substr(curPos, endLinePos - curPos);
		string decoded = dc85(this85);
		if (decoded == "")
		{
			// An empty decoded string indicates an error in my decode implementation
			cerr << "Error during Ascii85 decode" << endl;
			exit(1);
		}
		romDataBuf.append(decoded);
		
		// Move to the next line
		curPos = endLinePos + 1;
	}
}

void loadSoundFile(const char *filename)
{
	loadFile(filename, soundFileBuf);
	
	// Verify length of sound
	size_t soundLen = soundFileBuf.length();
	if (soundLen > SOUND_MAX_SIZE)
	{
		// Too long
		cerr << "Sound file \"" << filename << "\" is too long. Maximum size: " <<
			SOUND_MAX_SIZE << " bytes" << endl;
		exit(1);
	}
	else if (soundLen % 2)
	{
		// Not a multiple of 2 (which it has to be for it to be 16-bit sound)
		cerr << "Sound file \"" << filename << "\" does not appear to be encoded" <<
			" as a 16-bit sound." << endl;
		exit(1);
	}
	else if (soundLen < SOUND_MAX_SIZE)
	{
		// Shorter than the original sound, so fill the rest with silence -- not an error!
		while (soundLen < SOUND_MAX_SIZE)
		{
			// Append silence in pairs of two bytes until we're at the end
			soundFileBuf.append(2, 0);
			soundLen += 2;
		}
	}
	
	// Now, compress the sound file in IMA 4:1 format and ensure the compressed data is the
	// correct length (it WILL be -- but just to be safe, I'm checking...)
	imaEncode(soundFileBuf, compressedSoundBuf);
	if (compressedSoundBuf.length() != SOUND_COMPRESSED_SIZE)
	{
		cerr << "Sound file \"" << filename << "\" could not be compressed properly." << endl;
		exit(1);
	}
}

void openOutputFile(const char *filename)
{
	// Just open the file for output
	outFile.open(filename, ios::out | ios::trunc | ios::binary);
	
	// And verify that it opened successfully
	if (!outFile.is_open())
	{
		cerr << "Unable to open file \"" << filename << "\" for output." << endl;
		exitPrintUsage();
	}
	
	// We'll use the output file later
}

void injectChime()
{
	// Replace original chime data in romDataBuf with new chime data
	romDataBuf.replace(SOUND_ROM_IMAGE_OFFSET, SOUND_COMPRESSED_SIZE, compressedSoundBuf);
	
	// Remember original ROM buffer length -- we're about to pad it with zeros
	size_t bufLength = romDataBuf.length();
	
	// Recalculate adler32 checksum of romDataBuf (taking into account extra zeros at end
	// which bring total length up to ROM_IMAGE_ADLER_LENGTH);
	romDataBuf.append(ROM_IMAGE_ADLER_LENGTH - bufLength, 0);
	uint32_t romAdler = adler32(romDataBuf);
	
	// Truncate romDataBuf back to original length (no need to encode the zeros at the end)
	romDataBuf.erase(bufLength);
	
	// Encode romDataBuf into ascii85
	string encodedROMImage;
	size_t curPos = 0;
	while (curPos < romDataBuf.length())
	{
		// Encode the data, with no more than FIRMWARE_COLUMN_WIDTH characters per line
		// (not including "dc85 " and carriage return at end of line)
		// This just matches the format Apple used, so why not follow it?
		string encodedLine;
		size_t encodeLen = ec85(romDataBuf, encodedLine, curPos, FIRMWARE_COLUMN_WIDTH);
		encodedROMImage.append("dc85 ");
		encodedROMImage.append(encodedLine);
		encodedROMImage.append(1, '\r');
		curPos += encodeLen;
	}
	
	// Replace adler32 of ROM image with recalculated adler32 checksum
	ostringstream conv(ios::out | ios::binary);
	conv << noshowbase << hex << setw(8) << setfill('0') << uppercase << romAdler;
	if (conv.tellp() != 8)
	{
		// That should have resulted in 8 hex characters
		cerr << "Error saving adler32 checksum to file" << endl;
		exit(1);
	}
	firmwareFileBuf.replace(ROM_IMAGE_ADLER_POS, 8, conv.str());
	
	// Replace original ascii85 data with new ascii85 data (note: this may change the firmware length!)
	firmwareFileBuf.replace(ROM_IMAGE_OFFSET,
							ROM_IMAGE_END_OFFSET - ROM_IMAGE_OFFSET,
							encodedROMImage);
	
	// Calculate adler32 of new "iMac Firmware 3.0" file minus the comment at the end that is used
	// for verifying the adler32. Use offsets from END of file because firmware length may have changed.
	uint32_t fullAdler = adler32(firmwareFileBuf,
								firmwareFileBuf.length() - FIRMWARE_ADLER_END_POS_BACK);
	
	// Replace old adler32
	ostringstream conv2(ios::out | ios::binary);
	conv2 << noshowbase << hex << setw(8) << setfill('0') << uppercase << fullAdler;
	if (conv2.tellp() != 8)
	{
		// That should have resulted in 8 hex characters
		cerr << "Error saving adler32 checksum to file" << endl;
		exit(1);
	}
	firmwareFileBuf.replace(firmwareFileBuf.length() - FIRMWARE_ADLER_POS_BACK,
							8,
							conv2.str());
	
	// All done -- now just write it to the output file and close it
	outFile << firmwareFileBuf;
	outFile.close();
}

void exitPrintUsage()
{
	cerr << "usage: " << programName << " <iMac Firmware 3.0 file> <uncompressed 16-bit mono 44.1 kHz big-endian raw sound file> <output firmware update file>" << endl;
	exit(1);
}

