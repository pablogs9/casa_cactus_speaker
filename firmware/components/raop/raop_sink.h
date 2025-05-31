/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef RAOP_SINK_H
#define RAOP_SINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdarg.h>

#define RAOP_SAMPLE_RATE	44100

typedef enum {
    RAOP_SETUP,
    RAOP_STREAM,
    RAOP_PLAY,
    RAOP_FLUSH,
    RAOP_METADATA,
    RAOP_ARTWORK,
    RAOP_PROGRESS,
    RAOP_PAUSE,
    RAOP_STOP,
    RAOP_STALLED,
    RAOP_VOLUME,
    RAOP_TIMING,
    RAOP_PREV,
    RAOP_NEXT,
    RAOP_REW,
    RAOP_FWD,
    RAOP_VOLUME_UP,
    RAOP_VOLUME_DOWN,
    RAOP_RESUME,
    RAOP_TOGGLE } raop_event_t ;

inline const char * raop_event_to_string(raop_event_t event) {
    switch (event) {
        case RAOP_SETUP: return "RAOP_SETUP";
        case RAOP_STREAM: return "RAOP_STREAM";
        case RAOP_PLAY: return "RAOP_PLAY";
        case RAOP_FLUSH: return "RAOP_FLUSH";
        case RAOP_METADATA: return "RAOP_METADATA";
        case RAOP_ARTWORK: return "RAOP_ARTWORK";
        case RAOP_PROGRESS: return "RAOP_PROGRESS";
        case RAOP_PAUSE: return "RAOP_PAUSE";
        case RAOP_STOP: return "RAOP_STOP";
        case RAOP_STALLED: return "RAOP_STALLED";
        case RAOP_VOLUME: return "RAOP_VOLUME";
        case RAOP_TIMING: return "RAOP_TIMING";
        case RAOP_PREV: return "RAOP_PREV";
        case RAOP_NEXT: return "RAOP_NEXT";
        case RAOP_REW: return "RAOP_REW";
        case RAOP_FWD: return "RAOP_FWD";
        case RAOP_VOLUME_UP: return "RAOP_VOLUME_UP";
        case RAOP_VOLUME_DOWN: return "RAOP_VOLUME_DOWN";
        case RAOP_RESUME: return "RAOP_RESUME";
        case RAOP_TOGGLE: return "RAOP_TOGGLE";
    }
    return "<unknown>";
}

typedef bool (*raop_cmd_cb_t)(void * cb_args, raop_event_t event, ...);
typedef bool (*raop_cmd_vcb_t)(void * cb_args, raop_event_t event, va_list args);
typedef void (*raop_data_cb_t)(void * cb_args, const u8_t *data, size_t len, u32_t playtime);

/**
 * @brief     init sink mode (need to be provided)
 */
void raop_sink_init(raop_cmd_vcb_t cmd_cb, raop_data_cb_t data_cb, const char *sink_name, uint32_t ip, void *args);

#ifdef __cplusplus
}
#endif

#endif /* RAOP_SINK_H*/