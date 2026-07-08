#ifndef LOGGER_H
#define LOGGER_H

void log_init(const char* filename);
void log_event(const char* format, ...);
void log_close();

#endif