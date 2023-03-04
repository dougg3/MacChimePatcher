#ifndef IMA_H
#define IMA_H

#include <string>

// Takes input bytes (assumed to be a multiple of 64 2-byte samples) and encodes in IMA 4:1
void imaEncode(const std::string &input, std::string &output);

#endif

