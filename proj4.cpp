//
// Created by alexander on 10/6/21.
//

#include <iostream>
using namespace std;
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <queue>

#define MAX_LENGTH 1024

int badFiles = 0;
int directories = 0;
int regularFiles = 0;
int specialFiles = 0;
long regularFileBytes = 0;
int allText = 0;
long textFileBytes = 0;
int currentThreadCount = 0;
queue<pthread_t> runningProcesses;
pthread_t* runningProcessesList = (pthread_t*) malloc(sizeof(pthread_t) * 1024);

sem_t mutex;
sem_t countMutex;


bool isTextFile(int size, int fdIn){
    char *pchFile;
    // Map to memory
    if ((pchFile = (char *) mmap (NULL, size, PROT_READ, MAP_SHARED, fdIn, 0))
        == (char *) -1)	{
        perror("Could not mmap file");
    }

    for (int i = 0; i < size; i++) {
        if(!isprint(pchFile[i]) && !isspace(pchFile[i])){
            return false;
        }
    }

    // Unmap memory
    if(munmap(pchFile, size) < 0){
        perror("Could not unmap memory");
    }
    return true;
}

void* parser(void* arg){ // Adder thread
    char* name = (char*) arg;

    sem_wait(&countMutex);
    currentThreadCount++;
    sem_post(&countMutex);

    struct stat* sb = (struct stat*) malloc(sizeof(struct stat));

    if(stat(name, sb) < 0){ // Bad File
        sem_wait(&mutex);
        badFiles++;
        sem_post(&mutex);
    }
    if(S_ISDIR(sb -> st_mode)){ // Directory
        sem_wait(&mutex);
        directories++;
        sem_post(&mutex);
    } else if(S_ISREG(sb ->st_mode)){ // Regular
        sem_wait(&mutex);
        regularFiles++;
        regularFileBytes += sb -> st_size;
        sem_post(&mutex);
        int fdIn;
        if ((fdIn = open(name, O_RDONLY)) < 0) {
            cerr << "bad file descriptor\n";
        }
        if(isTextFile(sb -> st_size, fdIn)){
            //cout << sb -> st_size << "\n";

            sem_wait(&mutex);
            textFileBytes += sb -> st_size;
            allText++;
            sem_post(&mutex);
        }
        if (fdIn > 0)
            close(fdIn);

    } else {
        sem_wait(&mutex);
        specialFiles++;
        sem_post(&mutex);
    }

    sem_wait(&mutex);
    currentThreadCount--;
    sem_post(&mutex);
    pthread_exit(0);
}

int main(int argc, char *argv[]) {

    // Set target character

    if(argc == 3 && strcmp("thread", argv[1]) == 0){ // threads
        if(atoi(&argv[2][0]) > 15 || atoi(&argv[2][0]) < 1){
            cout << "Invalid thread count";
            exit(0);
        }

        if (sem_init(&mutex, 0, 1) < 0) {
            perror("sem_init");
            exit(1);
        }
        if (sem_init(&countMutex, 0, 1) < 0) {
            perror("sem_init");
            exit(1);
        }

        int nextSpace = 0;
        int currentPointer = 0;

        int maxThreads = atoi(&argv[2][0]);
        while(true){
            char* name = (char*)malloc(sizeof(char) * MAX_LENGTH);
            cin.getline(name, MAX_LENGTH);

            if(cin.fail()){
                //cout << "here";
                break;
            }

            sem_wait(&countMutex);
            int curCount = currentThreadCount;
            //cout << "thread count " << currentThreadCount << "\n";
            //cout.flush();
            sem_post(&countMutex);

            if(curCount < maxThreads) {
                pthread_t newThread;
                if (pthread_create(&newThread, NULL, parser, (void *) name) != 0) {
                    perror("pthread_create");
                }
                runningProcessesList[nextSpace] = newThread;
                nextSpace++;
                //runningProcesses.push(newThread);
            } else {
                pthread_join(runningProcesses.front(), NULL);
                pthread_t newThread;
                if (pthread_create(&newThread, NULL, parser, (void *) name) != 0) {
                    perror("pthread_create");
                }
                //runningProcesses.push(newThread);
                runningProcessesList[nextSpace] = newThread;
                nextSpace++;
            }

        }

        while(!runningProcesses.empty()){// wait for all threads to finish
            pthread_join(runningProcessesList[currentPointer], NULL);
            currentPointer++;
            //runningProcesses.pop();
        }


    } else { // Serial Structure
        while(true) {
            struct stat sb;

            char name[MAX_LENGTH];
            cin.getline(name, MAX_LENGTH);
            if(cin.fail()){
                break;
            }
            if(stat(name, &sb) < 0){ // Bad File
                badFiles++;
                continue;
            }
            if(S_ISDIR(sb.st_mode)){ // Directory
                directories++;
            } else if(S_ISREG(sb.st_mode)){ // Regular
                regularFiles++;
                regularFileBytes += sb.st_size;
                int fdIn;
                if ((fdIn = open(name, O_RDONLY)) < 0) {
                    cerr << "file open\n";
                    continue;
                }
                if(isTextFile(sb.st_size, fdIn)){
                    textFileBytes += sb.st_size;
                    allText++;
                }
                if (fdIn > 0)
                    close(fdIn);

            } else {
                specialFiles++;
            }
        }
    }

    cout << "Bad Files: " << badFiles << "\n";
    cout << "Directories: " << directories << "\n";
    cout << "Regular Files: " << regularFiles << "\n";
    cout << "Special Files: " << specialFiles << "\n";
    cout << "Bytes Used by Regular Files: " << regularFileBytes << "\n";
    cout << "Text Files: " << allText << "\n";
    cout << "Bytes Used by Text Files: " << textFileBytes << "\n";
    cout.flush();

    (void) sem_destroy(&mutex);

}

