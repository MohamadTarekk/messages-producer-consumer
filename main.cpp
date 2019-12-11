#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <cmath>
#include <queue>

using namespace std;

/// Threads Objects
typedef struct counterObject
{
    int id;
} counterObject;
typedef struct monitorObject
{
    int position;
} monitorObject;
typedef struct collectorObject
{
    int position;
} collectorObject;

/// Threads Handlers
void* counterHandler(void *ptr);
void* monitorHandler(void *ptr);
void* collectorHandler(void *ptr);

/// Global variables
bool monitorRunning;
bool collectorRunning;
int COUNTER;
int BUFFER_POSITION;
int THREADS_COUNT, BUFFER_SIZE, TIME_INTERVAL;
queue<int> BUFFER;
sem_t SEM_COUNT;
sem_t SEM_FULL;
sem_t SEM_EMPTY;
sem_t SEM_BUFFER;
pthread_t *MCOUNTERS;
pthread_t MONITOR_THREAD;
pthread_t COLLECTOR_THREAD;
counterObject *OBJ_MCOUNTERS;
monitorObject OBJ_MONITOR;
collectorObject OBJ_COLLECTOR;

/// Functions Prototypes
void initialize();
void dispatchMonitors();
void dispatchThreads();
void readInput();
int generateRandomInt(int low, int high);
void milli_sec_sleep(long period);

int main()
{
    initialize();
    dispatchMonitors();
    dispatchThreads();
    return 0;
}

void* counterHandler(void *ptr)
{
    // initialize thread object
    counterObject ob = *((counterObject *) ptr);
    // sleep randomly to simulate random incoming messages
    milli_sec_sleep(generateRandomInt(1000,4000)); //1 to 4 seconds
    printf("Counter thread %2d: received a message\n", (ob).id);
    // get counter value and set counter to 0
    /* wait if counter is busy*/
    int res = sem_trywait(&SEM_COUNT);
    if(res != 0)
    {
        printf("Counter thread %2d: waiting to write!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", (ob).id);
        sem_wait(&SEM_COUNT);
    }
    /* increment the counter*/
    COUNTER++;
    printf("Counter thread %2d: now adding to counter, counter value=%d\n", (ob).id, COUNTER);
    sem_post(&SEM_COUNT);

    return NULL;
}

void* monitorHandler(void *ptr)
{
    while(monitorRunning)
    {
        // sleeping
        int temp;
        milli_sec_sleep(generateRandomInt(1000,4000)); //1 to 4 seconds
        // get counter value and set counter to 0
        /* wait if counter is busy*/
        int res = sem_trywait(&SEM_COUNT);
        if(res != 0)
        {
            printf("Monitor thread: waiting to read the counter\n");
            sem_wait(&SEM_COUNT);
        }
        /* get the counter's value and reset it to 0*/
        printf("Monitor thread : reading the count of value=%d\n", COUNTER);
        temp  = COUNTER;
        COUNTER = 0;
        sem_post(&SEM_COUNT);
        // pass counter value to buffer
        /* wait if buffer is full*/
        res = sem_trywait(&SEM_FULL);
        if(res != 0)
        {
            printf("Monitor thread: Buffer full!!\n");
            sem_wait(&SEM_FULL);
        }
        /* wait if buffer is busy*/
        res = sem_trywait(&SEM_BUFFER);
        if(res != 0)
        {
            printf("Monitor thread: Buffer is busy\n");
            sem_wait(&SEM_BUFFER);
        }
        /* finally add new value to buffer*/
        BUFFER_POSITION++;
        printf("Monitor thread : writing to buffer at position %d\n", BUFFER_POSITION);
        BUFFER.push(temp);
        sem_post(&SEM_BUFFER);
        sem_post(&SEM_EMPTY);
    }

    return NULL;
}

void* collectorHandler(void *ptr)
{
    while(collectorRunning)
    {
        // sleeping
        int temp;
        milli_sec_sleep(generateRandomInt(1000,4000)); //1 to 4 seconds
        // read from buffer
        /* wait if buffer is full*/
        int res = sem_trywait(&SEM_EMPTY);
        if(res != 0)
        {
            printf("Collector thread: nothing is in the buffer!\n");
            sem_wait(&SEM_EMPTY);
        }
        /* wait if buffer is busy*/
        res = sem_trywait(&SEM_BUFFER);
        if(res != 0)
        {
            printf("Collector thread: Buffer is busy\n");
            sem_wait(&SEM_BUFFER);
        }
        /* finally add new value to buffer*/
        temp = (int) BUFFER.front();
        printf("Collector thread : reading the value %d from buffer at position 1 of %d\n", temp, BUFFER_POSITION);
        BUFFER.pop();
        BUFFER_POSITION--;
        sem_post(&SEM_BUFFER);
        sem_post(&SEM_FULL);
    }

    return NULL;
}

void initialize()
{
    //initialize counters and flags
    COUNTER = 0;
    readInput();
    //initialize semaphores
    sem_init(&SEM_COUNT, 0, 1);
    sem_init(&SEM_FULL, 0, BUFFER_SIZE);
    sem_init(&SEM_EMPTY, 0, 0);
    sem_init(&SEM_BUFFER, 0, 1);
    //initialize counter threads and there objects
    MCOUNTERS = (pthread_t*) malloc(THREADS_COUNT * sizeof(pthread_t));
    pthread_t newThreads[THREADS_COUNT+2];
    OBJ_MCOUNTERS = (counterObject*) malloc(THREADS_COUNT * sizeof(counterObject));
    counterObject newObj[THREADS_COUNT];
    int i;
    for(i = 0 ; i < THREADS_COUNT ; i++)
    {
        MCOUNTERS[i] = newThreads[i];
        newObj[i].id = i + 1;
        OBJ_MCOUNTERS[i] = newObj[i];
    }
    //initialize monitor thread and its object
    MONITOR_THREAD = newThreads[i];
    monitorObject m;
    OBJ_MONITOR = m;
    //initialize collector thread and its object
    i++;
    COLLECTOR_THREAD = newThreads[i];
    collectorObject c;
    OBJ_COLLECTOR = c;

    return;
}

void dispatchMonitors()
{
    // initialize monitor thread object
    BUFFER_POSITION = 0;
    // run monitor thread
    monitorRunning = true;
    pthread_create(&MONITOR_THREAD, NULL, monitorHandler, (void*) &OBJ_MONITOR);
    // run collector thread
    collectorRunning = true;
    pthread_create(&COLLECTOR_THREAD, NULL, collectorHandler, (void*) &OBJ_MONITOR);
}

void dispatchThreads()
{
    int i, j, c, limit;
    int batch_size = 1000;
    int batches = ceil(THREADS_COUNT / batch_size);
    // divide message counter threads into batches
    for(c = 0 ; c <= batches ; c++)
    {
        // set catch bounds
        limit = (c + 1) * batch_size;
        i = limit - batch_size;
        j = limit - batch_size;
        if(limit > THREADS_COUNT)
        {
            limit = THREADS_COUNT;
            i = limit - (THREADS_COUNT % batch_size);
            j = limit - (THREADS_COUNT % batch_size);
        }
        // run threads of batch c
        for( ; i < limit ; i++)
        {
            pthread_create(&MCOUNTERS[i], NULL, counterHandler, (void*) &OBJ_MCOUNTERS[i]);
        }
        // join threads of batch c
        for( ; j < limit ; j++)
        {
            pthread_join(MCOUNTERS[j], NULL);
        }
    }
    // wait for monitor to reset the counter
    while(COUNTER != 0);
    // terminate the monitor thread
    monitorRunning = false;
    // wait for buffer to be empty
    while(!BUFFER.empty());
    // terminate the collector thread
    collectorRunning = false;

    return;
}

void readInput()
{
    printf("Reading input...\n");
    FILE *f = fopen("input.txt", "r");
    fscanf(f, "%d\n", &THREADS_COUNT);
    fscanf(f, "%d\n", &BUFFER_SIZE);
    fscanf(f, "%d", &TIME_INTERVAL);
    fclose(f);
    printf("Number of thread: %d thread\nBuffer size: %d entry\nMonitor check interval: %d sec\n\nReading completed!\n\n", THREADS_COUNT, BUFFER_SIZE, TIME_INTERVAL);
    if(THREADS_COUNT > 100000)
    {
        printf("Too many message counter threads!!");
        exit(0);
    }
    if(BUFFER_SIZE > 100000)
    {
        printf("Too large buffer size!!");
        exit(0);
    }

    return;
}

int generateRandomInt(int low, int high)
{
    int range=(high-low)+1;
    return range * (rand() / (RAND_MAX + 1.0));
}

void milli_sec_sleep(long period)
{
    struct timespec t = {0};
    t.tv_sec = 0;
    t.tv_nsec = period * 1000000L;
    nanosleep(&t, (struct timespec *)NULL);

    return;
}
