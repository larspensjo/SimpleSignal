#include "SimpleSignal.h"

// g++ -Wall -O2 -std=gnu++11 -pthread test.cpp && ./a.out

#include <string>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <ctime>
#include <cstdlib>

static std::string string_printf (const char *format, ...) __attribute__ ((__format__ (__printf__, 1, 2)));
static std::string
string_printf (const char *format, ...)
{
  std::string result;
  char str[1000];
  va_list args;
  va_start (args, format);
  if (vsnprintf (str, sizeof str, format, args) >= 0)
    result = str;
  va_end (args);
  return result;
}

static uint64_t
timestamp_benchmark ()
{
  auto now = std::clock();
  return 1.0e9 / CLOCKS_PER_SEC * now;
}

struct TestCounter {
  static uint64_t get     ();
  static void     set     (uint64_t);
  static void     add2    (void*, uint64_t);
};

namespace { // Anon
void        (*test_counter_add2) (void*, uint64_t) = TestCounter::add2; // external symbol to prevent easy inlining
static uint64_t test_counter_var = 0;
} // Anon

class BasicSignalTests {
  static std::string accu;
  struct Foo {
    char
    foo_bool (float f, int i, std::string s)
    {
      accu += string_printf ("Foo: %.2f\n", f + i + s.size());
      return true;
    }
  };
  static char
  float_callback (float f, int, std::string)
  {
    accu += string_printf ("float: %.2f\n", f);
    return 0;
  }
public:
  static void
  run()
  {
    accu = "";
    Simple::Signal<char (float, int, std::string)> sig1;
    size_t id1 = sig1.connect(float_callback);
    size_t id2 = sig1.connect([] (float, int i, std::string) { accu += string_printf ("int: %d\n", i); return 0; });
    size_t id3 = sig1.connect([] (float, int, const std::string &s) { accu += string_printf ("string: %s\n", s.c_str()); return 0; });
    sig1.emit (.3, 4, "huhu");
    bool success;
    success = sig1.disconnect(id1); assert (success == true);  success = sig1.disconnect(id1); assert (success == false);
    success = sig1.disconnect(id2); assert (success == true);  success = sig1.disconnect(id3); assert (success == true);
    success = sig1.disconnect(id3); assert (success == false); success = sig1.disconnect(id2); assert (success == false);
    Foo foo;
    sig1.connect(Simple::slot (foo, &Foo::foo_bool));
    sig1.connect(Simple::slot (&foo, &Foo::foo_bool));
    sig1.emit (.5, 1, "12");

    Simple::Signal<void (std::string, int)> sig2;
    sig2.connect([] (std::string msg, int) { accu += string_printf ("msg: %s", msg.c_str()); });
    sig2.connect([] (std::string, int d)   { accu += string_printf (" *%d*\n", d); });
    sig2.emit ("in sig2", 17);

    accu += "DONE";

    const char *expected =
      "float: 0.30\n"
      "int: 4\n"
      "string: huhu\n"
      "Foo: 3.50\n"
      "Foo: 3.50\n"
      "msg: in sig2 *17*\n"
      "DONE";
    assert (accu == expected);
  }
};
std::string BasicSignalTests::accu;


class TestCollectorVector {
  static int handler1   ()  { return 1; }
  static int handler42  ()  { return 42; }
  static int handler777 ()  { return 777; }
  public:
  static void
  run ()
  {
    Simple::Signal<int (), Simple::CollectorVector<int>> sig_vector;
    sig_vector.connect(handler777);
    sig_vector.connect(handler42);
    sig_vector.connect(handler1);
    sig_vector.connect(handler42);
    sig_vector.connect(handler777);
    std::vector<int> results = sig_vector.emit();
    const std::vector<int> reference = { 777, 42, 1, 42, 777, };
    assert (results == reference);
  }
};

class TestCollectorUntil0 {
  bool check1, check2;
  TestCollectorUntil0() : check1 (0), check2 (0) {}
  bool handler_true  ()  { check1 = true; return true; }
  bool handler_false ()  { check2 = true; return false; }
  bool handler_abort ()  { std::abort(); }
  public:
  static void
  run ()
  {
    TestCollectorUntil0 self;
    Simple::Signal<bool (), Simple::CollectorUntil0<bool>> sig_until0;
    sig_until0.connect(Simple::slot (self, &TestCollectorUntil0::handler_true));
    sig_until0.connect(Simple::slot (self, &TestCollectorUntil0::handler_false));
    sig_until0.connect(Simple::slot (self, &TestCollectorUntil0::handler_abort));
    assert (!self.check1 && !self.check2);
    const bool result = sig_until0.emit();
    assert (!result && self.check1 && self.check2);
  }
};

class TestCollectorWhile0 {
  bool check1, check2;
  TestCollectorWhile0() : check1 (0), check2 (0) {}
  bool handler_0     ()  { check1 = true; return false; }
  bool handler_1     ()  { check2 = true; return true; }
  bool handler_abort ()  { std::abort(); }
  public:
  static void
  run ()
  {
    TestCollectorWhile0 self;
    Simple::Signal<bool (), Simple::CollectorWhile0<bool>> sig_while0;
    sig_while0.connect(Simple::slot (self, &TestCollectorWhile0::handler_0));
    sig_while0.connect(Simple::slot (self, &TestCollectorWhile0::handler_1));
    sig_while0.connect(Simple::slot (self, &TestCollectorWhile0::handler_abort));
    assert (!self.check1 && !self.check2);
    const bool result = sig_while0.emit();
    assert (result == true && self.check1 && self.check2);
  }
};

static void
bench_simple_signal()
{
  Simple::Signal<void (void*, uint64_t)> sig_increment;
  sig_increment.connect(test_counter_add2);
  const uint64_t start_counter = TestCounter::get();
  const uint64_t benchstart = timestamp_benchmark();
  uint64_t i;
  for (i = 0; i < 999999; i++)
    {
      sig_increment.emit (nullptr, 1);
    }
  const uint64_t benchdone = timestamp_benchmark();
  const uint64_t end_counter = TestCounter::get();
  assert (end_counter - start_counter == i);
  printf ("OK\n  Benchmark: Simple::Signal: %fns per emission (size=%u): ", size_t (benchdone - benchstart) * 1.0 / size_t (i),
          sizeof (sig_increment));
}

static void
bench_callback_loop()
{
  void (*counter_increment) (void*, uint64_t) = test_counter_add2;
  const uint64_t start_counter = TestCounter::get();
  const uint64_t benchstart = timestamp_benchmark();
  uint64_t i;
  for (i = 0; i < 999999; i++)
    {
      counter_increment (nullptr, 1);
    }
  const uint64_t benchdone = timestamp_benchmark();
  const uint64_t end_counter = TestCounter::get();
  assert (end_counter - start_counter == i);
  printf ("OK\n  Benchmark: callback loop: %fns per round: ", size_t (benchdone - benchstart) * 1.0 / size_t (i));
}

uint64_t
TestCounter::get ()
{
  return test_counter_var;
}

void
TestCounter::set (uint64_t v)
{
  test_counter_var = v;
}

void
TestCounter::add2 (void*, uint64_t v)
{
  test_counter_var += v;
}

int
main (int   argc,
      char *argv[])
{
  printf ("Signal/Basic Tests: ");
  BasicSignalTests::run();
  printf ("OK\n");

  printf ("Signal/CollectorVector: ");
  TestCollectorVector::run();
  printf ("OK\n");

  printf ("Signal/CollectorUntil0: ");
  TestCollectorUntil0::run();
  printf ("OK\n");

  printf ("Signal/CollectorWhile0: ");
  TestCollectorWhile0::run();
  printf ("OK\n");

  printf ("Signal/Benchmark: Simple::Signal: ");
  bench_simple_signal();
  printf ("OK\n");

  printf ("Signal/Benchmark: callback loop: ");
  bench_callback_loop();
  printf ("OK\n");

  return 0;
}
