#include <string.h>
#include <bitset>

const unsigned int KEYBITS = 160;
const unsigned int KEYBYTES = KEYBITS / 8;

typedef std::bitset<KEYBITS> Key;

// generate a random 160-bit key and write to BUFFER
void random_key(Key& buffer);
