#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <map>
#include <vector>
#include <sys/types.h>
#include <signal.h>

#define MSGSIZE 4096

using namespace std;

void worker(vector<int> named_pipes, uint i_worker){

    char const* fifo = "fifos/workers"; // where our named pipes are stored

    while (1){   // The worker never stops

        char filename[4096] = "";   // Every filename has at most 4096 characters (I think that's enough for a title :P)
        
        int fd, fd_out, fd_fifo; // File descriptors for the file that will be read, the output file and the named pipe
        char buffer[2] = " "; // Our buffer reads one byte at a time (byte,'\0')

        if ((fd_fifo = open (fifo, O_RDWR)) < 0){   // Open the named pipe for this worker with read/write access
            perror("fifo open problem");
            exit(3) ;
        }

        // read from the named pipe of the worker the name of the file that the manager wants to be processed by this worker
        int rsize;
        if ((rsize = read(named_pipes[i_worker], filename, MSGSIZE+1)) < 0){
            perror("problem in reading");
            exit(5) ;
        }

        if((fd = open(filename, O_RDONLY)) == -1){  // Open the file that the manager wants to be processed by this worker
            perror("file open problem1");
            exit(1);
        }

        // String mainpulation in order to create the output file name
        string folder_out = "outputs/";

        // We want to get only the token after the last "/", which is the actual filename without the path
        char* token = strtok(filename, "/");
        char previous_token[4096];  // assume that maximum file name size is 4096 characters (I think that's more than enough :P)
        while (token != NULL) {
            strcpy(previous_token, token);        
            token = strtok(NULL, "/");
        }

        char* filename_out = strcat((char*)folder_out.c_str(), previous_token);
        filename_out = strcat(filename_out, ".out");

        // Open the output file (or create it, if it does not exist), truncate its content
        if((fd_out = open(filename_out, O_CREAT|O_RDWR|O_TRUNC, 0644)) == -1){
            perror("file open problem2");
            exit(1);
        }

        // Now read the contents of the original file and search for "http://"
        // Save the found url in url string and then the domain-location in a map for immediate access its time
        int count = 0;
        string url = "";
        map<string, int> domains;

        // Read one byte at a time
        while (read(fd, &buffer[0], 1) == 1){

            // If we have found "http://" and there is no space or new line, continue reading
            if (count == 7 && strncmp(buffer, " ", 1) && strncmp(buffer, "\n", 1)){
                url += buffer;
                continue;
            // Else if we have found "http://" and there is a space or new line
            }else if(count == 7 && (!strncmp(buffer, " ", 1) || !strncmp(buffer, "\n", 1))){

                if (!strncmp(url.c_str(), "www.", 4)){  // remove "www.", if it exists
                    url = url.substr(4);;
                }

                url = strtok(const_cast<char*>(url.c_str()), "/");  // remove the path after the location

                // insert the location as a key in the map if it does not exist
                // and increment the value by one (the default value of value is 0) so it will initially be 1
                domains.insert(pair<string, int>(url, domains[url]+=1));

                url = "";   // reset url
                count = 0;  // reset count
            }

            // Check how many characters of "http://" we have have found each time
            if (count == 0 && !strncmp(buffer, "h", 1)){
                count = 1;
            }else if (count == 1 && !strncmp(buffer, "t", 1)){
                count = 2;
            }else if (count == 2 && !strncmp(buffer, "t", 1)){
                count = 3;
            }else if (count == 3 && !strncmp(buffer, "p", 1)){
                count = 4;
            }else if (count == 4 && !strncmp(buffer, ":", 1)){
                count = 5;
            }else if (count == 5 && !strncmp(buffer, "/", 1)){
                count = 6;
            }else if (count == 6 && !strncmp(buffer, "/", 1)){
                count = 7;
            }else{
                count = 0;
            }
        }

        close(fd);  // close file
        
        // Write the locations-domains found and their quantity in the output file
        for(const auto& elem : domains){
            write(fd_out, (elem.first).c_str(), elem.first.length());
            write(fd_out, " ", 1);
            write(fd_out, to_string(elem.second).c_str(), to_string(elem.second).length());
            write(fd_out, "\n", 1);
        }
        
        close(fd_out);  // close file
        close(fd_fifo); // close named pipe

        kill(getpid(), SIGSTOP);    // stop child
    }

    return;
}
