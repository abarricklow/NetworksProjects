# Allison Barricklow
# CSCI 4245
# Assign P1
# This file uses is a makefile to compile and run using GCC compiler

# Use compiler gcc
CC = gcc

# Show compiler errors
CFLAGS = -Wall

# C code to be compiled and run
TARGET = email_sender

# Default rule
# This takes p1 and compiles it using gcc
# with the added flag of showing compiler errors
# Only rebuilds the file if p1 has changed since the last run
$(TARGET): email_sender.c
	$(CC) $(CFLAGS) -o $(TARGET) email_sender.c