#pragma once
// codex benchmark in kernel space

#include <atomic>

class benchcodex {
public:

  // run by the AP processors
  static void ap(void);

  // run by the main processor
  static void main(void);

private:
  static std::atomic<unsigned int> _ctr;
};
