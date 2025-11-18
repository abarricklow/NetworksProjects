# Allison Barricklow
# CSCI 4245
# Assign P2
# This file uses is a makefile to compile and run using G++ compiler

# Use compiler g++
CC = g++

# Show compiler errors
CFLAGS = -Wall

# C code to be compiled and run
TARGET = httpserver

# Default rule
# This takes p2 and compiles it using g++
# with the added flag of showing compiler errors
# Only rebuilds the file if p2 has changed since the last run
$(TARGET): httpserver.cpp
	$(CC) $(CFLAGS) -o $(TARGET) httpserver.cpp