#pragma once
#include <chrono>

class Stopwatch
{
private:
    using clock = std::chrono::high_resolution_clock;
    std::chrono::time_point<clock> start_time;
    std::chrono::time_point<clock> end_time;
    bool running;

public:
    Stopwatch() : running(false) {}

    void start()
    {
        if (!running)
        {
            start_time = clock::now();
            running = true;
        }
    }

    void stop()
    {
        if (running)
        {
            end_time = clock::now();
            running = false;
        }
    }

    double elapsedMilliseconds() const
    {
        if (running)
        {
            auto current_time = clock::now();
            return std::chrono::duration<double, std::milli>(current_time - start_time).count();
        }
        else
        {
            return std::chrono::duration<double, std::milli>(end_time - start_time).count();
        }
    }

    double elapsedSeconds() const
    {
        return elapsedMilliseconds() / 1000.0;
    }

    void reset()
    {
        running = false;
    }
};