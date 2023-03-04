#ifndef ASCII85_H
#define ASCII85_H

// By Doug Brown
// Public domain. Do whatever you want with this code.

#include <string>

// Decodes provided string from Ascii85 format
std::string dc85(const std::string &s);
// Encodes provided string, starting at offset, into Ascii85. If maxStringLen > 0, only encodes
// enough data to print that many characters in Ascii85 format. Useful for formatting the encoded
// data at a max column width. Returns number of characters encoded (0 if problem).
size_t ec85(const std::string &s, std::string &output, size_t offset = 0, size_t maxStringLen = 0);

#endif // ASCII85_H

