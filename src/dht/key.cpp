#include "key.h"

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
