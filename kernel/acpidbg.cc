#include "types.h"
#include "kernel.hh"
#include "kstream.hh"

extern "C" {
#include "acpi.h"
}

sacpi_handle
sacpi(ACPI_HANDLE handle)
{
  return sacpi_handle{handle};
}

void
to_stream(print_stream *s, const sacpi_handle &o)
{
  ACPI_STATUS r;
  char name[128];
  ACPI_BUFFER buf{sizeof(name), name};
  if (ACPI_FAILURE(r = AcpiGetName(o.handle, ACPI_FULL_PATHNAME, &buf)))
    s->print(s, "<error ", AcpiFormatException(r), '>');
  else
    to_stream(s, name);
}

void
to_stream(print_stream *s, const struct acpi_device_info &o)
{
  s->print("acpi_device_info{", sbuf((char*)&o.Name, 4),
           " Type=", o.Type);
  if (o.Valid & ACPI_VALID_STA)
    s->print(" Sta=", sflags(o.CurrentStatus,
                             {{"Present", 1<<0},
                               {"Enabled", 1<<1},
                               {"UI", 1<<2},
                               {"Functioning", 1<<3},
                               {"Battery", 1<<4}}));
  if (o.Valid & ACPI_VALID_ADR)
    s->print(" Adr=", shex(o.Address));
  if (o.Valid & ACPI_VALID_HID)
    s->print(" HID=", o.HardwareId.String);
  if (o.Valid & ACPI_VALID_UID)
    s->print(" UID=", o.UniqueId.String);
  if (o.Valid & ACPI_VALID_CID) {
    s->print(" CID={");
    for (int i = 0; i < o.CompatibleIdList.Count; ++i) {
      if (i != 0)
        s->print(',');
      s->print(o.CompatibleIdList.Ids[i].String);
    }
    s->print('}');
  }
  s->print('}');
}

void
to_stream(print_stream *s, const struct acpi_pci_routing_table &o)
{
  s->print("PRT{Pin=", senum(o.Pin, {"A", "B", "C", "D"}),
           " Address=", shex(o.Address), " SourceIndex=", o.SourceIndex,
           " Source=", o.Source, "}");
}

void
to_stream(print_stream *s, const struct acpi_resource &r)
{
  auto &d = r.Data;

  to_stream(s, "acpi_resource{");
  // See acpica/source/components/resources/rsdump.c and
  // acpica/source/include/acrestyp.h
#define FIELD(field) " "#field "=", sub.field
#define HFIELD(field) " "#field "=", shex(sub.field)
#define F_PRODUCER_CONSUMER()                                     \
  " ", senum(sub.ProducerConsumer, {{"Producer", ACPI_PRODUCER},  \
      {"Consumer", ACPI_CONSUMER}})
#define F_TRIGGERING()                                         \
  " ", senum(sub.Triggering, {{"Level", ACPI_LEVEL_SENSITIVE}, \
      {"Edge", ACPI_EDGE_SENSITIVE}})
#define F_POLARITY()                                            \
  " ", senum(sub.Polarity, {{"High", ACPI_ACTIVE_HIGH},         \
      {"Low", ACPI_ACTIVE_LOW}, {"Both", ACPI_ACTIVE_BOTH}})
#define F_SHARABLE()                                          \
  " ", senum(sub.Sharable, {{"Exclusive", ACPI_EXCLUSIVE},    \
      {"Shared", ACPI_SHARED},                                \
      {"Exclusive-wake", ACPI_EXCLUSIVE_AND_WAKE},            \
      {"Shared-wake", ACPI_SHARED_AND_WAKE}})                 \

  switch (r.Type) {
  case ACPI_RESOURCE_TYPE_IRQ: {
    auto &sub = d.Irq;
    s->print("IRQ", F_TRIGGERING(), F_POLARITY(), F_SHARABLE(),
             " Interrupts={");
    for (int i = 0; i < d.Irq.InterruptCount; ++i)
      s->print(i == 0 ? "" : ",", d.Irq.Interrupts[i]);
    to_stream(s, "}");
    break;
  }
  case ACPI_RESOURCE_TYPE_IO: {
    auto &sub = d.Io;
    s->print("IO", FIELD(IoDecode), HFIELD(Minimum), HFIELD(Maximum),
             FIELD(Alignment), FIELD(AddressLength));
    break;
  }
  case ACPI_RESOURCE_TYPE_END_TAG:
    s->print("EndTag");
    break;
  case ACPI_RESOURCE_TYPE_ADDRESS16:
    s->print("Address16");
    goto address_common;
  case ACPI_RESOURCE_TYPE_ADDRESS32:
    s->print("Address32");
    goto address_common;
  case ACPI_RESOURCE_TYPE_ADDRESS64:
    s->print("Address64");
    goto address_common;
  address_common: {
      ACPI_RESOURCE_ADDRESS64 sub;
      AcpiResourceToAddress64(const_cast<ACPI_RESOURCE*>(&r), &sub);
      s->print(" ResourceType=",
               senum(sub.ResourceType,
                     {{"ACPI_MEMORY_RANGE", ACPI_MEMORY_RANGE},
                       {"ACPI_IO_RANGE", ACPI_IO_RANGE},
                       {"ACPI_BUS_NUMBER_RANGE", ACPI_BUS_NUMBER_RANGE}}));
      switch (sub.ResourceType) {
      case ACPI_MEMORY_RANGE: {
        auto &parent = sub;
        auto &sub = parent.Info.Mem;
        s->print(FIELD(WriteProtect), FIELD(Caching), FIELD(RangeType),
                 FIELD(Translation));
        break;
      }
      case ACPI_IO_RANGE: {
        auto &parent = sub;
        auto &sub = parent.Info.Io;
        s->print(FIELD(RangeType), FIELD(Translation), FIELD(TranslationType));
        break;
      }
      }
      s->print(F_PRODUCER_CONSUMER(), FIELD(Decode), FIELD(MinAddressFixed),
               FIELD(MaxAddressFixed), FIELD(Granularity), HFIELD(Minimum),
               HFIELD(Maximum), HFIELD(TranslationOffset),
               HFIELD(AddressLength), " ResourceSource=", sub.ResourceSource);
      break;
    }
  case ACPI_RESOURCE_TYPE_EXTENDED_IRQ: {
    auto &sub = d.ExtendedIrq;
    s->print("ExIRQ", F_PRODUCER_CONSUMER(), F_TRIGGERING(),
             F_POLARITY(), F_SHARABLE(),
             " ResourceSource=", sub.ResourceSource,
             " Interrupts={");
    for (int i = 0; i < sub.InterruptCount; ++i)
      s->print(i == 0 ? "" : ",", sub.Interrupts[i]);
    to_stream(s, "}");
    break;
  }
  default:
    s->print("Type=", r.Type, " ...");
    break;
  }
#undef FIELD
#undef HFIELD
  to_stream(s, "}");
}

void
to_stream(print_stream *s, const struct acpi_resource_source &r)
{
  if (r.Index == 0xFF || r.StringPtr == nullptr)
    s->print("None");
  else
    s->print("RS{", r.StringPtr, "/", r.Index, "}");
}
