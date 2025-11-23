
/*
 * 
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"

/*
 * Quick reference for some functions and methods used in this file:
 * - emplace_back(value): std::vector method that constructs an element in-place at the
 *     end of the vector (here we use matches.emplace_back(str) which appends a string).
 * - pattern.c_str(): returns a const char* pointer to the underlying C-string of a
 *     std::string. Used by C APIs (e.g., glob) that expect a C-style string.
 * - dup2(oldfd, newfd): duplicate file descriptor 'oldfd' onto 'newfd', closing newfd
 *     first if necessary. Commonly used to redirect stdin/stdout/stderr before exec.
 * - dup(fd): duplicate a file descriptor, returning a new descriptor referring to the
 *     same open file description.
 * - fork(): create a new process (child). Returns 0 in the child, >0 in the parent
 *     (the child's PID), or -1 on error.
 * - execvp(file, argv): replace the current process image with a new program. If
 *     successful, it does not return. argv is a null-terminated array of C-strings.
 * - waitpid(pid, &status, options): wait for process state changes (used to wait for
 *     child processes to finish). Passing -1 waits for any child.
 * - pipe(fdpipe): create a unidirectional data channel represented by two file
 *     descriptors: fdpipe[0] for reading and fdpipe[1] for writing.
 * - open(path, flags, mode): open a file and return a file descriptor. Flags include
 *     O_WRONLY, O_CREAT, O_APPEND, O_TRUNC, O_RDONLY, etc.
 * - glob(pattern, flags, errfunc, &results): POSIX function to expand wildcard
 *     patterns into matching pathnames. glob_t holds the results, globfree frees them.
 * - strdup(s): duplicate a C-string by allocating memory and copying the contents.
 * - malloc/realloc/free: low-level heap memory management functions.
 *
 * These short notes should help when you see these calls in the code below.
 */


static std::vector<std::string> expandWildcards(const std::string &pattern) {
    // Quick check: only attempt expansion if pattern contains '*' or '?'
    if (pattern.find('*') == std::string::npos && pattern.find('?') == std::string::npos) {
        return {pattern};
    }
    // hold the results of our wildcard search.
    glob_t results;
    
    //clear the results structure by setting all bytes to zero
    memset(&results, 0, sizeof(results));

    int flags = GLOB_NOCHECK | GLOB_TILDE;
    int ret = glob(pattern.c_str(), flags, NULL, &results);
    std::vector<std::string> matches;
    if (ret == 0) {
        // glob found matches (or returned the pattern via GLOB_NOCHECK).
        for (size_t i = 0; i < results.gl_pathc; ++i) {
            matches.emplace_back(results.gl_pathv[i]);
        }
    } else {

        matches.push_back(pattern);
    }

    globfree(&results);
    return matches;
}


void parse(std::vector<Token> &tokens)
{
    Command::_currentCommand.clear();
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentCommand.insertSimpleCommand(Command::_currentSimpleCommand);

    // every token 
    for (size_t i = 0; i < tokens.size(); i++)
    {
        // Get the current token
        const auto& token = tokens[i]; 

        // TOKEN_EOF marks the end of the token list produced by the tokenizer.
        // Stop parsing when we reach it.
        if (token.type == TOKEN_EOF)
        {
            break;
        }
        else if (token.type == TOKEN_COMMAND || token.type == TOKEN_ARGUMENT)
        {
            // Perform wildcard expansion: if the token contains '*' or '?', expand to matching filenames (or the pattern itself if no match).
            std::vector<std::string> expanded = expandWildcards(token.value);
            for (const auto &s : expanded) {
                Command::_currentSimpleCommand->insertArgument(strdup(s.c_str()));
            }
        }
        else if (token.type == TOKEN_PIPE)
        {
            Command::_currentSimpleCommand = new SimpleCommand();
            Command::_currentCommand.insertSimpleCommand(Command::_currentSimpleCommand);
        }
        else if (token.type == TOKEN_REDIRECT) // >
        {
            if (i + 1 < tokens.size()) {
                Command::_currentCommand._outFile = strdup(tokens[i + 1].value.c_str());
                i++;
            }
        }
        else if (token.type == TOKEN_INPUT) // <
        {
            if (i + 1 < tokens.size()) {
                Command::_currentCommand._inputFile = strdup(tokens[i + 1].value.c_str());
                i++; 
            }
        }
        else if (token.type == TOKEN_APPEND) // >>
        {
            if (i + 1 < tokens.size()) {
                Command::_currentCommand._append = 1; 
                Command::_currentCommand._outFile = strdup(tokens[i + 1].value.c_str());
                i++; 
            }
        }
        else if (token.type == TOKEN_ERROR) // 2>
        {
            if (i + 1 < tokens.size()) {
                Command::_currentCommand._errFile = strdup(tokens[i + 1].value.c_str());
                i++; 
            }
        }
        else if (token.type == TOKEN_REDIRECT_AND_ERROR) // >>&
        {
            if (i + 1 < tokens.size()) {
                Command::_currentCommand._outFile = strdup(tokens[i + 1].value.c_str());
                Command::_currentCommand._errFile = strdup(tokens[i + 1].value.c_str());
            
                Command::_currentCommand._append = 1;    
                Command::_currentCommand._out_error = 1; 
                i++; 
            }
        }
        else if (token.type == TOKEN_BACKGROUND) // &
        {
            Command::_currentCommand._background = 1;
        }
    }

    // Execute the command 
    Command::_currentCommand.execute();
}
//simple command class
SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1)
    {
        _numberOfAvailableArguments *= 2;
        _arguments = (char **)realloc(_arguments,_numberOfAvailableArguments * sizeof(char *));
    }

    _arguments[_numberOfArguments] = argument;
    _arguments[_numberOfArguments + 1] = NULL;

    _numberOfArguments++;
}

Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;
    _numberOfSimpleCommands = 0;
    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
    _append = 0;
    _out_error = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
                                                    _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        free(_simpleCommands[i]);
    }

    if (_outFile)
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile)
    {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _outFile = 0;
    _inputFile = 0;
    _errFile = 0;
    _background = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _errFile : "default"
           ,_background ? "YES" : "NO");
    printf("\n\n");
}

void Command::execute()
{
    
    print(); 
    if (_numberOfSimpleCommands == 0 || _simpleCommands[0]->_numberOfArguments == 0) {
        clear();
        // return to caller 
        return;   
    }
    const char *cmd = _simpleCommands[0]->_arguments[0];
    if (strcmp(cmd, "exit") == 0) {
        printf("Good bye!! \n");
        clear();  
        exit(0); 
    }else if (strcmp(cmd, "cd") == 0) {
        const char *dir;
        if (_simpleCommands[0]->_numberOfArguments > 1) {
            dir = _simpleCommands[0]->_arguments[1];
        } else {
            // No directory specified, default to HOME
            dir = getenv("HOME");
            if (dir == NULL) {
                fprintf(stderr, "cd: HOME environment variable not set\n");
            }
        }

        
        if (dir != NULL && chdir(dir) != 0) {

            perror("chdir");
        }

        
        if (dir != NULL) {
   
        printf("Changing to directory '%s'\n", dir);
        } else {
       
        printf("Changing to directory '%s'\n", "");
            }
        char cwdbuf[4096];
        if (getcwd(cwdbuf, sizeof(cwdbuf)) != NULL) {
            printf("You are now in %s\n", cwdbuf);
        }

        clear();
        return;
    }
    int numCmds = _numberOfSimpleCommands;
    int fdin; // Input file descriptor for the current command
    int status; // For waitpid

  
    if (_inputFile) {

        fdin = open(_inputFile, O_RDONLY);
        if (fdin < 0) {
            perror("open input file failed");
            return;
        }
    } else {

        fdin = dup(0); 
    }


    int fdpipe[2];
    pid_t pid;

    for (int i = 0; i < numCmds; i++) {

        dup2(fdin, 0);  
        close(fdin);

        int fdout; 
        if (i == numCmds - 1) {

            if (_outFile) {
           
                int flags;
                // O_WRONLY: Open the file for Writing-Only.
                // O_CREAT:  Create the file if it does not already exist.
                flags = O_WRONLY | O_CREAT;

                // add conditional flag based on _append
                if (_append) {
                    // This makes all writes go to the end of the file.
                    flags = flags | O_APPEND;
                } else {
                    // Otherwise, truncate the file on open so we overwrite.
                    flags = flags | O_TRUNC;
                }
                fdout = open(_outFile, flags, 0666);
                if (fdout < 0) {
                    perror("open output file failed");
                    return;
                }
            } else {
               
                fdout = dup(1); 
            }
        } else {
           
            
            if (pipe(fdpipe) == -1) {
                perror("pipe failed");
                exit(1);
            }
            fdout = fdpipe[1]; 
            fdin = fdpipe[0];  
        }

        // Fork Proces
        pid = fork();
        if (pid == 0) {

            dup2(fdout, 1); 
            close(fdout);

            if (_errFile != NULL) {
                int err_flags;

              
                // O_WRONLY: Open for Writing-Only.
                // O_CREAT:  Create the file if it doesn't exist.
                err_flags = O_WRONLY | O_CREAT;

              
                if (_append) {
                    // set by >>&, add the Append flag.
                    err_flags = err_flags | O_APPEND;
                } else {
                    // et by 2>, add the Truncate flag to overwrite.
                    err_flags = err_flags | O_TRUNC;
                }
                int fderr = open(_errFile, err_flags, 0666);
                if (fderr < 0) {
                    perror("open errFile failed");
                    exit(1);
                }
                dup2(fderr, 2); 
                close(fderr);
            } else if (_out_error) {
                // Handle >>&
                dup2(fdout, 2); 
            }

    
            execvp(_simpleCommands[i]->_arguments[0],_simpleCommands[i]->_arguments);
            

            perror("execvp failed");
            exit(1);

        } else if (pid < 0) {
            perror("fork failed");
            exit(1);
        }

        close(fdout); 
    }

    if (_background == 0) {
        waitpid(pid, &status, 0);
    }

    clear();
        
}

void Command::prompt()
{
    printf("myshell>");
    fflush(stdout);
    std::string input;
    while (true)
    {
        
        if (!std::getline(std::cin, input)) {
            // Print a newline for a clean prompt return if interactive
            printf("\n");
            break;
        }

        std::vector<Token> tokens = tokenize(input);
        parse(tokens);
    }
}
void on_child_exit(int signum) {
   
    const char msg[] = "A child process has terminated.\n";
    int fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0) {
        write(fd, msg, sizeof(msg) - 1);
        close(fd);
    }

    // Reap all dead children
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}
Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main()
{
    signal(SIGINT, SIG_IGN);
    signal(SIGCHLD, on_child_exit);
    Command::_currentCommand.prompt();
    return 0;
}
