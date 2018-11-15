/* memory safe string operation 
 * Avinfors, O.Nikitin 11.12.2018
 *
 * Упоротость и отвага!
*/

#include "string2.h"
#include <stdlib.h>
#include <string.h>

int strinit2(_string2_t **dst, int size)
{
	*dst = (_string2_t*)malloc(sizeof(_string2_t));
	(*dst)->buff = (char*)malloc(size + 2);
	(*dst)->buffLen = size;
	(*dst)->buffLenInc = STRING2_BUFF_INITIAL_SIZE;
	(*dst)->strLen = 0;

	return size;
}

int strcpy2(_string2_t **dst, const char *src)
{
	int srcLen = strlen(src);

	if((*dst)->buffLen < srcLen) {
		(*dst)->buff = (char*)realloc((*dst)->buff, srcLen + 2);
		(*dst)->buffLen = srcLen;
	}
	memcpy((*dst)->buff, src, srcLen);
	(*dst)->buff[srcLen] = 0;
	(*dst)->strLen = srcLen;

	return srcLen;
}

int strncat2(_string2_t **dst, const char *src, int srcLen, unsigned char fill)
{
	int		buffLenInc;

	if(((*dst)->buffLen - (*dst)->strLen) < srcLen) {
			buffLenInc = (srcLen > (*dst)->buffLenInc) ? srcLen : (*dst)->buffLenInc;
			(*dst)->buffLenInc <<= 1;
			(*dst)->buffLen += buffLenInc;
			(*dst)->buff = (char*)realloc((*dst)->buff, (*dst)->buffLen);
		}

	if(fill == 0) {
		memcpy((*dst)->buff + (*dst)->strLen, src, srcLen);
	} else {
		memset((*dst)->buff + (*dst)->strLen, fill, srcLen);
	}
	(*dst)->strLen += srcLen;
	(*dst)->buff[(*dst)->strLen] = 0;

	return srcLen;
}

int strcat2(_string2_t **dst, const char *src)
{
	int		srcLen = strlen(src);
	return strncat2(dst, src, srcLen, 0);
}

void strfree2(_string2_t **dst)
{
	free((*dst)->buff);
	free(*dst);
}

