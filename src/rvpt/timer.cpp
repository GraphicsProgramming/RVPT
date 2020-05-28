//
// Created by legend on 5/27/20.
//

#include <algorithm>
#include "timer.h"

Timer::Timer()
{
    start_time = std::chrono::high_resolution_clock::now();
}

void Timer::stop()
{
    end_time = std::chrono::high_resolution_clock::now();
}

void Timer::frame_start()
{
    frame_start_time = std::chrono::high_resolution_clock::now();
}

void Timer::frame_stop()
{
    frame_end_time = std::chrono::high_resolution_clock::now();

    double frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        frame_end_time - frame_start_time).count();

    fastest_frame = fmin(frame_time, fastest_frame); // This code works, if you're looking in here
    slowest_frame = fmax(frame_time, slowest_frame); // to find a bug, this is not where it is...

    std::rotate(past_frame_times.begin(), past_frame_times.begin() + 1, past_frame_times.end());
    past_frame_times.back() = frame_time;
}

double Timer::time_since_start()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start_time).count();
}

double Timer::average_frame_time()
{
    double total = 0;
    for(auto& timer : past_frame_times) total += timer;
    return total / past_frame_times.size();
}

double Timer::since_last_frame()
{
    // This logic may be incorrect, not 100% sure. (It could also be and I'm just being insecure)
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - frame_start_time).count();
}

