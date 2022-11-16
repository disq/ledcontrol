#include "iot.h"
#include <cstdio>
#include <string.h>
#include <time.h>
#include "config_iot.h"

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
_loop_cb(NULL) {
}

int IOT::init(const char *ssid, const char *password, uint32_t authmode, void (*loop_cb)()) {
  _loop_cb = loop_cb;

  cyw43_arch_enable_sta_mode();

  printf("[wifi] connecting...\n");
  if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, authmode, 30000)) {
    printf("[wifi] failed to connect\n");
    return -1;
  }

  printf("[wifi] connected\n");

  mqtt_run_test(MQTT_SERVER_HOST, MQTT_SERVER_PORT);
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

int IOT::mqtt_connect(ip_addr_t host_addr, uint16_t host_port, mqtt_wrapper_t *state) {
  struct mqtt_connect_client_info_t ci;
  err_t err;

  memset(&ci, 0, sizeof(ci));

  ci.client_id = MQTT_CLIENT_ID;
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

err_t IOT::mqtt_test_publish(mqtt_wrapper_t *state)
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

  sprintf(buffer, "hello from picow %s %d / %d %s", MQTT_CLIENT_ID, state->received, state->counter, TLS_STR);

  err_t err;
  u8_t qos = 2; /* 0 1 or 2, see MQTT specification */
  u8_t retain = 0;
  cyw43_arch_lwip_begin();
  err = mqtt_publish(state->mqtt_client, "pico_w/test", buffer, strlen(buffer), qos, retain, _iot_mqtt_pub_request_cb, &state);
  cyw43_arch_lwip_end();
  return err;
}

void IOT::_mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
//  mqtt_wrapper_t *state = (mqtt_wrapper_t*)arg;
  if (status != 0) {
    printf("[mqtt] connection failed (callback): %d\n", status);
    return;
  }
  printf("[mqtt] connected (callback)\n");
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
