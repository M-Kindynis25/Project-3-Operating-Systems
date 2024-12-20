/* visitor.cpp */
#include <iostream>
#include <sys/wait.h>
#include <cstring>
#include <sys/shm.h>
#include <semaphore.h>
#include "shared_memory.hpp"
#include "logger.hpp"

// Δομή Παραμέτρων
struct Parameters {
    int idVis;
    double t_rest;
    int shmid;
};

// Ανάλυση των ορισμάτων γραμμής εντολών
Parameters parseArguments(int argc, char* argv[]); 

// Συνάρτηση για υπολογισμό τυχαίας διάρκειας ξεκούραση
double getRandomRestTime(double t_rest);

// Συνάρτησης που ενημερώνει την ώρα άφιξης
void update_arrival_time(SharedMemoryStruct *shared_mem, double new_arrival_time);

// Συνάρτηση για τυχαία επιλογή παραγγελίας
void makeOrder(SharedMemoryStruct* shared_mem, int idVis);

// Ενημέρωση του συνολικού χρόνου παραμονής
void update_total_visit_time(SharedMemoryStruct *shared_mem, double visit_time);

// Συνάρτηση που εντοπίζει και "δεσμεύει" τραπέζι
int findAndSitAtTable(SharedMemoryStruct* shared_mem, int idVis);

// Συνάρτηση αποχώρησης επισκέπτη από το τραπέζι
void leaveTable(SharedMemoryStruct* shared_mem, int table_id, int idVis);

int main(int argc, char* argv[]) {
    srand(static_cast<unsigned int>(time(0))); 
    // Αρχικοποίηση ονομάτων αρχείων και παραμέτρων
    Parameters params = parseArguments(argc, argv);

    // Αν δεν έχει δοθεί ως παράμετρος
    if (params.idVis == -1) {
        // Δημιουργία ενός τυχαίου αριθμού
        int randomID = rand() % 1000 + 1; // Τυχαίος αριθμός από 1 έως 1000
        params.idVis = randomID;
    }

    // Σύνδεση στην κοινόχρηστη μνήμη
    void* addr = shmat(params.shmid, NULL, 0);
    if (addr == (void*) -1) {
        perror("shmat");
        return 1;
    }

    SharedMemoryStruct* shared_mem = static_cast<SharedMemoryStruct*>(addr);

    // Καταγραφή της ώρας άφιξης
    double arrival_time = static_cast<double>(time(NULL));
    update_arrival_time(shared_mem, arrival_time);

    Logger logger;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "The visitor %d has arrived at the bar.", params.idVis);
    logger.logEvent(buffer);

    // Εύρεση και κάθισμα σε τραπέζι
    int table_id = findAndSitAtTable(shared_mem, params.idVis);

    ////////////////////////////////  ΠΑΡΑΓΓΕΛΙΑ  ////////////////////////////////////////////
    // Αναμονή για τη σειρά του επισκέπτη
    logger.logEvent("The customer is waiting to place an order...");
    sem_wait(&shared_mem->sem_order_guard); // Περιμένει μέχρι να επιτραπεί να κάνει sem_post
    sem_post(&shared_mem->sem_order);       // Ξεκλειδώνει την κύρια λειτουργικότητα
    
    // Ο πελάτης κάνει την παραγγελία
    makeOrder(shared_mem, params.idVis);

    // Ο επισκέπτης ξεκουράζεται
    double rest_time = getRandomRestTime(params.t_rest);
    snprintf(buffer, sizeof(buffer), "The visitor %d is resting for %.2f seconds.", params.idVis, rest_time);
    logger.logEvent(buffer);
    sleep(rest_time);

    // Υπολογισμός και ενημέρωση  συνολικού χρόνου παραμονής
    double departure_time = static_cast<double>(time(NULL));
    double visit_time = departure_time - arrival_time;
    update_total_visit_time(shared_mem, visit_time);
    ///////////////////////////////////////////////////////////////////////////////////////////

    // Αποχώρηση από το τραπέζι
    leaveTable(shared_mem, table_id, params.idVis);

    return 0;
}

Parameters parseArguments(int argc, char* argv[]) {
    Parameters params = {-1, 0, 0};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-id") == 0 && i + 1 < argc) {
            params.idVis = std::atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            params.t_rest = std::stod(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            params.shmid = std::atoi(argv[i + 1]);
            i++;
        }
    }

    if (params.t_rest <= 0 || params.shmid <= 0) {
        std::cerr << "Usage: ./visitor -id idVis -d resttime -s shmid" << std::endl;
        std::exit(1);
    }

    return params;
}

double getRandomRestTime(double t_rest) {
    return (rand() % 300 + 700) / 1000.0 * t_rest;  // Τυχαία διάρκεια μεταξύ [70 - 100]%
}

void update_arrival_time(SharedMemoryStruct *shared_mem, double new_arrival_time) {
    pthread_mutex_lock(&shared_mem->arrival_mutex);
    shared_mem->arrival_time = new_arrival_time;
    pthread_mutex_unlock(&shared_mem->arrival_mutex);
}

void makeOrder(SharedMemoryStruct* shared_mem, int idVis) {
    srand(static_cast<unsigned>(time(0)));

    bool ordered_water = rand() % 2; // 50% πιθανότητα για νερό
    bool ordered_wine = rand() % 2;  // 50% πιθανότητα για κρασί
    if (!ordered_water && !ordered_wine) {
        ordered_water = true; // Υποχρεωτική επιλογή ενός από τα δύο
    }

    bool ordered_cheese = rand() % 2; // 50% πιθανότητα για τυρί
    bool ordered_salad = rand() % 2;  // 50% πιθανότητα για σαλάτα

    // Κλείδωμα του mutex πριν την πρόσβαση στα κοινόχρηστα δεδομένα
    pthread_mutex_lock(&shared_mem->stats_mutex);

    // Ενημέρωση στατιστικών
    shared_mem->stats.total_visitors++;
    if (ordered_water) {
        shared_mem->stats.total_water_orders++;
    }
    if (ordered_wine) {
        shared_mem->stats.total_wine_orders++;
    }
    if (ordered_cheese) {
        shared_mem->stats.total_cheese_orders++;
    }
    if (ordered_salad) {
        shared_mem->stats.total_salad_orders++;
    }

    // Ξεκλείδωμα του mutex
    pthread_mutex_unlock(&shared_mem->stats_mutex);

    Logger logger;
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "The visitor's %d order is: %s%s%s%s",
            idVis,
            ordered_water ? "Water " : "",
            ordered_wine ? "Wine " : "",
            ordered_cheese ? "Cheese " : "",
            ordered_salad ? "Salad " : "");
    logger.logEvent(buffer);
}

void update_total_visit_time(SharedMemoryStruct *shared_mem, double visit_time) {
    pthread_mutex_lock(&shared_mem->stats_mutex);
    shared_mem->stats.total_visit_time += visit_time;
    pthread_mutex_unlock(&shared_mem->stats_mutex);
}

int findAndSitAtTable(SharedMemoryStruct* shared_mem, int idVis) {
    while (true) {
        // Προσπαθούμε να δεσμεύσουμε μια διαθέσιμη καρέκλα σε κάποιο τραπέζι
        sem_wait(&shared_mem->available_seats);

        pthread_mutex_lock(&shared_mem->table_mutex);
        int assigned_table = -1;

        // Αναζήτηση τραπεζιού που:
        // 1) Δεν είναι κλειδωμένο (is_locked = false)
        // 2) Δεν έχει γεμίσει (count_chair < NUM_CHAIRS_PER_TABLE)
        for (int i = 0; i < NUM_TABLES; i++) {
            int table_id = (shared_mem->pos_table + i) % NUM_TABLES;
            if (!shared_mem->tables[table_id].is_locked &&
                shared_mem->tables[table_id].count_chair < NUM_CHAIRS_PER_TABLE) {
                assigned_table = table_id;
                // Βρήκαμε κατάλληλο τραπέζι
                break;
            }
        }

        if (assigned_table == -1) {
            // Δεν βρέθηκε κατάλληλο τραπέζι.
            // Επιστρέφουμε τη δεσμευμένη καρέκλα
            pthread_mutex_unlock(&shared_mem->table_mutex);
            sem_post(&shared_mem->available_seats);

            // Περιμένουμε λίγο πριν ξαναπροσπαθήσουμε
            sleep(1);
            continue; // Επανάληψη του βρόχου, ξαναπροσπάθεια
        }

        // Εδώ έχουμε ένα κατάλληλο τραπέζι
        int table_id = assigned_table;
        
        // Ξεκλειδώνουμε το mutex για να μην μπλοκάρουμε άλλες λειτουργίες όσο περιμένουμε
        pthread_mutex_unlock(&shared_mem->table_mutex);
        sem_wait(&shared_mem->tables[table_id].sem_table);

        // Τώρα ξανακλειδώνουμε το mutex για να ενημερώσουμε το state του τραπεζιού με ασφάλεια
        pthread_mutex_lock(&shared_mem->table_mutex);
        // Ελέγχουμε ξανά τις συνθήκες γιατί μπορεί να αλλάξει κάτι
        if (shared_mem->tables[table_id].is_locked ||
            shared_mem->tables[table_id].count_chair >= NUM_CHAIRS_PER_TABLE) {
            // Κάτι άλλαξε ενδιάμεσα; Επιστρέφουμε καρέκλα και διαθέσιμη θέση.
            pthread_mutex_unlock(&shared_mem->table_mutex);
            sem_post(&shared_mem->tables[table_id].sem_table);
            sem_post(&shared_mem->available_seats);

            // Δοκιμάζουμε ξανά από την αρχή.
            sleep(1);
            continue;
        }

        // Αυξάνουμε τις κατειλημμένες καρέκλες
        shared_mem->tables[table_id].count_chair++;
        int num_chair = shared_mem->tables[table_id].count_chair;

        pthread_mutex_unlock(&shared_mem->table_mutex);

        Logger logger;
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "The visitor %d sat at table: %d, Occupied chairs: %d/%d",
                idVis, table_id, num_chair, NUM_CHAIRS_PER_TABLE);
        logger.logEvent(buffer);

        return table_id;
    }
}

void leaveTable(SharedMemoryStruct* shared_mem, int table_id, int idVis) {
    pthread_mutex_lock(&shared_mem->table_mutex);
    // Μείωση κατειλημμένων καρεκλών
    shared_mem->tables[table_id].count_chair--;
    int num_chair = shared_mem->tables[table_id].count_chair;

    // Αν το τραπέζι άδειασε (num_chair == 0):
    // Τότε ξεκλειδώνει (is_locked = false) για να μπορεί να χρησιμοποιηθεί ξανά από την αρχή.
    if (num_chair == 0) {
        shared_mem->tables[table_id].is_locked = false; // Το τραπέζι άδειασε, ξαναγίνεται διαθέσιμο κανονικά.
    
    // Αν μετά την αποχώρηση το τραπέζι δεν άδειασε
    // Τότε τραπέζι κλειδώνει (is_locked = true) ώστε να μην κάτσει νέος πελάτης.
    } else {
        shared_mem->tables[table_id].is_locked = true;
    }

    pthread_mutex_unlock(&shared_mem->table_mutex);

    Logger logger;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "The visitor %d is leaving the table: %d, Remaining chairs: %d/%d",
            idVis, table_id, num_chair, NUM_CHAIRS_PER_TABLE);
    logger.logEvent(buffer);


    // Ενημερώνει ότι μια καρέκλα ελευθερώθηκε
    sem_post(&shared_mem->tables[table_id].sem_table);
    sem_post(&shared_mem->available_seats);
}












