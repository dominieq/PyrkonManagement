#include "main.h"
#include "string.h"

MPI_Datatype MPI_PAKIET_T;
pthread_t threadCom, threadM;

pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t modify_permits = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t modify_exited_from_pyrkon = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wait_for_agreement_to_enter = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wait_for_new_pyrkon = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t on_lecture_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t on_pyrkon_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ready_to_exit_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t allowing_pyrkon = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t allowing_lecture = PTHREAD_MUTEX_INITIALIZER;

volatile int state = BEFORE_PYRKON;
volatile int lamport_clock;
volatile int last_message_clock = 0;
volatile int pyrkon_number = 0;
volatile int exited_from_pyrkon = 0;
volatile int* permits;
volatile int* desired_lectures;

/* end == TRUE oznacza wyjście z main_loop */

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
 * Function returns state using state_mutex.
 * State can accessed by multiple threads so there is a need to protect it.
 * @return The value of current state of a process.
 */
int get_state(){
    pthread_mutex_lock( &state_mutex );
    int current_state = state;
    pthread_mutex_unlock( &state_mutex );
    return current_state;
}   

/**
 * Function changes state using state_mutex.
 * State can accessed by multiple threads so there is a need to protect it.
 * @param new_state New state of a process that is going to be changed
 */
void set_state(int new_state){
    pthread_mutex_lock( &state_mutex );
    state = new_state;
    pthread_mutex_unlock( &state_mutex );
}

/**
 * Function broadcasts packets to every process.
 * @param type Describes an activity that a process wants to do.
 * @param data Further information regarding
 * @param additional_data Even further information regarding action.
 */
void pyrkon_broadcast( int type, int data, char state_c[100] ) {
    packet_t newMessage;
    newMessage.ts = get_clock( TRUE );
    newMessage.data = data;
    newMessage.pyrkon_number = pyrkon_number;

    char type_c[100];
    char data_c[100];
    switch( type ) {
        case WANT_TO_ENTER: {
            strcpy( type_c, "WANT_TO_ENTER" );
            break;
        }
        case ALRIGHT_TO_ENTER: {
            strcpy( type_c, "ALRIGHT_TO_ENTER" );
            break;
        }
        case EXIT: {
            strcpy( type_c, "EXIT" );
            break;
        }
        default:
            break;
    }
    if( data == 0 ) {
        strcpy(data_c, "PYRKON");
    } else {
        strcpy(data_c, "LECTURE");
    }

    for ( int i = 0; i < size; i++ ){
        if ( rank != i ) {
            // println( "Is %s and sends %s %s[%d] to %d\n", state_c, type_c, data_c, newMessage.data, i )
            sendPacket( &newMessage, i, type );
            println( "Is %s and sent %s %s[%d] to %d\n", state_c, type_c, data_c, newMessage.data, i )
        }
    }
}

/**
 * Function draws int from interval; both inclusive.
 * @param min left number in an interval.
 * @param max right number in an interval.
 * @return drawn int
 */
int my_random_int( int min, int max ) {
    float tmp = (float) ( (float) rand() / (float) RAND_MAX );
    return (int) (tmp * ( max - min + 1 ) + min);
}

int main ( int argc, char **argv ) {
    inicjuj( &argc, &argv);
    mainLoop();
    finalizuj();
    return 0;
}

/**
 * Main thread - process is participating in Pyrkon
 */
void mainLoop ( void ) {
     int prob_of_sending = PROB_OF_SENDING;
     int dst;
     packet_t pakiet;

    struct timespec t = { 0, rank*50000 };
    struct timespec rem = { 1, 0 };
    nanosleep( &t, &rem);

    permits = malloc( ( LECTURE_COUNT + 1 ) * sizeof( int ) );
    desired_lectures = malloc( ( LECTURE_COUNT + 1 ) * sizeof( int ) );

    /* Process won't respond to anyone who wants to enter lecture. */
    pthread_mutex_lock( &allowing_lecture );

    while( !end ) {
	    int percent = rand()%2 + 1;
        struct timespec t2 = { percent, 0 };
        struct timespec rem2 = { 1, 0 };
        nanosleep( &t2, &rem2 );

        int current_state = get_state();
        switch( current_state ){
            case BEFORE_PYRKON: {
                /* Process is waiting in line to enter Pyrkon. It broadcasts question and waits for agreement. */
                pyrkon_broadcast(WANT_TO_ENTER, 0, "BEFORE_PYRKON");

                /* Process locks mutex two times to wait for another thread to allow it to proceed */
                pthread_mutex_lock( &wait_for_agreement_to_enter );
                println("Waiting for Pyrkon.\n") // display some info
                /* Waiting on closed mutex that will be unlocked in function "alright_enter_pyrkon_extension" */
                pthread_mutex_lock( &wait_for_agreement_to_enter );
                pthread_mutex_unlock( &wait_for_agreement_to_enter );

                /* Process was allowed to enter to so it changes it's state. */
                set_state(ON_PYRKON);

                /* Process locks on_pyrkon_mutex to prevent itself from granting access to another process. */
                pthread_mutex_lock( &on_pyrkon_mutex );

                /* Process won't respond to anyone who wants to enter pyrkon. */
                pthread_mutex_lock( &allowing_pyrkon );

                /* Access granted to anyone who wants to enter lectures. */
                pthread_mutex_unlock( &allowing_lecture );
                break;
            }
            case ON_PYRKON: {
                /* Process entered Pyrkon and chooses lectures. */
                for(int i = 1; i <= LECTURE_COUNT; i++) {
                    desired_lectures[i]=my_random_int(0,1);
                }

                /* When it's chosen its desired lectures it broadcasts that information to others. */
                for(int i = 1; i <= LECTURE_COUNT; i++) {
                    if(desired_lectures[i]) pyrkon_broadcast(WANT_TO_ENTER, i, "ON_PYRKON");
                }

                /* Process locks mutex two times to wait for another thread to allow it to proceed */
                pthread_mutex_lock( &ready_to_exit_mutex );
                println("Participating in Pyrkon.\n") // display some info
                /* Waiting on closed mutex that will be unlocked in function "alright_enter_lecture_extension" */
                pthread_mutex_lock( &ready_to_exit_mutex );
                pthread_mutex_unlock( &ready_to_exit_mutex );

                /* Process won't respond to anyone who wants to enter lecture. */
                pthread_mutex_lock( &allowing_lecture );
                break;
            }
            case AFTER_PYRKON: {
                /* Process broadcasts information that it's left Pyrkon */
                pyrkon_broadcast( EXIT, 0, "AFTER_PYRKON" );

                /* Process waits for everyone, mutex will be unlocked in function "exit_handler" */
                pthread_mutex_lock( &wait_for_new_pyrkon );
                set_state(BEFORE_PYRKON);
                pthread_mutex_unlock( &on_pyrkon_mutex );

                /* Access granted to anyone who wants to enter Pyrkon. */
                pthread_mutex_unlock( &allowing_pyrkon );
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
    packet_t *pakiet;

    while ( !end ) {
        pakiet = (packet_t *)malloc(sizeof(packet_t));
	    println("Waiting for messages.\n")
        MPI_Recv( pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status );
        pakiet->src = status.MPI_SOURCE;

        pthread_mutex_lock( &clock_mutex );
        if( lamport_clock < pakiet->ts ) {
            lamport_clock = pakiet->ts + 1;
        } else {
            lamport_clock ++;
        }
        pthread_mutex_unlock( &clock_mutex );

        if ( pyrkon_number == pakiet -> pyrkon_number ){
            println( "Received a valid message from  [%d]\n", pakiet->src )
            pthread_t new_thread;
            pthread_create(&new_thread, NULL, (void *)handlers[(int)status.MPI_TAG], pakiet);
        } else {
            println( "Received outdated message from [%d]\n", pakiet->src)
        }
    }
    return 0;
}

void allow_pyrkon ( packet_t *message ) {
    /* Other process wanted to enter Pyrkon. */
    println( "Received info that [%d] wants to enter Pyrkon.\n", message->src )

    while( TRUE ) {
        /* Process will proceed only if it's before Pyrkon. */
        pthread_mutex_lock( &allowing_pyrkon );

        int clock_allows = ( message->ts < last_message_clock ||
                ( message->ts == last_message_clock && rank > message->src ) );

        if( clock_allows ) {
            println( "Sends agreement to enter Pyrkon to [%d].\n", message->src )
            message->ts = get_clock( TRUE );
            sendPacket( message, message->src, ALRIGHT_TO_ENTER );
            break;
        }
        pthread_mutex_unlock( &allowing_pyrkon );
    }
    pthread_mutex_unlock( &allowing_pyrkon );
}

void allow_lecture ( packet_t *message ) {
    /* Other process wanted to enter lecture.*/
    println( "Received info that [%d] wants to enter lecture [%d].\n", message->src, message->data )

    int lecture = message->data;
    while( TRUE ){
        /* Process will proceed only if it's on Pyrkon */
        pthread_mutex_lock( &allowing_lecture );

        int clock_allows;
        if ( desired_lectures[lecture] ){
             clock_allows = ( message->ts < last_message_clock ||
                    ( message->ts == last_message_clock && rank > message->src ) );
        } else {
            clock_allows = TRUE;
        }
        if( clock_allows ){
            pthread_mutex_lock( &on_lecture_mutex );
            pthread_mutex_unlock( &on_lecture_mutex );

            println( "Sends agreement to enter lecture [%d] to [%d].\n", message->data, message->src )
            message->ts = get_clock( TRUE );
            sendPacket( message, message->src, ALRIGHT_TO_ENTER );
            break;
        }
        /* Access granted to everyone who wants to enter lecture. */
        pthread_mutex_unlock( &allowing_lecture );
    }
    /* Access granted to everyone who wants to enter lecture. */
    pthread_mutex_unlock( &allowing_lecture );
}

/**
 * Function received WANT_TO_ENTER message from other process and deals with it's content.
 * @param message packet_t received from other process.
 */
void want_enter_handler ( packet_t *message ) {
    /* Process has just received a message that other process wants to enter. */
    if( message->data == 0 ) {
        allow_pyrkon(message);
    } else {
        allow_lecture(message);
    }
    free(message);
}

void alright_enter_pyrkon_extension (packet_t *message ) {
    if( get_state() == BEFORE_PYRKON ){
        /* Process is waiting to enter Pyrkon and it's received message that allows it to go in. */
        println( "Received agreement to enter Pyrkon from [%d].\n ", message->src)

        pthread_mutex_lock( &modify_permits );
        int number_of_permits = ++ permits[ message->data ];
        pthread_mutex_unlock( &modify_permits );

        if( number_of_permits >= size - MAX_PEOPLE_ON_PYRKON ) {
            pthread_mutex_unlock( &wait_for_agreement_to_enter );
        }
    }
}

void alright_enter_lecture_extension(packet_t *message ) {
    if ( get_state() == ON_PYRKON ) {
        /* Process is on Pyrkon and receives a message allowing it to participate in one lecture. */
        println( "Received agreement to enter lecture [%d] from [%d].\n", message->data, message->src )

        pthread_mutex_lock( &modify_permits );
        int number_of_permits = ++ permits[ message->data ];

        if( desired_lectures[message->data] && number_of_permits >= size - MAX_PEOPLE_ON_LECTURE ) {

            pthread_mutex_lock( &on_lecture_mutex ); // Process is on lecture -> locks on_lecture_mutex.
            pthread_mutex_unlock( &modify_permits );

            desired_lectures[message->data] = 0;
            sleep(5000);
            pthread_mutex_unlock( &on_lecture_mutex ); // Process is after lecture -> unlocks on_lecture_mutex.

            int lectures_left = 0; // Process will be counting how many lecture it has to visit.
            for ( int i = 1 ; i <= LECTURE_COUNT ; i++ ) {
                lectures_left += desired_lectures[i];
            }

            /* If there are no lectures left process is ready to exit. */
            if ( !lectures_left ) {
                pthread_mutex_unlock( &ready_to_exit_mutex );
            }
        } else {
            pthread_mutex_unlock( &modify_permits );
        }
    }
}

/**
 * Function receives ALRIGHT_TO_ENTER message from other process and deals with it's content.
 * @param message packet_t received from other process.
 */
void alright_enter_handler ( packet_t * message ) {

    if( message->data == 0 ){
        alright_enter_pyrkon_extension(message);
    } else {
        alright_enter_lecture_extension(message);
    } 
    free(message);
}

/**
 * Function receives EXIT message from other process and deals with it's content.
 * @param message - packet_t received from other process.
 */
void exit_handler ( packet_t * message ) {
    /* Process has just received a message that other process has just left Pyrkon. */
    println( "Received info that [%d] has left Pyrkon.\n", message->src )

    /* Changing variable and using mutex to protect it from other processes. */
    pthread_mutex_lock( &modify_exited_from_pyrkon );
    exited_from_pyrkon++;
    pthread_mutex_unlock( &modify_exited_from_pyrkon );

    /* If everyone exited Pyrkon process can start another one. */
    if( exited_from_pyrkon == MAX_PEOPLE_ON_PYRKON ) {
        set_state(BEFORE_PYRKON);
        pthread_mutex_unlock( &wait_for_new_pyrkon );
    }
    free(message);
}