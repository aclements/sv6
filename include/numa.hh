#pragma once

#include "vector.hh"

#include <cstdint>

struct numa_node
{
  struct region
  {
    // Physical base and length of this memory range
    uint64_t base, length;

    region(uint64_t base, uint64_t length)
      : base(base), length(length) { }
  };

  enum {
    MAX_MEMS = 16
  };

  const std::size_t id;         // Index in numa_nodes
  static_vector<struct cpu*, NCPU> cpus;
  static_vector<region, MAX_MEMS> mems;

  numa_node(std::size_t id) : id(id) { }
};

enum {
  MAX_NUMA_NODES = 16,
};

extern static_vector<numa_node, MAX_NUMA_NODES> numa_nodes;
