#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "esp_netif.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_pthread.h"
#include "esp_system.h"
#include "freertos/timers.h"
#include "raop.h"
#include "log_util.h"

static EXT_RAM_BSS_ATTR struct raop_cb_s {
	raop_cmd_vcb_t cmd;
	raop_data_cb_t data;
} raop_cbs;

log_level	raop_loglevel = lINFO;
log_level	util_loglevel;

static log_level *loglevel = &raop_loglevel;
static struct raop_ctx_s *raop;
static raop_cmd_vcb_t cmd_handler_chain;

static void raop_volume_up(bool pressed) {
	if (!pressed) return;
	raop_cmd(raop, RAOP_VOLUME_UP, NULL);
	LOG_INFO("AirPlay volume up");
}

static void raop_volume_down(bool pressed) {
	if (!pressed) return;
	raop_cmd(raop, RAOP_VOLUME_DOWN, NULL);
	LOG_INFO("AirPlay volume down");
}

static void raop_toggle(bool pressed) {
	if (!pressed) return;
	raop_cmd(raop, RAOP_TOGGLE, NULL);
	LOG_INFO("AirPlay play/pause");
}

static void raop_pause(bool pressed) {
	if (!pressed) return;
	raop_cmd(raop, RAOP_PAUSE, NULL);
	LOG_INFO("AirPlay pause");
}

static void raop_play(bool pressed) {
	if (!pressed) return;
	raop_cmd(raop, RAOP_PLAY, NULL);
	LOG_INFO("AirPlay play");
}

static void raop_stop(bool pressed) {
	if (!pressed) return;
	raop_cmd(raop, RAOP_STOP, NULL);
	LOG_INFO("AirPlay stop");
}

static void raop_prev(bool pressed) {
	if (!pressed) return;
	raop_cmd(raop, RAOP_PREV, NULL);
	LOG_INFO("AirPlay previous");
}

static void raop_next(bool pressed) {
	if (!pressed) return;
	raop_cmd(raop, RAOP_NEXT, NULL);
	LOG_INFO("AirPlay next");
}

/****************************************************************************************
 * Command handler
 */
static bool cmd_handler(raop_event_t event, ...) {
	va_list args;

	va_start(args, event);

	// handle audio event and stop if forbidden
	if (!cmd_handler_chain(event, args)) {
		va_end(args);
		return false;
	}

	// now handle events for display
	switch(event) {
	case RAOP_SETUP:
		// displayer_control(DISPLAYER_ACTIVATE, "AIRPLAY", true);
        // displayer_artwork(NULL);
		break;
	case RAOP_PLAY:
		// displayer_control(DISPLAYER_TIMER_RUN);
		break;
	case RAOP_FLUSH:
		// displayer_control(DISPLAYER_TIMER_PAUSE);
		break;
    case RAOP_STALLED:
        raop_abort(raop);
        // actrls_unset();
        // displayer_control(DISPLAYER_SHUTDOWN);
        break;
	case RAOP_STOP:
		// actrls_unset();
		// displayer_control(DISPLAYER_SUSPEND);
		break;
	case RAOP_METADATA: {
		// char *artist = va_arg(args, char*), *album = va_arg(args, char*), *title = va_arg(args, char*);
		// displayer_metadata(artist, album, title);
		break;
	}
	case RAOP_ARTWORK: {
		// uint8_t *data = va_arg(args, uint8_t*);
		// displayer_artwork(data);
		break;
	}
	case RAOP_PROGRESS: {
		// int elapsed = va_arg(args, int), duration = va_arg(args, int);
		// displayer_timer(DISPLAYER_ELAPSED, elapsed, duration);
		break;
	}
	default:
		break;
	}

	va_end(args);

	return true;
}

/****************************************************************************************
 * Airplay sink de-initialization
 */
void raop_sink_deinit(void) {
	raop_delete(raop);
}

/****************************************************************************************
 * Airplay sink startup
 */
static void raop_sink_start(const char *sink_name) {
	esp_netif_ip_info_t ipInfo = { };
	uint8_t mac[6];

    // TODO(pgarrido): get mac here

	LOG_INFO( "starting Airplay for ip %s with servicename %s", inet_ntoa(ipInfo.ip.addr), sink_name);
	raop = raop_create(ipInfo.ip.addr, sink_name, mac, 0, cmd_handler, raop_cbs.data);
}

/****************************************************************************************
 * Airplay sink initialization
 */
void raop_sink_init(raop_cmd_vcb_t cmd_cb, raop_data_cb_t data_cb, const char *sink_name) {
	raop_cbs.cmd = cmd_cb;
	raop_cbs.data = data_cb;

    raop_sink_start(sink_name);
}

/****************************************************************************************
 * Airplay forced disconnection
 */
void raop_disconnect(void) {
	LOG_INFO("forced disconnection");
	// in case we can't communicate with AirPlay controller, abort session
	if (!raop_cmd(raop, RAOP_STOP, NULL)) cmd_handler(RAOP_STALLED);
}
