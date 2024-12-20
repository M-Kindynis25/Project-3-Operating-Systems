/* shared_memory.hpp */
#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <semaphore.h>

#define NUM_TABLES 3
#define NUM_CHAIRS_PER_TABLE 4

struct Table {
    bool is_locked;         // Αν το τραπέζι έχει κλειδώσει λόγω αποχώρησης
    int count_chair;        // Κατειλημμένες καρέκλες
    sem_t sem_table;        // Σημαφόρος για τις καρέκλες
};

struct Statistics {
    int total_visitors;         // Συνολικός αριθμός επισκεπτών
    double total_visit_time;    // Συνολικός χρόνος παραμονής επισκεπτών
    double total_wait_time;     // Συνολικός χρόνος αναμονής επισκεπτών
    int total_water_orders;     // Συνολικός αριθμός παραγγελιών νερού
    int total_wine_orders;      // Συνολικός αριθμός παραγγελιών κρασιού
    int total_cheese_orders;    // Συνολικός αριθμός παραγγελιών τυριού
    int total_salad_orders;     // Συνολικός αριθμός παραγγελιών σαλάτας
};

struct SharedMemoryStruct {
    sem_t sem_order;                // Σημαφόρος για παραγγελίες
    sem_t sem_order_guard;          // Σημαφόρος για διαχείριση

    pthread_mutex_t stats_mutex;    // Mutex για τα στατιστικά
    Statistics stats;               // Στατιστικά

    pthread_mutex_t arrival_mutex;  // Mutex για την ώρα άφιξης
    double arrival_time;            // Ώρα άφιξης του τελευταίου επισκέπτη
    
    pthread_mutex_t done_mutex;     // Mutex για τη σημαία τερματισμού
    bool is_done;                   // Σήμα για τον τερματισμό του receptionist

    pthread_mutex_t table_mutex;    // Mutex για τα δεδομένα των τραπεζιών
    sem_t available_seats;          // Σημαφόρος για το αν υπάρχουν διαθέσημες καρέκλες γενικά σε όλα τα τραπέζια
    Table tables[NUM_TABLES];       // Πίνακας με τα δεδομένα των τραπεζιών
    int pos_table;                  // Δείκτης για την κυκλική αναζήτηση διαθέσιμου τραπεζιού
};

#endif // SHARED_MEMORY_H
