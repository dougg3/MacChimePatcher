#include "adler32.h"

// Borrowed from Wikipedia page on Adler-32:
// http://en.wikipedia.org/wiki/Adler-32
// There are better algorithms, but I don't care about bad performance...
// This code is public domain.

static const int MOD_ADLER = 65521;

uint32_t adler32(const std::string &s, size_t len)
{
	unsigned long a = 1, b = 0;
	int x;
	if (len == 0) len = s.length(); // if len is 0, do the entire string
	
	for (x = 0; x < len; x++)
	{
		a = (a + static_cast<unsigned char>(s[x])) % MOD_ADLER;
		b = (b + a) % MOD_ADLER;
	}
	
	return (b << 16) | a;
}

