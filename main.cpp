/* main.cpp */
#include <iostream>
#include <sys/wait.h>
#include <cstring>
#include <sys/shm.h>
#include <semaphore.h>
#include "shared_memory.hpp"

// Δομή Παραμέτρων
struct Parameters {
    double t_order;
    double t_rest;
};

// Δομή για αποθήκευση file descriptors ενός pipe
struct PipeFD {
    int fd[2];
};

// Ανάλυση των ορισμάτων γραμμής εντολών
Parameters parseArguments(int argc, char* argv[]); 

// Συνάρτηση που μετατρέπει έναν ακέραιο ή double σε συμβολοσειρά
const char* intToStr(int number);
const char* doubleToStr(double number);


// Συνάρτησης που ενημερώνει τη σημαία τερματισμού
void set_is_done(SharedMemoryStruct *shared_mem, bool value);

int main(int argc, char* argv[]) {
    // Αρχικοποίηση ονομάτων αρχείων και παραμέτρων
    Parameters params = parseArguments(argc, argv);

    PipeFD pipe_in;
    if (pipe(pipe_in.fd) == -1) {     // Δημιουργία pipe για επικοινωνία USR2
        perror("pipe");
        exit(1);
    }

    // Εκκίνηση του initializeSM
    std::cout << "Launching initializeSM process..." << std::endl;
    pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork");
        return 3;
    } else if (pid == 0) {  // Διαδικασία παιδιού
        close(pipe_in.fd[0]);  // Κλείνουμε το read

        // Εκτέλεση του initializeSM μέσω execl
        execl("./initializeSM",
            "initializeSM",        
            "-p", intToStr(pipe_in.fd[1]),     
            (char*)NULL); 
        // Αν η exec αποτύχει
        std::perror("execl");
        return 3;
    }

    int shmid;
    if (read(pipe_in.fd[0], &shmid, sizeof(shmid)) != sizeof(shmid)) {
        perror("read");
        return 1;
    }

    void* addr = shmat(shmid, NULL, 0); // Προσάρτηση της κοινόχρηστης μνήμης στη διαδικασία
    if (addr == (void*) -1) { // Έλεγχος αποτυχίας της shmat
        perror("shmat");
        return 1;
    }

    SharedMemoryStruct* shared_mem = static_cast<SharedMemoryStruct*>(addr);

    // Εκκίνηση του receptionist
    std::cout << "Launching the receptionist process..." << std::endl;
    pid = fork();
    if (pid < 0) {
        std::perror("fork");
        return 3;
    } else if (pid == 0) {  // Διαδικασία παιδιού

        // Εκτέλεση του receptionist μέσω execl
        execl("./receptionist",
            "receptionist",    
            "-d", doubleToStr(params.t_order),     
            "-s", intToStr(shmid),                // Κοινόχρηστη Μνήμη
            (char*)NULL); 
        // Αν η exec αποτύχει
        std::perror("execl");
        return 3;
    }

    srand(time(NULL));

    int TOTAL_VISITORS = (rand() % 19) + 2;   // [2..20]
    int TOTAL_VISITORS2 = (rand() % 29) + 2;  // [2..30]
    // Εκκίνηση των visitors
    for (int i = 0; i < TOTAL_VISITORS; i++) {
        std::cout << "Launching visitors with id " << i << "..." << std::endl;
        pid = fork();
        if (pid < 0) {
            std::perror("fork");
            return 3;
        } else if (pid == 0) {  // Διαδικασία παιδιού

            // Εκτέλεση του visitor μέσω execl
            execl("./visitor",
                "visitor",   
                "-id", intToStr(i+1), 
                "-d", doubleToStr(params.t_rest),     
                "-s", intToStr(shmid),                // Κοινόχρηστη Μνήμη    
                (char*)NULL); 
            // Αν η exec αποτύχει
            std::perror("execl");
            return 3;
        }
        // Τυχαίο sleep μεταξύ 0 και 4 δευτερολέπτων
        sleep(rand() % 4);
    }

    // Τυχαίο sleep πριν την επόμενη ομάδα επισκεπτών
    sleep((rand() % 7) + 3); // π.χ. 3 έως 10 δευτερόλεπτα
    
    // Εκκίνηση των visitors
    for (int i = 0; i < TOTAL_VISITORS2; i++) {
        std::cout << "Launching visitors with id " << i + 1 + TOTAL_VISITORS << "..." << std::endl;
        pid = fork();
        if (pid < 0) {
            std::perror("fork");
            return 3;
        } else if (pid == 0) {  // Διαδικασία παιδιού

            // Εκτέλεση του visitor μέσω execl
            execl("./visitor",
                "visitor",   
                "-id", intToStr(i+1+TOTAL_VISITORS), 
                "-d", doubleToStr(params.t_rest),     
                "-s", intToStr(shmid),                // Κοινόχρηστη Μνήμη    
                (char*)NULL); 
            // Αν η exec αποτύχει
            std::perror("execl");
            return 3;
        }
        // Τυχαίο sleep μεταξύ 0 και 3 δευτερολέπτων
        sleep(rand() % 3);
    }

    // Αναμονή για τους visitors
    std::cout << "Waiting for all visitors to complete..." << std::endl;
    for (int i = 0; i < TOTAL_VISITORS+TOTAL_VISITORS2 + 1; i++) {
        wait(NULL);
    }

    // Εξυπηρετήθηκαν όλοι οι visitors
    set_is_done(shared_mem, true);
    
    std::cout << "Main program completed." << std::endl;

    return 0;
}

Parameters parseArguments(int argc, char* argv[]) {
    Parameters params = {0, 0};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            params.t_order = std::stod(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            params.t_rest = std::stod(argv[i + 1]);
            i++;
        }
    }

    if (params.t_order <= 0 || params.t_rest <= 0) {
        std::cerr << "Usage: ./lexan -o t_order -r t_rest" << std::endl;
        std::exit(1);
    }

    return params;
}

const char* intToStr(int number) {
    static char buffers[10][20];
    static int index = 0;

    char* str = buffers[index];
    index = (index + 1) % 10; 

    sprintf(str, "%d", number);
    return str;
}

const char* doubleToStr(double number) {
    static char buffers[10][50];  
    static int index = 0;

    char* str = buffers[index];
    index = (index + 1) % 10; 

    snprintf(str, 50, "%.10f", number);  
    return str;
}

void set_is_done(SharedMemoryStruct *shared_mem, bool value) {
    pthread_mutex_lock(&shared_mem->done_mutex);
    shared_mem->is_done = value;
    pthread_mutex_unlock(&shared_mem->done_mutex);
    sem_post(&shared_mem->sem_order);
}

