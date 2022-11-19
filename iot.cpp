#include "iot.h"
#include <cstdio>
#include <string.h>
#include <time.h>
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

extern cyw43_t cyw43_state;

IOT::IOT():
global_state(NULL),
state_topic{0},
command_topic{0},
config_topic{0},
_connect_cb(NULL),
_loop_cb(NULL) {
}

int IOT::init(const char *ssid, const char *password, uint32_t authmode, void (*loop_cb)(), void (*connect_cb)(), void (*command_cb)(const char *data, size_t len)) {
  _loop_cb = loop_cb;
  _connect_cb = connect_cb;
  _command_cb = command_cb;

  get_topic_name(state_topic, sizeof(state_topic), "", "");
  get_topic_name(command_topic, sizeof(command_topic), "", "/set");
  get_topic_name(config_topic, sizeof(config_topic), MQTT_HOME_ASSISTANT_DISCOVERY_PREFIX, "/config");
  printf("[mqtt] state topic: %s\n[mqtt] command topic: %s\n[mqtt] config topic: %s\n", state_topic, command_topic, config_topic);

  cyw43_arch_enable_sta_mode();

  printf("[wifi] connecting...\n");
  if (cyw43_arch_wifi_connect_async(ssid, password, authmode)) {
    printf("[wifi] failed to start connection\n");
    return -1;
  }

  while(true) {
    int res = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (res >= CYW43_LINK_UP) break;
    if (res < 0) {
      printf("[wifi] failed to connect: %d\n", res);
      return -2;
    }
    poll_wifi();
  }

  printf("[wifi] connected\n");
  return 0;
}

int IOT::connect() {
  mqtt_wrapper_t *state = new mqtt_wrapper_t();
  state->mqtt_client = NULL;
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
    return -2;
  }
  printf("[dns] resolved addr to %s.\n", ip4addr_ntoa(&host_addr));

  int res = mqtt_connect(host_addr, mqtt_port, state);
  if (res != 0) {
    printf("[mqtt] failed to connect: %d\n", res);
    return -3;
  }

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

void IOT::get_topic_name(char *buf, size_t buf_len, const char *prepend_str, const char *append_str) {
  static char prefix[128] = {0};

#ifdef MQTT_ADD_BOARD_ID_TO_TOPIC
  if (prefix[0] == 0) {
    pico_get_unique_board_id_string(prefix, sizeof(prefix));
  }
#endif

  int l;
  strncpy(buf, prepend_str, buf_len);
  l = strlen(buf);
  strncpy(&buf[l], MQTT_TOPIC_PREFIX, buf_len - l);
  l = strlen(buf);
  strncpy(&buf[l], prefix, buf_len - l);
  l = strlen(buf);
  strncpy(&buf[l], append_str, buf_len - l);
}

int IOT::mqtt_connect(ip_addr_t host_addr, uint16_t host_port, mqtt_wrapper_t *state) {
  struct mqtt_connect_client_info_t ci;
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
  ci.keep_alive = 10; // seconds. beware: mosquitto has an issue that rejects clients with 0 keep_alive
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
  err_t err = mqtt_client_connect(state->mqtt_client, &host_addr, host_port, _iot_mqtt_connection_cb, state, &ci);
  // memp_malloc: out of memory in pool TCP_PCB
  cyw43_arch_lwip_end();
  if (err != ERR_OK) {
    printf("[mqtt] mqtt_client_connect returned error: %d\n", err);
    return -2;
  }

  printf("[mqtt] mqtt_client_connect successful.\n");
  return 0;
}

int IOT::publish_state(const char *buffer) {
  u8_t qos = 2; // exactly once
  u8_t retain = 1;
  cyw43_arch_lwip_begin();
  err_t err = mqtt_publish(global_state->mqtt_client, state_topic, buffer, strlen(buffer), qos, retain, _iot_mqtt_pub_request_cb, global_state);
  cyw43_arch_lwip_end();

  printf("[mqtt] publish_state: mqtt_publish to %s %s: %d\n", state_topic, err == ERR_OK ? "successful" : "failed", err);
  return err == ERR_OK ? 0 : -1;
}

int IOT::publish_config(const char *effects) {
  char buffer[2048] = {0};
  snprintf(buffer, sizeof(buffer), "{\"board\": \"%s\", \"fw\":\"ledcontrol\", \"unique_id\":\"%s\", \"name\":\"%s\", "
                                   "\"schema\":\"json\", \"dev_cla\":\"light\", "
                                   "\"stat_t\":\"%s\", \"cmd_t\":\"%s\", \"brightness\":true, \"bri_scl\":100, "
                                   "\"color_mode\":true, \"supported_color_modes\":[\"hs\"], "
                                   "\"icon\":\"mdi:led-strip-variant\", \"opt\":false, "
                                   "\"effect\":true, \"effect_list\":[%s]}",
                                   PICO_BOARD,
                                   get_client_id(), // use client_id as unique id
                                   state_topic, // use state topic as device name
                                   state_topic, command_topic, effects);
  printf("msg to publish: %s\n", buffer);

  u8_t qos = 2; // exactly once
  u8_t retain = 1;
  cyw43_arch_lwip_begin();
  err_t err = mqtt_publish(global_state->mqtt_client, config_topic, buffer, strlen(buffer), qos, retain, _iot_mqtt_pub_request_cb, global_state);
  cyw43_arch_lwip_end();

  printf("[mqtt] publish_config: mqtt_publish to %s %s: %d\n", config_topic, err == ERR_OK ? "successful" : "failed", err);
  return err == ERR_OK ? 0 : -1;
}

void IOT::_mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
  if (status != MQTT_CONNECT_ACCEPTED) {
    printf("[mqtt] connection failed (callback): %d\n", status);
    return;
  }
  printf("[mqtt] connected (callback)\n");

  mqtt_set_inpub_callback(client, _iot_mqtt_publish_data_cb, _iot_mqtt_incoming_data_cb, NULL);

  printf("[mqtt] subscribing to %s\n", command_topic);
  err_t err = mqtt_subscribe(client, command_topic, 2, _iot_mqtt_sub_request_cb, NULL);
  if (err != ERR_OK) {
    printf("[mqtt] mqtt_subscribe %s returned error: %d\n", command_topic, err);
  }

  if (_connect_cb) _connect_cb();
}

void IOT::_mqtt_pub_request_cb(void *arg, err_t err) {
  if (err == ERR_OK) {
    printf("[mqtt] (cb) publish successful\n");
  } else {
    printf("[mqtt] (cb) publish failed: %d\n", err);
  }
}

void IOT::_mqtt_sub_request_cb(void *arg, err_t err) {
  if (err == ERR_OK) {
    printf("[mqtt] (cb) subscribe successful\n");
  } else {
    printf("[mqtt] (cb) subscribe failed: %d\n", err);
  }
}

void IOT::_mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
  printf("[mqtt] (cb) incoming data (len:%d, flags:%x): %.*s\n", len, flags, len, (const char*)data);
  if (_command_cb) _command_cb((const char*)data, (size_t)len);
}

void IOT::_mqtt_publish_data_cb(void *arg, const char *topic, u32_t tot_len) {
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
