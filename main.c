#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-msc30-c"
#pragma clang diagnostic ignored "-Wunused-parameter"

#include "main.h"

MPI_Datatype MPI_PAKIET_T;
pthread_t threadCom, threadM;

pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t modify_permits = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t modify_exited_from_pyrkon = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wait_for_agreement_to_enter = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wait_for_new_pyrkon = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t on_lecture_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t on_pyrkon_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile int state = BEFORE_PYRKON;
volatile int lamport_clock;
volatile int last_message_clock = 0;
volatile int pyrkon_number = 0;
volatile int exited_from_pyrkon = 0;
volatile int* permits;
volatile int* desired_lectures;
volatile int allowed_lecture = 0;



/* end == TRUE oznacza wyjście z main_loop */
volatile char end = FALSE;
void mainLoop(void);

/* Deklaracje zapowiadające handlery. */
void want_enter_handler(packet_t *message);
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
void pyrkon_broadcast( int type, int data, int additional_data ) {

    packet_t result;
    result.ts = get_clock( TRUE );
    result.data = data;
    result.additional_data = additional_data;

    for (int i = 0; i < size; i++){
        if (rank != i) sendPacket( &result, i, type );
    }
}

/**
 * Function draws int from interval.
 * @param min left number in an interval.
 * @param max right number in an interval.
 * @return drawn int
 */
int my_random_int( int min, int max ) {

    float tmp = (float)( (float) rand() / (float)RAND_MAX);
    return (int) tmp * ( max - min + 1 ) + min;
}

int main ( int argc, char **argv ) {

    /* Tworzenie wątków, inicjalizacja itp */
    inicjuj( &argc, &argv);

    mainLoop();

    finalizuj();
    return 0;
}

/**
 * Main thread - process is participating in Pyrkon
 */
void mainLoop ( void ) {

    // int prob_of_sending = PROB_OF_SENDING;
    // int dst;
    // packet_t pakiet;

    /* mały sleep, by procesy nie zaczynały w dokładnie tym samym czasie */
    struct timespec t = { 0, rank*50000 };
    struct timespec rem = { 1, 0 };
    nanosleep( &t, &rem);

    permits = malloc( ( LECTURE_COUNT + 1 ) * sizeof( int ) );
    desired_lectures = malloc( LECTURE_COUNT * sizeof( int ) );

    while( !end ) {
	    int percent = rand()%2 + 1;
        struct timespec t2 = { percent, 0 };
        struct timespec rem2 = { 1, 0 };
        nanosleep( &t2, &rem2 );


        switch( state ){

            case BEFORE_PYRKON: {
                println( "PROCESS [%d] is BEFORE_PYRKON.\n", rank )
                /* Process is waiting in line to enter Pyrkon. It broadcasts question and waits for agreement. */
                pyrkon_broadcast(WANT_TO_ENTER, 0, 0);
                pthread_mutex_lock( &wait_for_agreement_to_enter ); // All processes must enter Pyrkon eventually,
                state = ON_PYRKON; // so there is no "if" only mutex.
                pthread_mutex_lock( &on_pyrkon_mutex );

                println( "PROCESS [%d] is ON_PYRKON and about to choose it's lectures.\n", rank )
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
                println("PROCESS [%d] is ON_PYRKON and has chosen it's lectures.\n", rank)
                /* When on Pyrkon, process waits for agreement to enter one of its desired lectures. */
                pthread_mutex_lock( &wait_for_agreement_to_enter );
                state = ON_LECTURE;
                pthread_mutex_lock( &on_lecture_mutex );
                break;
            }

            case ON_LECTURE: {
                println( "PROCESS [%d] is ON_LECTURE number %d.\n", rank, allowed_lecture )
                /* When on lecture, process waits five seconds. */
                sleep(5000);
                /* Process checks if it wants to go to another lecture. */
                desired_lectures[ allowed_lecture ] = 0;
                for (int i = 0; i < LECTURE_COUNT + 1; i++) {
                    if(desired_lectures[i] == 1) {
                        state = ON_PYRKON; // When there are unvisited lectures, process goes back on Pyrkon.
                        pthread_mutex_unlock( &on_lecture_mutex );
                        break;
                    }
                }
                /* If there were no other lectures to visit, process exits Pyrkon. */
                if (state != ON_PYRKON) {

                    state = AFTER_PYRKON;
                    pthread_mutex_unlock( &on_pyrkon_mutex );
                }
                break;
            }

            case AFTER_PYRKON: {
                println( "PROCESS [%d] is AFTER_PYRKON.\n", rank )
                /* Process broadcasts information that it's left Pyrkon */
                pyrkon_broadcast(EXIT, 0, 0);
                pthread_mutex_lock( &wait_for_new_pyrkon ); // Process waits for everyone
                state = BEFORE_PYRKON; // and when everybody's ready new Pyrkon starts.
                pyrkon_number ++;
                break;
            }

            default:
                break;
        }

    }
}

/**
 * Communication thread - for every received message a handler is called.
 * @param ptr
 * @return 0
 */
void *comFunc ( void *ptr ) {

    MPI_Status status;
    packet_t pakiet;

    /* odbieranie wiadomości */
    while ( !end ) {
	    println("(COM_THREAD) PROCESS [%d] waits for messages.\n", rank)
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

/**
 * Function received WANT_TO_ENTER message from other process and deals with it's content.
 * @param message packet_t received from other process.
 */
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
                    pthread_mutex_unlock( &on_pyrkon_mutex );

                    if( clock_allows ) {

                        packet_t tmp;
                        tmp.data = message->data;
                        tmp.ts = get_clock( TRUE );
                        println( "PROCESS [%d] (BEFORE_PYRKON) sends agreement to enter Pyrkon to [%d].\n",
                                 rank, message->src )
                        sendPacket( &tmp, message->src, ALRIGHT_TO_ENTER );
                        just_sent = 1;
                    }
                }
            } else  {

                println( "PROCESS [%d] (BEFORE_PYRKON) received info that [%d] wants to enter lecture [%d].\n",
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

                println( "PROCESS [%d] (ON_PYRKON) received info that [%d] wants to enter Pyrkon.\n",
                         rank, message->src )
                packet_t tmp;
                tmp.data = message->data;
                tmp.ts = get_clock( TRUE );
                println( "PROCESS [%d] (ON_PYRKON) sends agreement to enter Pyrkon to [%d].\n",
                         rank, message->src )
                sendPacket( &tmp, message->src, ALRIGHT_TO_ENTER );
            } else {

                println( "PROCESS [%d] (ON_PYRKON) received info that [%d] wants to enter lecture [%d].\n",
                         rank, message->src, message->data )
                int lecture = message->data;
                if( desired_lectures[ lecture ] ) {

                    pthread_mutex_lock( &on_lecture_mutex );
                    int clock_allows = ( message->ts < lamport_clock ||
                            ( message->ts == lamport_clock && rank > message->src ) );
                    pthread_mutex_unlock( &on_lecture_mutex );

                    if( clock_allows ) {

                        packet_t tmp;
                        tmp.data = message->data;
                        tmp.ts = get_clock( TRUE );
                        println( "PROCESS [%d] (ON_PYRKON) sends agreement to enter lecture [%d] to [%d].\n",
                                rank, message->data, message->src )
                        sendPacket( &tmp, message->src, ALRIGHT_TO_ENTER );

                    }
                }
            }
            break;
        }
        case ON_LECTURE: {
            /* Process is participating in a lecture so it doesn't care
             * whether something wants to enter either Pyrkon or lecture. Process accepts that message. */

            println( "PROCESS [%d] (ON_LECTURE) received message that [%d] wants to enter something.\n",
                    rank, message->src )
            packet_t tmp;
            tmp.data = message->data;
            tmp.ts = get_clock(TRUE);
            sendPacket(&tmp, message->src, ALRIGHT_TO_ENTER);
            println( "PROCESS [%d] (ON_LECTURE) sends agreement to enter something to [%d].\n",
                     rank, message->src )
            break;
        }
        case AFTER_PYRKON: {
            /* Process has just exited Pyrkon and doesn't care
             * whether something wants to enter either Pyrkon or lecture. Process accepts that message. */

            println( "PROCESS [%d] (AFTER_PYRKON) received message that [%d] wants to enter something.\n",
                     rank, message->src )
            packet_t tmp;
            tmp.data = message->data;
            tmp.ts = get_clock(TRUE);
            sendPacket(&tmp, message->src, ALRIGHT_TO_ENTER);
            println( "PROCESS [%d] (AFTER_PYRKON) sends agreement to enter something to [%d].\n",
                     rank, message->src )
            break;
        }
        default:
            break;

    }
    free(message);
}

/**
 * Function receives ALRIGHT_TO_ENTER message from other process and deals with it's content.
 * @param message packet_t received from other process.
 */
void alright_enter_handler ( packet_t * message ) {

    switch ( state ) {
        case BEFORE_PYRKON: {

            if( message->data == ENTER_PYRKON ){
                /* Process is waiting to enter Pyrkon and it's received message that allows it to go in. */

                println( "PROCESS [%d] (BEFORE_PYRKON) received agreement to enter Pyrkon from [%d].\n ",
                        rank, message->src )
                pthread_mutex_lock( &modify_permits );
                int number_of_permits = ++ permits[ message->data ]; // Process increases number of received permits.
                pthread_mutex_unlock( &modify_permits );

                if( number_of_permits >= size - MAX_PEOPLE_ON_PYRKON ) {
                    /* Process broadcasts information that it's entering Pyrkon */

                    pthread_mutex_unlock( &wait_for_agreement_to_enter );
                }
            } else {
                /* Process is waiting to enter Pyrkon and it's received message that allows it to participate in lecture. */
                /* Since process isn't on Pyrkon yet this is probably a bug. */

                println( "PROCESS [%d] (BEFORE_PYRKON) received message to enter lecture [%d] from [%d].\n",
                        rank, message->data, message->src )
            }
            break;
        }
        case ON_PYRKON: {

            if( message->data != ENTER_PYRKON ){
                /* Process is on Pyrkon and receives a message allowing it to participate in one lecture. */

                println( "PROCESS [%d] (ON_PYRKON) received agreement to enter lecture [%d] from [%d].\n",
                        rank, message->data, message->src )
                pthread_mutex_lock( &modify_permits );
                int number_of_permits = ++ permits[ message->data ]; // Process increases number of received permits.
                pthread_mutex_unlock( &modify_permits );

                if( number_of_permits >= size - MAX_PEOPLE_ON_LECTURE ) {
                    /* Process broadcasts information that it's entering specific lecture */

                    allowed_lecture = message->data; // Process remembers information about allowed lecture.
                    pthread_mutex_unlock( &wait_for_agreement_to_enter );
                }
            } else {
                /* Process is on Pyrkon and receives a message allowing it to enter Pyrkon. */
                /* Since process is already on Pyrkon no further action is taken. */

                println( "PROCESS [%d] (ON_PYRKON) received agreement to enter Pyrkon from [%d].\n ",
                         rank, message->src )
            }
            break;
        }
        case ON_LECTURE: {

            if( message->data != ENTER_PYRKON ) {
                /* Process is on lecture and receives message allowing it to participate in one lecture. */
                /* Since process is currently participating in a lecture it only marks new agreement. */

                println( "PROCESS [%d] (ON_LECTURE) received agreement to enter lecture [%d] from [%d].\n",
                        rank, message->data, message->src )
                pthread_mutex_lock(&modify_permits);
                permits[message->data] ++; // Process increases number of received permits.
                pthread_mutex_unlock(&modify_permits);

            } else {
                /* Process is on lecture and receives message allowing it to enter Pyrkon. */
                /* Since process is already on Pyrkon, no further action is taken. */

                println( "PROCESS [%d] (ON LECTURE) received agreement to enter Pyrkon from [%d].\n ",
                         rank, message->src )
            }
            break;
        }
        case AFTER_PYRKON:{

            if( message->data == ENTER_PYRKON ) {
                /* Process has just exited Pyrkon and receives message allowing it to enter old Pyrkon. */
                /* Process doesn't care about that message anymore. */

                println( "PROCESS [%d] (AFTER_PYRKON) received agreement to enter Pyrkon from [%d].\n",
                        rank, message->src )
            } else {
                /* Process has just exited Pyrkon and receives message allowing it to participate in one lecture. */
                /* Process doesn't care about that message anymore. */

                println( "PROCESS [%d] (AFTER_PYRKON) received agreement to enter lecture [%d] from [%d].\n",
                        rank, message->data, message->src)
            }
            break;
        }
        default:
            break;
    }
    free(message);
}

/**
 * Function receives EXIT message from other process and deals with it's content.
 * @param message - packet_t received from other process.
 */
void exit_handler ( packet_t * message ) {

    switch ( state ) {
        case BEFORE_PYRKON: {
            /* Process is waiting to enter Pyrkon and receives a message that other process has just  left.
             * Process isn't interested in that information right now.
             * Process increases number of processes that left Pyrkon */

            println( "PROCESS [%d] (BEFORE_PYRKON) received info that [%d] has left Pyrkon.\n",
                    rank, message->src )
            pthread_mutex_lock( &modify_exited_from_pyrkon );
            exited_from_pyrkon++;
            pthread_mutex_unlock( &modify_exited_from_pyrkon );
            break;
        }
        case ON_PYRKON: {
            /* Process is on Pyrkon and receives a message that other process has just left.
             * Process isn't interested in that information right now.
             * Process increases number of processes that left Pyrkon */

            println( "PROCESS [%d] (ON_PYRKON) received info that [%d] has left Pyrkon.\n",
                     rank, message->src )
            pthread_mutex_lock( &modify_exited_from_pyrkon );
            exited_from_pyrkon++;
            pthread_mutex_unlock( &modify_exited_from_pyrkon );
            break;
        }
        case ON_LECTURE: {
            /* Process is on lecture and receives a message that other has just left.
             * Process isn't interested in that information right now.
             * Process increases number of processes that left Pyrkon */

            println( "PROCESS [%d] (ON_LECTURE) received info that [%d] has left Pyrkon.\n",
                     rank, message->src )
            pthread_mutex_lock( &modify_exited_from_pyrkon );
            exited_from_pyrkon++;
            pthread_mutex_unlock( &modify_exited_from_pyrkon );
            break;
        }
        case AFTER_PYRKON:{
            /* Process has just exited Pyrkon and receives information that other has just left.
             * Process wants to start new Pyrkon so he acknowledges that another process is after Pyrkon
             * then it checks whether it's possible to start new Pyrkon. */

            println( "PROCESS [%d] (AFTER_PYRKON) received info that [%d] has left Pyrkon.\n",
                     rank, message->src )
            pthread_mutex_lock( &modify_exited_from_pyrkon );
            exited_from_pyrkon++;
            pthread_mutex_unlock( &modify_exited_from_pyrkon );

            /* If everyone exited Pyrkon, process can start another one. */
            if( exited_from_pyrkon == size ) pthread_mutex_unlock( &wait_for_new_pyrkon );
            break;
        }
        default:
            break;
    }
    free(message);
}
#pragma clang diagnostic pop