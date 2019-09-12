#ifndef MAINH
#define MAINH

/* boolean */
#define TRUE 1
#define FALSE 0

#define MAX_PEOPLE_ON_PYRKON 5
#define MAX_PEOPLE_ON_LECTURE 5
#define LECTURE_COUNT 10

/* types of messages */
#define WANT_TO_ENTER 1
#define ALRIGHT_TO_ENTER 2
#define EXIT 3

/* types of packet's data */
#define ENTER_PYRKON 0
// and lectures from 1 to 10 but we don't need that information to be defined.

/* MAX_HANDLERS musi się równać wartości ostatniego typu pakietu + 1 */
#define MAX_HANDLERS 4

/* states of processes */
#define BEFORE_PYRKON 0
#define ON_PYRKON 1
#define ON_LECTURE 2
#define AFTER_PYRKON 3

#include <mpi.h>
#include <stdlib.h>
#include <stdio.h> 
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>

/* FIELDNO: liczba pól w strukturze packet_t */
#define FIELDNO 4
typedef struct {
    int ts; /* zegar lamporta */

    /* przy dodaniu nowych pól zwiększy FIELDNO i zmodyfikuj
       plik init.c od linijki 98 (funkcja inicjuj, od linijki "const int nitems=FIELDNO;" ) */
    int data;
    int dst; /* pole ustawiane w sendPacket */
    int src; /* pole ustawiane w wątku komunikacyjnym na rank nadawcy */

} packet_t;

extern int rank,size; 
extern volatile int lamport_clock;
extern volatile char end;
extern MPI_Datatype MPI_PAKIET_T;
extern pthread_t threadCom, threadM, threadDelay;


extern volatile int last_message_clock;
extern volatile int pyrkon_number;
extern volatile int exited_from_pyrkon;
extern volatile int* permits;
extern volatile int* desired_lectures;
extern volatile int allowed_lecture;

/* do użytku wewnętrznego (implementacja opóźnień komunikacyjnych) */
// extern GQueue *delayStack;

extern pthread_mutex_t clock_mutex;
extern pthread_mutex_t modify_exited_from_pyrkon;
extern pthread_mutex_t wait_for_new_pyrkon;
extern pthread_mutex_t modify_permits;
extern pthread_mutex_t wait_for_agreement_to_enter;
extern pthread_mutex_t on_lecture_mutex;
extern pthread_mutex_t on_pyrkon_mutex;

/* argument musi być, bo wymaga tego pthreads. Wątek monitora, który po jakimś czasie ma wykryć stan */
extern void *monitorFunc(void *);
/* argument musi być, bo wymaga tego pthreads. Wątek komunikacyjny */
extern void *comFunc(void *);

extern void sendPacket(packet_t *, int, int);

#define PROB_OF_SENDING 35
#define PROB_OF_PASSIVE 5
#define PROB_OF_SENDING_DECREASE 1
#define PROB_SENDING_LOWER_LIMIT 1
#define PROB_OF_PASSIVE_INCREASE 1

/* makra do wypisywania na ekranie */
#define P_WHITE printf("%c[%d;%dm",27,1,37);
#define P_BLACK printf("%c[%d;%dm",27,1,30);
#define P_RED printf("%c[%d;%dm",27,1,31);
#define P_GREEN printf("%c[%d;%dm",27,1,33);
#define P_BLUE printf("%c[%d;%dm",27,1,34);
#define P_MAGENTA printf("%c[%d;%dm",27,1,35);
#define P_CYAN printf("%c[%d;%d;%dm",27,1,36);
#define P_SET(X) printf("%c[%d;%dm",27,1,31+(6+X)%7);
#define P_CLR printf("%c[%d;%dm",27,0,37);

/* Tutaj dodaj odwołanie do zegara lamporta */
#define println(FORMAT, ...) printf("%c[%d;%dm [%d][%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, lamport_clock, ##__VA_ARGS__, 27,0,37);

/* macro debug - działa jak printf, kiedy zdefiniowano
   DEBUG, kiedy DEBUG niezdefiniowane działa jak instrukcja pusta 
   
   używa się dokładnie jak printfa, tyle, że dodaje kolorków i automatycznie
   wyświetla rank

   w związku z tym, zmienna "rank" musi istnieć.
*/
#ifdef DEBUG
#define debug(...) printf("%c[%d;%dm [%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, ##__VA_ARGS__, 27,0,37);

#else
#define debug(...) ;
#endif

/* Nie ruszać, do użytku wewnętrznego przez wątek komunikacyjny */
typedef struct {
    packet_t *newP;
    int type;
    int dst;
    } stackEl_t;

/* wrzuca pakiet (pierwszy argument) jako element stosu o numerze danym drugim argumentem 
   stos w zmiennej stack
*/
void push_pkt( stackEl_t *, int);
/* zdejmuje ze stosu o danym numerze pakiet  
   przykład użycia: packet_t *pakiet = pop_pkt(0); 
*/
stackEl_t *pop_pkt(int);
#endif
