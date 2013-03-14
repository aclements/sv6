#include "pmcdb.hh"
#include "cpuid.hh"
#include "bits.hh"
#include "vector.hh"

#include <strings.h>

#include <stdexcept>
#include <string>

struct sel_name
{
  const char *name;
  uint64_t sel;
};

static sel_name names_common[] = {
  {"LLC miss", 0x412e},
  {"instruction retired", 0x00c0},
  {}
};

static sel_name names_amd[] = {
  {"CPU cycle unhalted", 0x0076},
  {}
};

static sel_name names_intel[] = {
  {"CPU cycle unhalted", 0x003c},
  {}
};

static sel_name names_westmere[] = {
  {"L2 miss", 0xaa24},
  {"memory load retired", 0x100b},
  {}
};

static static_vector<sel_name*, 8> maps;

class name_map
{
public:
  name_map()
  {
    if (cpuid::vendor_is_amd()) {
      if (cpuid::model().family < 0x10)
        return;
      maps.push_back(names_common);
      maps.push_back(names_amd);
    } else if (cpuid::vendor_is_intel()) {
      if (cpuid::perfmon().version == 0)
        return;
      maps.push_back(names_common);
      maps.push_back(names_intel);
      if (cpuid::model().family != 6)
        return;
      switch (cpuid::model().model) {
      case 37: case 44: case 47:
        maps.push_back(names_westmere);
        break;
      }
    }
  }

  uint64_t lookup(const char *name)
  {
    for (auto &map : maps)
      for (sel_name *n = map; n->name; ++n)
        if (strcasecmp(name, n->name) == 0)
          return n->sel;
    throw std::invalid_argument(std::string("unknown PMC selector: ") + name);
  }

  static name_map &get_instance()
  {
    static name_map the_map;
    return the_map;
  }
};

uint64_t
pmcdb_parse_selector(const char *str)
{
  return name_map::get_instance().lookup(str) | PERF_SEL_USR | PERF_SEL_OS;
}
