#include "main.h"

int rank;
int size;

pthread_t threadDelay;
//GQueue *delayStack;
pthread_mutex_t packetMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stackMut = PTHREAD_MUTEX_INITIALIZER;

typedef struct stack_s {
    stackEl_t *pakiet;
    struct stack_s *prev;
    struct stack_s *next;
} stack_t;

stack_t **stack;

void check_thread_support(int provided)
{
    printf("THREAD SUPPORT: %d\n", provided);
    switch (provided) {
        case MPI_THREAD_SINGLE: 
            printf("Brak wsparcia dla wątków, kończę\n");
            /* Nie ma co, trzeba wychodzić */
	    fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
	    MPI_Finalize();
	    exit(-1);
	    break;
        case MPI_THREAD_FUNNELED: 
            printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
	    break;
        case MPI_THREAD_SERIALIZED: 
            /* Potrzebne zamki wokół wywołań biblioteki MPI */
            printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
	    break;
        case MPI_THREAD_MULTIPLE: printf("Pełne wsparcie dla wątków\n");
	    break;
        default: printf("Nikt nic nie wie\n");
    }
}


/* Wątek wprowadzający sztuczne opóźnienia komunikacyjne */
void *delayFunc(void *ptr)
{
    while (!end) {
	int percent = (rand()%5 + 1);
        if (!rank) percent = 0;
        struct timespec t = { 0, percent*25000000 };
        struct timespec rem = { 1, 0 };
        if (rank)
	    nanosleep(&t,&rem);

	pthread_mutex_lock( &packetMut );
	//stackEl_t *stackEl = g_queue_pop_tail( delayStack );
	stackEl_t *stackEl = pop_pkt(0);
	pthread_mutex_unlock( &packetMut );
        if (!end && stackEl) {
            /*if (stackEl->type == FINISH) {
                println( "FINISH DO %d", stackEl->dst );
            }*/
	    MPI_Send( stackEl->newP, 1, MPI_PAKIET_T, stackEl->dst, stackEl->type, MPI_COMM_WORLD);
	    free(stackEl->newP);
	    free(stackEl);
        }
    }

    //stackEl_t *tmp;
    //int i;
    /* 
    if (rank == 0)
    for (i=0;i<size;i++) {
	do { tmp = pop_pkt(i); 
             if (tmp) { 
		if (tmp->type == FINISH) 
		MPI_Send( tmp->newP, 1, MPI_PAKIET_T, tmp->dst, tmp->type, MPI_COMM_WORLD);
                free(tmp);
            } 
        } while(tmp);
    }
    */
    return 0;
}

void inicjuj(int *argc, char ***argv)
{
    int provided;
    //delayStack = g_queue_new();
    MPI_Init_thread(argc, argv,MPI_THREAD_MULTIPLE, &provided);
    check_thread_support(provided);

    /* Stworzenie typu */
    /* Poniższe (aż do MPI_Type_commit) potrzebne tylko, jeżeli
       brzydzimy się czymś w rodzaju MPI_Send(&typ, sizeof(pakiet_t), MPI_BYTE....
    */
    /* sklejone z stackoverflow */
    const int nitems=FIELDNO; // Struktura ma FIELDNO elementów - przy dodaniu pola zwiększ FIELDNO w main.h !
    int       blocklengths[FIELDNO] = {1,1,1,1}; /* tu zwiększyć na [4] = {1,1,1,1} gdy dodamy nowe pole */
    MPI_Datatype typy[FIELDNO] = {MPI_INT, MPI_INT,MPI_INT,MPI_INT}; /* tu dodać typ nowego pola (np MPI_BYTE, MPI_INT) */
    MPI_Aint     offsets[FIELDNO];

    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, kasa);
    offsets[2] = offsetof(packet_t, dst);
    offsets[3] = offsetof(packet_t, src);
    /* tutaj dodać offset nowego pola (offsets[2] = ... */

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);


    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    srand(rank);

    stack = malloc(sizeof(stack_t*)*size);
    memset( stack, 0, sizeof(stack_t*)*size);

    pthread_create( &threadCom, NULL, comFunc, 0);
    pthread_create( &threadDelay, NULL, delayFunc, 0);
    if (rank==ROOT) {
	pthread_create( &threadM, NULL, monitorFunc, 0);
    } 
}

void finalizuj(void)
{
    pthread_mutex_destroy( &konto_mut);
    /* Czekamy, aż wątek potomny się zakończy */
    //println("czekam na wątek \"komunikacyjny\"\n" );
    pthread_join(threadCom,NULL);
    //println("czekam na wątek \"opóźniający\"\n" );
    pthread_join(threadDelay,NULL);
    //if (rank==0) pthread_join(threadM,NULL);
    MPI_Type_free(&MPI_PAKIET_T);
    MPI_Finalize();
    //g_queue_free(delayStack);
    stackEl_t *tmp=0;
    int i;
    for (i=0;i<size;i++) {
	do { tmp = pop_pkt(i); 
             if (tmp) { 
                free(tmp);
            } 
        } while(tmp);
    }
    free(stack);
}

void push_pkt( stackEl_t *pakiet, int n)
{
    stack_t *tmp = malloc(sizeof(stack_t));
    tmp->pakiet = pakiet;

    tmp->next = 0;
    tmp->prev = 0;
    pthread_mutex_lock(&stackMut);
    if (!stack[n]) stack[n] = tmp;
    else {
	stack_t *head = stack[n];
        while (head->next) head = head->next; 
	tmp->prev = head; 
        head->next = tmp;
    }
    pthread_mutex_unlock(&stackMut);

}

stackEl_t *pop_pkt(int n)
{
    pthread_mutex_lock(&stackMut);
    if (stack[n] == NULL ) {
	pthread_mutex_unlock(&stackMut);
        return NULL;
    } else {
        stackEl_t *tmp = stack[n]->pakiet;
        stack_t *next = stack[n]->next;
        if (next)
	    next->prev = 0;
        free(stack[n]);
        stack[n] = next;
	pthread_mutex_unlock(&stackMut);
        return tmp; 
    }
}

void sendPacket(packet_t *data, int dst, int type)
{
    packet_t *newP = (packet_t *)malloc(sizeof(packet_t));
    memcpy(newP,data, sizeof(packet_t));
    stackEl_t *stackEl = (stackEl_t *)malloc(sizeof(stackEl_t));
    stackEl->dst = dst;
    stackEl->type = type;
    stackEl->newP = newP;
    push_pkt(stackEl, 0);
    
    /*
    pthread_mutex_lock( &packetMut );
    g_queue_push_head( delayStack, stackEl );
    pthread_mutex_unlock( &packetMut );
    */
}
