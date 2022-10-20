# URLs-Manager
👨‍💻 C++ low-level program monitoring changes in the files of a directory and processing the URLs of each file by forking new processes. A bash script sums up the results.

First, a manager process (parent process) is created, which creates another listener process (child process) and one or more worker processes (child processes). The manager is informed for new files by the listener via an unnamed pipe (which is created in the directory 'fifos', which **must have been created by the user beforehand**) and creates as many workers as the files that were read for the first time. Then, the manager sends the workers one file name at a time (via a named pipe), in order for them to process it. In case there is no worker available (which can happen after the first time, if more files than the first time are given), a new worker is created and the file is sent to it, a process that is repeated when necessary.<br >

The worker, then, receives from the manager the name of the file that should be processed and keeps only its last part, which is the actual name of the file (not containing the path). It then opens the specified file, while simultaneously creating a corresponding `.out` file in the directory 'outputs' (which **must have been created by the user beforehand**). It then reads the text of the file, one byte at a time, and when an 'http://' combination is found, the process continues until a blank character or line break is found. This string now forms a url. Each url, after being processed, is reduced to a location, which is added to a map for quick access to it every time we find the same location again in the same file.<br>

The program has been tested for memory leaks.

Compile with:

```
make
```

Run with:

```
./sniffer [-p path]
```
e.g.
```
./sniffer
or
./sniffer -p myDir/
```
**Attention: path should be terminated with a '/'**

Using `make run`, the `./sniffer` is executed without the `-p option`, while a `make clean` deletes any object or executable files created.<br><br>

The finder bash script (`finder.sh`) searches for Top Level Domains in the files created by the program described above. The script initially checks for the path option, and if the former does not exist, it searches within the current directory. Then, for each Top Level Domain given as an argument, the finder initializes a counter, opens each `.out` file in the directory, and reads each line of the file. If a location ending with the desired TLD is found, the `num_of_appearances` variable is added to the counter. Before continuing to the next TLD, the number of occurrences of the current TLD is printed.

Run with:

```
./finder.sh [-p path] TLDs
```
e.g.
```
./finder.sh -p myDir com gr
or
./finder.sh com gr
```