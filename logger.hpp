/* logger.hpp */
#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <fcntl.h>
#include <unistd.h>
#include <ctime>
#include <cstring>
#include <cstdio>

#define FILENAME "events.log"

class Logger {
private:
    int logFile;                 // File descriptor για το αρχείο log
    char logFileName[256];       // Όνομα αρχείου log (μέγιστο μέγεθος 256 χαρακτήρες)

    // Συνάρτηση για την επιστροφή της χρονικής σήμανσης (timestamp)
    void getCurrentTimestamp(char* buffer, size_t bufferSize) {
        time_t now = time(nullptr);
        struct tm* timeInfo = localtime(&now);
        snprintf(buffer, bufferSize, "%04d-%02d-%02d %02d:%02d:%02d",
                 timeInfo->tm_year + 1900,
                 timeInfo->tm_mon + 1,
                 timeInfo->tm_mday,
                 timeInfo->tm_hour,
                 timeInfo->tm_min,
                 timeInfo->tm_sec);
    }

public:
    // Constructor: Ανοίγει το αρχείο log
    Logger() {
        logFile = open(FILENAME, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (logFile == -1) {
            perror("Error opening log file");
        }
    }

    // Destructor: Κλείνει το αρχείο log
    ~Logger() {
        if (logFile != -1) {
            close(logFile);
        }
    }

    // Συνάρτηση για την καταγραφή μηνύματος
    void logEvent(const char* message) {
        if (logFile != -1) {
            char buffer[512]; // Μέγιστο μήκος εγγραφής (512 χαρακτήρες)
            char timestamp[64];
            getCurrentTimestamp(timestamp, sizeof(timestamp));

            // Δημιουργία του τελικού μηνύματος
            snprintf(buffer, sizeof(buffer), "[%s] %s\n", timestamp, message);

            // Γράψιμο στο αρχείο
            write(logFile, buffer, strlen(buffer));
        } else {
            perror("Log file not initialized");
        }
    }
};

#endif // LOGGER_HPP
