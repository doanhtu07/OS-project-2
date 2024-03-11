#include <cstring>
#include <string>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <queue>

#define errexit(code, str)                              \
    fprintf(stderr, "%s: %s\n", (str), strerror(code)); \
    exit(1);

// ----- Utils

int stoiHandler(std::string s)
{
    int num;
    num = 0;

    int idx = 0;
    while (idx < s.length() && s.at(idx) >= '0' && s.at(idx) <= '9')
    {
        num = num * 10 + (s.at(idx) - '0');
        idx++;
    }

    return num;
}

// Both bounds are inclusive
int randomInRange(int lb, int ub)
{
    return (rand() % (ub - lb + 1)) + lb;
}

void semWait(sem_t &sem, const char *name)
{
    if (sem_wait(&sem) == -1)
    {
        printf("Wait on semaphore %s\n", name);
        exit(1);
    }
}

void semPost(sem_t &sem, const char *name)
{
    if (sem_post(&sem) == -1)
    {
        printf("Post semaphore %s\n", name);
        exit(1);
    }
}

void semInit(sem_t &sem, const char *name, int value)
{
    /* Initialize semaphore to 0 (3rd parameter) */
    if (sem_init(&sem, 0, value) == -1)
    {
        printf("Init semaphore %s\n", name);
        exit(1);
    }
}

// ----- Thread procedures

int numDoctors;
int numNurses;
int numPatients;

sem_t receptionistReady;

int registerPatientId = -1;
sem_t receptionistPatientRegister;
sem_t receptionistPatientRegisterDone;
int receptionistPatients = 0;

int *patientAssignNurseIds;
sem_t *patientsWaiting;

sem_t *nurseQueueProtect;
std::queue<int> *nurseQueues;

sem_t *doctorsReady;

void *patientThread(void *arg)
{
    int patientId = *(int *)arg;

    // Register phase

    printf("Patient %d enters waiting room, waits for receptionist\n", patientId);

    semWait(receptionistReady, "receptionistReady");

    registerPatientId = patientId;

    semPost(receptionistPatientRegister, "receptionistPatientRegister");
    semWait(receptionistPatientRegisterDone, "receptionistPatientRegisterDone");

    semPost(receptionistReady, "receptionistReady");

    // Nurse phase

    printf("Patient %d leaves receptionist and sits in waiting room\n", patientId);

    int assignedNurseId = patientAssignNurseIds[patientId];
    int assignedDoctorId = assignedNurseId;

    semWait(patientsWaiting[patientId], "patientsWaiting - patientId");

    return arg;
}

void *receptionistThread(void *arg)
{
    while (receptionistPatients < numPatients)
    {
        semWait(receptionistPatientRegister, "receptionistPatientRegister");

        printf("Receptionist receives patient %d\n", registerPatientId);

        int randomNurseId = randomInRange(0, numNurses - 1);
        patientAssignNurseIds[registerPatientId] = randomNurseId;

        semWait(nurseQueueProtect[randomNurseId], "nurseQueueProtect - randomNurseId");
        nurseQueues[randomNurseId].push(registerPatientId);
        semPost(nurseQueueProtect[randomNurseId], "nurseQueueProtect - randomNurseId");

        receptionistPatients++;

        semPost(receptionistPatientRegisterDone, "receptionistPatientRegisterDone");
    }

    return arg;
}

void *doctorThread(void *arg)
{
    int doctorId = *(int *)arg;
    std::cout << "Doctor " << doctorId << std::endl;
    return arg;
}

void *nurseThread(void *arg)
{
    int nurseId = *(int *)arg;
    int doctorId = nurseId;

    semWait(doctorsReady[doctorId], "doctorsReady - doctorId");

    semWait(nurseQueueProtect[nurseId], "nurseQueueProtect - randomNurseId");
    int patientId = nurseQueues[nurseId].front();
    nurseQueues[nurseId].pop();
    semPost(nurseQueueProtect[nurseId], "nurseQueueProtect - randomNurseId");

    printf("Nurse %d takes patient %d to doctor's office", nurseId, patientId);

    semPost(patientsWaiting[patientId], "patientsWaiting - patientId");

    return arg;
}

// ----- Init threads

pthread_t receptionist;

pthread_t patients[15];
int patientIds[15];

pthread_t doctors[3];
int doctorIds[3];

pthread_t nurses[3];
int nurseIds[3];

int errcode; /* holds pthread error code */
int *status; /* holds return code */

void initSemaphores()
{
    semInit(receptionistReady, "receptionistReady", 1);
    semInit(receptionistPatientRegister, "receptionistPatientRegister", 0);
    semInit(receptionistPatientRegisterDone, "receptionistPatientRegisterDone", 0);

    for (int patientId = 0; patientId < numPatients; patientId++)
    {
        semInit(patientsWaiting[patientId], "patientsWaiting - patientId", 0);
    }

    for (int nurseId = 0; nurseId < numNurses; nurseId++)
    {
        semInit(nurseQueueProtect[nurseId], "nurseQueueProtect - nurseId", 1);
    }
}

void initPatients()
{
    int patientId;

    /* Create patient threads */
    for (patientId = 0; patientId < numPatients; patientId++)
    {
        // Save patient id
        patientIds[patientId] = patientId;

        /* create thread */
        errcode = pthread_create(&patients[patientId], /* thread struct             */
                                 NULL,                 /* default thread attributes */
                                 patientThread,        /* start routine             */
                                 &patientIds[patientId]);

        if (errcode)
        {
            /* arg to routine */
            errexit(errcode, "pthread_create");
        }
    }
}

void initReceptionist()
{
    /* create thread */
    errcode = pthread_create(&receptionist,      /* thread struct             */
                             NULL,               /* default thread attributes */
                             receptionistThread, /* start routine             */
                             NULL);

    if (errcode)
    {
        /* arg to routine */
        errexit(errcode, "pthread_create");
    }
}

void initDoctors()
{
    int doctorId;

    /* Create doctor threads */
    for (doctorId = 0; doctorId < numDoctors; doctorId++)
    {
        // Save doctor id
        doctorIds[doctorId] = doctorId;

        /* create thread */
        errcode = pthread_create(&doctors[doctorId], /* thread struct             */
                                 NULL,               /* default thread attributes */
                                 doctorThread,       /* start routine             */
                                 &doctorIds[doctorId]);

        if (errcode)
        {
            /* arg to routine */
            errexit(errcode, "pthread_create");
        }
    }
}

void initNurses()
{
    int nurseId;

    /* Create doctor threads */
    for (nurseId = 0; nurseId < numNurses; nurseId++)
    {
        // Save doctor id
        nurseIds[nurseId] = nurseId;

        /* create thread */
        errcode = pthread_create(&nurses[nurseId], /* thread struct             */
                                 NULL,             /* default thread attributes */
                                 nurseThread,      /* start routine             */
                                 &nurseIds[nurseId]);

        if (errcode)
        {
            /* arg to routine */
            errexit(errcode, "pthread_create");
        }
    }
}

void exitThreads()
{
    for (int patientId = 0; patientId < numPatients; patientId++)
    {
        errcode = pthread_join(patients[patientId], (void **)&status);

        if (errcode)
        {
            errexit(errcode, "pthread_join");
        }

        /* check thread's exit status, should be the same as the
        thread number for this example */
        if (*status != patientId)
        {
            fprintf(stderr, "thread %d terminated abnormally\n", patientId);
            exit(1);
        }
    }

    errcode = pthread_join(receptionist, NULL);
    if (errcode)
    {
        errexit(errcode, "pthread_join");
    }

    for (int doctorId = 0; doctorId < numDoctors; doctorId++)
    {
        errcode = pthread_join(doctors[doctorId], (void **)&status);

        if (errcode)
        {
            errexit(errcode, "pthread_join");
        }

        /* check thread's exit status, should be the same as the
        thread number for this example */
        if (*status != doctorId)
        {
            fprintf(stderr, "thread %d terminated abnormally\n", doctorId);
            exit(1);
        }
    }

    for (int nurseId = 0; nurseId < numNurses; nurseId++)
    {
        errcode = pthread_join(nurses[nurseId], (void **)&status);

        if (errcode)
        {
            errexit(errcode, "pthread_join");
        }

        /* check thread's exit status, should be the same as the
        thread number for this example */
        if (*status != nurseId)
        {
            fprintf(stderr, "thread %d terminated abnormally\n", nurseId);
            exit(1);
        }
    }
}

// ----- Main

int main(int argc, char **argv)
{
    // Initialization for random
    srand(time(NULL));

    // Get command line inputs
    numDoctors = stoiHandler(argv[1]);
    numNurses = numDoctors;
    numPatients = stoiHandler(argv[2]);

    patientAssignNurseIds = new int[numPatients];
    patientsWaiting = new sem_t[numPatients];

    nurseQueueProtect = new sem_t[numNurses];
    nurseQueues = new std::queue<int>[numNurses];

    doctorsReady = new sem_t[numDoctors];

    std::cout << "Run with " << numPatients << " patients, "
              << numNurses << " nurses, "
              << numDoctors << " doctors"
              << std::endl
              << std::endl;

    initSemaphores();

    initPatients();
    initReceptionist();
    initDoctors();
    initNurses();

    exitThreads();

    return 0;
}