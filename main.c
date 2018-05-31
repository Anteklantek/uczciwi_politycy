// thread0.c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <zconf.h>

#define ACKI_CHANNEL 10
#define ZADANIA_CHANNEL 11
#define ADD_TO_QUEUE 12
#define RELEASE 13

struct queue_element {
    int process_rank;//0
    int lamport_clock;//1
};


int maxy(int a, int b){
    if(b > a){
        return b;}
    else{
        return a;}
}

int lamport;
int *starsza_wiadomosc;
int lamport_zadania;
struct queue_element *queue;

int sumuj_tablice(int *tab);

void insert_into_queue_at_good_position(struct queue_element element_to_insert);

int world_size;
int process_rank;
pthread_t odbieraj_acki_thread, odbieraj_zadania_thread;

void *odbieraj_acki(void *arg)
{
    int dane_odbierane[2];
    while(1){
        MPI_Recv(&dane_odbierane, 2, MPI_INT, MPI_ANY_SOURCE, ACKI_CHANNEL, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if(dane_odbierane[1] > lamport_zadania){
            starsza_wiadomosc[dane_odbierane[0]] = 1;
        }
    }

    pthread_exit(NULL);
}

void *odbieraj_zadania(void *arg)
{
    struct queue_element received;
    int dane_odbierane[2];
    while(1){
        MPI_Recv(&dane_odbierane, 2, MPI_INT, MPI_ANY_SOURCE, ZADANIA_CHANNEL, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if(dane_odbierane[1] > lamport_zadania){
            starsza_wiadomosc[dane_odbierane[0]] = 1;
        }
        lamport = maxy(lamport, received.lamport_clock) + 1;
        received.process_rank = dane_odbierane[0];
        received.lamport_clock = dane_odbierane[1];
        insert_into_queue_at_good_position(received);
        //wyslij acka w odpowiedzi
    }

    pthread_exit(NULL);
}

void insert_into_queue_at_good_position(struct queue_element element_to_insert) {

    for(int i = 0; i< world_size; i++){
        if(queue[i].lamport_clock > element_to_insert.lamport_clock ||  queue[i].process_rank == -1){
            for(int j = i; j <  )
        }
    }

}

void swap(struct queue_element queue_element1, struct queue_element queue_element2){
    struct queue_element temp;
    temp.process_rank = queue_element1.process_rank;
    temp.lamport_clock = queue_element1.lamport_clock;
    queue_element1.process_rank = queue_element2.process_rank;
    queue_element1.lamport_clock = queue_element2.lamport_clock;
    queue_element2.process_rank = temp.process_rank;
    queue_element2.lamport_clock = temp.lamport_clock;
}


int initialize(){
    lamport = 0;

    int thread_support_provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &thread_support_provided);
    if (thread_support_provided != MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "doesn't support multithreading\n");
        MPI_Finalize();
        return -1;
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &process_rank);
    //printf("My process rank is: %d\n", process_rank);

    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    //printf("World size is: %d\n",world_size);

    starsza_wiadomosc = malloc(world_size * sizeof(int));
    for (int i = 0; i < world_size; i++) {
        starsza_wiadomosc[i] = 0;
    }

    queue = malloc(world_size * sizeof(struct queue_element));
    for(int i = 0; i < world_size; i++){
        queue[i].process_rank = -1;
    }



    return 0;
}



int main()
{
    int error = initialize();
    if(error == 0){

        pthread_create(&odbieraj_acki_thread, NULL, &odbieraj_acki, (void *)(long)process_rank);
        pthread_create(&odbieraj_zadania_thread, NULL, &odbieraj_zadania, (void *)(long)process_rank);
        pthread_create(&odbieraj_release_thread, NULL, &odbieraj_zadania, (void *)(long)process_rank);

        struct queue_element elem;
        elem.lamport_clock = lamport;
        elem.process_rank = process_rank;

        while(1){
            lamport_zadania = lamport;
            for(int i = 0; i < world_size; i++){
                MPI_Send(&elem, 2, MPI_INT, i, ADD_TO_QUEUE, MPI_COMM_WORLD);
                lamport += 1;
            }
            while(1){
                if( sumuj_tablice(starsza_wiadomosc) == world_size -1 && queue[0].process_rank == process_rank){
                    break;
                }
            }
            printf("Jestem w sekcji krytycznej, hurra!! Jestem nr: %d\n", process_rank);
            sleep(2);
            printf("Uciekam z sekcji krytycznej, :((. Jestem nr: %d\n", process_rank);

            int release = 0;
            for(int i = 0; i < world_size; i++){
                MPI_Send(&release, 1, MPI_INT, i, RELEASE, MPI_COMM_WORLD);
            }
            if (release) break;
        }

        MPI_Finalize();

        pthread_exit(NULL);
        return 0;
    }
}

int sumuj_tablice(int *tab) {
    int sum = 0;
    for (int i = 0; i < world_size; i++) {
        sum += tab[i];
    }
    return sum;
}