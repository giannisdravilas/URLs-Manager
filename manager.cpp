#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
#include <string>
#include <limits>
#include <cstring>
#include <queue>
#include <vector>

#define MSGSIZE 4096    // The maximum message size to be used for the buffers (this can be changed according to our needs)

using namespace std;

void worker(vector<int> named_pipes, uint i_worker);

////////// ****GLOBAL variable in order to be able to be used by the signal handler****   //////////
int stopped_worker_exists = 0;  // bool to check whether a stopped worker exists, in order to search for it later

// a handler that handles SIGCHLD signals (when a child process stops or terminates)
void sigchld_handler(int i) {
    stopped_worker_exists = 1;
}

////////// ****GLOBAL variables in order to be able to be used by the signal handler****   //////////

vector<char*> filenames;    // a vector containing the names of the files to be transmitted to the workers
char* path = new char [4096];   // containing the path to the file, assuming that maximum path size is 4096 characters
                                // (I think that's more than enough :P)
pid_t listener; // listener process
int pipe_listener_manager[2];   // pipe for communication between listener and manager
vector<pid_t> workers;  // a vector containing worker processes
vector<int> named_pipes;    // a vector containing the named pipes between the manager and each of the workers

////////////////////////////////////////////////////////////////////////////////////////////////////

// a handler that handles SIGINT signals (when we press Ctrl+C)
void sigint_handler(int i) {
    close(pipe_listener_manager[0]);    // we close the two ends of the manager-listener pipe
    close(pipe_listener_manager[1]);
    for (uint i = 0; i < named_pipes.size(); i++){  // we close the named pipes
        close(named_pipes[i]);
    }
    for (uint i = 0; i < workers.size(); i++){  // we kill the worker processes
        kill(workers[i], SIGKILL);
    }
    for (uint i = 0; i < filenames.size(); i++){  // delete the memory allocated by the names of the files
        delete[] filenames[i];
    }
    delete[] path;  // delete memory used by the path variable
    kill(listener, SIGKILL);    // we also kil the listener process
    wait(NULL); // just to be sure that all child processes have finished
    cout << "\nKilled everything and exiting!" << endl;
    exit(0);
}

// A helper function that counts how many files a substring of inotifywait's return contains, according
// to the number of commas that exist
// For example, string "file1" has no commas and so contains only 1 filename, while stirng "fil1,file2,file3,file4"
// has 3 commas and so contains 3+1=4 filenames.
int count_files(char* s) {
    int commas = 0;
    for (uint i = 0; i < strlen(s); i++)
        if (s[i] == ',') commas++;
    if (commas == 0){
        return 1;
    }else{
        return commas+1;
    }
}

int main(int argc, char *argv[]) {

    // Initialize path as "./" (current directory) and change according to optional user input
    strcpy(path, "./");
    if (argc == 3 and !strcmp(argv[1], "-p")) {
        strcpy(path, argv[2]);
    }else if (argc == 3 and strcmp(argv[2], "-p")){
        perror("wrong arguments");
    }else if (argc > 3){
        perror("wrong arguments");
    }

    int rsize = 0;  // size returned by read()
    char inbuf[MSGSIZE] = "";   // buffer for read()
    char const* fifo = "fifos/workers"; // where our named pipes will be stored
    priority_queue<pid_t> available_workers;    // a priority queue containing the available workers
    
    // Initialize the pipe between listener and manager
    if (pipe(pipe_listener_manager) == -1){
        perror("pipe call");
        exit(1);
    }

    // Create a new listener process
    if ((listener = fork()) == -1){
        perror("fork");
        exit(1);
    }
    
    // Code for listener process
    if (listener == 0){
        dup2(pipe_listener_manager[1], 1);  // Allocate new file descriptor for the pipe
        execlp("inotifywait", "inotifywait", "-m", "-e", "create", "-e", "moved_to", path, NULL);   // execute inotifywait

    // Code for manager process
    }else{
        int i = 0;  // Count in order to know when we are reading files for the first time
        do{
            signal(SIGINT, sigint_handler); // When we receive a SIGINT signal from keyboard
            if ((rsize = read(pipe_listener_manager[0], inbuf, MSGSIZE)) < 0){  // read from manager-listener pipe
                perror("problem in reading");
                exit(5);
            }

            // Get the last token of the string read with read() in inbuf (split with a space character)
            // This token contains the list of files created or moved to the current directory
            char* token = strtok(inbuf, " ");
            char previous_token[MSGSIZE];
            while (token != NULL) {
                strcpy(previous_token, token);
                token = strtok(NULL, " ");
            }

            int n = count_files(previous_token);    // Count the number files this string contains

            for (uint i = 0; i < filenames.size(); i++){  // delete the memory allocated by the names of the files
                delete[] filenames[i];
            }
            filenames.clear();  // and clear the vector to insert the new filenames that we got from the listener

            // Now get every filename this last token contains and save it to the vector for further use
            // (split with a comma)
            char* filename;

            filename = strtok(previous_token, ",");

            if (filename){  // it may be NULL
                char *new_filename = new char[strlen(filename)+strlen(path)+1]; // one more character for the '\0' of strcpy
                strcpy(new_filename, path); // also append path
                strcat(new_filename, filename); //concatenate
                filenames.push_back(new_filename);  // add the new filename (with is path) to the vector
            }
            while (filename != NULL) {
                filename = strtok(NULL, ",");
                if (filename){  // it may be NULL
                    char *new_filename = new char[strlen(filename)+strlen(path)+1]; // one more character for the '\0' of strcpy
                    strcpy(new_filename, path); // also append path
                    strcat(new_filename, filename); // concatenate
                    filenames.push_back(new_filename);  // add the new filename (with is path) to the vector
                }
            }

            // If this is the first time we read the ouput of inotifywait then we want to create as many
            // worker processes as the number of files initially created or moved here            
            if (i == 0){

                // For every file that is initally read, we create a work process
                for (int j = 0; j < n; j++){

                    int returned;
                    if ((returned = fork()) < 0){ // Create a new worker process
                        perror("Failed to create process");
                        return 1;
                    }

                    workers.push_back(returned);    // Add this process to the vector of workers
                    available_workers.push(returned);   // Add this process to the priority queue of available workers

                    if (mkfifo(fifo, 0666) == -1){  // Create the named pipe for this worker
                        if (errno != EEXIST){
                            perror("receiver : mkfifo");
                            exit(6);
                        }
                    }

                    if ((returned = open(fifo, O_RDWR)) < 0){   // Open the named pipe for this worker with read/write access
                        perror("fifo open problem");
                        exit(3) ;
                    }

                    named_pipes.push_back(returned);    // And add this named pipe to the vector of named pipes

                    // Code for worker process
                    // It should me mentioned that because of the way the insertion to the vectors is handled,
                    // the named pipes vector contains in each position the named pipe for the worker that exists
                    // in the corresponding position of workers' vector
                    if (workers[j] == 0){
                        worker(named_pipes, j);
                    }
                }
            }

            // For every file read from inotifywait
            for (int j = 0; j < n; j++){
                
                signal(SIGCHLD, sigchld_handler);   // catch SIGCHLD signals occuring when a child stops or terminates

                if (stopped_worker_exists){ // the sigchld_handler has found whether there are stopeed workers or not
                    pid_t stopped;
                    // check which children have their state changed and push them again to the priority queue
                    // -1 for any child, WUNTRACED for stopped children and WNOHANG to return immediately if no child has exited
                    while ((stopped = waitpid(-1, NULL, WUNTRACED|WNOHANG))){
                        available_workers.push(stopped);
                    }
                }

                // If there are still no workers available
                if (available_workers.empty()){

                    int returned;
                    if ((returned = fork()) < 0){   // Create a new worker process
                        perror("Failed to create process");
                        return 1;
                    }

                    workers.push_back(returned);    // Add this process to the vector of workers
                    available_workers.push(returned);   // Add this process to the priority queue of available workers

                    if (mkfifo(fifo, 0666) == -1){  // Create the named pipe for this worker
                        if (errno != EEXIST){
                            perror("receiver : mkfifo");
                            exit(6);
                        }
                    }

                    if ((returned = open(fifo, O_RDWR)) < 0){ // Open the named pipe for this worker with read/write access
                        perror("fifo open problem");
                        exit(3) ;
                    }

                    named_pipes.push_back(returned);    // And add this named pipe to the vector of named pipes

                    // Code for the new worker process which is now on the last position of the vector
                    if (workers[workers.size()-1] == 0){
                        worker(named_pipes, workers.size()-1);
                    }
                }

                // Now that we have an available worker, get it from the top of the priority queue
                pid_t available_worker = available_workers.top();
                available_workers.pop();

                // Search in the workers vector for the position of the available worker
                uint i_worker;
                for (i_worker = 0; i_worker < workers.size(); i_worker++){
                    if (workers[i_worker] == available_worker){
                        break;
                    }
                }

                // Write the filename to process in the named pipe of this worker
                int nwrite;
                if ((nwrite = write(named_pipes[i_worker], filenames[j], strlen(filenames[j])-1)) == -1){
                    perror("Error in Writing");
                    exit(2);
                }

                kill(available_worker, SIGCONT);    // Let the worker continue its work by senting it a SIGCONT signal
            }
            
            memset(inbuf, 0, MSGSIZE);  // Reset the buffer
            i++;    // Change the iteration counter
        }while (rsize>0);   // Only stop when bad input is read
    }

    close(pipe_listener_manager[0]);    // we close the two ends of the manager-listener pipe
    close(pipe_listener_manager[1]);
    for (uint i = 0; i < *(uint*)(named_pipes.size()); i++){    // we close the named pipes
        close(named_pipes[i]);
    }
    for (uint i = 0; i < workers.size(); i++){  // we kill the worker processes
        kill(workers[i], SIGKILL);
    }
    for (uint i = 0; i < filenames.size(); i++){  // we kill the worker processes
        delete[] filenames[i];
    }
    delete[] path;  // delete memory used by the path variable
    kill(listener, SIGKILL);    // we also kil the listener process
    wait(NULL); // just to be sure that all child processes have finished
    cout << "\nKilled everything and exiting!" << endl;
    exit(0);
}