# myshell
A simple Bash shell for Ubuntu 16.04 built for an operating systems class. This project was used to learn basic system calls and how to handle basic process creation, management, and termination. Also serves as a short introduction to signal handling.

## Features
* Stores history for the last 10 given commands
* Displays command history on SIGINT signal and using 'history' command
* Launches processes for valid commands while rejecting invalid ones
* Ignores empty inputs, bypassing command storage
* Implements custom `exit`, `pwd`, `cd`, and `type` commands

## How to Use

To launch the program, navigate to the Makefile directory and run the following commands:

```bash
make
./shell
```

During runtime, `pwd`, `cd`, and `type` all work the same as in regular Bash. The `exit` command can be used to quit the `myshell` program. Also, sending a `SIGINT` command with `CTRL+C` or typing `history` will give a history of the last ten commands.
