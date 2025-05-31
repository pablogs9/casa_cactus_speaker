#include <stdio.h>
#include <span>
#include <stdlib.h>

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <nvs_flash.h>
#include <mdns.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <WifiManager.hpp>
#include <SongPlayer.hpp>
#include <Event.hpp>
#include <ButtonController.hpp>
#include <RotaryController.hpp>
#include <SongsProvider.hpp>
#include <RAOPPlayer.hpp>

void player_task(
        void* arg)
{
    I2SSink sink;
    SongsProvider songs_provider;

    ButtonController button_controller;
    RotaryController rotary_controller;
    EventQueue& event_queue = EventQueue::get_instance();

    bool initialized = false;

    while (true)
    {
        ESP_LOGI("app_main", "Getting next song");
        const std::string& song = songs_provider.get_next_song();

        ESP_LOGI("app_main", "Playing song %s", song.c_str());

        SongPlayer player(song, sink);

        if (!initialized)
        {
            // Play start beep
            sink.mute();
            sink.beep(I2SSink::BeepType::START);
            vTaskDelay(pdMS_TO_TICKS(1000));
            sink.unmute();
            initialized = true;
        }

        bool continue_playing = true;

        while (!player.finished() && continue_playing)
        {
            Event event = event_queue.pop();

            switch (event)
            {
                case Event::BUTTON_CLICKED:
                    sink.toggle_mute();
                    sink.beep(I2SSink::BeepType::BEEP);
                    break;

                case Event::TURNED_LEFT:
                    sink.volume_down();
                    break;

                case Event::TURNED_RIGHT:
                    sink.volume_up();
                    break;

                case Event::BUTTON_DOUBLE_CLICKED:
                    continue_playing = false;
                    sink.beep(I2SSink::BeepType::BEEP);
                    break;

                case Event::BUTTON_LONG_CLICKED:
                    songs_provider.next_playlist();
                    continue_playing = false;
                    sink.beep(I2SSink::BeepType::START);
                    break;

                case Event::SONG_END:
                    ESP_LOGI("app_main", "Song ended");
                    break;

                default:
                    ESP_LOGW("app_main", "Unknown event %d", static_cast<int>(event));
                    break;
            }
        }
    }

    vTaskDelete(NULL);
}

extern "C" void app_main(
        void)
{
    // ------------------------
    // System reset
    // ------------------------
    {
        ButtonController button_controller;

        if (button_controller.pressed())
        {
            ESP_LOGI("app_main", "Resetting to factory defaults");
            ESP_ERROR_CHECK(nvs_flash_erase());
            esp_restart();
        }
    }

    // ------------------------
    // System Initialization
    // ------------------------

    // Initialize Non-Volatile Storage (NVS) flash memory
    // If no free pages are available or a new version is found, erase and reinitialize
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Initialize TCP/IP stack for network communications
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop for system events
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize mDNS service
    ESP_ERROR_CHECK(mdns_init());

    // Set hostname
    mdns_hostname_set("cactusspeaker");
    // Set default instance
    mdns_instance_name_set("Casa Cactus Speaker");

    // Disable Power Save modes for improved performance
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Provision the Wi-Fi connection
    WifiManager wifi_manager("CactusSpeaker");
    wifi_manager.wait_for_connection();

    // ------------------------
    // Application Logic
    // ------------------------
    // xTaskCreate(player_task, "ControllerTask", 4096, NULL, 6, NULL);

    I2SSink sink;
    RAOPPlayer raop_player(sink, wifi_manager);

    // Wait forever
    while(true)
    {
        // // Print all tasks info every second
        // ESP_LOGI("app_main", "\n=== TASK INFORMATION ===");

        // // Get task list with state information
        // char* task_list_buffer = (char*)malloc(2048);
        // if (task_list_buffer != NULL) {
        //     //Print header
        //     snprintf(task_list_buffer, 2048, "Task Name\tState\tPriority\tStack\tCore\n");
        //     ESP_LOGI("app_main", "Task List Header:\n%s", task_list_buffer);
        //     vTaskList(task_list_buffer);
        //     ESP_LOGI("app_main", "Task List:\n%s", task_list_buffer);
        //     free(task_list_buffer);
        // }

        // // Get runtime statistics
        // char* stats_buffer = (char*)malloc(2048);
        // if (stats_buffer != NULL) {
        //     // Print header
        //     snprintf(stats_buffer, 2048, "Task Name\tTime (ms)\tCount\tMax Stack\tCore\n");
        //     ESP_LOGI("app_main", "Runtime Stats Header:\n%s", stats_buffer);
        //     vTaskGetRunTimeStats(stats_buffer);
        //     ESP_LOGI("app_main", "Runtime Stats:\n%s", stats_buffer);
        //     free(stats_buffer);
        // }

        // // Print number of tasks and free heap
        // ESP_LOGI("app_main", "Number of tasks: %u", uxTaskGetNumberOfTasks());
        // ESP_LOGI("app_main", "Free heap size: %lu bytes", esp_get_free_heap_size());
        // ESP_LOGI("app_main", "Minimum free heap size: %lu bytes", esp_get_minimum_free_heap_size());

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
