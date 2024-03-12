#include <cstring>
#include <string>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <queue>
#include <cstdlib>
#include <time.h>
#include <stdio.h>

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

sem_t waitEachPatientRegister; // Each patient register in turn

int receptionistPatients = 0; // Count of patients the receptionist has received

int registerPatientId = -1;     // Current patient id that receptionist is processing
sem_t patientCheckIn;           // Patient just registered. Receptionist waits for a patient to checking in before register that patient with the nurse
sem_t receptionistRegisterDone; // Receptionist finishes registering for current patient

sem_t nursePatientsProtect; // Protection for count since all nurses have access to
int nursePatients = 0;      // Count of patients all nurses have processed

int *nurseOfPatient;     // Array (Size = Patients) - Assigned nurse of a patient
sem_t *patientWaitNurse; // Array (Size = Patients) - Nurses can use this to signal each patient that it's their turn

sem_t *nurseQueueProtect;    // Array (Size = Nurses) - Protection for queue since the receptionist and nurses can work concurrently on these queues
std::queue<int> *nurseQueue; // Array (Size = Nurses) - Waiting room of patients for each nurse
sem_t *patientJoinWaitRoom;  // Array (Size = Nurses) - Each nurse takes a patient from waiting room. Patient posts when they join wait room

sem_t doctorPatientsProtect; // Protection of count since all doctors have access to
int doctorPatients = 0;      // Count of patients all doctors have processed

sem_t *doctorReady;   // Array (Size = Doctors) - Whether each doctor is ready or not. For nurse to send in new patient
int *patientOfDoctor; // Array (Size = Doctors) - Current patient of a doctor. Default to -1 meaning no patient

sem_t *patientSymptom; // Array (Size = Doctors) - Each doctor listens to patient symptom
sem_t *doctorAdvice;   // Array (Size = Doctors) - Each doctor gives out advice
sem_t *patientLeave;   // Array (Size = Doctors) - Each doctor waits for patient to leave

void *patientThread(void *arg)
{
    int patientId = *(int *)arg;

    // --- Register phase

    printf("Patient %d enters waiting room, waits for receptionist\n", patientId);

    // Wait for the patient's turn to check in and register
    semWait(waitEachPatientRegister, "waitEachPatientRegister");

    registerPatientId = patientId;

    semPost(patientCheckIn, "patientCheckIn");

    semWait(receptionistRegisterDone, "receptionistRegisterDone");

    int assignedNurseId = nurseOfPatient[patientId];
    int assignedDoctorId = assignedNurseId;

    printf("Patient %d leaves receptionist and sits in waiting room\n", patientId);

    semPost(waitEachPatientRegister, "waitEachPatientRegister");

    // --- Nurse phase

    semPost(patientJoinWaitRoom[assignedNurseId], "patientJoinWaitRoom - assignedNurseId");

    semWait(patientWaitNurse[patientId], "patientWaitNurse - patientId");

    // --- Doctor phase

    printf("Patient %d enters doctor %d's office\n", patientId, assignedDoctorId);

    semPost(patientSymptom[assignedDoctorId], "patientSymptom - assignedDoctorId");

    semWait(doctorAdvice[assignedDoctorId], "doctorAdvice - assignedDoctorId");

    printf("Patient %d receives advice from doctor %d\n", patientId, assignedDoctorId);

    semPost(patientLeave[assignedDoctorId], "patientLeave - assignedDoctorId");

    // --- Leave phase

    printf("Patient %d leaves\n", patientId);

    return arg;
}

void *receptionistThread(void *arg)
{
    while (receptionistPatients < numPatients)
    {
        // Wait for a patient to check in
        semWait(patientCheckIn, "patientCheckIn");

        printf("Receptionist receives patient %d\n", registerPatientId);

        // Randomly assign that patient to a nurse
        int randomNurseId = randomInRange(0, numNurses - 1);
        nurseOfPatient[registerPatientId] = randomNurseId;

        // Add that patient to that nurse's wait room
        semWait(nurseQueueProtect[randomNurseId], "nurseQueueProtect - randomNurseId");
        nurseQueue[randomNurseId].push(registerPatientId);
        semPost(nurseQueueProtect[randomNurseId], "nurseQueueProtect - randomNurseId");

        // Increase processed patients for the only receptionist
        receptionistPatients++;

        // Tell patient that registration is done
        semPost(receptionistRegisterDone, "receptionistRegisterDone");
    }

    return arg;
}

void *nurseThread(void *arg)
{
    int nurseId = *(int *)arg;
    int doctorId = nurseId;

    while (true)
    {
        // Check break condition for nurse
        if (nursePatients >= numPatients)
            break;

        // Check nurse queue and get patient id
        if (nurseQueue[nurseId].empty())
            continue;

        int patientId = nurseQueue[nurseId].front();

        // Wait for front patient to join wait room
        semWait(patientJoinWaitRoom[nurseId], "patientJoinWaitRoom - nurseId");

        // Wait for doctor to ready
        semWait(doctorReady[doctorId], "doctorReady - doctorId");

        // Take patient out of wait room and to doctor's office
        semWait(nurseQueueProtect[nurseId], "nurseQueueProtect - randomNurseId");
        nurseQueue[nurseId].pop();
        semPost(nurseQueueProtect[nurseId], "nurseQueueProtect - randomNurseId");

        printf("Nurse %d takes patient %d to doctor's office\n", nurseId, patientId);

        patientOfDoctor[doctorId] = patientId;

        // Increase processed patients of all nurses
        semWait(nursePatientsProtect, "nursePatientsProtect");
        nursePatients++;
        semPost(nursePatientsProtect, "nursePatientsProtect");

        // Signal front patient that it's their turn
        semPost(patientWaitNurse[patientId], "patientWaitNurse - patientId");
    }

    return arg;
}

void *doctorThread(void *arg)
{
    int doctorId = *(int *)arg;

    while (true)
    {
        // Check break condition for doctor
        if (doctorPatients >= numPatients)
            break;

        // Check if doctor has patient
        int patientId = patientOfDoctor[doctorId];
        if (patientId == -1)
            continue;

        // Wait for current patient to tell symptoms
        semWait(patientSymptom[doctorId], "patientSymptom");

        printf("Doctor %d listens to symptoms from patient %d\n", doctorId, patientId);

        // Give advice to patient
        semPost(doctorAdvice[doctorId], "doctorAdvice - doctorId");

        // Wait for patient to leave
        semWait(patientLeave[doctorId], "patientLeave - doctorId");

        // Reset current patient of doctor to no one
        patientOfDoctor[doctorId] = -1;

        // Increase processed patients of all doctors
        semWait(doctorPatientsProtect, "doctorPatientsProtect");
        doctorPatients++;
        semPost(doctorPatientsProtect, "doctorPatientsProtect");

        // Tell nurse that doctor is ready for next patient
        semPost(doctorReady[doctorId], "doctorReady - doctorId");
    }

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
    /*
        int numDoctors;
        int numNurses;
        int numPatients;

        sem_t waitEachPatientRegister; // Each patient register in turn

        int receptionistPatients = 0; // Count of patients the receptionist has received

        int registerPatientId = -1;     // Current patient id that receptionist is processing
        sem_t patientCheckIn;           // Patient just registered. Receptionist waits for a patient to checking in before register that patient with the nurse
        sem_t receptionistRegisterDone; // Receptionist finishes registering for current patient

        sem_t nursePatientsProtect; // Protection for count since all nurses have access to
        int nursePatients = 0;      // Count of patients all nurses have processed

        int *nurseOfPatient;     // Array (Size = Patients) - Assigned nurse of a patient
        sem_t *patientWaitNurse; // Array (Size = Patients) - Nurses can use this to signal each patient that it's their turn

        sem_t *nurseQueueProtect;    // Array (Size = Nurses) - Protection for queue since the receptionist and nurses can work concurrently on these queues
        std::queue<int> *nurseQueue; // Array (Size = Nurses) - Waiting room of patients for each nurse
        sem_t *patientJoinWaitRoom;  // Array (Size = Nurses) - Each nurse takes a patient from waiting room. Patient posts when they join wait room

        sem_t doctorPatientsProtect; // Protection of count since all doctors have access to
        int doctorPatients = 0;      // Count of patients all doctors have processed

        sem_t *doctorReady;   // Array (Size = Doctors) - Whether each doctor is ready or not. For nurse to send in new patient
        int *patientOfDoctor; // Array (Size = Doctors) - Current patient of a doctor. Default to -1 meaning no patient

        sem_t *patientSymptom; // Array (Size = Doctors) - Each doctor listens to patient symptom
        sem_t *doctorAdvice;   // Array (Size = Doctors) - Each doctor gives out advice
        sem_t *patientLeave;   // Array (Size = Doctors) - Each doctor waits for patient to leave
    */

    semInit(waitEachPatientRegister, "waitEachPatientRegister", 1);

    semInit(patientCheckIn, "patientCheckIn", 0);
    semInit(receptionistRegisterDone, "receptionistRegisterDone", 0);

    semInit(nursePatientsProtect, "nursePatientsProtect", 1);

    for (int patientId = 0; patientId < numPatients; patientId++)
    {
        semInit(patientWaitNurse[patientId], "patientWaitNurse - patientId", 0);
    }

    for (int nurseId = 0; nurseId < numNurses; nurseId++)
    {
        semInit(nurseQueueProtect[nurseId], "nurseQueueProtect - nurseId", 1);
        semInit(patientJoinWaitRoom[nurseId], "patientJoinWaitRoom - nurseId", 0);
    }

    semInit(doctorPatientsProtect, "doctorPatientsProtect", 1);

    for (int doctorId = 0; doctorId < numDoctors; doctorId++)
    {
        semInit(doctorReady[doctorId], "doctorReady - doctorId", 1);

        semInit(patientSymptom[doctorId], "patientSymptom - doctorId", 0);
        semInit(doctorAdvice[doctorId], "doctorAdvice - doctorId", 0);
        semInit(patientLeave[doctorId], "patientLeave - doctorId", 0);
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

    /*
        int numDoctors;
        int numNurses;
        int numPatients;

        sem_t waitEachPatientRegister; // Each patient register in turn

        int receptionistPatients = 0; // Count of patients the receptionist has received

        int registerPatientId = -1;     // Current patient id that receptionist is processing
        sem_t patientCheckIn;           // Patient just registered. Receptionist waits for a patient to checking in before register that patient with the nurse
        sem_t receptionistRegisterDone; // Receptionist finishes registering for current patient

        sem_t nursePatientsProtect; // Protection for count since all nurses have access to
        int nursePatients = 0;      // Count of patients all nurses have processed

        int *nurseOfPatient;     // Array (Size = Patients) - Assigned nurse of a patient
        sem_t *patientWaitNurse; // Array (Size = Patients) - Nurses can use this to signal each patient that it's their turn

        sem_t *nurseQueueProtect;    // Array (Size = Nurses) - Protection for queue since the receptionist and nurses can work concurrently on these queues
        std::queue<int> *nurseQueue; // Array (Size = Nurses) - Waiting room of patients for each nurse
        sem_t *patientJoinWaitRoom;  // Array (Size = Nurses) - Each nurse takes a patient from waiting room. Patient posts when they join wait room

        sem_t doctorPatientsProtect; // Protection of count since all doctors have access to
        int doctorPatients = 0;      // Count of patients all doctors have processed

        sem_t *doctorReady;   // Array (Size = Doctors) - Whether each doctor is ready or not. For nurse to send in new patient
        int *patientOfDoctor; // Array (Size = Doctors) - Current patient of a doctor. Default to -1 meaning no patient

        sem_t *patientSymptom; // Array (Size = Doctors) - Each doctor listens to patient symptom
        sem_t *doctorAdvice;   // Array (Size = Doctors) - Each doctor gives out advice
        sem_t *patientLeave;   // Array (Size = Doctors) - Each doctor waits for patient to leave
    */

    // Get command line inputs
    numDoctors = stoiHandler(argv[1]);
    numNurses = numDoctors;
    numPatients = stoiHandler(argv[2]);

    nurseOfPatient = new int[numPatients];
    patientWaitNurse = new sem_t[numPatients];

    nurseQueueProtect = new sem_t[numNurses];
    nurseQueue = new std::queue<int>[numNurses];
    patientJoinWaitRoom = new sem_t[numNurses];

    doctorReady = new sem_t[numDoctors];
    patientOfDoctor = new int[numDoctors];
    for (int i = 0; i < numDoctors; i++)
        patientOfDoctor[i] = -1;

    patientSymptom = new sem_t[numDoctors];
    doctorAdvice = new sem_t[numDoctors];
    patientLeave = new sem_t[numDoctors];

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

    printf("Simulation complete\n");

    return 0;
}