/* monitor.cpp */
#include <iostream>
#include <sys/wait.h>
#include <cstring>
#include <sys/shm.h>
#include <semaphore.h>
#include "shared_memory.hpp"
#include "logger.hpp"

// Δομή Παραμέτρων
struct Parameters {
    int shmid;
};

// Ανάλυση των ορισμάτων γραμμής εντολών
Parameters parseArguments(int argc, char* argv[]);

// Συνάρτηση που εκτυπώνει την κατάσταση του μπαρ
void displayBarStatus(SharedMemoryStruct* shared_mem);

int main(int argc, char* argv[]) {
    // Ανάλυση παραμέτρων γραμμής εντολών
    Parameters params = parseArguments(argc, argv);

    // Σύνδεση στην κοινόχρηστη μνήμη
    void* addr = shmat(params.shmid, NULL, 0);
    if (addr == (void*)-1) {
        perror("shmat");
        return 1;
    }

    SharedMemoryStruct* shared_mem = static_cast<SharedMemoryStruct*>(addr);

    // Εμφάνιση της κατάστασης του μπαρ
    displayBarStatus(shared_mem);

    // Αποσύνδεση από την κοινή μνήμη
    shmdt(addr);

    return 0;
}

Parameters parseArguments(int argc, char* argv[]) {
    Parameters params = {0};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            params.shmid = std::atoi(argv[i + 1]);
            i++;
        }
    }

    if (params.shmid <= 0) {
        std::cerr << "Usage: ./monitor -s shmid" << std::endl;
        std::exit(1);
    }

    return params;
}

void displayBarStatus(SharedMemoryStruct* shared_mem) {
    Logger logger;

    /// Κλείδωμα mutex για ασφαλή ανάγνωση
    pthread_mutex_lock(&shared_mem->stats_mutex);

    char buffer[2048]; // Μεγάλο buffer για να χωρέσουν όλα τα δεδομένα
    int offset = 0;    // Θέση εγγραφής στον buffer

    // Προσθήκη στατιστικών στο buffer
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n========== Bar Status ==========\n");
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Total number of visitors: %d\n", shared_mem->stats.total_visitors);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Total visit time: %.2f seconds\n", shared_mem->stats.total_visit_time);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Total waiting time: %.2f seconds\n", shared_mem->stats.total_wait_time);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Water orders: %d\n", shared_mem->stats.total_water_orders);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Wine orders: %d\n", shared_mem->stats.total_wine_orders);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Cheese orders: %d\n", shared_mem->stats.total_cheese_orders);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Salad orders: %d\n", shared_mem->stats.total_salad_orders);

    // Προσθήκη κατάστασης τραπεζιών στο buffer
    for (int i = 0; i < NUM_TABLES; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                           "Table %d: %s, %d/%d chairs occupied\n",
                           i + 1,
                           shared_mem->tables[i].is_locked ? "Locked" : "Unlocked",
                           shared_mem->tables[i].count_chair,
                           NUM_CHAIRS_PER_TABLE);
    }

    // Προσθήκη τελικής γραμμής στο buffer
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "===================================");

    // Καταγραφή του buffer στο log αρχείο
    logger.logEvent(buffer);

    // Ξεκλείδωμα mutex
    pthread_mutex_unlock(&shared_mem->stats_mutex);
}