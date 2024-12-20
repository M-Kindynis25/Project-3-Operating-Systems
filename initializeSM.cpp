/* initializeSM.cpp */
#include <iostream>
#include <sys/wait.h>
#include <cstring>
#include <sys/shm.h>
#include <semaphore.h>
#include "shared_memory.hpp"

// Δομή Παραμέτρων
struct Parameters {
    int pipe_write_fd;
};

// Συνάρτηση για ανάλυση των ορισμάτων
Parameters parseArguments(int argc, char* argv[]);

// Συνάρτηση για την αρχικοποίηση της κοινόχρηστης μνήμης
void initializeSharedMemory(int shmid);

int main(int argc, char* argv[]) {
    // Ανάλυση ορισμάτων αν έχει
    Parameters params = parseArguments(argc, argv);

    // Δημιουργία κοινόχρηστης μνήμης
    key_t key = 6677; // Μπορείς να χρησιμοποιήσεις ένα αυθαίρετο κλειδί
    int shmid = shmget(key, sizeof(SharedMemoryStruct), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        return 1;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "Shared Memory ID: " << shmid << std::endl;
    std::cout << "========================================" << std::endl;

    // Αρχικοποίηση της κοινόχρηστης μνήμης
    initializeSharedMemory(shmid);


    // Αν έχει δοθεί pipe
    if (params.pipe_write_fd >= 0) {
        // Αποστολή του `shmid` μέσω του pipe
        if (write(params.pipe_write_fd, &shmid, sizeof(shmid)) != sizeof(shmid)) {
            perror("write");
            return 1;
        }
    }

    return 0;
}

Parameters parseArguments(int argc, char* argv[]) {
    Parameters params = {-1};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            params.pipe_write_fd = std::atoi(argv[i + 1]);
            i++;
        }
    }

    if (params.pipe_write_fd < 0) {
        std::cout << "Usage: ./initializeSM -p pipe_write_fd " << std::endl;
    }

    return params;
}


void initializeSharedMemory(int shmid) {
    void* addr = shmat(shmid, NULL, 0); // Προσάρτηση της κοινόχρηστης μνήμης στη διαδικασία
    if (addr == (void*) -1) { // Έλεγχος αποτυχίας της shmat
        perror("shmat");
        return;
    }

    SharedMemoryStruct* shared_mem = static_cast<SharedMemoryStruct*>(addr);

    // Αρχικοποίηση mutex
    pthread_mutex_init(&shared_mem->stats_mutex, nullptr);  
    pthread_mutex_init(&shared_mem->arrival_mutex, nullptr);  
    pthread_mutex_init(&shared_mem->done_mutex, nullptr);  
    pthread_mutex_init(&shared_mem->table_mutex, nullptr); 

    // Αρχικοποίηση σημαφόρων για παραγγελίες
    sem_init(&shared_mem->sem_order, 1, 0);    // Κύριος σημαφόρος αρχικά κλειδωμένος
    sem_init(&shared_mem->sem_order_guard, 1, 1);   // Σημαφόρος διαχείρισης αρχικά "ξεκλειδωμένος"

    // Αρχικοποίηση των στατιστικών
    shared_mem->stats.total_visitors = 0;
    shared_mem->stats.total_visit_time = 0.0;
    shared_mem->stats.total_wait_time = 0.0;
    shared_mem->stats.total_water_orders = 0;
    shared_mem->stats.total_wine_orders = 0;
    shared_mem->stats.total_cheese_orders = 0;
    shared_mem->stats.total_salad_orders = 0;

    shared_mem->pos_table = 0;  // Αρχικός δείκτης για την κυκλική αναζήτηση τραπεζιού
    sem_init(&shared_mem->available_seats, 1, NUM_TABLES * NUM_CHAIRS_PER_TABLE); // Ολικός αριθμός διαθέσιμων καρεκλών

    // Αρχικοποίηση τραπεζιών και καρεκλών
    for (int i = 0; i < NUM_TABLES; i++) {
        shared_mem->tables[i].is_locked = false;    // Τα τραπέζια ξεκινούν ξεκλείδωτα
        shared_mem->tables[i].count_chair = 0;      // Όλες οι καρέκλες είναι αρχικά κενές
        sem_init(&shared_mem->tables[i].sem_table, 1, NUM_CHAIRS_PER_TABLE);  // Ορίζουμε τον αριθμό καρεκλών ανά τραπέζι
    }

    shared_mem->is_done = false; // Ξεκινάμε με false για να τρέχει ο receptionist
}
