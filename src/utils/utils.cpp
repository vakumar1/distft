#include "utils.h"

Key random_key() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(0, 1);
  Key key;
  for (int i = 0; i < KEYBITS; i++) {
    key[i] = dist(gen);
  }
  return key;
}

Key key_from_data(const char* data, size_t size) {
  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(data), size, hash);
  Key key;
  for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
    unsigned char byte = hash[i];
    for (int j = 0; j < 8; j++) {
      if (byte >> j & 0x1) {
        key.set(8 * i + j);
      } else {
        key.reset(8 * i + j);
      }
    }
  }
  return key;
}

Key key_from_string(std::string s) {
  return key_from_data(s.c_str(), s.length());
}

std::string hex_string(Key k) {
  std::string res;
  std::string bits = k.to_string();
  for (int i = 0; i < KEYBITS / 8; i++) {
    std::string byte = bits.substr(8 * i, 8 * (i + 1));
    std::bitset<8> byte_bitset(byte);
    std::stringstream byte_hex;
    byte_hex << std::hex << byte_bitset.to_ulong();
    res.append(byte_hex.str());
  }
  return res.substr(0, 6);
}

const bool Dist::operator < (const Dist& d) const {
  for (int i = KEYBITS - 1; i >= 0; i--) {
    if (this->value[i] && !d.value[i]) {
      return false;
    } else if (!this->value[i] && d.value[i]) {
      return true;
    }
  }
  return false;
}

const bool Dist::operator >= (const Dist& d) const {
  for (int i = KEYBITS - 1; i >= 0; i--) {
    if (this->value[i] && !d.value[i]) {
      return true;
    } else if (!this->value[i] && d.value[i]) {
      return false;
    }
  }
  return true;
}
