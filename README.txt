Creates a small shell program in C that directly handles certain commands (exit, status, cd), and uses fork() to spawn child processes (which call exec()) to handle all others.

To compile, you can:
1. Use the make command in the root directory
2. Run in the following command: gcc -std=gnu99 -g -Wall main.c -o smallsh
