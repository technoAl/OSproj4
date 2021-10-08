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

#define MAX_LENGTH 1024 // Max File Name Size

int badFiles = 0;
int directories = 0;
int regularFiles = 0;
int specialFiles = 0;
long long regularFileBytes = 0;
int allText = 0;
long long textFileBytes = 0;
//int currentThreadCount = 0;
queue<pthread_t> runningProcesses;
//pthread_t* runningProcessesList = (pthread_t*) malloc(sizeof(pthread_t) * 2048); // List can hold up to 2048 threads, if there are more than 2048 files, then will fail.

sem_t mutex; // Mutex between threads

bool isTextFile(int size, int fdIn){ // checks if a file is text using MMAP
    char *pchFile = (char *) mmap (NULL, size, PROT_READ, MAP_SHARED, fdIn, 0);
    // Map to memory
    if (pchFile == (char *) -1)	{
        perror("Could not mmap file");
        return false;
    }

    for (int i = 0; i < size; i++) { // Loops through each character
        if(!isprint(pchFile[i]) && !isspace(pchFile[i])){
            return false; // If any character fails
        }
    }

    // Unmap memory
    if(munmap(pchFile, size) < 0){
        perror("Could not unmap memory");
    }
    return true;
}

void* parser(void* arg){ // Parser Thread
    char* name = (char*) arg; // File Name passed

    // Stat
    struct stat* sb = (struct stat*) malloc(sizeof(struct stat));

    // Before editing any global, mutex is ensured and editing is done in critical region
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
        //cout << sb -> st_size << "\n";
        sem_wait(&mutex);
        regularFiles++;
        regularFileBytes += sb -> st_size;
        sem_post(&mutex);

        // Need to open file to check is Text
        int fdIn;
        if ((fdIn = open(name, O_RDONLY)) < 0) {
            cerr << "bad file descriptor\n";
            pthread_exit(0);
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

    // Exit thread when done
    pthread_exit(0);
}

int main(int argc, char *argv[]) {

    if(argc == 3 && strcmp("thread", argv[1]) == 0){ // threads
        if(atoi(&argv[2][0]) > 15 || atoi(&argv[2][0]) < 1){// Check thread count
            cout << "Invalid thread count";
            exit(0);
        }

        // Init sems for mutex
        if (sem_init(&mutex, 0, 1) < 0) {
            perror("sem_init");
            exit(1);
        }

        // Pointer in List of threads
        int nextSpace = 0;
        int currentPointer = 0;

        int maxThreads = atoi(&argv[2][0]);

        // Create threads to process each file
        while(true){

            // New buffer for name passing
            char* name = (char*)malloc(sizeof(char) * MAX_LENGTH);

            // Get line
            cin.getline(name, MAX_LENGTH);

//            cout << name << "\n";
//            cout.flush();

            if(cin.fail()){ // EOF
                break;
            }

            if(runningProcesses.size() < maxThreads) { // Create a new thread
                pthread_t newThread;
                if (pthread_create(&newThread, NULL, parser, (void *) name) != 0) {
                    perror("pthread_create");
                }
                runningProcesses.push(newThread); // add this thread to the queu
            } else {
                pthread_join(runningProcesses.front(), NULL); // Wait for thread to finish
                runningProcesses.pop(); // remove process from queue as it's done

                // Then create a new thread for the file
                pthread_t newThread;
                if (pthread_create(&newThread, NULL, parser, (void *) name) != 0) {
                    perror("pthread_create");
                }
                //runningProcesses.push(newThread);

                // Add this thread to the process lists next open space
                runningProcesses.push(newThread); // add new process to queue
            }

        }

        while(!runningProcesses.empty()){// wait for all threads to finish
            pthread_join(runningProcesses.front(), NULL);
            //currentPointer++;
            runningProcesses.pop();
        }


    } else { // Serial Structure
        while(true) {
            struct stat sb;

            char name[MAX_LENGTH];
            cin.getline(name, MAX_LENGTH);
            if(cin.fail()){ // EOF
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
                if(isTextFile(sb.st_size, fdIn)){ // Check Text File
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

    // Printout
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

