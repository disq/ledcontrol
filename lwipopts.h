#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#include "lwipopts_examples_common.h"

/* TCP WND must be at least 16 kb to match TLS record size
   or you will get a warning "altcp_tls: TCP_WND is smaller than the RX decrypion buffer, connection RX might stall!" */
#undef TCP_WND
#define TCP_WND 16384

#undef MEM_SIZE
#define MEM_SIZE 32768

#define MQTT_OUTPUT_RINGBUF_SIZE 16384

#define LWIP_ALTCP               1
#define LWIP_ALTCP_TLS           1
#define LWIP_ALTCP_TLS_MBEDTLS   1

#define LWIP_DEBUG 1
#define ALTCP_MBEDTLS_DEBUG  LWIP_DBG_ON

#undef MEM_DEBUG
#undef MEMP_DEBUG
#define MEM_DEBUG                   LWIP_DBG_ON
#define MEMP_DEBUG                  LWIP_DBG_ON

#undef MEM_STATS
#define MEM_STATS                   1

#undef SYS_STATS
#define SYS_STATS                   1

#undef MEMP_STATS
#define MEMP_STATS                  1

#define MEM_OVERFLOW_CHECK 1
#define MEMP_OVERFLOW_CHECK 1

// this is the important bit. 8 is arbitrary.
#define MEMP_NUM_SYS_TIMEOUT            (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 8)

#endif
