#ifndef __json_h
#define __json_h

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../string2/string2.h"

// Описание ошибок
#define				JSON_ERR_NO_ERROR				(int)	0
#define				JSON_ERR_OBJ_IN_OBJ				(int)	1
#define				JSON_ERR_SINGLE_KEY				(int)	2
#define				JSON_ERR_UNEXPECTED_END			(int)	3
#define				JSON_ERR_STRING_WITHOUT_QUOTA	(int)	4
#define				JSON_ERR_ILLEGAL_SYMBOL			(int)	5
#define				JSON_ERR_UNEXPECTED_SYMBOL		(int)	6

// типы кавычек
typedef enum {
	JSON_QUOTA_SINGLE = 1,
	JSON_QUOTA_DOUBLE = 2
} _jsonQuota_t;

// примитивы json'а (объект, массив, ключ, значение)
typedef enum {
	JSON_OBJECT = 1,
	JSON_ARRAY = 2,
	JSON_KEY = 3,
	JSON_VALUE = 4
} _jsonType_t;

// типы значений
typedef enum {
	JSON_VALUE_NOT_VALUE = 0,
	JSON_VALUE_NULL = 1,
	JSON_VALUE_BOOL = 2,
	JSON_VALUE_INT = 3,
	JSON_VALUE_FLOAT = 4,
	JSON_VALUE_STRING = 5
} _jsonValueType_t;

// элементы (узлы) разбираемого json'a
typedef struct
{
	int					id;				// ID токена, используется для отладки и отрисовки деревьев
	int					start;			// начало наименования токена
	int					end;			// конец наименования токена
	int					parent;			// индекс родителя
	int					fChild;			// индекс первого дочернего узла
	int					lChild;			// индекс последнего дочернего узла
	int					nextToken;		// следующий по порядку токен этого же уровня
	_jsonType_t			type;			// тип токена
	_jsonValueType_t	valueType;		// тип значения (только для type == JSON_VALUE!)
} _jsonToken_t;

// описание ошибки
typedef struct
{
	int				code;
	int				line;
	int				col;
	const char		*message;
} _jsonErr_t;

// json-объект
typedef struct
{
	const char		*json;			// исходный json
	_jsonToken_t	*token;
	int				count;
	int				nesting;
} _jsonObj_t;

int					jsonParser(char *str, _jsonObj_t **jsonObj, unsigned int jsonLen);
_jsonToken_t*		assignNewToken(_jsonObj_t **jsonObj, int *expectTokenCount, int pos, int parent);
void				clearFlatJsonObj(_jsonObj_t **jsonObj);
_jsonToken_t*		xPath(const char *path, _jsonObj_t *jsonObj);
char*				getJsonStr(const char *key, _jsonObj_t *jsonObj);
long long			getJsonInt(const char *key, _jsonObj_t *jsonObj);
long double			getJsonDouble(const char *key, _jsonObj_t *jsonObj);

void jsonFormat(_jsonObj_t *jsonObj);
void setError(int line, int col, char ch, int errNum, int **parent);
_jsonErr_t* getLastError();

void tokenRecursive(_jsonObj_t *jsonObj, _jsonToken_t *token, int level, int leftKey);
void processToken(_jsonObj_t *jsonObj, _jsonToken_t *token, int level, int leftKey);

char* jsonAsString(_jsonObj_t *jsonObj);
void processToken1(_jsonObj_t *jsonObj, _jsonToken_t *token, _string2_t **res, int level, int leftKey, bool nextToken);
void tokenRecursive1(_jsonObj_t *jsonObj, _jsonToken_t *token, _string2_t **res, int level, int leftKey, bool nextToken);

#endif