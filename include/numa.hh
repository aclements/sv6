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

  // Index in numa_nodes
  const std::size_t id;
  // Hardware ID of this node (ACPI proximity domain)
  const uint32_t hwid;
  // IDs of CPUs belonging to this node.  We initialize this early (in
  // initnuma).
  static_vector<int, NCPU> cpuids;

  static_vector<region, MAX_MEMS> mems;
  static_vector<struct cpu*, NCPU> cpus;

  numa_node(std::size_t id, uint32_t hwid) : id(id), hwid(hwid) { }
};

enum {
  MAX_NUMA_NODES = 16,
};

extern static_vector<numa_node, MAX_NUMA_NODES> numa_nodes;
