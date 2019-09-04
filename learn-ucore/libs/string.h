#ifndef __LIBS_STRING_H__
#define __LIBS_STRING_H__

#include "defs.h"

size_t strlen(const char *s);
size_t strnlen(const char *s, size_t len);

char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t len);
char *strcat(char *dst, const char *src);
char *strdup(const char *src);
char *stradd(const char *src1, const char *src2);

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

char *strchr(const char *s, char c);
char *strfind(const char *s, char c);
long strtol(const char *s, char **endptr, int base);
char *strtok(char *s, const char *demial);

void *memset(void *s, char c, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
int memcmp(const void *v1, const void *v2, size_t n);
void bzero(void *s, size_t n);

char *index(char *, char);
int	atoi(const char *);

int stricmp(const char *s1, const char *s2);

#define	BLKEQU(b1, b2, len)	(!memcmp((b1), (b2), len))
#define blkcopy  memcpy

bool blkequ(void* first, void* second, int nbytes);

#define	isodd(x)	(01&(int)(x))

#endif /* !__LIBS_STRING_H__ */

