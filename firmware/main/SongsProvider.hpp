#pragma once

#include <string>

#include <songlists/songs_lofigirl.hpp>
#include <songlists/songs_rain.hpp>
#include <songlists/songs_coffee_jazz.hpp>

class SongsProvider
{
public:

    SongsProvider()
    {
        srand(time(NULL));
    }

    std::string get_next_song()
    {
        std::string song;

        if (playlist_ == 0)
        {
            song = songs_lowfigirl[rand() % songs_lowfigirl_size];
            song = songs_lowfigirl_prefix + song;
            current_song_index_ = (current_song_index_ + 1) % songs_lowfigirl_size;
        }
        else if (playlist_ == 1)
        {
            song = songs_rain[rand() % songs_rain_size];
            song = songs_rain_prefix + song;
            current_song_index_ = (current_song_index_ + 1) % songs_rain_size;
        }
        else if (playlist_ == 2)
        {
            song = songs_coffee_jazz[rand() % songs_coffee_jazz_size];
            song = songs_coffee_jazz_prefix + song;
            current_song_index_ = (current_song_index_ + 1) % songs_coffee_jazz_size;
        }

        return song;
    }

    void next_playlist()
    {
        playlist_ = (playlist_ + 1) % 3;
        current_song_index_ = 0;
    }

private:

    size_t playlist_ = 0;
    size_t current_song_index_ = 0;
};
