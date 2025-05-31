#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>

#include "esp_netif.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_pthread.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/timers.h"
#include <mbedtls/psa_util.h>
#include "raop.h"
#include "log_util.h"

static struct raop_ctx_s *raop;
static raop_cmd_vcb_t cmd_vcb = NULL;

// Wrapper function to convert from raop_cmd_cb_t to raop_cmd_vcb_t
static bool raop_cmd_wrapper(void *cb_args, raop_event_t event, ...) {
    if (cmd_vcb == NULL) return false;

    va_list args;
    va_start(args, event);
    bool result = cmd_vcb(cb_args, event, args);
    va_end(args);

    return result;
}

// static void raop_volume_up(bool pressed) {
// 	if (!pressed) return;
// 	raop_cmd(raop, RAOP_VOLUME_UP, NULL);
// 	LOG_INFO("AirPlay volume up");
// }

// static void raop_volume_down(bool pressed) {
// 	if (!pressed) return;
// 	raop_cmd(raop, RAOP_VOLUME_DOWN, NULL);
// 	LOG_INFO("AirPlay volume down");
// }

// static void raop_toggle(bool pressed) {
// 	if (!pressed) return;
// 	raop_cmd(raop, RAOP_TOGGLE, NULL);
// 	LOG_INFO("AirPlay play/pause");
// }

// static void raop_pause(bool pressed) {
// 	if (!pressed) return;
// 	raop_cmd(raop, RAOP_PAUSE, NULL);
// 	LOG_INFO("AirPlay pause");
// }

// static void raop_play(bool pressed) {
// 	if (!pressed) return;
// 	raop_cmd(raop, RAOP_PLAY, NULL);
// 	LOG_INFO("AirPlay play");
// }

// static void raop_stop(bool pressed) {
// 	if (!pressed) return;
// 	raop_cmd(raop, RAOP_STOP, NULL);
// 	LOG_INFO("AirPlay stop");
// }

// static void raop_prev(bool pressed) {
// 	if (!pressed) return;
// 	raop_cmd(raop, RAOP_PREV, NULL);
// 	LOG_INFO("AirPlay previous");
// }

// static void raop_next(bool pressed) {
// 	if (!pressed) return;
// 	raop_cmd(raop, RAOP_NEXT, NULL);
// 	LOG_INFO("AirPlay next");
// }

/****************************************************************************************
 * Airplay sink initialization
 */
void raop_sink_init(raop_cmd_vcb_t cmd_cb, raop_data_cb_t data_cb, const char *sink_name, uint32_t ip, void *args)
{
    psa_crypto_init();

    uint8_t mac[6];

    esp_wifi_get_mac(WIFI_IF_STA, mac);

    // Store the va_list callback for use in the wrapper
    cmd_vcb = cmd_cb;

	LOG_INFO( "starting Airplay for ip %s with servicename %s", inet_ntoa(ip), sink_name);
	raop = raop_create(ip, sink_name, mac, 0, raop_cmd_wrapper, data_cb, args);
}