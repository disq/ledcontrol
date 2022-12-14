#ifndef CONFIG_IOT_H
#define CONFIG_IOT_H

#include "pico/cyw43_arch.h"

// Set this if you want to connect to MQTT securely
//#define MQTT_TLS

// MQTT server host name or ip address
#define MQTT_SERVER_HOST "test.mosquitto.org"

// MQTT ports
#ifdef MQTT_TLS
#define MQTT_SERVER_PORT 8883
#else
#define MQTT_SERVER_PORT 1883
#endif

// Enable this if you don't want certificate verification but still want to connect with TLS
//#define MQTT_TLS_INSECURE

//#define MQTT_SERVER_HOST "91.121.93.94"
// add contents of https://test.mosquitto.org/ssl/mosquitto.org.crt (or your own cert) here, if you want to connect securely
#define MQTT_TLS_CERT "-----BEGIN CERTIFICATE-----\n" \
"MIIEAzCCAuugAwIBAgIUBY1hlCGvdj4NhBXkZ/uLUZNILAwwDQYJKoZIhvcNAQEL\n" \
"BQAwgZAxCzAJBgNVBAYTAkdCMRcwFQYDVQQIDA5Vbml0ZWQgS2luZ2RvbTEOMAwG\n" \
"A1UEBwwFRGVyYnkxEjAQBgNVBAoMCU1vc3F1aXR0bzELMAkGA1UECwwCQ0ExFjAU\n" \
"BgNVBAMMDW1vc3F1aXR0by5vcmcxHzAdBgkqhkiG9w0BCQEWEHJvZ2VyQGF0Y2hv\n" \
"by5vcmcwHhcNMjAwNjA5MTEwNjM5WhcNMzAwNjA3MTEwNjM5WjCBkDELMAkGA1UE\n" \
"BhMCR0IxFzAVBgNVBAgMDlVuaXRlZCBLaW5nZG9tMQ4wDAYDVQQHDAVEZXJieTES\n" \
"MBAGA1UECgwJTW9zcXVpdHRvMQswCQYDVQQLDAJDQTEWMBQGA1UEAwwNbW9zcXVp\n" \
"dHRvLm9yZzEfMB0GCSqGSIb3DQEJARYQcm9nZXJAYXRjaG9vLm9yZzCCASIwDQYJ\n" \
"KoZIhvcNAQEBBQADggEPADCCAQoCggEBAME0HKmIzfTOwkKLT3THHe+ObdizamPg\n" \
"UZmD64Tf3zJdNeYGYn4CEXbyP6fy3tWc8S2boW6dzrH8SdFf9uo320GJA9B7U1FW\n" \
"Te3xda/Lm3JFfaHjkWw7jBwcauQZjpGINHapHRlpiCZsquAthOgxW9SgDgYlGzEA\n" \
"s06pkEFiMw+qDfLo/sxFKB6vQlFekMeCymjLCbNwPJyqyhFmPWwio/PDMruBTzPH\n" \
"3cioBnrJWKXc3OjXdLGFJOfj7pP0j/dr2LH72eSvv3PQQFl90CZPFhrCUcRHSSxo\n" \
"E6yjGOdnz7f6PveLIB574kQORwt8ePn0yidrTC1ictikED3nHYhMUOUCAwEAAaNT\n" \
"MFEwHQYDVR0OBBYEFPVV6xBUFPiGKDyo5V3+Hbh4N9YSMB8GA1UdIwQYMBaAFPVV\n" \
"6xBUFPiGKDyo5V3+Hbh4N9YSMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEL\n" \
"BQADggEBAGa9kS21N70ThM6/Hj9D7mbVxKLBjVWe2TPsGfbl3rEDfZ+OKRZ2j6AC\n" \
"6r7jb4TZO3dzF2p6dgbrlU71Y/4K0TdzIjRj3cQ3KSm41JvUQ0hZ/c04iGDg/xWf\n" \
"+pp58nfPAYwuerruPNWmlStWAXf0UTqRtg4hQDWBuUFDJTuWuuBvEXudz74eh/wK\n" \
"sMwfu1HFvjy5Z0iMDU8PUDepjVolOCue9ashlS4EB5IECdSR2TItnAIiIwimx839\n" \
"LdUdRudafMu5T5Xma182OC0/u/xRlEm+tvKGGmfFcN0piqVl8OrSPBgIlb+1IKJE\n" \
"m/XriWr/Cq4h/JfB7NTsezVslgkBaoU=\n" \
"-----END CERTIFICATE-----\n";

// Enable and configure these (similar format to MQTT_TLS_CERT) if you want client side certificates
/*
#define MQTT_TLS_KEY \
"-----BEGIN RSA PRIVATE KEY-----\n" \
"-----END RSA PRIVATE KEY-----";
#define MQTT_TLS_CA \
"-----BEGIN CERTIFICATE-----\n" \
"-----END CERTIFICATE-----";
*/

// MQTT client id. Board id is appended to this string. Due to the 23 byte soft limit, recommended to keep it at (or under) 7 chars
// This value is also used as part of the unique_id in MQTT discovery
#define MQTT_CLIENT_ID "PicoW_"

// MQTT username and password. If empty, don't #define and leave commented out
//#define MQTT_CLIENT_USER ""
//#define MQTT_CLIENT_PASS ""

#ifndef CYW43_HOST_NAME
#define CYW43_HOST_NAME "picow-ledcontrol"
#endif

// Topic prefix, without the trailing slash
#define MQTT_TOPIC_PREFIX "picow/ledcontrol" // if enabled, leave MQTT_HOME_ASSISTANT_DISCOVERY_PREFIX as set below
//#define MQTT_TOPIC_PREFIX "homeassistant/light/picow/ledcontrol" // if enabled, set MQTT_HOME_ASSISTANT_DISCOVERY_PREFIX to empty string below

// Disable this to remove board id from the topic prefix. Leave as-is if unsure so that every PicoW automatically gets its own unique id and topic.
#define MQTT_ADD_BOARD_ID_TO_TOPIC

// This prefix is required for Home Assistant autodiscovery to work. MQTT_TOPIC_PREFIX (and if enabled, board id) is
// added to this before the string "/config". If you want your light to publish and read state under
// "homeassistant/light/picow/ledcontrol" as well (and not just use the homeassistant prefix for autodiscovery) you
// should set MQTT_TOPIC_PREFIX above to "homeassistant/light/picow/ledcontrol" and set this one to empty string.
#define MQTT_HOME_ASSISTANT_DISCOVERY_PREFIX  "homeassistant/light/"
//#define MQTT_HOME_ASSISTANT_DISCOVERY_PREFIX  ""

// Country code. Optionally, enable and change according to your country. Full list in https://raspberrypi.github.io/pico-sdk-doxygen/cyw43__country_8h.html
//#define WIFI_COUNTRY_CODE CYW43_COUNTRY_UK

#endif
