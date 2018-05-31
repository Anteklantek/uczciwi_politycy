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

int get_index_of_last_elem();

int get_index_of_given_process_rank(int process_rank);

void insert_into_queue(struct queue_element element_to_insert) {

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

    int index = get_index_of_given_process_rank(process_rank_to_delete);
    int last_index = get_index_of_last_elem();
    if(index == -1){
        printf("No such element my_process_rank:%d, to_delete: %d,\n", process_rank, process_rank_to_delete );
        return;
    }
    for (int i = index ; i < last_index; i++){
        queue[i].process_rank = queue[i+1].process_rank;
        queue[i].lamport_clock = queue[i+1].lamport_clock;

    }
    queue[last_index].process_rank = -1;

}

int get_index_of_given_process_rank(int process_rank){
    for(int i = 0; i < world_size; i++){
        if(queue[i].process_rank == process_rank)
            return i;
    }
    return -1;
}


int sumuj_tablice(int *tab);

void *odbieraj_acki(void *arg)
{
    if(process_rank == 0){
        printf("Odbieram acki\n");
    }
    int dane_odbierane[2];
    while(1){
        MPI_Recv(&dane_odbierane, 2, MPI_INT, MPI_ANY_SOURCE, ACKI_CHANNEL, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("(proc %d, clock %d) odebral od (proc %d, clock %d)\n", process_rank, lamport, dane_odbierane[0], dane_odbierane[1]);
        if(dane_odbierane[1] > lamport_zadania){
            starsza_wiadomosc[dane_odbierane[0]] = 1;
        }

    }

    pthread_exit(NULL);
}


void *odbieraj_zadania(void *arg)
{
    printf("odbieram żądania \n");
    struct queue_element received;
    int dane_odbierane[2];
    while(1){
        MPI_Recv(&dane_odbierane, 2, MPI_INT, MPI_ANY_SOURCE, ADD_TO_QUEUE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("(proc %d, clock %d) odebral od (proc %d, clock %d)\n", process_rank, lamport, dane_odbierane[0], dane_odbierane[1]);
        if(dane_odbierane[1] > lamport_zadania){
            starsza_wiadomosc[dane_odbierane[0]] = 1;
        }
        lamport = maxy(lamport, dane_odbierane[1]) + 1;
        received.process_rank = dane_odbierane[0];
        received.lamport_clock = dane_odbierane[1];
        insert_into_queue(received);

        //wyslij acka w odpowiedzi
        int dane_wysylane[2] =  { process_rank, lamport };
        MPI_Send(&dane_wysylane, 2, MPI_INT, dane_odbierane[0], ACKI_CHANNEL, MPI_COMM_WORLD);
        printf("(proc %d, clock %d) wyslal ACK do proc %d\n", process_rank, dane_wysylane[0], dane_wysylane[1]);
    }

    pthread_exit(NULL);
}

void *odbieraj_release(void *arg) {
    int dane_odbierane[2];
    while (1) {
        MPI_Recv(&dane_odbierane, 2, MPI_INT, MPI_ANY_SOURCE, RELEASE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        delete_from_queue(dane_odbierane[0]);
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
//    printf("My process rank is: %d\n", process_rank);

    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
//    printf("World size is: %d\n",world_size);

    starsza_wiadomosc = (int*)malloc(world_size * sizeof(int));
    for (int i = 0; i < world_size; i++) {
        starsza_wiadomosc[i] = 0;
    }

    queue = (struct queue_element*)malloc(world_size * sizeof(struct queue_element));
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
        pthread_create(&odbieraj_release_thread, NULL, &odbieraj_release, (void *)(long)process_rank);


        while(1) {
            for( int i = 0; i < world_size; i++) {
                starsza_wiadomosc[i] = 0;
            }

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
                lamport += 1;
            }

            while(1){
                if( sumuj_tablice(starsza_wiadomosc) == world_size -1 && queue[0].process_rank == process_rank){
                    break;
                }
            }

            fflush(stdout);
            printf("Jestem w sekcji krytycznej, hurra!! Jestem nr: %d\n", process_rank);
            sleep(2);
            printf("Uciekam z sekcji krytycznej, :((. Jestem nr: %d\n", process_rank);

            int release;
            for(int i = 0; i < world_size; i++){
                MPI_Send(&release, 1, MPI_INT, i, RELEASE, MPI_COMM_WORLD);
            }
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