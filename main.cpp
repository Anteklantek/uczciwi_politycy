// thread0.c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <zconf.h>

#define ACKI_CHANNEL 10
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
int world_size;
int process_rank;
pthread_t odbieraj_acki_thread, odbieraj_zadania_thread, odbieraj_release_thread;
pthread_mutex_t lamport_lock, printf_lock, queue_lock, starsza_lock;

int get_index_of_last_elem();

int get_index_of_given_process_rank(int process_rank);

void insert_into_queue(struct queue_element element_to_insert) {
    pthread_mutex_lock(&queue_lock);
    for(int i = 0; i< world_size; i++){
        if(queue[i].process_rank == -1){
            queue[i].process_rank = element_to_insert.process_rank;
            queue[i].lamport_clock = element_to_insert.lamport_clock;
            break;
        } else if((queue[i].lamport_clock > element_to_insert.lamport_clock) ||
                  (queue[i].lamport_clock == element_to_insert.lamport_clock && queue[i].process_rank > element_to_insert.process_rank)){
            for(int j = get_index_of_last_elem()+1; j > i; j--){
                queue[j].process_rank = queue[j-1].process_rank;
                queue[j].lamport_clock = queue[j-1].lamport_clock;
            }
            queue[i].process_rank = element_to_insert.process_rank;
            queue[i].lamport_clock = element_to_insert.lamport_clock;
            break;
        }
    }
    pthread_mutex_unlock(&queue_lock);
}

int get_index_of_last_elem(){
    for(int i = 0; i < world_size; i++){
        if(queue[i].process_rank == -1){
            return i-1;
        }
    }
    return world_size - 1;
}

void delete_from_queue(int process_rank_to_delete){
    pthread_mutex_lock(&queue_lock);

    int index = get_index_of_given_process_rank(process_rank_to_delete);
    int last_index = get_index_of_last_elem();
    if(index == -1){

        pthread_mutex_lock(&printf_lock);
        printf("(proc %d, lamport %d) No such element with process rank: %d,\n", process_rank, lamport, process_rank_to_delete );
        fflush(stdout);
        pthread_mutex_unlock(&printf_lock);

        return;
    }
    for (int i = index ; i < last_index; i++){
        queue[i].process_rank = queue[i+1].process_rank;
        queue[i].lamport_clock = queue[i+1].lamport_clock;

    }
    queue[last_index].process_rank = -1;
    pthread_mutex_unlock(&queue_lock);

}

int get_index_of_given_process_rank(int process_rank){
    for(int i = 0; i < world_size; i++){
        if(queue[i].process_rank == process_rank)
            return i;
    }
    return -1;
}


int sumuj_tablice(int *tab);

void print_kolejka();

void print_starsze();

void *odbieraj_acki(void *arg)
{

    int dane_odbierane[2];
    while(1){
        MPI_Recv(&dane_odbierane, 2, MPI_INT, MPI_ANY_SOURCE, ACKI_CHANNEL, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        pthread_mutex_lock(&printf_lock);
        printf("(proc %d, lamport %d) odebral ACK od proc %d, z lamportem %d\n", process_rank, lamport, dane_odbierane[0], dane_odbierane[1]);
        fflush(stdout);
        pthread_mutex_unlock(&printf_lock);

        if(dane_odbierane[1] > lamport_zadania){
            pthread_mutex_lock(&starsza_lock);
            starsza_wiadomosc[dane_odbierane[0]] = 1;
            pthread_mutex_unlock(&starsza_lock);
        }

    }

    pthread_exit(NULL);
}


void *odbieraj_zadania(void *arg)
{
    struct queue_element received;
    int dane_odbierane[2];
    while(1){
        MPI_Recv(&dane_odbierane, 2, MPI_INT, MPI_ANY_SOURCE, ADD_TO_QUEUE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        pthread_mutex_lock(&printf_lock);
        printf("(proc %d, lamport %d) odebral żądanie od proc %d, z lamportem %d\n", process_rank, lamport, dane_odbierane[0], dane_odbierane[1]);
        fflush(stdout);
        pthread_mutex_unlock(&printf_lock);

        if(dane_odbierane[1] > lamport_zadania){
            pthread_mutex_lock(&starsza_lock);
            starsza_wiadomosc[dane_odbierane[0]] = 1;
            pthread_mutex_unlock(&starsza_lock);
        }
        pthread_mutex_lock(&lamport_lock);
        lamport = maxy(lamport, dane_odbierane[1]) + 1;
        pthread_mutex_unlock(&lamport_lock);
        received.process_rank = dane_odbierane[0];
        received.lamport_clock = dane_odbierane[1];
        insert_into_queue(received);

        //wyslij acka w odpowiedzi
        int dane_wysylane[2] =  { process_rank, lamport };
        MPI_Send(&dane_wysylane, 2, MPI_INT, dane_odbierane[0], ACKI_CHANNEL, MPI_COMM_WORLD);

        pthread_mutex_lock(&printf_lock);
        printf("(proc %d, lamport %d) wyslal ACK do proc %d z lamportem %d\n", process_rank, lamport, dane_odbierane[0], dane_wysylane[1]);
        fflush(stdout);
        pthread_mutex_unlock(&printf_lock);

    }

    pthread_exit(NULL);
}

void *odbieraj_release(void *arg) {
    int released_process_rank;
    while (1) {
        MPI_Recv(&released_process_rank, 2, MPI_INT, MPI_ANY_SOURCE, RELEASE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        pthread_mutex_lock(&printf_lock);
        printf("(proc %d, lamport %d) odebrał release od proc %d\n", process_rank, lamport, released_process_rank);
        fflush(stdout);
        pthread_mutex_unlock(&printf_lock);

        delete_from_queue(released_process_rank);

        pthread_mutex_lock(&printf_lock);
        print_kolejka();
        fflush(stdout);
        pthread_mutex_unlock(&printf_lock);


    }
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
    printf("My process rank is: %d\n", process_rank);

    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
//    printf("World size is: %d\n",world_size);

    starsza_wiadomosc = (int*)malloc(world_size * sizeof(int));
    pthread_mutex_lock(&starsza_lock);
    for (int i = 0; i < world_size; i++) {
        starsza_wiadomosc[i] = 0;
    }
    pthread_mutex_unlock(&starsza_lock);

    queue = (struct queue_element*)malloc(world_size * sizeof(struct queue_element));
    for(int i = 0; i < world_size; i++){
        queue[i].process_rank = -1;
    }

    if (pthread_mutex_init(&lamport_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&printf_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&queue_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }
    if (pthread_mutex_init(&starsza_lock, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }
    

    return 0;
}



int main()
{

    int error = initialize();
    if(error == 0){

        pthread_create(&odbieraj_acki_thread, NULL, &odbieraj_acki, (void *)(long)process_rank);
        pthread_create(&odbieraj_zadania_thread, NULL, &odbieraj_zadania, (void *)(long)process_rank);
        pthread_create(&odbieraj_release_thread, NULL, &odbieraj_release, (void *)(long)process_rank);


        while(1) {
            pthread_mutex_lock(&starsza_lock);
            for( int i = 0; i < world_size; i++) {
                starsza_wiadomosc[i] = 0;
            }
            pthread_mutex_unlock(&starsza_lock);

            lamport_zadania = lamport;
            int dane_wysylane[2] = { process_rank, lamport_zadania };

            for(int i = 0; i < world_size; i++){
                if(i == process_rank){
                    struct queue_element elem;
                    elem.process_rank = dane_wysylane[0];
                    elem.lamport_clock = dane_wysylane[1];
                    insert_into_queue(elem);
                    continue;
                }
                MPI_Send(&dane_wysylane, 2, MPI_INT, i, ADD_TO_QUEUE, MPI_COMM_WORLD);

                pthread_mutex_lock(&printf_lock);
                printf("(proc %d, lamport %d) wyslal żądanie do proc %d z lamportem %d\n", process_rank, lamport, i, dane_wysylane[1]);
                fflush(stdout);
                pthread_mutex_unlock(&printf_lock);

                lamport += 1;
            }

            while(1){
                pthread_mutex_lock(&queue_lock);
                if( (sumuj_tablice(starsza_wiadomosc) == world_size - 1) && (queue[0].process_rank == process_rank)){
                    pthread_mutex_lock(&printf_lock);
                    printf("(proc %d, lamport %d) rank pierwszego procesu w kolejce: %d\n",process_rank, lamport, queue[0].process_rank);
                    print_kolejka();
                    printf("(proc %d, lamport %d) SEKCJA KRYTYCZNA\n", process_rank, lamport);
                    print_kolejka();
                    print_starsze();
                    fflush(stdout);
                    pthread_mutex_unlock(&printf_lock);
                    break;
                } else {
                    printf("(proc %d, lamport %d) szturm na sekcję krytyczną nie udany\n", process_rank, lamport);
                }
                pthread_mutex_unlock(&queue_lock);
                for(int wait = 0; wait < 3600000; wait++){
                    wait+=1;
                }
            }

            for(int wait2 = 0; wait2 < 3600000; wait2++){
                wait2+=1;
            }

            pthread_mutex_lock(&printf_lock);
            printf("(proc %d, lamport %d) KONIEC SEKCJI KRYTYCZNEJ .\n", process_rank, lamport);
            fflush(stdout);
            pthread_mutex_unlock(&printf_lock);

            for(int i = 0; i < world_size; i++){
                if(process_rank == i){
                    delete_from_queue(i);
                    continue;
                }
                MPI_Send(&process_rank, 1, MPI_INT, i, RELEASE, MPI_COMM_WORLD);
            }
        }

        MPI_Finalize();

        pthread_exit(NULL);
        return 0;
    }
}

void print_starsze() {
    printf("(proc %d, lamport %d) ",process_rank, lamport);
    printf("starsza_wiadmość [");
    for(int i=0; i < world_size; i++){
        printf("%d,", starsza_wiadomosc[i]);
    }
    printf("]\n");
}

void print_kolejka() {
    printf("(proc %d, lamport %d) ",process_rank, lamport);
    printf("kolejka [");
    for(int i=0; i < world_size; i++){
        printf("%d,", queue[i].process_rank);
    }
    printf("]\n");
}

int sumuj_tablice(int *tab) {
    int sum = 0;
    for (int i = 0; i < world_size; i++) {
        sum += tab[i];
    }
    return sum;
}