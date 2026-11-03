#ifndef TOY_H
#define TOY_H
#include <stddef.h>
/* Uppercases ASCII in place; returns the number of bytes changed. */
size_t toy_upper(char *buf, size_t len);
#endif
