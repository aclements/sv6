#pragma once
// codex benchmark in kernel space

class testcase {
public:
  virtual ~testcase() {}
  virtual void do_work(void) = 0;
  virtual void validate_work(void) = 0;
};

class benchcodex {
public:

  static void init(void);

  // run by the AP processors
  static void ap(testcase *t);

  // run by the main processor
  static void main(testcase *t);

  // create a new test case
  static testcase *
  singleton_testcase(void);
};
