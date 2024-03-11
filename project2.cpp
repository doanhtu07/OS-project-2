#include <string>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>

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

void semWait(sem_t &sem, char *name)
{
    if (sem_wait(&sem) == -1)
    {
        printf("Wait on semaphore %s\n", name);
        exit(1);
    }
}

void semPost(sem_t &sem, char *name)
{
    if (sem_post(&sem) == -1)
    {
        printf("Post semaphore %s\n", name);
        exit(1);
    }
}

void semInit(sem_t &sem, char *name, int value)
{

    /* Initialize semaphore to 0 (3rd parameter) */
    if (sem_init(&sem, 0, value) == -1)
    {
        printf("Init semaphore %s\n", name);
        exit(1);
    }
}

// ----- Thread procedures

sem_t receptionistReady;

int registerPatientId = -1;
sem_t receptionistRegister;
sem_t receptionistRegisterDone;

void *patientThread(void *arg)
{
    int patientId = *(int *)arg;

    // Register phase

    printf("Patient %d enters waiting room, waits for receptionist", patientId);

    semWait(receptionistReady, "receptionistReady");

    registerPatientId = patientId;

    semPost(receptionistRegister, "receptionistRegister");
    semWait(receptionistRegisterDone, "receptionistRegisterDone");

    printf("Patient %d leaves receptionist and sits in waiting room", patientId);

    semPost(receptionistReady, "receptionistReady");

    return arg;
}

void *receptionistThread(void *arg)
{
    semWait(receptionistRegister, "receptionistRegister");

    printf("Receptionist receives patient %d", registerPatientId);

    semPost(receptionistRegisterDone, "receptionistRegisterDone");

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
    std::cout << "Nurse " << nurseId << std::endl;
    return arg;
}

// ----- Init threads

void initSemaphores()
{
    semInit(receptionistReady, "receptionistReady", 1);
    semInit(receptionistRegister, "receptionistRegister", 0);
    semInit(receptionistRegisterDone, "receptionistRegisterDone", 0);
}

void initPatients(int numPatients)
{
    pthread_t patients[15];
    int patientIds[15];
    int errcode; /* holds pthread error code */
    int *status; /* holds return code */

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

    /* join the threads as they exit */
    for (patientId = 0; patientId < numPatients; patientId++)
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
}

void initReceptionist()
{
    pthread_t receptionist;
    int errcode; /* holds pthread error code */
    int *status; /* holds return code */

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

    errcode = pthread_join(receptionist, NULL);

    if (errcode)
    {
        errexit(errcode, "pthread_join");
    }
}

void initDoctors(int numDoctors)
{
    pthread_t doctors[3];
    int doctorIds[3];
    int errcode; /* holds pthread error code */
    int *status; /* holds return code */

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

    /* join the threads as they exit */
    for (doctorId = 0; doctorId < numDoctors; doctorId++)
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
}

void initNurses(int numNurses)
{
    pthread_t nurses[3];
    int nurseIds[3];
    int errcode; /* holds pthread error code */
    int *status; /* holds return code */

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

    /* join the threads as they exit */
    for (nurseId = 0; nurseId < numNurses; nurseId++)
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
    // Get command line inputs
    int numDoctors = stoiHandler(argv[1]);
    int numNurses = numDoctors;
    int numPatients = stoiHandler(argv[2]);

    std::cout << "Run with " << numPatients << " patients, "
              << numNurses << " nurses, "
              << numDoctors << " doctors"
              << std::endl
              << std::endl;

    initSemaphores();

    initPatients(numPatients);
    initReceptionist();
    initDoctors(numDoctors);
    initNurses(numNurses);

    return 0;
}