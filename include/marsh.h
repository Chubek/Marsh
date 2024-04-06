#ifndef MARSH_H
#define MARSH_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef String String;
typedef Arena Arena;
typedef ProcessIO ProcessIO;

void ProcessIO_init(ProcessIO* pio, struct Arena* arena);
void ProcessIO_redirect_in(ProcessIO* pio, String* file_path);
void ProcessIO_redirect_out(ProcessIO* pio, String* file_path);
void ProcessIO_redirect_err(ProcessIO* pio, String* file_path);
void ProcessIO_cleanup(ProcessIO* pio); 









#endif
