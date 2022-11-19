#ifndef IOT_H
#define IOT_H

#include <cstdio>
#include <cstdint>
#include "config_iot.h"
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
    } mqtt_wrapper_t;

    mqtt_wrapper_t *global_state;
    char state_topic[256], command_topic[256], config_topic[256];

    void (*_connect_cb)();
    void (*_command_cb)(const char *data, size_t len);
    void (*_loop_cb)();

    void poll_wifi(uint32_t min_sleep_ms = 100);
    int run_dns_lookup(const char *host, ip_addr_t *addr);
    int mqtt_connect(ip_addr_t host_addr, uint16_t host_port, mqtt_wrapper_t *state);
    void mqtt_disconnect_and_free(mqtt_wrapper_t *state);
    int mqtt_fresh_state(const char *mqtt_host, uint16_t mqtt_port, mqtt_wrapper_t *state);
    void get_topic_name(char *buf, size_t buf_len, const char *prepend_str, const char *append_str);

  public:
    IOT();
    int init(const char *ssid, const char *password, uint32_t authmode, void (*loop_cb)(), void (*connect_cb)());
    int init(const char *ssid, const char *password, uint32_t authmode, void (*loop_cb)(), void (*connect_cb)(), void (*command_cb)(const char *data, size_t len));
    int connect();
    const char* get_client_id();
    int publish_state(const char *buffer);
    int publish_config(const char *effects);

    // callbacks
    void _dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
    void _mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
    void _mqtt_pub_request_cb(void *arg, err_t err);
    void _mqtt_sub_request_cb(void *arg, err_t err);
    void _mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
    void _mqtt_publish_data_cb(void *arg, const char *topic, u32_t tot_len);

};

extern IOT iot;

static void _iot_dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
static void _iot_mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void _iot_mqtt_pub_request_cb(void *arg, err_t err);
static void _iot_mqtt_sub_request_cb(void *arg, err_t err);
static void _iot_mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
static void _iot_mqtt_publish_data_cb(void *arg, const char *topic, u32_t tot_len);

#endif //IOT_H
