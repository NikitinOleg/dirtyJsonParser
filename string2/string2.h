#ifndef __string2_h
#define __string2_h

#define		STRING2_BUFF_INITIAL_SIZE	(int)		1024

typedef struct
{
	char		*buff;			// буфер
	int			buffLen;		// текущий размер буфера
	int			buffLenInc;		// текущий инкримент для буфера
	int			strLen;			// текущий размер строки
} _string2_t;

int strinit2(_string2_t **dst, int size);
int strcpy2(_string2_t **dst, const char *src);
int strncat2(_string2_t **dst, const char *src, int srcLen, unsigned char fill);
int strcat2(_string2_t **dst, const char *src);
void strfree2(_string2_t **dst);

#endif