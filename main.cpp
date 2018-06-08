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

#define POLITYCY_MAIN_ID 111
#define SANATORIA_MAIN_ID 112

#define LICZBA_POLITYKOW 20
#define POJEMNOSC_SANATORIUM 10

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
int *starsza_politycy;
int lamport_zadania_politycy;
vector <queue_element> politycy_queue;
int *starsza_sanatoria;
int lamport_zadania_sanatoria;
vector <queue_element> sanatorium_queue;
int world_size;
int process_rank;
int zapotrzebowanie_na_politykow;
pthread_t odbieraj_thread;
pthread_mutex_t lamport_lock, printf_lock, queue_lock_politycy, starsza_lock_politycy, queue_lock_sanatoria, starsza_lock_sanatoria;


void przed_printf() {
    pthread_mutex_lock(&printf_lock);
    printf("(lamport: %d, lamport żądania: %d, proc: %d) ", lamport, lamport_zadania_politycy, process_rank);
}

void po_printf() {
    printf(" kolejka: [");
    for (int i = 0; i < politycy_queue.size(); i++) {
        printf("%d, ", politycy_queue.at(i).process_rank);
    }
    printf("] ");
//    printf("starsza: [");
//    for (int i = 0; i < world_size - 1; i++) {
//        printf("%d, ", starsza_politycy[i]);
//    }
//    printf("%d] ", starsza_politycy[world_size - 1]);
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


int get_index_of_given_process_rank(int process_rank, int queue_identifier);

int get_position_to_insert(int insert_process_rank, int insert_lamport_clock, int queue_identifier) {
    if (queue_identifier == POLITYCY_MAIN_ID) {
        for (int i = 0; i < politycy_queue.size(); i++) {
            if (politycy_queue[i].lamport_clock > insert_lamport_clock ||
                (politycy_queue[i].lamport_clock == insert_lamport_clock &&
                 politycy_queue[i].process_rank > insert_process_rank)) {
                return i;
            }
        }
        return -1;
    } else if (queue_identifier == SANATORIA_MAIN_ID) {
        for (int i = 0; i < sanatorium_queue.size(); i++) {
            if (sanatorium_queue[i].lamport_clock > insert_lamport_clock ||
                (sanatorium_queue[i].lamport_clock == insert_lamport_clock &&
                 sanatorium_queue[i].process_rank > insert_process_rank)) {
                return i;
            }
        }
        return -1;
    }
}

void insert_into_queue(struct queue_element element_to_insert, int queue_identifier) {

    if (queue_identifier == POLITYCY_MAIN_ID) {
        pthread_mutex_lock(&queue_lock_politycy);

        int where = get_position_to_insert(element_to_insert.process_rank, element_to_insert.lamport_clock,
                                           POLITYCY_MAIN_ID);

        if (where == -1) {
            politycy_queue.push_back(element_to_insert);
        } else {
            politycy_queue.insert(politycy_queue.begin() + where, element_to_insert);
        }

        pthread_mutex_unlock(&queue_lock_politycy);
    } else if (queue_identifier == SANATORIA_MAIN_ID) {

        pthread_mutex_lock(&queue_lock_sanatoria);

        int where = get_position_to_insert(element_to_insert.process_rank, element_to_insert.lamport_clock,
                                           SANATORIA_MAIN_ID);

        if (where == -1) {
            sanatorium_queue.push_back(element_to_insert);
        } else {
            sanatorium_queue.insert(sanatorium_queue.begin() + where, element_to_insert);
        }

        pthread_mutex_unlock(&queue_lock_sanatoria);


    }
}

void delete_from_queue(int process_rank_to_delete, int queue_identifier) {
    if (queue_identifier == POLITYCY_MAIN_ID) {
        pthread_mutex_lock(&queue_lock_politycy);
        int index = get_index_of_given_process_rank(process_rank_to_delete, POLITYCY_MAIN_ID);
        if (index == -1) {
            print1("Brak elementu z process rank: %d", process_rank_to_delete);
            pthread_mutex_unlock(&queue_lock_politycy);
            return;
        } else {
            politycy_queue.erase(politycy_queue.begin() + index);
        }
        pthread_mutex_unlock(&queue_lock_politycy);
    } else if (queue_identifier == SANATORIA_MAIN_ID) {
        pthread_mutex_lock(&queue_lock_sanatoria);
        int index = get_index_of_given_process_rank(process_rank_to_delete, SANATORIA_MAIN_ID);
        if (index == -1) {
            print1("Brak elementu z process rank: %d", process_rank_to_delete);
            pthread_mutex_unlock(&queue_lock_sanatoria);
            return;
        } else {
            sanatorium_queue.erase(politycy_queue.begin() + index);
        }
        pthread_mutex_unlock(&queue_lock_sanatoria);
    }
}


int get_index_of_given_process_rank(int comparing_process_rank, int queue_identifier) {
    if (queue_identifier == POLITYCY_MAIN_ID) {
        for (int i = 0; i < politycy_queue.size(); i++) {
            if (politycy_queue[i].process_rank == comparing_process_rank)
                return i;
        }
        return -1;
    } else if (queue_identifier == SANATORIA_MAIN_ID) {
        for (int i = 0; i < sanatorium_queue.size(); i++) {
            if (sanatorium_queue[i].process_rank == comparing_process_rank)
                return i;
        }
        return -1;
    }
}


int sumuj_tablice(int *tab);


void *odbieraj(void *arg) {

    int dane_odbierane[5];
    while (1) {
        MPI_Recv(&dane_odbierane, 5, MPI_INT, MPI_ANY_SOURCE, MAIN_CHANNEL, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (dane_odbierane[4] == POLITYCY_MAIN_ID) {
            if (dane_odbierane[2] == ACKI_ID) {

                if (dane_odbierane[1] > lamport_zadania_politycy) {
                    pthread_mutex_lock(&starsza_lock_politycy);
                    starsza_politycy[dane_odbierane[0]] = 1;
                    pthread_mutex_unlock(&starsza_lock_politycy);
                }
//            print2("odebral ACK od proc %d, z lamportem %d", dane_odbierane[0], dane_odbierane[1]);
            } else if (dane_odbierane[2] == ZADANIE_ID) {
                struct queue_element received;
                if (dane_odbierane[1] > lamport_zadania_politycy) {
                    pthread_mutex_lock(&starsza_lock_politycy);
                    starsza_politycy[dane_odbierane[0]] = 1;
                    pthread_mutex_unlock(&starsza_lock_politycy);
                }
                pthread_mutex_lock(&lamport_lock);
                lamport = maxy(lamport, dane_odbierane[1]) + 1;
                pthread_mutex_unlock(&lamport_lock);

                received.process_rank = dane_odbierane[0];
                received.lamport_clock = dane_odbierane[1];
                received.zapotrzebowanie = dane_odbierane[3];
                insert_into_queue(received, POLITYCY_MAIN_ID);

//            print3("odebral żądanie od proc %d, z lamportem %d i zapotrzebowaniem %d", dane_odbierane[0],
//                   dane_odbierane[1], dane_odbierane[3]);

                pthread_mutex_lock(&lamport_lock);
                lamport += 1;
                pthread_mutex_unlock(&lamport_lock);

                //wyslij acka w odpowiedzi
                int dane_wysylane[5] = {process_rank, lamport, ACKI_ID, -1, POLITYCY_MAIN_ID};
                MPI_Send(&dane_wysylane, 5, MPI_INT, dane_odbierane[0], MAIN_CHANNEL, MPI_COMM_WORLD);

//            print2("wyslal ACK do proc %d z lamportem %d", dane_odbierane[0], dane_wysylane[1]);

            } else if (dane_odbierane[2] = RELEASE_ID) {
                pthread_mutex_lock(&lamport_lock);
                lamport = maxy(lamport, dane_odbierane[1]) + 1;
                pthread_mutex_unlock(&lamport_lock);
                delete_from_queue(dane_odbierane[0], POLITYCY_MAIN_ID);
//            print1("odebrał release od proc %d", dane_odbierane[0]);
            }
        } else if (dane_odbierane[4] == SANATORIA_MAIN_ID) {
            if (dane_odbierane[2] == ACKI_ID) {
                if (dane_odbierane[1] > lamport_zadania_sanatoria) {
                    pthread_mutex_lock(&starsza_lock_sanatoria);
                    starsza_sanatoria[dane_odbierane[0]] = 1;
                    pthread_mutex_unlock(&starsza_lock_sanatoria);
                }
//            print2("odebral ACK od proc %d, z lamportem %d", dane_odbierane[0], dane_odbierane[1]);
            } else if (dane_odbierane[2] == ZADANIE_ID) {
                struct queue_element received;
                if (dane_odbierane[1] > lamport_zadania_sanatoria) {
                    pthread_mutex_lock(&starsza_lock_sanatoria);
                    starsza_sanatoria[dane_odbierane[0]] = 1;
                    pthread_mutex_unlock(&starsza_lock_sanatoria);
                }
                pthread_mutex_lock(&lamport_lock);
                lamport = maxy(lamport, dane_odbierane[1]) + 1;
                pthread_mutex_unlock(&lamport_lock);

                received.process_rank = dane_odbierane[0];
                received.lamport_clock = dane_odbierane[1];
                received.zapotrzebowanie = dane_odbierane[3];
                insert_into_queue(received, SANATORIA_MAIN_ID);

//            print3("odebral żądanie od proc %d, z lamportem %d i zapotrzebowaniem %d", dane_odbierane[0],
//                   dane_odbierane[1], dane_odbierane[3]);

                pthread_mutex_lock(&lamport_lock);
                lamport += 1;
                pthread_mutex_unlock(&lamport_lock);

                //wyslij acka w odpowiedzi
                int dane_wysylane[5] = {process_rank, lamport, ACKI_ID, -1, SANATORIA_MAIN_ID};
                MPI_Send(&dane_wysylane, 5, MPI_INT, dane_odbierane[0], MAIN_CHANNEL, MPI_COMM_WORLD);

//            print2("wyslal ACK do proc %d z lamportem %d", dane_odbierane[0], dane_wysylane[1]);

            } else if (dane_odbierane[2] = RELEASE_ID) {
                pthread_mutex_lock(&lamport_lock);
                lamport = maxy(lamport, dane_odbierane[1]) + 1;
                pthread_mutex_unlock(&lamport_lock);
                delete_from_queue(dane_odbierane[0], SANATORIA_MAIN_ID);
                print1("odebrał release od proc %d", dane_odbierane[0]);
            }


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

    starsza_politycy = (int *) malloc(world_size * sizeof(int));
    pthread_mutex_lock(&starsza_lock_politycy);
    for (int i = 0; i < world_size; i++) {
        starsza_politycy[i] = 0;
    }
    pthread_mutex_unlock(&starsza_lock_politycy);

    starsza_sanatoria = (int *) malloc(world_size * sizeof(int));
    pthread_mutex_lock(&starsza_lock_sanatoria);
    for (int i = 0; i < world_size; i++) {
        starsza_sanatoria[i] = 0;
    }
    pthread_mutex_unlock(&starsza_lock_sanatoria);


    if (pthread_mutex_init(&lamport_lock, NULL) != 0) {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&printf_lock, NULL) != 0) {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&queue_lock_politycy, NULL) != 0) {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&starsza_lock_politycy, NULL) != 0) {
        printf("\n mutex init failed\n");
        return 1;
    }

    srand((unsigned) time(0));

    return 0;
}

int get_my_index_in_queue(int queue_identifier) {
    if (queue_identifier == POLITYCY_MAIN_ID) {
        for (int i = 0; i < politycy_queue.size(); i++) {
            if (politycy_queue.at(i).process_rank == process_rank) {
                return i;
            }
        }
        print("nie ma mnie w kolejce");
        return -1;
    } else if (queue_identifier == SANATORIA_MAIN_ID) {
        for (int i = 0; i < sanatorium_queue.size(); i++) {
            if (sanatorium_queue.at(i).process_rank == process_rank) {
                return i;
            }
        }
        print("nie ma mnie w kolejce");
        return -1;
    }
}


int suma_zapotrzebowan_przede_mna(int queue_identifier) {
    if (queue_identifier == POLITYCY_MAIN_ID) {
        int my_index = get_my_index_in_queue(POLITYCY_MAIN_ID);
        if (my_index == -1) {
            //block entering by
            return LICZBA_POLITYKOW;
        } else {
            int sum = 0;
            for (int i = 0; i < my_index; i++) {
                sum += politycy_queue.at(i).zapotrzebowanie;
            }
//        print1("suma zapotrzebowania: %d", sum);
            return sum;
        }
    } else if (queue_identifier == SANATORIA_MAIN_ID) {
        int my_index = get_my_index_in_queue(SANATORIA_MAIN_ID);
        if (my_index == -1) {
            //block entering by
            return LICZBA_POLITYKOW;
        } else {
            int sum = 0;
            for (int i = 0; i < my_index; i++) {
                sum += sanatorium_queue.at(i).zapotrzebowanie;
            }
//        print1("suma zapotrzebowania: %d", sum);
            return sum;
        }
    }
}

int sumuj_tablice_politycy() {
    pthread_mutex_lock(&starsza_lock_politycy);
    int sum = 0;
    for (int i = 0; i < world_size; i++) {
        sum += starsza_politycy[i];
    }
    pthread_mutex_unlock(&starsza_lock_politycy);
    return sum;
}

int sumuj_tablice_sanatoria() {
    pthread_mutex_lock(&starsza_lock_sanatoria);
    int sum = 0;
    for (int i = 0; i < world_size; i++) {
        sum += starsza_sanatoria[i];
    }
    pthread_mutex_unlock(&starsza_lock_sanatoria);
    return sum;
}


int main() {

    int error = initialize();
    if (error == 0) {

        pthread_create(&odbieraj_thread, NULL, &odbieraj, (void *) (long) process_rank);

        while (1) {
            pthread_mutex_lock(&starsza_lock_politycy);
            for (int i = 0; i < world_size; i++) {
                starsza_politycy[i] = 0;
            }
            pthread_mutex_unlock(&starsza_lock_politycy);
//            print("wyzerowałem starsze");
            zapotrzebowanie_na_politykow = 7;//rand() % 5;
            //      print1("wylosowałem zapotrzebowanie na politykow: %d", zapotrzebowanie_na_politykow);
            lamport_zadania_politycy = lamport;
            int dane_wysylane_politycy[5] = {process_rank, lamport_zadania_politycy, ZADANIE_ID,
                                             zapotrzebowanie_na_politykow,
                                             POLITYCY_MAIN_ID};

            for (int i = 0; i < world_size; i++) {
                if (i == process_rank) {
                    struct queue_element elem;
                    elem.process_rank = dane_wysylane_politycy[0];
                    elem.lamport_clock = dane_wysylane_politycy[1];
                    elem.zapotrzebowanie = dane_wysylane_politycy[2];
                    insert_into_queue(elem, POLITYCY_MAIN_ID);
                } else {
                    MPI_Send(&dane_wysylane_politycy, 5, MPI_INT, i, MAIN_CHANNEL, MPI_COMM_WORLD);
//                    print3("wysłał żądanie o politykow do proc %d z lamportem %d, zapotrzebowanie %d", i, dane_wysylane[1],
//                           dane_wysylane[3]);
                    pthread_mutex_lock(&lamport_lock);
                    lamport += 1;
                    pthread_mutex_unlock(&lamport_lock);
                }
            }


            while (1) {
                pthread_mutex_lock(&queue_lock_politycy);
                if ((sumuj_tablice_politycy() == world_size - 1) &&
                    (suma_zapotrzebowan_przede_mna(POLITYCY_MAIN_ID) + zapotrzebowanie_na_politykow <=
                     LICZBA_POLITYKOW)) {
                    print("SEKCJA KRYTYCZNA, POLITYCY PRACUJĄ");
                    pthread_mutex_unlock(&queue_lock_politycy);
                    break;
                }
                pthread_mutex_unlock(&queue_lock_politycy);
                sleep(1);
            }

            sleep(rand() % 5 + 3);

            print("KONIEC SEKCJI KRYTYCZNEJ, POLITYCY POTRZEBUJĄ SANATORIUM");

            lamport_zadania_sanatoria = lamport;
            int dane_wysylane_sanatorium[5] = {process_rank, lamport_zadania_politycy, ZADANIE_ID,
                                               zapotrzebowanie_na_politykow,
                                               SANATORIA_MAIN_ID};

            for (int i = 0; i < world_size; i++) {
                if (i == process_rank) {
                    struct queue_element elem;
                    elem.process_rank = dane_wysylane_sanatorium[0];
                    elem.lamport_clock = dane_wysylane_sanatorium[1];
                    elem.zapotrzebowanie = dane_wysylane_sanatorium[2];
                    insert_into_queue(elem, SANATORIA_MAIN_ID);
                } else {
                    MPI_Send(&dane_wysylane_sanatorium, 5, MPI_INT, i, MAIN_CHANNEL, MPI_COMM_WORLD);
//                    print3("wysłał żądanie o sanatorium do proc %d z lamportem %d, zapotrzebowanie %d", i, dane_wysylane[1],
//                           dane_wysylane[3]);
                    pthread_mutex_lock(&lamport_lock);
                    lamport += 1;
                    pthread_mutex_unlock(&lamport_lock);
                }
            }

            while (1) {
                pthread_mutex_lock(&queue_lock_politycy);
                if ((sumuj_tablice_sanatoria() == world_size - 1) &&
                    (suma_zapotrzebowan_przede_mna(SANATORIA_MAIN_ID) + zapotrzebowanie_na_politykow <=
                     POJEMNOSC_SANATORIUM)) {
                    print("SEKCJA KRYTYCZNA, POLITYCY ODPOCZYWAJĄ W SANATORIUM");
                    pthread_mutex_unlock(&queue_lock_politycy);
                    break;
                }
                pthread_mutex_unlock(&queue_lock_politycy);
                sleep(1);
            }

            sleep(rand() % 5 + 3);

            print("KONIEC SEKCJI KRYTYCZNEJ, POLITYCY SĄ PONOWNIE DOSTĘPNI");


            for (int z = 0; z < world_size; z++) {

                pthread_mutex_lock(&lamport_lock);
                lamport += 1;
                pthread_mutex_unlock(&lamport_lock);

                if (process_rank == z) {
                    delete_from_queue(z, SANATORIA_MAIN_ID);
                } else {
                    int dane_wysylane_release_sanatorium[5] = {process_rank, lamport, RELEASE_ID, -1,
                                                               SANATORIA_MAIN_ID};
                    MPI_Send(&dane_wysylane_release_sanatorium, 5, MPI_INT, z, MAIN_CHANNEL, MPI_COMM_WORLD);
                }
            }


            for (int z = 0; z < world_size; z++) {

                pthread_mutex_lock(&lamport_lock);
                lamport += 1;
                pthread_mutex_unlock(&lamport_lock);

                if (process_rank == z) {
                    delete_from_queue(z, POLITYCY_MAIN_ID);
                } else {
                    int dane_wysylane_release_politycy[5] = {process_rank, lamport, RELEASE_ID, -1, POLITYCY_MAIN_ID};
                    MPI_Send(&dane_wysylane_release_politycy, 5, MPI_INT, z, MAIN_CHANNEL, MPI_COMM_WORLD);
                }
            }
            //    print("koniec cyklu procesu");
        }

        MPI_Finalize();

        pthread_exit(NULL);
        return 0;
    }
}


