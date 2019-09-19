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
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gtfo_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t i_can_allow_pyrkon_entering_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t i_can_allow_lecture_entering_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile int state = BEFORE_PYRKON;
volatile int lamport_clock;
volatile int last_message_clock = 0;
volatile int pyrkon_number = 0;
volatile int exited_from_pyrkon = 0;
volatile int* permits;
volatile int* desired_lectures;
volatile int allowed_lecture = 0;



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

int get_state(){
    pthread_mutex_lock( &state_mutex );
    int a = state;
    pthread_mutex_unlock( &state_mutex );
    return a;
}   

void change_state(int new_state){
    pthread_mutex_lock( &state_mutex );
    state = new_state;
    pthread_mutex_unlock( &state_mutex );
}

/**
 * Function broadcasts packets to every process.
 * @param type Describes an activity that a process wants to do.
 * @param data Further information regardi * @param additional_data Even further information regarding action.
ng action.
 */
void pyrkon_broadcast( int type, int data ) {

    packet_t newMessage;
    newMessage.ts = get_clock( TRUE );
    newMessage.data = data;
    newMessage.pyrkon_number = pyrkon_number;

    for (int i = 0; i < size; i++){
        if (rank != i) {
            println("Wysylam %d %d do %d\n", type, newMessage.data, i);
            sendPacket( &newMessage, i, type );
            println("Wyslalem %d %d do %d\n", type, newMessage.data, i);        
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

// void init_lecture_mutexes(){
//     for (int i = 0; i < LECTURE_COUNT; i++)
//         pthread_mutex_init(&on_lecture_mutex[i], NULL);
// }

int main ( int argc, char **argv ) {

    /* Tworzenie wątków, inicjalizacja itp */
    inicjuj( &argc, &argv);
    // init_lecture_mutexes();
    pthread_mutex_lock( &i_can_allow_lecture_entering_mutex );
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
    desired_lectures = malloc( ( LECTURE_COUNT + 1 ) * sizeof( int ) );

    while( TRUE ) {
	    int percent = rand()%2 + 1;
        struct timespec t2 = { percent, 0 };
        struct timespec rem2 = { 1, 0 };
        nanosleep( &t2, &rem2 );

        int current_state = get_state();
        switch( current_state ){

            case BEFORE_PYRKON: {
                /* Process is waiting in line to enter Pyrkon. It broadcasts question and waits for agreement. */

                println( "PROCESS [%d] is BEFORE_PYRKON.\n", rank )

                pyrkon_broadcast(WANT_TO_ENTER, 0);
                pthread_mutex_lock( &wait_for_agreement_to_enter );
                pthread_mutex_lock( &wait_for_agreement_to_enter ); // All processes must enter Pyrkon eventually,
                pthread_mutex_unlock( &wait_for_agreement_to_enter );
                change_state(ON_PYRKON);// so there is no "if" only mutex.
                pthread_mutex_lock( &on_pyrkon_mutex );
                pthread_mutex_lock( &i_can_allow_pyrkon_entering_mutex );
                pthread_mutex_unlock( &i_can_allow_lecture_entering_mutex );
                break;
            }

            case ON_PYRKON: {
                /* When on Pyrkon, process waits for agreement to enter one of its desired lectures. */
                /* Process entered Pyrkon and chooses lectures. */
                println( "PROCESS [%d] is ON_PYRKON and about to choose it's lectures.\n", rank )
                for(int i = 1; i <= LECTURE_COUNT; i++) {
                    desired_lectures[i]=my_random_int(0,1);
                }

                /* When it's chosen its desired lectures it broadcasts that information to others. */
                for(int i = 1; i <= LECTURE_COUNT; i++) {
                    if(desired_lectures[i]) pyrkon_broadcast(WANT_TO_ENTER, i);
                }

                println("PROCESS [%d] is ON_PYRKON and waits for lectures.\n", rank)
                pthread_mutex_lock( &gtfo_mutex ); //blokada
                pthread_mutex_lock( &gtfo_mutex ); //czekanko
                pthread_mutex_unlock( &gtfo_mutex ); //odblokowanie, żeby w następnym obiegu dało się odblokować
                pthread_mutex_lock( &i_can_allow_lecture_entering_mutex );
                break;
            }

            // case ON_LECTURE: {
            //     /* When on lecture, process waits five seconds. */

            //     println( "PROCESS [%d] is ON_LECTURE number %d.\n", rank, allowed_lecture )
            //     sleep(5000);

            //     /* Process checks if it wants to go to another lecture. */
            //     desired_lectures[ allowed_lecture ] = 0;
            //     for (int i = 0; i <= LECTURE_COUNT; i++) {
            //         if(desired_lectures[i]) {
            //             state = ON_PYRKON; // When there are unvisited lectures, process goes back on Pyrkon.
            //             pthread_mutex_unlock( &on_lecture_mutex );
            //             break;
            //         }
            //     }
            //     /* If there were no other lectures to visit, process exits Pyrkon. */
            //     if (state != ON_PYRKON) {
            //         state = AFTER_PYRKON;
            //         pthread_mutex_unlock( &on_pyrkon_mutex );
            //     }
            //     break;
            // }

            case AFTER_PYRKON: {
                /* Process broadcasts information that it's left Pyrkon */

                println( "PROCESS [%d] is AFTER_PYRKON.\n", rank )
                pyrkon_broadcast(EXIT, 0);
                pthread_mutex_lock( &wait_for_new_pyrkon ); // Process waits for everyone
                change_state(BEFORE_PYRKON); // and when everybody's ready new Pyrkon starts.
                pthread_mutex_unlock( &i_can_allow_pyrkon_entering_mutex );
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

    /* odbieranie wiadomości */
    while ( TRUE ) {

        pakiet = (packet_t *)malloc(sizeof(packet_t));
	    println("(COM_THREAD) PROCESS [%d] waits for messages.\n", rank)
        MPI_Recv( pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        pakiet->src = status.MPI_SOURCE;
        println("Dostalem cos\n");
        pthread_mutex_lock( &clock_mutex );
        if( lamport_clock < pakiet->ts ) {
            lamport_clock = pakiet->ts + 1;
        } else {
            lamport_clock ++;
        }
        pthread_mutex_unlock( &clock_mutex );
        if (pyrkon_number == pakiet -> pyrkon_number){
            println("Nr pyrkonu sie zgadza\n");
            pthread_t new_thread;
            pthread_create(&new_thread, NULL, (void *)handlers[(int)status.MPI_TAG], pakiet);
        }
    }
    return 0;
}

void allow_pyrkon ( packet_t *message ) {
    println( "PROCESS [%d] (BEFORE_PYRKON) received info that [%d] wants to enter Pyrkon.\n",
            rank, message->src )

    while( TRUE ) {
        pthread_mutex_lock( &i_can_allow_pyrkon_entering_mutex );
        int clock_allows = ( message->ts < last_message_clock ||
                ( message->ts == last_message_clock && rank > message->src ) );
        if( clock_allows ) {
            message->ts = get_clock( TRUE );
            println( "PROCESS [%d] (BEFORE_PYRKON) sends agreement to enter Pyrkon to [%d].\n",
                        rank, message->src )
            sendPacket( message, message->src, ALRIGHT_TO_ENTER );
            break;
        }
        pthread_mutex_unlock( &i_can_allow_pyrkon_entering_mutex );
    }
    pthread_mutex_unlock( &i_can_allow_pyrkon_entering_mutex );
}

void allow_lecture ( packet_t *message ) {
    println( "PROCESS [%d] (ON_PYRKON) received info that [%d] wants to enter lecture [%d].\n",
                rank, message->src, message->data )
    int lecture = message->data;
    while(TRUE){
        pthread_mutex_lock( &i_can_allow_lecture_entering_mutex );
        int clock_allows = FALSE;
        if (desired_lectures[lecture]){
             clock_allows = ( message->ts < last_message_clock ||
                    ( message->ts == last_message_clock && rank > message->src ) );
        } else {
            clock_allows = TRUE;
        }
        if(clock_allows){
            pthread_mutex_lock( &on_lecture_mutex );
            pthread_mutex_unlock( &on_lecture_mutex );
            message->ts = get_clock( TRUE );
            println( "PROCESS [%d] (ON_PYRKON) sends agreement to enter lecture [%d] to [%d].\n",
                    rank, message->data, message->src )
            sendPacket( message, message->src, ALRIGHT_TO_ENTER );
            break;
        }
        pthread_mutex_unlock( &i_can_allow_lecture_entering_mutex );    
    }
    pthread_mutex_unlock( &i_can_allow_lecture_entering_mutex );
}
/**
 * Function received WANT_TO_ENTER message from other process and deals with it's content.
 * @param message packet_t received from other process.
 */
void want_enter_handler ( packet_t *message ) {

    if( message->data == ENTER_PYRKON ) {
        println("Dostałem pytanie o pyrkon od %d\n", message -> src)
        allow_pyrkon(message);
    } else {
        println("Dostałem pytanie o warsztat %d od %d\n", message -> data, message -> src)
        allow_lecture(message);
    }
    free(message);
}

void zgredek_dostal_pozwolenie_na_pyrkon ( packet_t *message ) {
    if( get_state() == BEFORE_PYRKON ){
        /* Process is waiting to enter Pyrkon and it's received message that allows it to go in. */

        println( "PROCESS [%d] (BEFORE_PYRKON) received agreement to enter Pyrkon from [%d].\n ",
                rank, message->src )
        pthread_mutex_lock( &modify_permits );
        int number_of_permits = ++ permits[ message->data ]; // Process increases number of received permits.
        pthread_mutex_unlock( &modify_permits );

        if( number_of_permits >= size - MAX_PEOPLE_ON_PYRKON ) {
            pthread_mutex_unlock( &wait_for_agreement_to_enter );
        }
    }
}

void zgredek_dostal_pozwolenie_na_warsztat ( packet_t *message ) {
    if ( get_state() == ON_PYRKON) {
        /* Process is on Pyrkon and receives a message allowing it to participate in one lecture. */
        println( "PROCESS [%d] (ON_PYRKON) received agreement to enter lecture [%d] from [%d].\n",
                rank, message->data, message->src )
        pthread_mutex_lock( &modify_permits );
        int number_of_permits = ++ permits[ message->data ]; // Process increases number of received permits.  
        if( desired_lectures[message->data] && number_of_permits >= size - MAX_PEOPLE_ON_LECTURE ) {
            /* Process broadcasts information that it's entering specific lecture */
            pthread_mutex_lock( &on_lecture_mutex );
            pthread_mutex_unlock( &modify_permits );
            desired_lectures[message->data] = 0;
            sleep(5000);
            pthread_mutex_unlock( &on_lecture_mutex );
            int mamo_ile_jeszcze = 0;
            for (int i=1 ; i<= LECTURE_COUNT ; i++) {
                mamo_ile_jeszcze += desired_lectures[i];
            }
            if (!mamo_ile_jeszcze){
                pthread_mutex_unlock( &gtfo_mutex );
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
    if( message->data == ENTER_PYRKON ){
        zgredek_dostal_pozwolenie_na_pyrkon(message);
    } else {
        zgredek_dostal_pozwolenie_na_warsztat(message);
    } 
    free(message);
}

/**
 * Function receives EXIT message from other process and deals with it's content.
 * @param message - packet_t received from other process.
 */
void exit_handler ( packet_t * message ) {
    println( "PROCESS [%d] (AFTER_PYRKON) received info that [%d] has left Pyrkon.\n",
                     rank, message->src )
    pthread_mutex_lock( &modify_exited_from_pyrkon );
    exited_from_pyrkon++;
    pthread_mutex_unlock( &modify_exited_from_pyrkon );

    /* If everyone exited PyrkonDostalem, process can start another one. */
    if( exited_from_pyrkon == MAX_PEOPLE_ON_PYRKON ) {
        change_state(BEFORE_PYRKON);
        pthread_mutex_unlock( &wait_for_new_pyrkon );
    }
    free(message);
}
#pragma clang diagnostic pop