#include "ascii85.h"
#include <iostream>
#include <stdint.h>

// By Doug Brown
// Public domain. Do whatever you want with this code.

using namespace std;

static const uint32_t pow85[] = {85U*85U*85U*85U, 85U*85U*85U, 85U*85U, 85U, 1};

string dc85(const string &s)
{
	string retval;
	
	size_t curPos = 0;
	while (curPos < s.length())
	{
		char c = s[curPos];
		if (c == 'z') // a "z" represents four zero bytes
		{
			retval.append("\x00\x00\x00\x00", 4);
			curPos++;
		}
		else if (c == 'y') // a "y" represents four 0xFF bytes in Apple's version instead of the typical 0x20
		{
			retval.append("\xFF\xFF\xFF\xFF", 4);
			curPos++;
		}
		else // any other character represents the start of a 5-character string representing 4 characters
		{
			if ((curPos + 5) > s.length())
			{
				cerr << "Invalid Ascii85 format during decode" << endl;
				return ""; // "" indicates invalid decode
			}
			
			// Read five characters, translate from printable range to actual range
			uint32_t val = 0;
			for (int x = 0; x < 5; x++)
			{
				char b = s[curPos + x];
				if ((b < '!') || (b > 'u'))
				{
					cerr << "Invalid Ascii85 format during decode" << endl;
					return ""; // "" indicates invalid decode
				}
				val += pow85[x] * static_cast<uint32_t>(b - '!');
			}
			
			// Now pull the bytes out one by one and spit them out in order, big endian
			for (int x = 3; x >= 0; x--)
			{
				char b = (val >> (x*8)) & 0xFF;
				retval.append(&b, 1);
			}
			
			// We used 5 characters from the buffer to do this
			curPos += 5;
		}
	}
	
	return retval;
}

size_t ec85(const std::string &s, std::string &output, size_t offset, size_t maxStringLen)
{
	output.clear();
	if (maxStringLen == 0) maxStringLen = 0xFFFFFFFFUL; // 0 means go forever. This is close enough.
	size_t charsEncoded = 0;
	while ((offset < s.length()) && (output.length() < maxStringLen))
	{
		// Make sure there's room in the max length to write five more characters.
		// Note: this is kind of nonoptimal because it is still technically possible to fit a "y"
		// or "z" even if there aren't 5 spaces left, but this is how Apple's encoding algorithm
		// appears to work. I want my encoding to match theirs in case of any weird assumptions
		// that the Forth code makes which I am not aware of. If I try to fit extra "z"s and "y"s
		// when there is room for them but not room for 5 characters, my encoding doesn't match
		// theirs. Better safe than sorry.
		if ((output.length() + 5) > maxStringLen)
		{
			break;
		}
		
		// See how many characters are left to go
		uint32_t value = 0;
		int charsRemaining = s.length() - offset;
		if (charsRemaining > 4)
		{
			charsRemaining = 4;
		}
		
		// Grab up to four bytes, assume any extras are 0 (this is perfectly OK to do)
		for (int x = 0; x < charsRemaining; x++)
		{
			value |= ((static_cast<unsigned char>(s[offset+x])) << (8*(3-x)));
		}
		
		// Encode the value
		if (value == 0)
		{
			// Special case. Four zeros encode to a single 'z'.
			output.append(1, 'z');
		}
		else if (value == 0xFFFFFFFFUL)
		{
			// Special case. Four FFs encode to a single 'y'.
			output.append(1, 'y');
		}
		else
		{
			// Encode the four bytes into five Ascii85 bytes.
			char bytes[5];
			for (int x = 0; x < 5; x++)
			{
				bytes[x] = (value % 85) + '!';
				value /= 85;
			}
			
			// Now save them in reverse order
			for (int x = 4; x >= 0; x--)
			{
				output.append(1, bytes[x]);
			}
		}
			
		// We encoded 4 bytes successfully
		offset += 4;
		charsEncoded += charsRemaining;
	}
	
	// Return number of characters encoded
	return charsEncoded;
}

