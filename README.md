---

# C++ Mini-Shell

This project is a simple, custom-built Unix shell implemented in **C++** for an operating systems lab. It demonstrates core OS concepts including **process creation (`fork`)**, **command execution (`execvp`)**, **signal handling**, **file I/O redirection**, and **inter-process communication (pipes)**.

---

## Features

This shell supports a wide range of features found in standard shells like bash:

### Command Execution

Runs any external command found in your system's `PATH` (e.g., `ls`, `grep`, `wc`).

### Piping (`|`)

Chains multiple commands together, piping the output of one command to the input of the next.
**Example:**

```bash
ls -l | grep .cpp | wc -l
```

### File I/O Redirection

* **Output (`>`)**: Redirects standard output to a file, overwriting the file.
* **Input (`<`)**: Reads standard input from a file.
* **Append (`>>`)**: Redirects standard output to a file, appending to the end of it.
* **Error (`2>`)**: Redirects standard error to a file, overwriting.
* **Append Error & Output (`>>&`)**: Appends both standard output and standard error to the same file.

### Background Processes (`&`)

Runs a command in the background, allowing the shell to accept new commands immediately.
**Example:**

```bash
sleep 10 &
```

### Built-in Commands

* **exit**: Terminates the mini-shell.
* **cd [dir]**: Changes the current working directory. If `[dir]` is omitted, it changes to the `HOME` directory.

### Signal Handling

* **SIGINT (Ctrl-C)**: The shell itself ignores this signal, so Ctrl-C only terminates a running child process, not the shell.
* **SIGCHLD**: Automatically detects when a child process (including background ones) terminates, logs the event, and "reaps" the child process to prevent zombies.

### Wildcard Expansion

Supports `*` and `?` wildcards for file matching in command arguments (e.g., `ls *.txt`).

### Process Termination Logging

Logs an entry to `log.txt` every time a child process finishes.

---

## Dependencies

To build and run this project, you will need a standard Unix-like environment (Linux, macOS) with the following tools:

* **g++** (or **clang++**): A C++ compiler that supports the C++11 standard.
* **make**: The build automation tool.
* **Standard C/C++ Libraries**: `stdio`, `stdlib`, `unistd`, `sys/wait`, `fcntl`, `signal`, `glob`, etc.

---

## How to Build and Run

### Clone the repository

```bash
git clone <your-repo-url>
cd <your-repo-directory>
```

### Compile the project

Simply run the make command, which will use the provided Makefile.

```bash
make
```

This creates a single executable file named **myshell**.

### Run the shell

```bash
./myshell
```

You will be greeted with the `myshell>` prompt.

### Clean up

To remove the compiled executable and object files:

```bash
make clean
```

This also removes the `log.txt` file.

---

## File Structure

* **command.h / command.cc**: Contains the core logic.
  Defines the `SimpleCommand` and `Command` data structures.
  `parse()`: The parser that translates tokens into the Command struct.
  `execute()`: The executor that handles built-ins, fork, pipe, dup2, and execvp.

* **main(), prompt(), on_child_exit()**: The main program loop and signal handlers.

* **tokenizer.h / tokenizer.cc**: (Provided) Handles splitting the raw user input string into a list of Token objects.

* **Makefile**: The build script. Compiles and links all `.cc` files into the `myshell` executable.

* **log.txt**: (Generated at runtime) A log file where all SIGCHLD events (child terminations) are recorded.

---

## How It Works (High-Level)

The shell operates on a classic **Read-Eval-Print Loop (REPL)**:

### Read

`prompt()` reads a line of input from the user using `std::getline`.

### Parse (Eval Part 1)

`parse()` receives the list of tokens from `tokenize()`. It iterates through this list, populating a single static Command object.

* `|` creates a new SimpleCommand in the Command's list.
* `>`, `<`, `>>`, etc., set the file redirection and append/error flags on the Command object.
* `&` sets the `_background` flag.
* `*.txt` is expanded using `glob()` into a list of arguments.

### Execute (Eval Part 2)

`execute()` is called.

* First, it checks for built-in commands (`exit`, `cd`) and runs them directly in the parent (shell) process.
* If it's not a built-in, it loops through the list of SimpleCommands.
* It uses `dup2()` to set the correct input (`fdin`) for the current command (which is either `stdin` or the read-end of the last pipe).
* It determines the output (`fdout`). If it's the last command, `fdout` is `stdout` or the `_outFile`. If it's not the last command, `pipe()` is called, and `fdout` is set to the write-end of the new pipe.
* It calls `fork()` to create a child process.

  * **Child Process**: The child redirects its `stdout` to `fdout` and `stderr` (if needed), then calls `execvp()`. `execvp` replaces the child process with the command to be run.
  * **Parent Process**: The parent closes its end of the pipe and loops to the next command. After the loop, it calls `waitpid()` to wait for the last child to finish (unless it's a background job).

### Loop

`execute()` returns, and the `while(true)` loop in `prompt()` starts over, displaying the prompt again.

---
