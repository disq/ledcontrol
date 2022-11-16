#ifndef IOT_H
#define IOT_H

#include <cstdio>
#include <cstdint>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/structs/rosc.h"
#include "lwip/pbuf.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/dns.h"
#include "lwip/apps/mqtt.h"

class IOT {
  private:
    typedef struct {
        mqtt_client_t *mqtt_client;
        u8_t receiving;
        u32_t received;
        u32_t counter;
        u32_t reconnect;
    } mqtt_wrapper_t;

    void (*_loop_cb)();

    void poll_wifi(uint32_t min_sleep_ms = 100);
    int run_dns_lookup(const char *host, ip_addr_t *addr);
    int mqtt_connect(ip_addr_t host_addr, uint16_t host_port, mqtt_wrapper_t *state);
    void mqtt_disconnect_and_free(mqtt_wrapper_t *state);
    err_t mqtt_test_publish(mqtt_wrapper_t *state);
    int mqtt_fresh_state(const char *mqtt_host, uint16_t mqtt_port, mqtt_wrapper_t *state);

  public:
    IOT();
    int init(const char *ssid, const char *password, uint32_t authmode, void (*loop_cb)());
    int mqtt_run_test(const char *mqtt_host, uint16_t mqtt_port);

    // callbacks
    void _dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
    void _mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
    void _mqtt_pub_request_cb(void *arg, err_t err);

};

extern IOT iot;

static void _iot_dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
static void _iot_mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void _iot_mqtt_pub_request_cb(void *arg, err_t err);

#endif //IOT_H
