#ifndef ADLER_H
#define ADLER_H

#include <stdint.h>
#include <string>

// Borrowed from Wikipedia page on Adler-32:
// http://en.wikipedia.org/wiki/Adler-32
// There are better algorithms, but I don't care about bad performance...
// This code is public domain.

// Calculate adler32 of a string. Optionally supply a length
// to stop at.
uint32_t adler32(const std::string &s, size_t len = 0);

#endif // ADLER_H
