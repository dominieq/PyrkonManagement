#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-msc30-c"
#pragma clang diagnostic ignored "-Wunused-parameter"

#include "main.h"

MPI_Datatype MPI_PAKIET_T;
pthread_t threadCom, threadM;

pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t permits_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t allow_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t on_lecture_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t on_pyrkon_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile int state = BEFORE_PYRKON;
volatile int lamport_clock;
volatile int pyrkon_number = 0;
volatile int people_on_pyrkon = 0;
volatile int* desired_lectures;
volatile int last_clock = 0;
volatile int received_agreement = 0;
volatile int allowed_lecture = 0;
volatile int* permits;
volatile int* exited_from_pyrkon;

/* end == TRUE oznacza wyjście z main_loop */
volatile char end = FALSE;
void mainLoop(void);

/* Deklaracje zapowiadające handlery. */
void want_enter_handler(packet_t *message);
void entering_handler(packet_t *message);
void alright_enter_handler(packet_t *message);
void exit_handler(packet_t *message);

/* typ wskaźnik na funkcję zwracającej void i z argumentem packet_t* */
typedef void (*f_w)(packet_t *);

/* Lista handlerów dla otrzymanych pakietów
   Nowe typy wiadomości dodaj w main.h, a potem tutaj dodaj wskaźnik do handlera.
   Funkcje handlerowe są na końcu pliku.
   Nie zapomnij dodać deklaracji zapowiadającej funkcji!
*/
f_w handlers[MAX_HANDLERS] = {
        [WANT_TO_ENTER] = want_enter_handler,
        [ENTERING_TO] = entering_handler,
        [ALRIGHT_TO_ENTER] = alright_enter_handler,
        [EXIT] = exit_handler
};

extern void inicjuj(int *argc, char ***argv);
extern void finalizuj(void);

int get_clock(int to_increase) {
    int result = 0;

    pthread_mutex_lock( &clock_mutex );
    if (to_increase) lamport_clock++;
    result = lamport_clock;
    pthread_mutex_unlock( &clock_mutex );

    return result;
}

void pyrkon_broadcast(int type, int data, int additional_data) {

    last_clock = get_clock(TRUE);

    packet_t result;
    result.ts = last_clock;
    result.data = data;
    result.additional_data = additional_data;

    for (int i = 0; i < size; i++){
        if (rank != i) sendPacket( &result, i, type );
    }
}

void wait_for_agreement(){

    pthread_mutex_lock(&allow_mutex);
    // pthread_mutex_unlock(&allow_mutex);
}

int my_random_int(int min, int max) {

    float tmp = (float)((float)rand() / (float)RAND_MAX);
    return (int) tmp * (max - min + 1) + min;
}

int main ( int argc, char **argv ) {

    /* Tworzenie wątków, inicjalizacja itp */
    inicjuj( &argc, &argv);

    mainLoop();

    finalizuj();
    return 0;
}


/* Wątek główny - przesyłający innym pieniądze */
void mainLoop ( void ) {

    int prob_of_sending = PROB_OF_SENDING;
    int dst;
    packet_t pakiet;

    /* mały sleep, by procesy nie zaczynały w dokładnie tym samym czasie */
    struct timespec t = { 0, rank*50000 };
    struct timespec rem = { 1, 0 };
    nanosleep(&t,&rem); 

    exited_from_pyrkon = malloc(size * sizeof(int));
    permits = malloc((LECTURE_COUNT + 1) * sizeof(int));
    desired_lectures = malloc(LECTURE_COUNT * sizeof(int));

    while ( !end ) {
	    int percent = rand()%2 + 1;
        struct timespec t = { percent, 0 };
        struct timespec rem = { 1, 0 };
        nanosleep(&t,&rem);


        switch( state ){

            case BEFORE_PYRKON: {
                println( "PROCESS [%d] is waiting for Pyrkon.\n", rank )
                /* Process is waiting in line to enter Pyrkon. It broadcasts question and waits for agreement. */
                pyrkon_broadcast(WANT_TO_ENTER, 0, 0);
                wait_for_agreement(); // All processes must enter Pyrkon eventually,
                state = ON_PYRKON; // so there is no "if" only mutex.

                println( "PROCESS [%d] Enters Pyrkon.\n", rank )
                /* Process entered Pyrkon and chooses lectures. */
                int lectures_number = my_random_int(1, LECTURE_COUNT - 1);
                for(int i = 1; i < LECTURE_COUNT + 1; i++) {
                    int lecture = my_random_int(1, lectures_number);
                    desired_lectures[lecture] = 1;
                }

                /* When it's chosen its desired lectures it broadcasts that information to others. */
                for(int i = 1; i < LECTURE_COUNT + 1; i++) {
                    if(desired_lectures[i] == 1) {
                        pyrkon_broadcast(WANT_TO_ENTER, i, 0);
                    }
                }
                break;
            }

            case ON_PYRKON: {
                println("PROCESS [%d] has chosen it's lectures and is on Pyrkon.\n", rank)
                /* When on Pyrkon, process waits for agreement to enter one of its desired lectures. */
                wait_for_agreement();
                state = ON_LECTURE;
                break;
            }

            case ON_LECTURE: {
                println( "PROCESS [%d] is on lecture number %d.\n", rank, allowed_lecture )
                /* When on lecture, process waits five seconds. */
                sleep(5000);
                /* Process checks if it wants to go to another lecture. */
                desired_lectures[ allowed_lecture ] = 0;
                for (int i = 0; i < LECTURE_COUNT + 1; i++) {
                    if(desired_lectures[i] == 1) {
                        state = ON_PYRKON; // When there are unvisited lectures, process goes back on Pyrkon.
                        break;
                    }
                }
                /* If there were no other lectures to visit, process exits Pyrkon. */
                if (state != ON_PYRKON) state = AFTER_PYRKON;
                break;
            }

            case AFTER_PYRKON: {
                println( "PROCESS [%d] has left Pyrkon.\n", rank )
                /* Process broadcasts information that it's left Pyrkon */
                pyrkon_broadcast(EXIT, 0, 0);
                wait_for_agreement(); // Process waits for everyone
                state = BEFORE_PYRKON; // and when everybody's ready new Pyrkon starts.
                break;
            }

            default:
                break;
        }

    }
}

/* Wątek komunikacyjny - dla każdej otrzymanej wiadomości wywołuje jej handler */
void *comFunc ( void *ptr ) {

    MPI_Status status;
    packet_t pakiet;

    /* odbieranie wiadomości */
    while ( !end ) {
	    println("PROCESS [%d] waits for messages.\n", rank)
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        pakiet.src = status.MPI_SOURCE;

        pthread_t new_thread;
        pthread_create(&new_thread, NULL, (void *)handlers[(int)status.MPI_TAG], &pakiet);
    }

    pthread_mutex_lock(&clock_mutex);

    if(lamport_clock < pakiet.ts) {
        lamport_clock = pakiet.ts + 1;
    } else if(lamport_clock > pakiet.ts) {
        lamport_clock ++;
    }

    pthread_mutex_unlock(&clock_mutex);

    println(" The End! ")
    return 0;
}


void want_enter_handler ( packet_t *message ) {

    switch ( state ) {
        case BEFORE_PYRKON: {
            if ( message->data == ENTER_PYRKON ) {
                pthread_mutex_lock( &on_pyrkon_mutex );
                int clock_allows = (message->ts < last_clock || (message->ts == last_clock && rank > message->src));
                if ( clock_allows ) {

                    packet_t tmp;
                    tmp.data = message->data;
                    tmp.ts = get_clock(TRUE);
                    sendPacket(&tmp, message->src, ALRIGHT_TO_ENTER);
                    pthread_mutex_unlock( &on_pyrkon_mutex );
                }
                else {

                    pthread_mutex_unlock( &on_pyrkon_mutex );
                    if ( end ) break;
                }
            } else if ( message->data >= ENTER_FIRST_LECTURE && message->data <= ENTER_LAST_LECTURE ) {
                // TODO process accepts this message
            }
            break;
        }
        case ON_PYRKON: {
            // TODO process accepts messages that want to enter Pyrkon
            //      and checks send message about lectures with changed clock.
            break;
        }
        case ON_LECTURE: {
            // TODO process accepts all messages.
            break;
        }
        case AFTER_PYRKON: {
            // TODO process accepts all messages.
            break;
        }
        default:
            break;

    }
    free(message);
}

void entering_handler( packet_t * message ) {

    switch ( state ) {
        case BEFORE_PYRKON: {
            // TODO Process increases numbers of People on Pyrkon.
            break;
        }
        case ON_PYRKON: {
            // TODO Does nothing. (?)
            break;
        }
        case ON_LECTURE: {
            // TODO Does nothing. (?)
            break;
        }
        case AFTER_PYRKON:{
            // TODO Does nothing.
            break;
        }
    }
    free(message);
}

void alright_enter_handler ( packet_t * message ) {

    switch ( state ) {
        case BEFORE_PYRKON: {
            // TODO Process marks that someone agreed to its message.
            break;
        }
        case ON_PYRKON: {
            // TODO Process marks that someone agreed to its message.
            break;
        }
        case ON_LECTURE: {
            // TODO Process marks that someone agreed to its message.
            break;
        }
        case AFTER_PYRKON:{
            // TODO Process does nothing.
            break;
        }
    }
    free(message);
}

void exit_handler ( packet_t * message ) {

    switch ( state ) {
        case BEFORE_PYRKON: {
            // TODO Process increases number of other processes that exited Pyrkon.
            break;
        }
        case ON_PYRKON: {
            // TODO Process increases number of other processes that exited Pyrkon.
            break;
        }
        case ON_LECTURE: {
            // TODO Process increases number of other processes that exited Pyrkon.
            break;
        }
        case AFTER_PYRKON:{
            // TODO Process increases number of other processes that exited Pyrkon.
            break;
        }
    }
    free(message);
}
#pragma clang diagnostic pop