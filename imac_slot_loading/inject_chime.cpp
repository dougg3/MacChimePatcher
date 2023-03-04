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
#include "../util/adler32.h"
#include "../util/ima.h"

// TODO: Allow big or little endian raw data sound files (configured with a flag)
// TODO: Allow reading of .AIFF or .WAV files
// TODO: Allow saving the IMA-encoded chime as .AIFC so user can verify it sounds right

// Note: to convert 16-bit little endian to big endian sound file, do this:
// dd conv=swab < little_endian_file > big_endian_file

using namespace std;

// Used to ensure that the MD5 of the "iMac Firmware" file checks out
static const char *IMAC_FIRMWARE_MD5 = "9df1737e52474ca77d682603a66b3c91";

// The sound is stored in the actual firmware file at:
// 0xD1E2C-0xE02DF (0xE4B4 bytes total). This is inside
// of the section labeled "sboot", which starts at 0x6E07C.
#define SBOOT_SECTION_OFFSET	0x6E07C
#define SBOOT_SECTION_SIZE_USED	0x72280
#define SBOOT_POTENTIAL_SIZE	0x80000
// The checksum is in a table near the start of the firmware data
#define SBOOT_CHECKSUM_OFFSET	0x6890

// Note that there seems to be a 16-byte sound header, so the table in the
// firmware actually says that it's 0xE4C4 bytes and starts at 0x63DA0 instead.
// I'm just going with what I find convenient.
#define SOUND_SBOOT_OFFSET		0x63DB0
#define NUM_SOUND_PACKETS		1722
#define BYTES_PER_PACKET		34
#define SAMPLES_PER_PACKET		64
#define BYTES_PER_SAMPLE		2 /* 16 bits */
#define SOUND_SAMPLES_MAX		(NUM_SOUND_PACKETS * SAMPLES_PER_PACKET)
#define SOUND_MAX_SIZE			(SOUND_SAMPLES_MAX * BYTES_PER_SAMPLE)
#define SOUND_COMPRESSED_SIZE	(NUM_SOUND_PACKETS * BYTES_PER_PACKET)

static string programName; // name of program as called
static string firmwareFileBuf; // the entire "iMac Firmware" file
static string soundFileBuf; // the provided sound file
static string compressedSoundBuf; // the compressed sound data
static string romDataBuf; // decoded ROM image (SBOOT section)
static ofstream outFile; // file we write the patched firmware to
static char tmpBuf[1024]; // temporary buffer for reading

// Declarations of functions
void loadFile(const char *filename, string &buf); // loads complete contents of file into the given buffer
void loadFirmwareFile(const char *filename); // loads the firmware file, verifies it
void loadSoundFile(const char *filename); // loads the new sound chime and encodes it in IMA 4:1 format
void openOutputFile(const char *filename); // prepares for saving new firmware by opening output file
void injectChime(); // sticks the new sound in place, recalculates checksums, saves new firmware
void exitPrintUsage(); // exits with a message showing how to use the program

int main(int argc, char *argv[])
{
	programName = argv[0];

	// Need an exact number of arguments
	if (argc != 4)
	{
		exitPrintUsage();
	}

	// Make sure that the first file ("iMac Firmware") is the correct
	// iMac Firmware file, and if it is, load it
	loadFirmwareFile(argv[1]);

	// Make sure the second file (raw audio) is not too big, and
	// convert it to IMA 4:1
	loadSoundFile(argv[2]);

	// Open output file and make sure we're good to go
	openOutputFile(argv[3]);

	// Do the work -- inject sound, fix checksums, save
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
	if (md5(firmwareFileBuf) != IMAC_FIRMWARE_MD5)
	{
		cerr << "Error: iMac Firmware file supplied is not the original iMac Firmware file." << endl;
		exit(1);
	}

	// extract just the ROM image portion of the file out, as long as there is room
	if (firmwareFileBuf.length() < SBOOT_SECTION_OFFSET + SBOOT_SECTION_SIZE_USED)
	{
		cerr << "Error: iMac Firmware file is shorter than expected." << endl;
		exit(1);
	}
	romDataBuf = firmwareFileBuf.substr(SBOOT_SECTION_OFFSET, SBOOT_SECTION_SIZE_USED);
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
	romDataBuf.replace(SOUND_SBOOT_OFFSET, SOUND_COMPRESSED_SIZE, compressedSoundBuf);

	// Remember original ROM buffer length -- we're about to pad it with 0xFF
	size_t bufLength = romDataBuf.length();

	// Recalculate adler32 checksum of romDataBuf (taking into account extra 0xFF at end
	// which brings total length up to SBOOT_POTENTIAL_SIZE - 4). I believe in
	// the actual flash chip, the checksum will be stored in those last 4 bytes.
	romDataBuf.append(SBOOT_POTENTIAL_SIZE - SBOOT_SECTION_SIZE_USED - 4, 0xFF);
	uint32_t sbootAdler = adler32(romDataBuf);

	// Truncate romDataBuf back to original length (no need to encode the zeros at the end)
	romDataBuf.erase(bufLength);

	// Replace adler32 of sboot section (writing the number out big-endian)
	string sbootAdlerString;
	sbootAdlerString.append(1, static_cast<char>((sbootAdler >> 24) & 0xFF));
	sbootAdlerString.append(1, static_cast<char>((sbootAdler >> 16) & 0xFF));
	sbootAdlerString.append(1, static_cast<char>((sbootAdler >> 8) & 0xFF));
	sbootAdlerString.append(1, static_cast<char>((sbootAdler >> 0) & 0xFF));
	firmwareFileBuf.replace(SBOOT_CHECKSUM_OFFSET, 4, sbootAdlerString);

	// Put the data back into firmwareFileBuf
	firmwareFileBuf.replace(SBOOT_SECTION_OFFSET, SBOOT_SECTION_SIZE_USED, romDataBuf);

	// Calculate adler32 of new "iMac Firmware" file. It's the entire file except
	// for the last 4 bytes, which is where the adler32 is stored.
	uint32_t fullAdler = adler32(firmwareFileBuf,
								firmwareFileBuf.length() - 4);

	// Replace old adler32
	string fullAdlerString;
	fullAdlerString.append(1, static_cast<char>((fullAdler >> 24) & 0xFF));
	fullAdlerString.append(1, static_cast<char>((fullAdler >> 16) & 0xFF));
	fullAdlerString.append(1, static_cast<char>((fullAdler >> 8) & 0xFF));
	fullAdlerString.append(1, static_cast<char>((fullAdler >> 0) & 0xFF));
	firmwareFileBuf.replace(firmwareFileBuf.length() - 4,
							4,
							fullAdlerString);

	// All done -- now just write it to the output file and close it
	outFile << firmwareFileBuf;
	outFile.close();
}

void exitPrintUsage()
{
	cerr << "usage: " << programName << " <iMac Firmware file> <uncompressed 16-bit mono 44.1 kHz big-endian raw sound file> <output firmware update file>" << endl;
	exit(1);
}
