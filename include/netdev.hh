#pragma once

class netdev
{
public:
  virtual int transmit(void *buf, uint32_t len) = 0;
  virtual void get_hwaddr(uint8_t *hwaddr) = 0;
};

// For now, we only support one network device
extern netdev *the_netdev;
