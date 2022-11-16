#include "iot.h"
#include <cstdio>
#include <string.h>
#include <time.h>
#include "config_iot.h"
#include "pico/unique_id.h"

#ifdef MQTT_TLS
#ifdef MQTT_TLS_CERT
const char *cert = MQTT_TLS_CERT;
#endif
#ifdef MQTT_TLS_CA
const char *ca = MQTT_TLS_CA;
#endif
#ifdef MQTT_TLS_KEY
const char *key = MQTT_TLS_KEY;
#endif
#endif

IOT::IOT():
global_state(NULL),
_connect_cb(NULL),
_loop_cb(NULL) {
}

int IOT::init(const char *ssid, const char *password, uint32_t authmode, void (*loop_cb)(), void (*connect_cb)()) {
  _loop_cb = loop_cb;
  _connect_cb = connect_cb;

  cyw43_arch_enable_sta_mode();

  printf("[wifi] connecting...\n");
  if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, authmode, 30000)) {
    printf("[wifi] failed to connect\n");
    return -1;
  }

  printf("[wifi] connected\n");

//  mqtt_run_test(MQTT_SERVER_HOST, MQTT_SERVER_PORT);
  return 0;
}

int IOT::connect() {
  mqtt_wrapper_t *state = new mqtt_wrapper_t();
  state->mqtt_client = NULL;
  state->reconnect = 0;
  state->received = 0;
  state->receiving = 0;
  state->counter = 0;
  int res = mqtt_fresh_state(MQTT_SERVER_HOST, MQTT_SERVER_PORT, state);
  if (res != 0) {
    printf("[mqtt] failed to init: %d\n", res);
    return res;
  }
  global_state = state;

  // not supporting polling mode here for now

  return 0;
}

void IOT::poll_wifi(uint32_t min_sleep_ms) {
//  if (min_sleep_ms < 1) min_sleep_ms = 1;

  uint32_t start_time = to_ms_since_boot(get_absolute_time());
  uint32_t last_time;
  do {
#if PICO_CYW43_ARCH_POLL
    cyw43_arch_poll();
    sleep_ms(1);
#endif
    if (_loop_cb != NULL) (*_loop_cb)();
    last_time = to_ms_since_boot(get_absolute_time());
  } while(start_time + min_sleep_ms > last_time);
}

void IOT::_dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
  printf("[dns] found! %s\n", ip4addr_ntoa(ipaddr));
  auto ip = (ip_addr_t*)callback_arg;
  *ip = *ipaddr;
}

// run_dns_lookup tries to resolve the given host and set it to addr
int IOT::run_dns_lookup(const char *host, ip_addr_t *addr) {
  addr->addr = 0;
  printf("[dns] looking up %s\n", host);

  cyw43_arch_lwip_begin();
  err_t err = dns_gethostbyname(host, addr, _iot_dns_found_cb, addr);
  cyw43_arch_lwip_end();

  if (err == ERR_ARG) {
    printf("[dns] failed to start query\n");
    return -1;
  }

  if (err == ERR_OK) {
    printf("[dns] no lookup needed\n");
    // addr was already written to from the cache, nothing to do
    return 0;
  }

  while (addr->addr == 0) poll_wifi();
  return 0;
}

void IOT::mqtt_disconnect_and_free(mqtt_wrapper_t *state) {
  if (!state->mqtt_client) return;

  mqtt_disconnect(state->mqtt_client);
  mqtt_client_free(state->mqtt_client);
  state->mqtt_client = NULL;
}

int IOT::mqtt_fresh_state(const char *mqtt_host, uint16_t mqtt_port, mqtt_wrapper_t *state) {
  // clean up previous state
  mqtt_disconnect_and_free(state);

  state->mqtt_client = mqtt_client_new();
  if (state->mqtt_client == NULL) {
    printf("[mqtt] failed to create client\n");
    return -1;
  }

  ip_addr_t host_addr;
  if (run_dns_lookup(mqtt_host, &host_addr)) {
    printf("[dns] failed to resolve host\n");
    return -1;
  }
  printf("[dns] resolved addr to %s.\n", ip4addr_ntoa(&host_addr));

  state->mqtt_client = mqtt_client_new();
  if (state->mqtt_client == NULL) {
    printf("[mqtt] failed to create client\n");
    return -2;
  }

  int res = mqtt_connect(host_addr, mqtt_port, state);
  if (res != 0) {
    printf("[mqtt] failed to connect: %d\n", res);
    return -3;
  }

  return 0;
}

int IOT::mqtt_run_test(const char *mqtt_host, uint16_t mqtt_port) {
  mqtt_wrapper_t *state = new mqtt_wrapper_t();
  state->mqtt_client = NULL;
  state->reconnect = 0;
  state->received = 0;
  state->receiving = 0;
  state->counter = 0;
  int res = mqtt_fresh_state(mqtt_host, mqtt_port, state);
  if (res != 0) {
    printf("[mqtt] failed to init: %d\n", res);
    return res;
  }

#if PICO_CYW43_ARCH_POLL
  const uint32_t max_wait = 5000;

  u32_t notReady = max_wait;
#endif

  while (true) {
    poll_wifi();
#if PICO_CYW43_ARCH_POLL
    if (!notReady--) {
#else
    {
#endif
#if PICO_CYW43_ARCH_POLL
      notReady = max_wait;
#endif

      cyw43_arch_lwip_begin();
      bool connected = mqtt_client_is_connected(state->mqtt_client);
      cyw43_arch_lwip_end();
      if (!connected) {
        printf(",");
        continue;
      }

      printf("[mqtt] connected\n");
      state->receiving = 1;
      err_t err = mqtt_test_publish(state);
      if (err != ERR_OK) {
        printf("[mqtt] failed to publish, will retry: %d\n", err);
        continue;
      }
      printf("[mqtt] published %d\n", state->counter);
      state->counter++;
      poll_wifi(5000);

      int res = mqtt_fresh_state(mqtt_host, mqtt_port, state);
      if (res != 0) {
        printf("[mqtt] failed to reinit: %d\n", res);
        return res;
      }
    }
  } // while

  return 0;
}

const char* IOT::get_client_id() {
  static char client_id[128] = {0};
  if (client_id[0] != 0) {
    return client_id;
  }

  strncpy(client_id, MQTT_CLIENT_ID, sizeof(client_id));
  pico_get_unique_board_id_string(&client_id[strlen(client_id)], sizeof(client_id) - strlen(client_id));
  return client_id;
}

const char* IOT::get_topic_prefix() {
#ifndef MQTT_ADD_BOARD_ID_TO_TOPIC
  return MQTT_TOPIC_PREFIX;
#endif

  static char prefix[128] = {0};
  if (prefix[0] != 0) {
    return prefix;
  }

  strncpy(prefix, MQTT_TOPIC_PREFIX, sizeof(prefix));
  pico_get_unique_board_id_string(&prefix[strlen(prefix)], sizeof(prefix) - strlen(prefix));
  return prefix;
}

int IOT::mqtt_connect(ip_addr_t host_addr, uint16_t host_port, mqtt_wrapper_t *state) {
  struct mqtt_connect_client_info_t ci;
  err_t err;

  memset(&ci, 0, sizeof(ci));

  ci.client_id = get_client_id();
#ifdef MQTT_CLIENT_USER
  ci.client_user = MQTT_CLIENT_USER;
#else
  ci.client_user = NULL;
#endif
#ifdef MQTT_CLIENT_PASS
  ci.client_pass = MQTT_CLIENT_PASS;
#else
  ci.client_pass = NULL;
#endif
  ci.keep_alive = 0;
  ci.will_topic = NULL;
  ci.will_msg = NULL;
  ci.will_retain = 0;

#ifdef MQTT_TLS
  struct altcp_tls_config *tls_config;

#ifdef MQTT_TLS_INSECURE
  printf("[mqtt] Setting up TLS insecure mode...\n");
  tls_config = altcp_tls_create_config_client(NULL, 0);
#elif defined(MQTT_TLS_CERT) && defined(MQTT_TLS_KEY) && defined(MQTT_TLS_CA)
  printf("[mqtt] Setting up TLS with 2wayauth...\n");
  tls_config = altcp_tls_create_config_client_2wayauth(
      (const u8_t *)ca, 1 + strlen(ca),
      (const u8_t *)key, 1 + strlen(key),
      (const u8_t *)"", 0,
      (const u8_t *)cert, 1 + strlen(cert)
  );
#elif defined(MQTT_TLS_CERT)
  printf("[mqtt] Setting up TLS with cert...\n");
  tls_config = altcp_tls_create_config_client((const u8_t *) cert, 1 + strlen(cert));
#else
  #error "MQTT_TLS set but no TLS config. Please edit config_iot.h"
#endif // MQTT_TLS_INSECURE

  if (tls_config == NULL) {
      // check error code shown with `strerror`, eg. `strerror -8576`
      printf("\n[mqtt] Failed to initialize TLS config\n");
      return -1;
  }

//  mbedtls_ssl_set_hostname(altcp_tls_context((struct altcp_pcb *)conn), MQTT_SERVER_HOST);

  ci.tls_config = tls_config;
#endif // MQTT_TLS

  cyw43_arch_lwip_begin();
  err = mqtt_client_connect(state->mqtt_client, &host_addr, host_port, _iot_mqtt_connection_cb, state, &ci);
  // memp_malloc: out of memory in pool TCP_PCB
  cyw43_arch_lwip_end();
  if (err != ERR_OK) {
    printf("[mqtt] mqtt_client_connect returned error: %d\n", err);
    return -2;
  }

  printf("[mqtt] mqtt_client_connect successful.\n");
  return 0;
}

int IOT::mqtt_test_publish(mqtt_wrapper_t *state)
{
  char buffer[128];

#ifdef MQTT_TLS
#ifdef MQTT_TLS_INSECURE
#define TLS_STR "TLS insecure"
#else
#define TLS_STR "TLS"
#endif
#else
#define TLS_STR ""
#endif

  sprintf(buffer, "hello from picow %s %d / %d %s", get_client_id(), state->received, state->counter, TLS_STR);

  return topic_publish("testing", buffer);
}

int IOT::topic_publish(const char *topicsuffix, const char *buffer)
{
  char topic[256] = {0};
  strncpy(topic, get_topic_prefix(), sizeof(topic));
  strncat(topic, "/", sizeof(topic) - strlen(topic));
  strncat(topic, topicsuffix, sizeof(topic) - strlen(topic));

  err_t err;
  u8_t qos = 2; /* 0 1 or 2, see MQTT specification */
  u8_t retain = 1;
  cyw43_arch_lwip_begin();
  err = mqtt_publish(global_state->mqtt_client, topic, buffer, strlen(buffer), qos, retain, _iot_mqtt_pub_request_cb, global_state);
  cyw43_arch_lwip_end();

  if (err == ERR_OK) {
    printf("[mqtt] mqtt_publish to %s successful.\n", topic);
    return 0;
  }

  printf("[mqtt] mqtt_publish to %s failed: %d\n", topic, err);
  return -1;
}

void IOT::_mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
//  mqtt_wrapper_t *state = (mqtt_wrapper_t*)arg;
  if (status != 0) {
    printf("[mqtt] connection failed (callback): %d\n", status);
    return;
  }
  printf("[mqtt] connected (callback)\n");

  mqtt_set_inpub_callback(client, _iot_mqtt_publish_data_cb, _iot_mqtt_incoming_data_cb, arg);

  // subscribe to various topics
  const char* suffixes[3] = {"/light/switch", "/brightness/set", "/rgb/set"};
  for(unsigned int i = 0; i < sizeof(suffixes); i++) {
    char topic[256];
    sprintf(topic, "%s%s", get_topic_prefix(), suffixes[i]);
    printf("[mqtt] subscribing to %s\n", topic);
    err_t err = mqtt_subscribe(client, topic, 2, _iot_mqtt_sub_request_cb, topic);
    if (err != ERR_OK) {
      printf("[mqtt] mqtt_subscribe %s returned error: %d\n", topic, err);
    }
    break;
  }

  printf("should call connect_cb\n");
  if (_connect_cb) {
    printf("should call connect_cb here\n");
    _connect_cb();
    printf("called\n");
  }
//  if (_connect_cb) _connect_cb();
}

void IOT::publish_switch_state(bool on) {
  topic_publish("light/status", on ? light_on : light_off);
}

void IOT::publish_brightness(float brightness) {
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%d", int(brightness*mqtt_brightness_scale));
  topic_publish("brightness/status", buffer);
}

void IOT::publish_colour(float r, float g, float b) {
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%d,%d,%d", int(r*mqtt_rgb_scale), int(g*mqtt_rgb_scale), int(b*mqtt_rgb_scale));
  topic_publish("rgb/status", buffer);
}

void IOT::_mqtt_pub_request_cb(void *arg, err_t err) {
  mqtt_wrapper_t *state = (mqtt_wrapper_t*)arg;
  if (err == ERR_OK) {
    printf("[mqtt] (cb) publish successful\n");
  } else {
    printf("[mqtt] (cb) publish failed: %d\n", err);
  }
  state->receiving=0;
  state->received++;
}

void IOT::_mqtt_sub_request_cb(void *arg, err_t err) {
  auto topic = (const char*)arg;
  if (err == ERR_OK) {
    printf("[mqtt] (cb) subscribe to %s successful\n", topic);
  } else {
    printf("[mqtt] (cb) subscribe to %s failed: %d\n", topic, err);
  }
}

void IOT::_mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
//  mqtt_wrapper_t *state = (mqtt_wrapper_t *) arg;
  printf("[mqtt] (cb) incoming data: %s\n", (const char*)data);
}

void IOT::_mqtt_publish_data_cb(void *arg, const char *topic, u32_t tot_len) {
//  mqtt_wrapper_t *state = (mqtt_wrapper_t *) arg;
  printf("[mqtt] (cb) publish data on topic: %s (length: %d)\n", topic, tot_len);
}

IOT iot;

// callback "bindings" to homemade static methods
static void _iot_dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
  iot._dns_found_cb(name, ipaddr, callback_arg);
}

static void _iot_mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
  iot._mqtt_connection_cb(client, arg, status);
}

static void _iot_mqtt_pub_request_cb(void *arg, err_t err) {
  iot._mqtt_pub_request_cb(arg, err);
}

static void _iot_mqtt_sub_request_cb(void *arg, err_t err) {
  iot._mqtt_sub_request_cb(arg, err);
}

static void _iot_mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
  iot._mqtt_incoming_data_cb(arg, data, len, flags);
}

static void _iot_mqtt_publish_data_cb(void *arg, const char *topic, u32_t tot_len) {
  iot._mqtt_publish_data_cb(arg, topic, tot_len);
}
