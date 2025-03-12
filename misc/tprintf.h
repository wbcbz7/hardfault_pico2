#pragma once
#include <stdint.h>
#include <stdio.h>

int tputchar(int x, int y, int ch);
int tputstr(int x, int y, const char *str);
int tprintf(int x, int y, const char *format, ...);
