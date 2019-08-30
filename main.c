#include "main.h"

MPI_Datatype MPI_PAKIET_T;
pthread_t threadCom, threadM;

/* zamek do synchronizacji zmiennych współdzielonych */
pthread_mutex_t konto_mut = PTHREAD_MUTEX_INITIALIZER;
sem_t all_sem;

/* Ile każdy proces ma na początku pieniędzy */
int konto=STARTING_MONEY;

/* suma zbierana przez monitor */
int sum = 0;

/* end == TRUE oznacza wyjście z main_loop */
volatile char end = FALSE;
void mainLoop(void);

/* Deklaracje zapowiadające handlerów. */
void myStateHandler(packet_t *pakiet);
void finishHandler(packet_t *pakiet);
void appMsgHandler(packet_t *pakiet);
void giveHandler(packet_t *pakiet);

/* typ wskaźnik na funkcję zwracającej void i z argumentem packet_t* */
typedef void (*f_w)(packet_t *);
/* Lista handlerów dla otrzymanych pakietów
   Nowe typy wiadomości dodaj w main.h, a potem tutaj dodaj wskaźnik do 
     handlera.
   Funkcje handleróœ są na końcu pliku. Nie zapomnij dodać
     deklaracji zapowiadającej funkcji!
*/
f_w handlers[MAX_HANDLERS] = { [GIVE_YOUR_STATE]=giveHandler,
            [FINISH] = finishHandler,
            [MY_STATE_IS] = myStateHandler,
            [APP_MSG] = appMsgHandler };

extern void inicjuj(int *argc, char ***argv);
extern void finalizuj(void);

int main(int argc, char **argv)
{
    /* Tworzenie wątków, inicjalizacja itp */
    inicjuj(&argc,&argv);

    mainLoop();

    finalizuj();
    return 0;
}


/* Wątek główny - przesyłający innym pieniądze */
void mainLoop(void)
{
    int prob_of_sending=PROB_OF_SENDING;
    int dst;
    packet_t pakiet;
    /* mały sleep, by procesy nie zaczynały w dokładnie tym samym czasie */
    struct timespec t = { 0, rank*50000 };
    struct timespec rem = { 1, 0 };
    nanosleep(&t,&rem); 

    /* pętla główna: sen, wysyłanie przelewów innym bankom */
    while (!end) {
	int percent = rand()%2 + 1;
        struct timespec t = { percent, 0 };
        struct timespec rem = { 1, 0 };
        nanosleep(&t,&rem);

	percent = rand()%100;
        /* czy wysłać komuś przelew? */
	if ((percent < prob_of_sending ) && (konto >0)) {
            /* losujemy, komu wysłać przelew */
	    do {
		dst = rand()%(size);
		//if (dst==size) MPI_Abort(MPI_COMM_WORLD,1); // strzeżonego :D
	    } while (dst==rank);

            /* losuję, ile kasy komuś wysłać */
	    percent = rand()%konto;
	    pakiet.kasa = percent;
	    pthread_mutex_lock(&konto_mut);
            konto-=percent;
	    pthread_mutex_unlock(&konto_mut);
	    sendPacket(&pakiet, dst, APP_MSG);
            /* z biegiem czasu coraz rzadziej wysyłamy (przyda się do wykrywania zakończenia) */
	    if (prob_of_sending > PROB_SENDING_LOWER_LIMIT)
		prob_of_sending -= PROB_OF_SENDING_DECREASE;

	    println("-> wysłałem %d do %d\n", pakiet.kasa, dst);
        }  
    }
}

/* Wątek monitora - tylko u ROOTa */
void *monitorFunc(void *ptr)
{
    packet_t data;
	/* MONITOR; Jego zadaniem ma być wykrycie, ile kasy jest w systemie */

	// 5 sekund, coby procesy zdążyły namieszać w stanie globalnym
    sleep(5);
	// TUTAJ WYKRYWANIE STANu        
    int i;
    sem_init(&all_sem,0,0);
    println("MONITOR START \n");
    for (i=0;i<size;i++)  {
	sendPacket(&data, i, GIVE_YOUR_STATE);
    }
    sem_wait(&all_sem);

    for (i=1;i<size;i++) {
	sendPacket(&data, i, FINISH);
    }
    sendPacket(&data, 0, FINISH);
    P_RED; printf("\n\tW systemie jest: [%d]\n\n", sum);P_CLR
    return 0;
}

/* Wątek komunikacyjny - dla każdej otrzymanej wiadomości wywołuje jej handler */
void *comFunc(void *ptr)
{

    MPI_Status status;
    packet_t pakiet;
    /* odbieranie wiadomości */
    while ( !end ) {
	println("[%d] czeka na recv\n", rank);
        MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        pakiet.src = status.MPI_SOURCE;

        handlers[(int)status.MPI_TAG](&pakiet); // zamiast wielkiego switch status.MPI_TAG... case X: handler()
    }
    println(" Koniec! ");
    return 0;
}

/* Handlery */
void myStateHandler(packet_t *pakiet)
{
    static int statePacketsCnt = 0;

    statePacketsCnt++;
    sum += pakiet->kasa;
    println("Suma otrzymana: %d, total: %d\n", pakiet->kasa, sum);
    //println( "%d statePackets from %d\n", statePacketsCnt, pakiet->src);
    if (statePacketsCnt == size ) {
        sem_post(&all_sem);
    }
    
}

void finishHandler( packet_t *pakiet)
{
    println("Otrzymałem FINISH" );
    end = TRUE; 
}

void giveHandler( packet_t *pakiet)
{
    /* monitor prosi, by mu podać stan kasy */
    /* tutaj odpowiadamy monitorowi, ile mamy kasy. Pamiętać o muteksach! */
    println("dostałem GIVE STATE");
   
    packet_t tmp;
    tmp.kasa = konto; 
    sendPacket(&tmp, ROOT, MY_STATE_IS);
}

void appMsgHandler( packet_t *pakiet)
{
    /* ktoś przysłał mi przelew */
    println("\tdostałem %d od %d\n", pakiet->kasa, pakiet->src);
    pthread_mutex_lock(&konto_mut);
	konto+=pakiet->kasa;
    println("Stan obecny: %d\n", konto);
    pthread_mutex_unlock(&konto_mut);
}
