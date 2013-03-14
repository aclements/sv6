#pragma once

#include <cstdint>
#include <new>

class cpuid
{
public:
  enum class leafid : uint32_t {
    basic = 0,
    features = 1,
    cache_and_tlb = 2,
    serial_number = 3,
    cache_params = 4,           // Depends on ecx
    mwait = 5,
    thermal = 6,
    ext_features = 7,           // Depends on ecx
    dca = 9,
    perfmon = 0xa,
    topology = 0xb,             // Depends on ecx
    ext_state = 0xd,            // Depends on ecx
    qos = 0xf,                  // Depends on ecx

    extended_info = 0x80000000,
    extended_features = 0x80000001,
  };

  struct leaf
  {
    bool valid;
    uint32_t a, b, c, d;
  };

private:
  // Leaf cache.  Only for ECX = 0.  extended_ starts at
  // extended_info.
  enum {
    MAX_BASIC = 0x10,
    MAX_EXTENDED = 0x9
  };
  leaf basic_[MAX_BASIC], extended_[MAX_EXTENDED];

  char vendor_[13];

  cpuid();

  static class cpuid instance_;

  static cpuid &get_instance() {
    // This may be called before static initializers are run, in which
    // case we do construction now.  This is obviously not
    // thread-safe, but construction should happen one way or the
    // other before threads can even be created.
    if (!instance_.basic_[0].valid)
      new (&instance_) cpuid();
    return instance_;
  }

  static leaf read_leaf(uint32_t eax, uint32_t ecx = 0)
  {
    uint32_t ebx, edx;
    __asm volatile("cpuid"
                   : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
                   : "a" (eax), "c" (ecx));
    return {true, eax, ebx, ecx, edx};
  }

public:
  // Return CPUID leaf 'leaf'.  For leafs with sub-leafs, a value of
  // ecx must be provided.  Whenever possible, this will return a
  // cached value.  If the desired value may differ between CPUs in a
  // system, such as the initial APIC ID, then 'force' should be true
  // to bypass the cache.  If the requested leaf is not valid, the
  // 'valid' field of the result will be false and a, b, c, and d will
  // be zero.
  static leaf get_leaf(leafid leaf, uint32_t ecx = 0, bool force = false)
  {
    if (!force && ecx == 0 && (uint32_t)leaf < MAX_BASIC)
      return get_instance().basic_[(int)leaf];
    if ((uint32_t)leaf > get_instance().basic_[0].a)
      return {false};
    return read_leaf((uint32_t)leaf, ecx);
  }

  // Leaf basic

  static const char *vendor()
  {
    return get_instance().vendor_;
  }

  static bool vendor_is_intel()
  {
    leaf l = get_leaf(leafid::basic);
    return l.b == 0x756e6547 && l.d == 0x49656e69 && l.c == 0x6c65746e;
  }

  static bool vendor_is_amd()
  {
    leaf l = get_leaf(leafid::basic);
    return l.b == 0x68747541 && l.d == 0x69746e65 && l.c == 0x444d4163;
  }

  // Leaf features

  struct model_info
  {
    int family, model, stepping;
  };

  static model_info model()
  {
    uint32_t a = get_leaf(leafid::features).a;
    model_info info;
    info.family = (a >> 8) & 0xF;
    if (info.family == 0xF)
      info.family += (a >> 20) & 0xFF;
    info.model = (a >> 4) & 0xF;
    if (info.family == 0x0f || (info.family == 0x06 && vendor_is_intel()))
      info.model |= (a >> 12) & (0xF0);
    info.stepping = a & 0xF;
    return info;
  }

  static uint8_t initial_apicid()
  {
    return (get_leaf(leafid::features, 0, true).b >> 24) & 0xFF;
  }

  // Leaf mwait

  struct mwait_info
  {
    uint16_t smallest_line, largest_line;
  };

  static mwait_info mwait()
  {
    leaf l = get_leaf(leafid::mwait);
    mwait_info info;
    info.smallest_line = l.a & 0xFFFF;
    info.largest_line = l.b & 0xFFFF;
    return info;
  }

  // Leaf perfmon

  struct perfmon_info
  {
    uint8_t version;
    uint8_t num_counters;
  };

  static perfmon_info perfmon()
  {
    leaf l = get_leaf(leafid::perfmon);
    perfmon_info info;
    info.version = l.a & 0xFF;
    info.num_counters = (l.a >> 8) & 0xFF;
    return info;
  }

  // Features (leafs features and extended_features)

  struct features_info
  {
    // 1.ECX
    bool mwait : 1;
    bool pdcm : 1;              // Perfmon and debug
    bool x2apic : 1;

    // 1.EDX
    bool apic : 1;              // "APIC on chip"
    bool ds : 1;                // Debug store

    // 80000001.EDX
    bool page1GB : 1;
  };

  static features_info &features()
  {
    return get_instance().features_;
  }

private:
  features_info features_;
};
