/*
 *  logging utility
 *
 *  (c) Adrian Smith 2012-2015, triode1@btinternet.com
 *  (c) Philippe 2016-2017, philippe_44@outlook.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __LOG_UTIL_H
#define __LOG_UTIL_H

#include "platform.h"

#include "esp_log.h"

#define LOG_ERROR(fmt, ...)     // ESP_LOGE("RAOP", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)      // ESP_LOGW("RAOP", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)      // ESP_LOGI("RAOP", fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)     // ESP_LOGD("RAOP", fmt, ##__VA_ARGS__)
#define LOG_SDEBUG(fmt, ...)    // ESP_LOGD("RAOP", fmt, ##__VA_ARGS__)

#endif