/* end_day.cpp */
#include <iostream>
#include <sys/wait.h>
#include <cstring>
#include <sys/shm.h>
#include <semaphore.h>
#include "shared_memory.hpp"

// Δομή Παραμέτρων
struct Parameters {
    int shmid;
};

// Ανάλυση των ορισμάτων γραμμής εντολών
Parameters parseArguments(int argc, char* argv[]); 

// Συνάρτησης που ενημερώνει τη σημαία τερματισμού
void set_is_done(SharedMemoryStruct *shared_mem);

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

    set_is_done(shared_mem);

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
        std::cerr << "Usage: ./end_day -s shmid" << std::endl;
        std::exit(1);
    }

    return params;
}

void set_is_done(SharedMemoryStruct *shared_mem) {
    pthread_mutex_lock(&shared_mem->done_mutex);
    shared_mem->is_done = true;
    pthread_mutex_unlock(&shared_mem->done_mutex);
    sem_post(&shared_mem->sem_order);
}
