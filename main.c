// import necessary header files(libraries)
#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

/* The maximum input size is not determined in the description.
So,I decided to make it 1024 as I thought it will be enough for this project.
We can increase it according to our needs for a more complicated shell project. */
#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_NUMBER 256
#define MAX_PATH_TOKEN_NUMBER 1024
#define MAX_LINE_LENGTH 100
#define MAX_OUTPUT_LENGTH 1024
// global variable to store the last executed command (will be used in bellos)
char *last_executed_command = "";
int alias_token_count = 0;

FILE *filem;

//  function to search the alias_name in the function
int searchInFile(const char *filename, const char *input) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening file %s\n", filename);
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    char leftSide[MAX_LINE_LENGTH];
    char rightSide[MAX_LINE_LENGTH];

    while (fgets(line, sizeof(line), file) != NULL) {
        // seperate the right and the left sides of the equality sign
        if (sscanf(line, "%[^=]=%[^\n]", leftSide, rightSide) == 2) {
            char *trimmedLeftSide = strdup(leftSide);
            size_t length = strlen(trimmedLeftSide);

            // remove redundant whitespaces and add terminator  \0 to the end of the string
            while (length > 0 && (trimmedLeftSide[length - 1] == ' ' || trimmedLeftSide[length - 1] == '\t')) {
                trimmedLeftSide[--length] = '\0';
            }

            // compare the left side with the command entered by the user
            if (strcmp(trimmedLeftSide, input) == 0) {
                FILE *cmdOutput = popen(rightSide, "r");
                if (cmdOutput == NULL) {
                    fprintf(stderr, "Error executing command\n");
                    fclose(file);
                    free(trimmedLeftSide);
                    return -1;
                }

                char output[MAX_OUTPUT_LENGTH];
                while (fgets(output, sizeof(output), cmdOutput) != NULL) {
                    printf("%s", output);
                }

                // close and free to prevent segmentation fault
                pclose(cmdOutput);
                fclose(file);
                free(trimmedLeftSide);
                return 1;
            }
            free(trimmedLeftSide);
        }
    }

    fclose(file);
    return 0;
}

/*
takes a string as param. and returns the reversed string.
for example "hello" to "olleh"
*/
char *reverse_string(char *str) {
    int length = strlen(str);
    int i, j;
    char temp;
    for (i = 0, j = length - 1; i < j; ++i, --j) {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
    }
    return str;
}

void alias_handler(char *tokens[]) {

    // Extract alias name and command from tokens
    char *alias_name = tokens[1];
    char *command = tokens[3];

    // Open the aliases.txt file in append mode
    FILE *alias_file = fopen("aliases.txt", "a");
    if (alias_file == NULL) {
        perror("Error opening file");
        return;
    }

    // Write the alias to the file
    fprintf(alias_file, "%s = %s\n", alias_name, command);

    // Close the file
    fclose(alias_file);
}

/*
function to append the reversed string to the file.
there is reverse_string function call inside this.
it will be called in the main function when >>> operator
is encountered in the input command.
*/
void reverse_output_append(char *filename, char *output) {
    FILE *file = fopen(filename, "a"); // Open file in append mode
    if (file == NULL) {
        perror("shell");
        return;
    }
    reverse_string(output);
    fprintf(file, "%s\n", output);
    fclose(file);
}

/*
                     MOST IMPORTANT PART OF THE PROJECT
                     FUNCTION TO CHECK THE PATH MANUALLY, AS IT IS ASKED IN THE DESCRIPTION
*/
int check_path(char *tokens[]) {
    int a = 0;
    // get the path environment variable
    char *path = getenv("PATH");
    char *pathtokens[MAX_PATH_TOKEN_NUMBER];
    // parsing the path into tokens based on ":" delimiter
    char *path_token = strtok(path, ":");
    int pathtokenCount = 0;
    while (path_token != NULL && pathtokenCount < MAX_PATH_TOKEN_NUMBER) {
        pathtokens[pathtokenCount] = path_token;
        path_token = strtok(NULL, ":");
        pathtokenCount++;
    }
    pathtokens[pathtokenCount] = NULL;

    // add the command (tokens[0]) to the path_token and check whether it exists in the path or not.
    for (int i = 0; i < pathtokenCount; i++) {
        char full_path[256];
        // here do the addition of command to the path_token to consturct the full_path
        snprintf(full_path, sizeof(full_path), "%s/%s", pathtokens[i], tokens[0]);
        if (access(full_path, F_OK) == 0) {
            // the command is in the path
            a = 1;
            return a;
        }
    }
    // otherwise command not found.
    a = -1;
    return a;
}

/* function to get the number of running processes
by actually running the ps command
(it will be used in bello). */
int get_running_processes() {
    FILE *fp;
    int process_count = 0;
    char buffer[128];

    fp = popen("ps | wc -l", "r");

    if (fp == NULL) {
        perror("popen");
        exit(EXIT_FAILURE);
    }

    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        process_count = atoi(buffer) - 1;
    }
    pclose(fp);
    return process_count;
}

/*
function to implement the bello command.
it will be called in the main when bello is typed in the input command.
*/
void bello_command() {
    int process_count = get_running_processes();

    // username
    struct passwd *pw = getpwuid(getuid());
    if (pw == NULL) {
        perror("getpwuid");
        return;
    }
    char *username = getenv("USER");

    // hostname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname");
        return;
    }
    char *last_command = "";

    // tty
    char *tty = ttyname(STDIN_FILENO);
    if (tty == NULL) {
        perror("ttyname");
        return;
    }

    // shell name
    char *shell_name = getenv("SHELL");
    if (shell_name == NULL) {
        perror("getenv");
        return;
    }

    // home location
    char *home_location = pw->pw_dir;

    // current time and date
    time_t t;
    time(&t);
    struct tm *local_time = localtime(&t);
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);

    printf("1. Username: %s\n", username);
    printf("2. Hostname: %s\n", hostname);
    printf("3. Last Executed Command: %s\n", last_executed_command);
    printf("4. TTY: %s\n", tty);
    printf("5. Current Shell Name: %s\n", shell_name);
    printf("6. Home Location: %s\n", home_location);
    printf("7. Current Time and Date: %s\n", time_str);
    printf("8. Current Number of Processes: %d\n", process_count);
}

int main() {

    char input[MAX_INPUT_SIZE];
    char cwd[PATH_MAX];
    char hostname[256 + 1];
    char *tokens[MAX_TOKEN_NUMBER];
    int j = 0;
    char s_to_reverse[256];
    char *output;
    char original_command[MAX_TOKEN_NUMBER];
    // get the username
    char *username = getenv("USER");

    // get the current working directory
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        exit(EXIT_FAILURE);
    }

    // get the hostname
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        perror("gethostname");
        exit(EXIT_FAILURE);
    }

    FILE *alias_file = fopen("aliases.txt", "w");
    if (alias_file == NULL) {
        printf("Error: Failed to create  aliases.txt file.\n");
        return 1;
    }
    fclose(alias_file);

    // infinite loop to take command input from the user infinitely
    while (1) {
        // print the prompt specified in the description for every input
        printf("%s@%s %s --- ", username, hostname, cwd);
        // take the input
        if (fgets(input, sizeof(input), stdin) == NULL) {
            // print the error message if no-character typed
            fprintf(stderr, "Error reading input\n");
            // exit(EXIT_FAILURE);
        }

        // remove the new-line from the end of the input line
        input[strcspn(input, "\n")] = '\0';

        // we should exit the program if "exit" is typed
        if (strcmp(input, "exit") == 0) {
            break;
        }

        // parse the input string into tokens based on the whitespace
        char *token = strtok(input, " ");
        // initialize tokenCount with 0.
        int tokenCount = 0;

        // parse the input
        while (token != NULL && tokenCount < MAX_TOKEN_NUMBER) {
            tokens[tokenCount] = token;
            token = strtok(NULL, " ");
            tokenCount++;
        }
        alias_token_count = tokenCount;

        // print the tokens for debugging purposes

        /*
        printf("Tokens:\n");
        for (int i = 0; i < tokenCount; i++) {
            printf("%d: %s\n", i + 1, tokens[i]);
        }
        */

        strcpy(original_command, tokens[0]);

        tokens[tokenCount] = NULL;
        if (strcmp(tokens[0], "alias") == 0) {
            alias_handler(tokens);
        }
        // background process
        int background = 0;
        if (tokenCount > 0 && strcmp(tokens[tokenCount - 1], "&") == 0) {
            background = 1;
            tokens[tokenCount-- - 1] = NULL;
        }

        // redirection
        int redirect_output = 0; // > operator
        int append_output = 0;   // >> operator
        int reverse_output = 0;  // >>> operator
        char *output_file = NULL;

        for (int i = 0; i < tokenCount; i++) {
            if (strcmp(tokens[i], ">") == 0) {
                redirect_output = 1;
                output_file = tokens[i + 1];
                if (tokenCount >= 3) {
                    tokens[i] = NULL;
                }
            } else if (strcmp(tokens[i], ">>") == 0) {
                append_output = 1;
                output_file = tokens[i + 1];
                tokens[i] = NULL;
                if (tokenCount >= 3) {
                    tokens[i] = NULL;
                }
            } else if (strcmp(tokens[i], ">>>") == 0) {
                reverse_output = 1;
                output_file = tokens[i + 1];
                if (tokenCount >= 3) {
                    tokens[i] = NULL;
                }
                output = tokens[i - 1];
            }
        }

        // our shell must work in fork/exec manner as it was said in the description
        pid_t pid = fork();

        if (pid == -1) {
            fprintf(stderr, "Error while fork()\n");
        } else if (pid == 0) {
            // Child process
            if (redirect_output) {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd == -1) {
                    fprintf(stderr, "Failed to open output file\n");
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            } else if (append_output) {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
                if (fd == -1) {
                    fprintf(stderr, "Failed to open output file\n");
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            } else if (reverse_output) {
                // strcpy(last_executed_command, tokens[0]);
                strcpy(last_executed_command, original_command);
                // en basic version echo "hello" >>> try6.txt dogru calisiyor.
                if ((strcmp(tokens[0], "echo") == 0) & (tokenCount == 4)) {
                    int of_dene = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
                    reverse_output_append(output_file, output);
                } else {
                    // pipe for the executed command's output
                    int pipefd[2];
                    pipe(pipefd);

                    pid_t pid = fork();
                    if (pid == 0) {
                        close(pipefd[0]);
                        dup2(pipefd[1], STDOUT_FILENO);
                        execvp(tokens[0], tokens);
                        // If execvp fails
                        perror("execvp");
                        exit(EXIT_FAILURE);
                    } else if (pid > 0) {
                        close(pipefd[1]);
                        // read the executed command's output from pipe
                        char buffer[1000];
                        ssize_t bytesRead = read(pipefd[0], buffer, sizeof(buffer));
                        close(pipefd[0]);

                        if (bytesRead > 0) {
                            // add terminator \0 to the end of the string
                            buffer[bytesRead] = '\0';

                            // if >>> operator appears (as it is the case) reverse the ouput
                            if (reverse_output) {
                                reverse_string(buffer);
                            }

                            // reversed output to the specified file
                            if (output_file != NULL) {
                                FILE *file = fopen(output_file, "a");
                                if (file != NULL) {
                                    if (buffer[0] != '\0') {
                                        fputs(buffer + 1, file);
                                    }
                                    fclose(file);
                                } else {
                                    perror("fopen");
                                }
                            } else {
                                // print the reversed output for debugging purposes
                                // printf("%s", buffer + 1);
                            }
                        }
                        // wait for the child process
                        wait(NULL);
                    } else {
                        perror("fork");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            // execute the command      !!!!!!
            bool flag = true;
            if (searchInFile("aliases.txt", tokens[0])) {
                alias_handler(tokens);
                flag = false;
            }

            if (flag) {
                for (int i = 0; i < tokenCount; i++) {
                    if (!reverse_output) {
                        if (strcmp(tokens[0], "alias") != 0) {
                            if ((execvp(tokens[0], tokens) == -1) & (strcmp(tokens[0], "bello") != 0)) {
                                printf("Command not found\n");
                                // exit(EXIT_FAILURE);
                            }
                        }
                    }
                }

                // handle the bello command
                if (strcmp(tokens[0], "bello") == 0) {
                    if (strcmp(last_executed_command, "") == 0) {
                        last_executed_command = "bello (first command within the program)";
                    }
                    bello_command();
                    last_executed_command = "bello";
                }
            }

        } else {
            if (strcmp(tokens[0], "bello") != 0) {
                last_executed_command = strdup(input);
            }
            // parent
            if (!background) {
                // if it is not background, parent must wait
                waitpid(pid, NULL, 0);
            }
        }
    }
    return 0;
}
