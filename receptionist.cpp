/* receptionist.cpp */
#include <iostream>
#include <sys/wait.h>
#include <cstring>
#include <sys/shm.h>
#include <semaphore.h>
#include "shared_memory.hpp"
#include "logger.hpp"

// Δομή Παραμέτρων
struct Parameters {
    double t_order;
    int shmid;
};

// Ανάλυση των ορισμάτων γραμμής εντολών
Parameters parseArguments(int argc, char* argv[]); 

// Δημιουργία τυχαίου χρόνου παραγγελίας
double getRandomOrderTime(double t_order);

// Παράδειγμα συνάρτησης που ελέγχει τη σημαία τερματισμού
bool check_is_done(SharedMemoryStruct *shared_mem);

// Ανάκτηση της ώρας άφιξης
double get_arrival_time(SharedMemoryStruct *shared_mem);

// Ενημέρωση του συνολικού χρόνου αναμονής
void update_total_wait_time(SharedMemoryStruct *shared_mem, double wait_time);

// Εκτύπωση στατιστικών
void printStatistics(SharedMemoryStruct* shared_mem);

// Συνάρτηση για τον καθαρισμού της κοινόχρηστης μνήμης
void cleanupSharedMemory(SharedMemoryStruct* shared_mem, int shmid);

int main(int argc, char* argv[]) {
    // Αρχικοποίηση ονομάτων αρχείων και παραμέτρων
    Parameters params = parseArguments(argc, argv);

    // Σύνδεση στην κοινόχρηστη μνήμη
    void* addr = shmat(params.shmid, NULL, 0);
    if (addr == (void*) -1) {
        perror("shmat");
        return 1;
    }

    SharedMemoryStruct* shared_mem = static_cast<SharedMemoryStruct*>(addr);

    Logger logger;
    logger.logEvent("The receptionist is ready to take orders...");


    while (true) {
        double time_order = getRandomOrderTime(params.t_order);

        // Περιμένει για παραγγελία
        sem_wait(&shared_mem->sem_order);

        // Έλεγχος αν το πρόγραμμα πρέπει να τερματίσει
        if (check_is_done(shared_mem)) {
            break; // Έξοδος από τον βρόχο
        }
        
        // Ανάκτηση της ώρας άφιξης
        double arrival_time = get_arrival_time(shared_mem);

        // Υπολογισμός και ενημέρωση του συνολικού χρόνου αναμονής
        double wait_time = static_cast<double>(time(NULL)) - arrival_time;
        update_total_wait_time(shared_mem, wait_time);
 
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Customer %d has been served, Wait time: %.3f seconds, Order time: %.3f seconds.",
                shared_mem->stats.total_visitors, wait_time, time_order);
        logger.logEvent(buffer);

        // Χρόνος παραγγελίας
        sleep(time_order);

        // Ελευθερώνει τον σημαφόρο για την επόμενη παραγγελία
        sem_post(&shared_mem->sem_order_guard);
    }

    // Εκτύπωση στατιστικών
    printStatistics(shared_mem);

    // Καθαρισμός πόρων στο τέλος
    cleanupSharedMemory(shared_mem,  params.shmid);

    return 0;
}

Parameters parseArguments(int argc, char* argv[]) {
    Parameters params = {0, 0};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            params.t_order = std::stod(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            params.shmid = std::atoi(argv[i + 1]);
            i++;
        }
    }

    if (params.t_order <= 0 || params.shmid <= 0) {
        std::cerr << "Usage: ./receptionist -d ordertime -s shmid" << std::endl;
        std::exit(1);
    }

    return params;
}

double getRandomOrderTime(double t_order) {
    // Τυχαία διάρκεια μεταξύ [0.50 * t_order, t_order]
    return ((rand() % 51 + 50) / 100.0) * t_order;  // Χρησιμοποιώντας το εύρος [50 - 100]%
}

bool check_is_done(SharedMemoryStruct *shared_mem) {
    bool done;
    pthread_mutex_lock(&shared_mem->done_mutex);
    done = shared_mem->is_done;
    pthread_mutex_unlock(&shared_mem->done_mutex);
    return done;
}

double get_arrival_time(SharedMemoryStruct *shared_mem) {
    double arrival_time;
    pthread_mutex_lock(&shared_mem->arrival_mutex);
    arrival_time = shared_mem->arrival_time;
    pthread_mutex_unlock(&shared_mem->arrival_mutex);
    return arrival_time;
}

void update_total_wait_time(SharedMemoryStruct *shared_mem, double wait_time) {
    pthread_mutex_lock(&shared_mem->stats_mutex);
    shared_mem->stats.total_wait_time += wait_time;
    pthread_mutex_unlock(&shared_mem->stats_mutex);
}

void printStatistics(SharedMemoryStruct* shared_mem) {
    // Κλείδωμα του mutex πριν την πρόσβαση στα κοινόχρηστα δεδομένα
    pthread_mutex_lock(&shared_mem->stats_mutex);

    // Υπολογισμός μέσων τιμών
    double avg_visit_time = shared_mem->stats.total_visit_time / shared_mem->stats.total_visitors;
    double avg_wait_time = shared_mem->stats.total_wait_time / shared_mem->stats.total_visitors;

    // Εκτύπωση στατιστικών
    Logger logger;
    char buffer[512];
    int offset = 0;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n========== Final Statistics ==========\n");
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Number of visitors: %d\n", shared_mem->stats.total_visitors);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Average visit time: %.2f seconds\n", avg_visit_time);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Average waiting time: %.2f seconds\n", avg_wait_time);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Total water orders: %d\n", shared_mem->stats.total_water_orders);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Total wine orders: %d\n", shared_mem->stats.total_wine_orders);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Total cheese orders: %d\n", shared_mem->stats.total_cheese_orders);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Total salad orders: %d\n", shared_mem->stats.total_salad_orders);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "========================================");

    // Log the constructed message
    logger.logEvent(buffer);

    // Ξεκλείδωμα του mutex
    pthread_mutex_unlock(&shared_mem->stats_mutex);
}


void cleanupSharedMemory(SharedMemoryStruct* shared_mem, int shmid) {
    if (shared_mem == nullptr) {
        return; // Αν η κοινόχρηστη μνήμη δεν είναι προσβάσιμη, έξοδος
    }

    // Καταστροφή των mutex
    pthread_mutex_destroy(&shared_mem->stats_mutex);
    pthread_mutex_destroy(&shared_mem->arrival_mutex);
    pthread_mutex_destroy(&shared_mem->done_mutex);
    pthread_mutex_destroy(&shared_mem->table_mutex);

    // Καταστροφή των σημαφόρων
    sem_destroy(&shared_mem->sem_order);
    sem_destroy(&shared_mem->sem_order_guard);
    sem_destroy(&shared_mem->available_seats);

    // Καταστροφή σημαφόρων για κάθε τραπέζι
    for (int i = 0; i < NUM_TABLES; i++) {
        sem_destroy(&shared_mem->tables[i].sem_table);
    }

    // Αποσύνδεση της κοινόχρηστης μνήμης
    if (shmdt(shared_mem) == -1) {
        perror("shmdt"); // Αναφορά σφάλματος αν η αποσύνδεση αποτύχει
    }

    // Διαγραφή της κοινόχρηστης μνήμης
    if (shmctl(shmid, IPC_RMID, nullptr) == -1) {
        perror("shmctl"); // Αναφορά σφάλματος αν η διαγραφή αποτύχει
    }
}