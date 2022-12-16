#include "engine/logger.h"
#include "engine/timer.h"

#include <iostream>
#include <windows.h> 

static HANDLE consoleHandle{ NULL };

void Logger::log(LogLevel aLevel, const char* aMessage, ...)
{
    if (!consoleHandle) consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    SetConsoleTextAttribute(consoleHandle, 7);

    //print the time
    double time = worldTimer.getTotalTime();

    int seconds = static_cast<int>(time);
    int minutes = seconds / 60;
    int hours = minutes / 60;
    int decimal = static_cast<int>((time - static_cast<double>(seconds)) * 1000);

    int correctedSeconds = seconds % 60;
    int correctedMinutes = minutes % 60;

    if (hours == 0)
    {
        printf("%02d:%02d.%03d ", correctedMinutes, correctedSeconds, decimal);
    }
    else
    {
        printf("%02d:%02d:%02d.%03d ", hours, correctedMinutes, correctedSeconds, decimal);
    }

    switch (aLevel)
    {
    case LogLevel::info:
        printf("\x1B[34m[\033\x1B[36mInfo\033\x1B[34m]:    \033[0m");
        break;
    case LogLevel::warning:
        printf("\x1B[33m[\033\x1B[93mWarning\033\x1B[33m]: \033[0m");
        break;
    case LogLevel::error:
        printf("\x1B[31m[\033\x1B[91mError\033\x1B[31m]:   \033[0m");
        break;
    }

    SetConsoleTextAttribute(consoleHandle, 7);

    va_list args;
    va_start(args, aMessage);

    vprintf(aMessage, args);

    va_end(args);

    std::cout << std::endl;
}
