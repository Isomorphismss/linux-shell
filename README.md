# Custom Shell (`shell`)

![MIT License](https://img.shields.io/badge/License-MIT-yellow.svg)

This project is a custom Linux shell that simulates basic shell functionalities, including executing commands, handling input/output redirection, and managing background/suspended jobs.

**Note**: This shell is designed to run in a Linux environment. It should work on any Linux distribution, including Ubuntu, Debian, and others.

## Features

- **Command Execution**: Supports execution of commands both via absolute and relative paths.
- **Built-in Commands**:
  - `cd <directory>`: Change the current working directory.
  - `jobs`: List currently suspended jobs.
  - `fg <job number>`: Resume a suspended job in the foreground.
  - `exit`: Terminate the shell (if there are no suspended jobs).
- **Input/Output Redirection**: Supports `>`, `<`, and `>>` operators for redirecting input/output.
- **Pipes**: Supports piping between commands using `|`.
- **Signal Handling**: Properly handles `SIGINT`, `SIGQUIT`, and `SIGTSTP` signals, allowing users to suspend and resume jobs.

## Compilation and Usage

### Compilation
To compile the program, use the following command:
```bash
gcc -o shell shell.c
```
### Running the Shell
```bash
./shell
```
### Running with Docker
If you prefer running the shell inside a Docker container, follow these steps:
1. Create a `DockerFile` with the following content:

    ```bash
    FROM ubuntu:latest

    RUN apt-get update && \
        apt-get install -y gcc && \
        apt-get clean

    COPY shell.c /usr/src/shell/

    RUN gcc -o /usr/local/bin/shell /usr/src/shell/shell.c

    ENTRYPOINT ["/usr/local/bin/shell"]
    ```

2. Build the Docker image:

    ```bash
    docker build -t custom-shell .
    ```

3. Run the Container:

    ```bash
    docker run -it custom-shell
    ```

### Examples
1. Executing Commands:

    ```bash
    [shell ~]$ ls -l
    [shell ~]$ /bin/echo "Hello, World!"
    ```
2. Input/Output redirection:

    ```bash
    [shell ~]$ cat < input.txt > output.txt
    [shell ~]$ grep "pattern" < input.txt >> results.txt
    ```
3. Using Pipes:

    ```bash
    [shell ~]$ cat file.txt | grep "search term" | sort
    ```
4. Built-in commands:

    ```bash
    [shell ~]$ cd /path/to/directory
    [shell ~]$ jobs
    [shell ~]$ fg 1
    [shell ~]$ exit
    ```

## Notes
- The shell supports a maximum of 100 suspended jobs.
- Commands with pipes can have I/O redirection, but with restrictions:
    - Output redirection is only allowed for the last command in the pipeline.
    - Input redirection is only allowed for the first command in the pipeline.
- Errors are displayed to `stderr` with appropriate error messages when commands are invalid.

## License
This project is licensed under the MIT License.