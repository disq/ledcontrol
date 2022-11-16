#include "hardware/structs/rosc.h"
#include "pico/stdlib.h"

/* cribbed from https://github.com/peterharperuk/pico-examples/tree/add_mbedtls_example */
/* Function to feed mbedtls entropy. May be better to move it to pico-sdk */
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen) {
  /* Code borrowed from pico_lwip_random_byte(), which is static, so we cannot call it directly */
  static uint8_t byte;

  for(size_t p=0; p<len; p++) {
    for(int i=0;i<32;i++) {
      // picked a fairly arbitrary polynomial of 0x35u - this doesn't have to be crazily uniform.
      byte = ((byte << 1) | rosc_hw->randombit) ^ (byte & 0x80u ? 0x35u : 0);
      // delay a little because the random bit is a little slow
      busy_wait_at_least_cycles(30);
    }
    output[p] = byte;
  }

  *olen = len;
  return 0;
}
