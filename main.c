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
volatile int recieved_agreement = 0;
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
    inicjuj(&argc,&argv);

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

    while (!end) {
	    int percent = rand()%2 + 1;
        struct timespec t = { percent, 0 };
        struct timespec rem = { 1, 0 };
        nanosleep(&t,&rem);

        switch(state){

            case BEFORE_PYRKON: {
                pyrkon_broadcast(WANT_TO_ENTER, 0, 0);
                wait_for_agreement();
                state = ON_PYRKON;

                int lectures_number = my_random_int(1, LECTURE_COUNT - 1);
                for(int i = 1; i < LECTURE_COUNT + 1; i++) {
                    int lecture = my_random_int(1, lectures_number);
                    desired_lectures[lecture] = 1;
                }
                for(int i = 1; i < LECTURE_COUNT + 1; i++) {
                    if(desired_lectures[i] == 1) {
                        pyrkon_broadcast(WANT_TO_ENTER, i, 0);
                    }
                }
                break;
            }

            case ON_PYRKON: {
                wait_for_agreement();
                state = ON_LECTURE;
                break;
            }

            case ON_LECTURE: {
                println("Jestem na warsztacie")
                sleep(5000);

                desired_lectures[allowed_lecture] = 0;
                for (int i = 0; i < LECTURE_COUNT + 1; i++) {
                    if(desired_lectures[i] == 1) {
                        state = ON_PYRKON;
                        break;
                    }
                }
                if (state != ON_PYRKON) state = AFTER_PYRKON;
                break;
            }

            case AFTER_PYRKON: {
                pyrkon_broadcast(EXIT, 0, 0);
                wait_for_agreement();
                state = BEFORE_PYRKON;
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
	    println("[%d] czeka na recv\n", rank)
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        pakiet.src = status.MPI_SOURCE;

        pthread_t new_thread;
        pthread_create(&new_thread, NULL, (void *)handlers[(int)status.MPI_TAG], &pakiet);
        // handlers[(int)status.MPI_TAG](&pakiet); // zamiast wielkiego switch status.MPI_TAG... case X: handler()
    }

    pthread_mutex_lock(&clock_mutex);

    if(lamport_clock < pakiet.ts) {
        lamport_clock = pakiet.ts + 1;
    } else if(lamport_clock > pakiet.ts) {
        lamport_clock ++;
    }

    pthread_mutex_unlock(&clock_mutex);

    println(" Koniec! ")
    return 0;
}


void want_enter_handler ( packet_t *message ) {

    int just_sent = FALSE;
    switch ( message->data ) {
        case 0: {
            while ( !just_sent ) {

                pthread_mutex_lock( &on_pyrkon_mutex );
                int clock_allows = (message->ts < last_clock || (message->ts == last_clock && rank > message->src));
                if ( clock_allows ) {

                    packet_t tmp;
                    tmp.data = message->data;
                    tmp.ts = get_clock(TRUE);
                    // TODO write sending packets
                    // sendPacket(&tmp, message->src, POZWALAM);
                    pthread_mutex_unlock( &on_pyrkon_mutex );
                    just_sent = TRUE;
                }
                else {

                    pthread_mutex_unlock( &on_pyrkon_mutex );
                    if (end) break;
                }
            }
            break;
        }
        default: break;

    }
    free(message);
}

void entering_handler( packet_t * message ) {

    // TODO write handler for message type ENTERING
    free(message);
}

void alright_enter_handler ( packet_t * message ) {

    // TODO write handler for message type ALRIGHT_TO_ENTER
    free(message);
}

void exit_handler ( packet_t * message ) {

    // TODO write handler for message type EXIT
    free(message);
}
#pragma clang diagnostic pop