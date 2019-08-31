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
volatile int last_message_clock = 0;
volatile int pyrkon_number = 0;
volatile int people_on_pyrkon = 0;
volatile int* people_on_lecture;
volatile int* exited_from_pyrkon;
volatile int* permits;
volatile int* desired_lectures;
volatile int received_agreement = 0;
volatile int allowed_lecture = 0;



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

/**
 * Function increments Lamport clock based on information from input.
 * @param to_increase integer has value either 0 or 1.
 * @return value of Lamport clock either incremented or not.
 */
int get_clock( int to_increase ) {

    pthread_mutex_lock( &clock_mutex );

    if( to_increase ) lamport_clock++;
    last_message_clock = lamport_clock; // We are remembering the value of Lamport clock.

    pthread_mutex_unlock( &clock_mutex );
    return last_message_clock;
}

/**
 * Function broadcasts packets to every process.
 * @param type Describes an activity that a process wants to do.
 * @param data Further information regarding action.
 * @param additional_data Even further information regarding action.
 */
void pyrkon_broadcast(int type, int data, int additional_data) {

    packet_t result;
    result.ts = get_clock( TRUE );
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
    nanosleep( &t, &rem);

    exited_from_pyrkon = malloc( size * sizeof( int ) );
    permits = malloc( ( LECTURE_COUNT + 1 ) * sizeof( int ) );
    desired_lectures = malloc( LECTURE_COUNT * sizeof( int ) );

    while( !end ) {
	    int percent = rand()%2 + 1;
        struct timespec t2 = { percent, 0 };
        struct timespec rem2 = { 1, 0 };
        nanosleep( &t2, &rem2 );


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

    int just_sent = 0;
    switch ( state ) {
        case BEFORE_PYRKON: {
            /* Process is waiting in line and receives a message that
             * another process wants to enter either Pyrkon or lecture */
            if( message->data == ENTER_PYRKON ) {
                println( "PROCESS [%d] (BEFORE_PYRKON) received info that [%d] wants to enter Pyrkon.\n",
                        rank, message->src )
                while( !just_sent ) {
                    pthread_mutex_lock( &on_pyrkon_mutex );
                    int clock_allows = ( message->ts < last_message_clock ||
                            ( message->ts == last_message_clock && rank > message->src ) );
                    if( clock_allows ) {

                        packet_t tmp;
                        tmp.data = message->data;
                        tmp.ts = get_clock( TRUE );
                        println( "PROCESS [%d] (BEFORE_PYRKON) sends agreement to enter Pyrkon to [%d].\n",
                                 rank, message->src )
                        sendPacket( &tmp, message->src, ALRIGHT_TO_ENTER );
                        pthread_mutex_unlock( &on_pyrkon_mutex );
                        just_sent = 1;
                    }
                    else {

                        pthread_mutex_unlock( &on_pyrkon_mutex );
                        if( end ) break;
                    }
                }
            } else  {

                println( "PROCESS [%d] received info that [%d] wants to enter lecture [%d].\n",
                        rank, message->src, message->data )
                packet_t tmp;
                tmp.data = message->data;
                tmp.ts = get_clock( TRUE );
                println( "PROCESS [%d] (BEFORE_PYRKON) sends agreement to enter lecture [%d] to [%d].\n",
                         rank, message->data, message->src )
                sendPacket( &tmp, message->src, ALRIGHT_TO_ENTER );
            }
            break;
        }
        case ON_PYRKON: {
            /* Process is on Pyrkon and wants to participate in it's lectures.
             * It receives a message that another process wants to enter either Pyrkon or lecture */
            if( message->data == ENTER_PYRKON ) {

                println( "PROCESS [%d] (BEFORE_PYRKON) received info that [%d] wants to enter Pyrkon.\n",
                         rank, message->src )
                packet_t tmp;
                tmp.data = message->data;
                tmp.ts = get_clock( TRUE );
                println( "PROCESS [%d] (ON_PYRKON) sends agreement to enter Pyrkon to [%d].\n",
                         rank, message->src )
                sendPacket( &tmp, message->src, ALRIGHT_TO_ENTER );
            } else {

                println( "PROCESS [%d] received info that [%d] wants to enter lecture [%d].\n",
                         rank, message->src, message->data )
                int lecture = message->data;
                if( desired_lectures[ lecture ] ) {
                    pthread_mutex_lock( &on_lecture_mutex );
                    int clock_allows = ( message->ts < lamport_clock ||
                            ( message->ts == lamport_clock && rank > message->src ) );
                    if( clock_allows ) {

                        packet_t tmp;
                        tmp.data = message->data;
                        tmp.ts = get_clock( TRUE );
                        println( "PROCESS [%d] (ON_PYRKON) sends agreement to enter lecture [%d] to [%d].\n",
                                rank, message->data, message->src )
                        sendPacket( &tmp, message->src, ALRIGHT_TO_ENTER );
                        pthread_mutex_unlock( &on_lecture_mutex );
                    } else {

                        pthread_mutex_unlock( &on_lecture_mutex );
                        if( end ) break;
                    }
                }
            }
            break;
        }
        case ON_LECTURE: {
            /* Process is participating in a lecture so it doesn't care
             * whether something wants to enter either Pyrkon or lecture. Process accepts that message. */
            packet_t tmp;
            tmp.data = message->data;
            tmp.ts = get_clock(TRUE);
            sendPacket(&tmp, message->src, ALRIGHT_TO_ENTER);
            break;
        }
        case AFTER_PYRKON: {
            /* Process has just exited Pyrkon and doesn't care
             * whether something wants to enter either Pyrkon or lecture. Process accepts that message. */
            packet_t tmp;
            tmp.data = message->data;
            tmp.ts = get_clock(TRUE);
            sendPacket(&tmp, message->src, ALRIGHT_TO_ENTER);
            break;
        }
        default:
            break;

    }
    free(message);
}

void entering_handler( packet_t * message ) {

    /* Regardless of state process marks change. It increases number of people on specific activity. */
    if( message->data == ENTER_PYRKON ) {
        people_on_pyrkon++;
    } else {
        people_on_lecture[message->data]++;
    }
    free(message);
}

void alright_enter_handler ( packet_t * message ) {

    switch ( state ) {
        case BEFORE_PYRKON: {

            if( message->data == ENTER_PYRKON ){

                println( "PROCESS [%d] (BEFORE_PYRKON) received agreement to enter Pyrkon from [%d].\n ",
                        rank, message->src )
                pthread_mutex_lock( &permits_mutex );
                int number_of_permits = ++ permits[ message->data ];
                pthread_mutex_unlock( &permits_mutex );

                if( number_of_permits >= MAX_PEOPLE_ON_PYRKON / 2 ) {

                    pyrkon_broadcast(ENTERING_TO, ENTER_PYRKON, 0);
                    pthread_mutex_unlock( &allow_mutex );
                }
            } else {

                println( "PROCESS [%d] (BEFORE_PYRKON) received message to enter lecture [%d] from [%d].\n",
                        rank, message->data, message->src )
            }
            break;
        }
        case ON_PYRKON: {

            if( message->data != ENTER_PYRKON ){

                println( "PROCESS [%d] (ON_PYRKON) received agreement to enter lecture [%d] from [%d].\n",
                        rank, message->data, message->src )
                pthread_mutex_lock( &permits_mutex );
                int number_of_permits = ++ permits[ message->data ];
                pthread_mutex_unlock( &permits_mutex );

                if( number_of_permits >= MAX_PEOPLE_ON_LECTURE / 2 ) {

                    pyrkon_broadcast(ENTERING_TO, message->data, 0);
                    pthread_mutex_unlock( &allow_mutex );
                }
            } else {
                println( "PROCESS [%d] (ON_PYRKON) received agreement to enter Pyrkon from [%d].\n ",
                         rank, message->src )
            }
            break;
        }
        case ON_LECTURE: {

            if( message->data != ENTER_PYRKON ) {

                println( "PROCESS [%d] (ON_LECTURE) received agreement to enter lecture [%d] from [%d].\n",
                        rank, message->data, message->src )
                pthread_mutex_lock(&permits_mutex);
                permits[message->data] ++;
                pthread_mutex_unlock(&permits_mutex);
            } else {

                println( "PROCESS [%d] (ON LECTURE) received agreement to enter Pyrkon from [%d].\n ",
                         rank, message->src )
            }
            break;
        }
        case AFTER_PYRKON:{

            if( message->data == ENTER_PYRKON ) {

                println( "PROCESS [%d] received agreement to enter Pyrkon from [%d].\n",
                        rank, message->src )
            } else {

                println( "PROCESS [%d] received agreement to enter lecture [%d] from [%d].\n",
                        rank, message->data, message->src)
            }
            break;
        }
        default:
            break;
    }
    free(message);
}

void exit_handler ( packet_t * message ) {

    switch ( state ) {
        case BEFORE_PYRKON: {

            println( "PROCESS [%d] (BEFORE_PYRKON) received info that [%d] has left Pyrkon.\n",
                    rank, message->src )
            exited_from_pyrkon++;
            break;
        }
        case ON_PYRKON: {
            println( "PROCESS [%d] (ON_PYRKON) received info that [%d] has left Pyrkon.\n",
                     rank, message->src )
            exited_from_pyrkon++;
            break;
        }
        case ON_LECTURE: {
            println( "PROCESS [%d] (ON_LECTURE) received info that [%d] has left Pyrkon.\n",
                     rank, message->src )
            exited_from_pyrkon++;
            break;
        }
        case AFTER_PYRKON:{
            println( "PROCESS [%d] (AFTER_PYRKON) received info that [%d] has left Pyrkon.\n",
                     rank, message->src )
            exited_from_pyrkon++;
            // TODO implement starting new Pyrkon
            break;
        }
        default:
            break;
    }
    free(message);
}
#pragma clang diagnostic pop