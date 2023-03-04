#include "ima.h"
#include <stdint.h>

// Index table used by encoding algorithm
static const int ima_index_table[] = {
  -1, -1, -1, -1, 2, 4, 6, 8,
  -1, -1, -1, -1, 2, 4, 6, 8
}; 

// Step table used by encoding algorithm
static const int ima_step_table[] = { 
  7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 
  19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 
  50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 
  130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
  337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
  876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 
  2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
  5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 
  15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767 
};
#define NUM_STEP_TABLE_ENTRIES (sizeof(ima_step_table)/sizeof(ima_step_table[0]))

void imaEncode(const std::string &input, std::string &output)
{
	size_t sampleCounter = 0;
	size_t curPos = 0;
	
	// Predictors start at zero
	int32_t predictedSample = 0;
	int32_t index = 0;
	int32_t stepsize = ima_step_table[index];
	
	// Saves samples to accumulate 2 nibbles
	uint8_t tempNibbles = 0;
	while (curPos < input.length())
	{
		// If it's time to write out a header, do so. This saves the
		// predictor and the step index.
		if (sampleCounter == 0)
		{
			// save most significant 9 bits of predicted sample
			uint16_t header = predictedSample & 0xFF80;
			// lower 7 bits are the index.
			header |= index;
			
			// write them to the output buffer
			output.append(1, (header >> 8));
			output.append(1, (header & 0xFF));
		}
		
		// Safely grab sample bytes out of the data stream as a big-endian int16_t
		uint8_t sampleB1 = static_cast<uint8_t>(input[curPos]);
		uint8_t sampleB2 = static_cast<uint8_t>(input[curPos + 1]);
		//uint8_t sampleB1 = static_cast<uint8_t>(input[curPos + 1]); // little-endian test
		//uint8_t sampleB2 = static_cast<uint8_t>(input[curPos]);
		int16_t sample = static_cast<int16_t>((sampleB1 << 8) | sampleB2);

		// This is just the standard IMA encoding algorithm at this point
		int32_t difference = sample - predictedSample;
		uint8_t newSample;
		if (difference >= 0)
		{
			newSample = (0 << 3); // sign bit = 0
		}
		else
		{
			newSample = (1 << 3); // sign bit = 1
			difference = -difference;
		}
		
		// Now the difference is an absolute value.
		
		// Following loop really computes:
		// newSample[2:0] = 4 * (difference / stepsize)
		uint8_t mask = (1 << 2);
		int tempStepSize = stepsize;
		while (mask)
		{
			if (difference >= tempStepSize)
			{
				newSample |= mask;
				difference -= tempStepSize;
			}
			tempStepSize >>= 1;
			mask >>= 1;
		}
		
		// We now have an IMA sample -- save it!
		if ((sampleCounter % 2) == 0)
		{
			// First sample? Put it in the lower nibble.
			tempNibbles = newSample & 0x0F;
		}
		else
		{
			// Second sample? Put it in the upper nibble, then save the byte.
			tempNibbles |= (newSample << 4);
			output.append(1, tempNibbles);
		}
		
		// Figure out the next predictor
		// Really computes:
		// difference = (newSample + 0.5f) * stepsize/4
		// without needing floating-point
		difference = 0;
		if (newSample & (1 << 2))
		{
			difference += stepsize;
		}
		if (newSample & (1 << 1))
		{
			difference += stepsize >> 1;
		}
		if (newSample & (1 << 0))
		{
			difference += stepsize >> 2;
		}
		difference += stepsize >> 3;
		
		// Sign bit, of course
		if (newSample & (1 << 3))
		{
			difference = -difference;
		}
		
		// Adjust the predictor and clamp it.
		predictedSample += difference;
		if (predictedSample > 32767) predictedSample = 32767;
		else if (predictedSample < -32768) predictedSample = -32768;
		
		// Figure out next index, clamp it, and grab the next step size.
		index += ima_index_table[newSample];
		if (index < 0) index = 0;
		else if (index >= NUM_STEP_TABLE_ENTRIES) index = NUM_STEP_TABLE_ENTRIES - 1;
		stepsize = ima_step_table[index];
		
		// Move on to the next sample -- figure out when it's time to do another header.
		sampleCounter = (sampleCounter + 1) % 64;
		
		// We processed two bytes in the input stream.
		curPos += 2;
	}
}

