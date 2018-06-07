// thread0.c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <zconf.h>
#include <vector>
#include <cstdlib>
#include <ctime>

#define MAIN_CHANNEL 13

#define ACKI_ID 1
#define RELEASE_ID 2
#define ZADANIE_ID 3

#define LICZBA_POLITYKOW 20

using namespace std;

struct queue_element {
    int process_rank;//0
    int lamport_clock;//1
    int zapotrzebowanie;//2
};


int maxy(int a, int b) {
    if (b > a) {
        return b;
    } else {
        return a;
    }
}

int lamport;
int *starsza_wiadomosc;
int lamport_zadania;
vector <queue_element> queue;
int world_size;
int process_rank;
int zapotrzebowanie_na_politykow;
pthread_t odbieraj_thread;
pthread_mutex_t lamport_lock, printf_lock, queue_lock, starsza_lock;


void przed_printf() {
    pthread_mutex_lock(&printf_lock);
    printf("(lamport: %d, lamport żądania: %d, proc: %d) ", lamport, lamport_zadania, process_rank);
}

void po_printf() {
    printf(" kolejka: [");
    for (int i = 0; i < queue.size(); i++) {
        printf("%d, ", queue.at(i).process_rank);
    }
    printf("] ");

    printf("starsza: [");
    for (int i = 0; i < world_size - 1; i++) {
        printf("%d, ", starsza_wiadomosc[i]);
    }
    printf("%d] ", starsza_wiadomosc[world_size - 1]);
    printf("\n");
    fflush(stdout);
    pthread_mutex_unlock(&printf_lock);
}


void print(const char *text) {
    przed_printf();
    printf("%s", text);
    po_printf();
}

void print1(const char *text, int a) {
    przed_printf();
    printf(text, a);
    po_printf();
}

void print2(const char *text, int a, int b) {
    przed_printf();
    printf(text, a, b);
    po_printf();
}

void print3(const char *text, int a, int b, int c) {
    przed_printf();
    printf(text, a, b, c);
    po_printf();
}


int get_index_of_given_process_rank(int process_rank);

int get_position_to_insert(int insert_process_rank, int insert_lamport_clock) {
    for (int i = 0; i < queue.size(); i++) {
        if (queue[i].lamport_clock > insert_lamport_clock ||
            (queue[i].lamport_clock == insert_lamport_clock && queue[i].process_rank > insert_process_rank)) {
            return i;
        }
    }
    return -1;
}

void insert_into_queue(struct queue_element element_to_insert) {
    pthread_mutex_lock(&queue_lock);

    int where = get_position_to_insert(element_to_insert.process_rank, element_to_insert.lamport_clock);

    if (where == -1) {
        queue.push_back(element_to_insert);
    } else {
        queue.insert(queue.begin() + where, element_to_insert);
    }

    pthread_mutex_unlock(&queue_lock);
}

void delete_from_queue(int process_rank_to_delete) {
    pthread_mutex_lock(&queue_lock);
    int index = get_index_of_given_process_rank(process_rank_to_delete);
    if (index == -1) {
        print1("Brak elementu z process rank: %d", process_rank_to_delete);
        pthread_mutex_unlock(&queue_lock);
        return;
    } else {
        queue.erase(queue.begin() + index);
    }
    pthread_mutex_unlock(&queue_lock);
}

int get_index_of_given_process_rank(int comparing_process_rank) {
    for (int i = 0; i < queue.size(); i++) {
        if (queue[i].process_rank == comparing_process_rank)
            return i;
    }
    return -1;
}


int sumuj_tablice(int *tab);


void *odbieraj(void *arg) {

    int dane_odbierane[4];
    while (1) {
        MPI_Recv(&dane_odbierane, 4, MPI_INT, MPI_ANY_SOURCE, MAIN_CHANNEL, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (dane_odbierane[2] == ACKI_ID) {

            if (dane_odbierane[1] > lamport_zadania) {
                pthread_mutex_lock(&starsza_lock);
                starsza_wiadomosc[dane_odbierane[0]] = 1;
                pthread_mutex_unlock(&starsza_lock);
            }
            print2("odebral ACK od proc %d, z lamportem %d", dane_odbierane[0], dane_odbierane[1]);
        } else if (dane_odbierane[2] == ZADANIE_ID) {
            struct queue_element received;
            if (dane_odbierane[1] > lamport_zadania) {
                pthread_mutex_lock(&starsza_lock);
                starsza_wiadomosc[dane_odbierane[0]] = 1;
                pthread_mutex_unlock(&starsza_lock);
            }
            pthread_mutex_lock(&lamport_lock);
            lamport = maxy(lamport, dane_odbierane[1]) + 1;
            pthread_mutex_unlock(&lamport_lock);

            received.process_rank = dane_odbierane[0];
            received.lamport_clock = dane_odbierane[1];
            received.zapotrzebowanie = dane_odbierane[3];
            insert_into_queue(received);

            print3("odebral żądanie od proc %d, z lamportem %d i zapotrzebowaniem %d", dane_odbierane[0],
                   dane_odbierane[1], dane_odbierane[3]);

            pthread_mutex_lock(&lamport_lock);
            lamport += 1;
            pthread_mutex_unlock(&lamport_lock);

            //wyslij acka w odpowiedzi
            int dane_wysylane[4] = {process_rank, lamport, ACKI_ID, -1};
            MPI_Send(&dane_wysylane, 4, MPI_INT, dane_odbierane[0], MAIN_CHANNEL, MPI_COMM_WORLD);

            print2("wyslal ACK do proc %d z lamportem %d", dane_odbierane[0], dane_wysylane[1]);

        } else if (dane_odbierane[2] = RELEASE_ID) {
            pthread_mutex_lock(&lamport_lock);
            lamport = maxy(lamport, dane_odbierane[1]) + 1;
            pthread_mutex_unlock(&lamport_lock);
            delete_from_queue(dane_odbierane[0]);
            print1("odebrał release od proc %d", dane_odbierane[0]);
        }
    }
    pthread_exit(NULL);
}


int initialize() {
    lamport = 0;

    int thread_support_provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &thread_support_provided);
    if (thread_support_provided != MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "doesn't support multithreading");
        MPI_Finalize();
        return -1;
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &process_rank);
    //printf("My process rank is: %d\n", process_rank);

    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    //printf("World size is: %d\n",world_size);

    starsza_wiadomosc = (int *) malloc(world_size * sizeof(int));
    pthread_mutex_lock(&starsza_lock);
    for (int i = 0; i < world_size; i++) {
        starsza_wiadomosc[i] = 0;
    }
    pthread_mutex_unlock(&starsza_lock);


    if (pthread_mutex_init(&lamport_lock, NULL) != 0) {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&printf_lock, NULL) != 0) {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&queue_lock, NULL) != 0) {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&starsza_lock, NULL) != 0) {
        printf("\n mutex init failed\n");
        return 1;
    }

    srand((unsigned) time(0));

    return 0;
}

int get_my_index_in_queue() {
    for (int i = 0; i < queue.size(); i++) {
        if (queue.at(i).process_rank == process_rank) {
            return i;
        }
    }
    print("nie ma mnie w kolejce");
    return -1;
}


int suma_zapotrzebowan_przede_mna() {
    int my_index = get_my_index_in_queue();
    if (my_index == -1) {
        //block entering by
        printf("block entering");
        return LICZBA_POLITYKOW;
    } else {
        int sum = 0;
        for (int i = 0; i < my_index; i++) {
            sum += queue.at(i).zapotrzebowanie;
        }
        print1("suma zapotrzebowania: %d", sum);
        return sum;
    }
}


int main() {

    int error = initialize();
    if (error == 0) {

        pthread_create(&odbieraj_thread, NULL, &odbieraj, (void *) (long) process_rank);

        while (1) {
            pthread_mutex_lock(&starsza_lock);
            for (int i = 0; i < world_size; i++) {
                starsza_wiadomosc[i] = 0;
            }
            pthread_mutex_unlock(&starsza_lock);
//            print("wyzerowałem starsze");
            zapotrzebowanie_na_politykow = 7;//rand() % 5;
            //      print1("wylosowałem zapotrzebowanie na politykow: %d", zapotrzebowanie_na_politykow);
            lamport_zadania = lamport;
            int dane_wysylane[4] = {process_rank, lamport_zadania, ZADANIE_ID, zapotrzebowanie_na_politykow};

            for (int i = 0; i < world_size; i++) {
                if (i == process_rank) {
                    struct queue_element elem;
                    elem.process_rank = dane_wysylane[0];
                    elem.lamport_clock = dane_wysylane[1];
                    elem.zapotrzebowanie = dane_wysylane[2];
                    insert_into_queue(elem);
                } else {
                    MPI_Send(&dane_wysylane, 4, MPI_INT, i, MAIN_CHANNEL, MPI_COMM_WORLD);
                    print3("wysłał żądanie do proc %d z lamportem %d, zapotrzebowanie %d", i, dane_wysylane[1],
                           dane_wysylane[3]);
                    pthread_mutex_lock(&lamport_lock);
                    lamport += 1;
                    pthread_mutex_unlock(&lamport_lock);
                }
            }


            while (1) {
                pthread_mutex_lock(&queue_lock);
                if ((sumuj_tablice(starsza_wiadomosc) == world_size - 1) &&
                    (suma_zapotrzebowan_przede_mna() + zapotrzebowanie_na_politykow <= LICZBA_POLITYKOW)) {
                    print("SEKCJA KRYTYCZNA");
                    pthread_mutex_unlock(&queue_lock);
                    break;
                }
//                print("nie udało się wejsc do sekcji");
                pthread_mutex_unlock(&queue_lock);
                sleep(1);
            }

            sleep(4);

            print("KONIEC SEKCJI KRYTYCZNEJ");

            for (int z = 0; z < world_size; z++) {
                pthread_mutex_lock(&lamport_lock);
                lamport += 1;
                pthread_mutex_unlock(&lamport_lock);

                if (process_rank == z) {
                    delete_from_queue(z);
                } else {
                    int dane_wysylane[3] = {process_rank, lamport, RELEASE_ID};
                    MPI_Send(&dane_wysylane, 3, MPI_INT, z, MAIN_CHANNEL, MPI_COMM_WORLD);
                }
            }
            //    print("koniec cyklu procesu");
        }

        MPI_Finalize();

        pthread_exit(NULL);
        return 0;
    }
}

int sumuj_tablice(int *tab) {
    pthread_mutex_lock(&starsza_lock);
    int sum = 0;
    for (int i = 0; i < world_size; i++) {
        sum += tab[i];
    }
    pthread_mutex_unlock(&starsza_lock);
    return sum;
}
