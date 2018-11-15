/* single pass dirty json parser
 * Avinfors, O.Nikitin 10.04.2015, 08.11.2018
 *
 * Упоротость и отвага!
*/

/* Примеры вызова: см в ./test/jstest.c
*/

#include "json.h"
#include <stdio.h>

const char* JSON_ERROR_LIST[] = {
	"No error",
	"Object in object",
	"Key without value",		// ACHTUNG! В случае последнего элемента в объекте позицию "}"!
	"Unexpected end of json",
	"String value not in quotas",
	"Illegal symbol",			// not in [0-9, -, .], повторная точка в числе
	"Unexpected symbol"
};

_jsonErr_t	error;
char		cErr[256];
_string2_t	*jsonString = NULL;


/* основная функция парсера
 * str				IN  строка содержащая json
 * jsonObj			OUT неинициализированный указатель на _jsonObj_t
 *
 * возврат:
 * 0 - успех
 * 1 - invalid json
 *
 * Идеология:
 * Парсер формирует полную древовидную структуру json'а в оперативной памяти (технология DOM)
 * Разбор строки и формирование DOM модели json'а выполняется за один проход по строке json'а.
 * Токен - минимальная составная единица json-строки (исключая запятые, пробелы, табуляторы, символы переноса строк). 
 * Токеном может быть либо какой-либо объект/массив/ключ/значение ключа/значение массива
 *  Пример: {"a":"b","c":{"d":["e","f"]}}. Здесь токены: a, b, c, d, e, f и все скобки { и [, которые задают объект или массив
 * json-строка разбирается на токены последовательно, дочерние токены ВСЕГДА будут следовать за своими родителями
 * т.е. дочерние токены всегда будут иметь больший индекс в массиве токенов.
 * Массив токенов представлен в виде линейного блока памяти, состоящих из последовательных структур типа _jsonToken_t
 * Каждый токен имеет следующие атрибуты:
 *  - ID (порядковый номер в общем массиве токенов)
 *  - имя (индексы начала и конца имени в json строке)
 *  - тип токена (объект, массив, ключ, значение
 *  - тип значения. Используется только с токенами, имеющими тип "значение"
 *  - ссылка на индекс родителя
 *  - ссылка на индекс первого дочернего токена
 *  - ссылка на индекс первого дочернего токена
 *  - ссылка на индекс последнего дочернего токена (используется только при разборе json'а)
 *  - ссылка на индекс следующего токена такого же уровня вложенности (ссылка на соседа справа)
 *
 * Обрабатываемые ошибки в json'е
 *  - объекты в объектах (без ключа)
 *  - появление JSON_VALUE, если родитель не JSON_ARRAY и не JSON_KEY
 *  - level (текущий уровень вложенности) > 0 после парсинга всего объекта
 *  - строковое значение не в кавычках (только для JSON_VALUE)
 *  - числовое значение, содержащее символ не из множества [0-9, -, .]
 *  - на неожиданный символ (много чего, см https://www.json.org/)
 *  Левые символы после " или ' в JSON_VALUE пока покрываются другими (описанными выше) ошибками
 *  При возникновении ошибки формируется сообщения об ошибке: строка, стролбец описание ошибки
 *  его можно получить вызвав getLastError()
 *
*/
int jsonParser(char *str, _jsonObj_t **jsonObj, unsigned int jsonLen)
{
	// start - признак того, что мы находимся внутри имени токена, или внутри его значения
	bool				inQuotes = false, start = false;
	unsigned int		i, len;
	int					maxNesting = 0, level = 0, parentCount = 8;		// первоначально предполагаем глубину вложенности не более 8
	int					*parent = (int*)malloc(sizeof(int*) * parentCount);		// массив указателей индексов родительских токенов
	int					line = 1, col = 0, expectTokenCount;
	char				lastControlSymbol;
	_jsonToken_t		*token;
	_jsonQuota_t		quotaType;

	len = (jsonLen == 0) ? strlen(str) : jsonLen;

	expectTokenCount = (len < 2048) ? 64 : (len >> 4);		// ожидаемое кол-во токенов в json'е (считаем, что токен в среднем 16 байт)

	*jsonObj = (_jsonObj_t*)malloc(sizeof(_jsonObj_t));
	(*jsonObj)->count = (*jsonObj)->nesting = 0;
	token = (*jsonObj)->token = (_jsonToken_t*)malloc(sizeof(_jsonToken_t) * expectTokenCount);
	(*jsonObj)->token->start = (*jsonObj)->token->end = 0;
	(*jsonObj)->token->id = 0;
	(*jsonObj)->token->parent = 0;
	(*jsonObj)->token->fChild = 0;
	(*jsonObj)->token->lChild = 0;
	(*jsonObj)->token->nextToken = 0;
	(*jsonObj)->token->valueType = 0;

	(*jsonObj)->json = str;
	parent[0] = 0;

	for(i=0; i<len; i++) {
		col++;
		switch(str[i]) {
			// исключение комментариев
			case '/':
				if(inQuotes) {
					break;
				}
				if(i > 0) {
					if(str[i-1] == '\\') {
						break;
					}
				}
				if(str[i+1] == '/') {
					// однострочный
					i+=2;
					while((str[i] != '\r') && (str[i] != '\n')) {
						// не вышли из комментария!
						if(i == len)
							return 1;
						i++;
					}
					col = 0;
					break;
				}
				if(str[i+1] == '*') {
					// многострочный
					i+=2;
					while((str[i] != '*') || (str[i+1] != '/')) {
						// не вышли из комментария!
						if(i == len)
							return 1;
						i++;
						if(str[i] == '\n') {
							line++;
							col = 0;
						} else {
							col++;
						}
					}
					i++;
					break;
				}
				// подавление "warning: this statement may fall through [-Wimplicit-fallthrough=]"
				__attribute__ ((fallthrough));

			case '"':
				if(str[i-1] != '\\') {
					if(!inQuotes) {
						quotaType = JSON_QUOTA_DOUBLE;
						inQuotes = !inQuotes;
					} else if(quotaType == JSON_QUOTA_DOUBLE) {
						// обработка пустых кавычек
						if((str[i-1] == '\"') && (str[i-2] != '\\')) {
							token = assignNewToken(jsonObj, &expectTokenCount, i, parent[level]);
							token->type = JSON_VALUE;
							lastControlSymbol = 0;
							token->valueType = JSON_VALUE_STRING;
						}
						start = false;
						token->end = i;
						inQuotes = !inQuotes;
					} else if((quotaType == JSON_QUOTA_SINGLE) && (start == false)) {
						// обработка варианта '"..... (двойная сразу после одинарной)
						start = true;
						token = assignNewToken(jsonObj, &expectTokenCount, i, parent[level]);
						token->type = JSON_VALUE;
						lastControlSymbol = 0;
						token->valueType = JSON_VALUE_STRING;
					}
				}
				break;
			case '\'':
				if(str[i-1] != '\\') {
					if(!inQuotes) {
						quotaType = JSON_QUOTA_SINGLE;
						inQuotes = !inQuotes;
					} else if(quotaType == JSON_QUOTA_SINGLE) {
						// обработка пустых кавычек
						if((str[i-1] == '\'') && (str[i-2] != '\\')) {
							token = assignNewToken(jsonObj, &expectTokenCount, i, parent[level]);
							token->type = JSON_VALUE;
							lastControlSymbol = 0;
							token->valueType = JSON_VALUE_STRING;
						}
						start = false;
						token->end = i;
						inQuotes = !inQuotes;
					} else if((quotaType == JSON_QUOTA_DOUBLE) && (start == false)) {
						// обработка варианта "'..... (одинарная сразу после двойной)
						start = true;
						token = assignNewToken(jsonObj, &expectTokenCount, i, parent[level]);
						token->type = JSON_VALUE;
						lastControlSymbol = 0;
						token->valueType = JSON_VALUE_STRING;
					}
				}
				break;

			// пропускаемые символы (если не в кавычках)
			case ' ': case '\t': case '\r': case '\n':
				if(str[i] == '\n') {
					line++;
					col = 0;
				}
				if((!inQuotes) && (start)) {
					start = false;
					token->end = i;
				}
				// если в кавычках - продолжаем обработку (проваливаемся в default)
				if(!inQuotes) {
					break;
				}
				// подавление "warning: this statement may fall through [-Wimplicit-fallthrough=]"
				__attribute__ ((fallthrough));

			// управляющие символы:
			case '[': case '{': case ':': case ',': case '}': case ']':
				if(!inQuotes) {
					start = false;
					// ВАЛИДАЦИЯ: неожиданный символ (JSON_ERR_UNEXPECTED_SYMBOL)
					if(
						((lastControlSymbol == '{') && ((str[i] == ']') || (str[i] == ':') || (str[i] == ','))) ||	// {	[ "]",":","," ]
						((lastControlSymbol == '[') && ((str[i] == '}') || (str[i] == ':') || (str[i] == ','))) ||	// [	[ "}",":","," ]
						(((lastControlSymbol == ':') || (lastControlSymbol == ',')) && ((str[i] == ':') || (str[i] == ','))))	// :	: , и ,	: ,
					{
						setError(line, col, str[i], JSON_ERR_UNEXPECTED_SYMBOL, &parent);
						return 1;
					}
					lastControlSymbol = str[i];

					switch(str[i]) {
						case '[': case '{':
							if(level > 0) {
								// ВАЛИДАЦИЯ: объекты в объектах (JSON_ERR_OBJ_IN_OBJ)
								if(str[i] == '{') {
									if(token->type != JSON_KEY) {
										if(((*jsonObj)->token + parent[level])->type == JSON_OBJECT) {
											setError(line, col, '.', JSON_ERR_OBJ_IN_OBJ, &parent);
											return 1;
										}
									}
								}
								// объекты и массивы
								token = assignNewToken(jsonObj, &expectTokenCount, i, parent[level]);
							}
							token->type = (str[i]=='{')? JSON_OBJECT:JSON_ARRAY;
							token->start = i;
							token->end = i+1;
							level++;
							if(parentCount == level)
								parent = (int*)realloc(parent, sizeof(int*) * (parentCount <<= 1));
							parent[level] = (*jsonObj)->count;
							if(level > maxNesting)
								maxNesting = level;
							break;
						case ':': case ',': case '}': case ']':
							// ВАЛИДАЦИЯ: Ключ без значения (JSON_ERR_SINGLE_KEY)
							if((str[i] == '}') || (str[i] == ',')) {
								if((token->type == JSON_KEY) && (((*jsonObj)->token + token->parent)->type == JSON_OBJECT)) {
									setError(line, col, '.', JSON_ERR_SINGLE_KEY, &parent);
									return 1;
								}
							}
							// ВАЛИДАЦИЯ: В массиве не может быть ":" (JSON_ERR_UNEXPECTED_SYMBOL)
							if((str[i] == ':') && (((*jsonObj)->token + token->parent)->type == JSON_ARRAY)) {
								setError(line, col, ':', JSON_ERR_UNEXPECTED_SYMBOL, &parent);
								return 1;
							}
							if(token->end == 0) {
								token->end = i;
							}
							if((str[i] == '}') || (str[i] == ']')) {
								// Проверка на корректное закрытие массива/объекта
								_jsonType_t ParentType = ((*jsonObj)->token + parent[level])->type;
								if(
									((ParentType == JSON_OBJECT) && (str[i] == ']')) ||
									((ParentType == JSON_ARRAY) && (str[i] == '}')))
								{
									setError(line, col, str[i], JSON_ERR_UNEXPECTED_SYMBOL, &parent);
									return 1;
								}
								level--;
							}
							break;
					}
					break;
				}
				// если inQuotes, то проваливаемся в default
				// подавление "warning: this statement may fall through [-Wimplicit-fallthrough=]"
				__attribute__ ((fallthrough));
			// все остальный символы
			default:
				// Обработка ключей и значений
				if(!start) {
					start = true;
					token = assignNewToken(jsonObj, &expectTokenCount, i, parent[level]);
					// Ключ может быть только в объектах!
					token->type = (((*jsonObj)->token + token->parent)->type == JSON_OBJECT) ? JSON_KEY : JSON_VALUE;

					if(!inQuotes) {
						// ВАЛИДАЦИЯ: неожиданный символ (JSON_ERR_UNEXPECTED_SYMBOL)
						if(((lastControlSymbol == ']') || (lastControlSymbol == '}')) && ((str[i] != ']') && (str[i] != '}'))) {
							// для ] и }	неожиданно всё, кроме ] и }
							setError(line, col, str[i], JSON_ERR_UNEXPECTED_SYMBOL, &parent);
							return 1;
						}
						lastControlSymbol = 0;

						// обработка значений свойств (типизирование и валидация)
						if(token->type == JSON_VALUE) {
							if(
								((str[i] >= '0') && (str[i] <= '9')) ||
								((str[i] == '-') && (str[i+1] >= '0') && (str[i+1] <= '9'))
							) {
								// число
								i++;
								col++;
								if(str[i] == '-') {
									i++;
									col++;
								}
								token->valueType = JSON_VALUE_INT;
								while (((str[i] >= '0') && (str[i] <= '9')) || (str[i] == '.')) {
									if(str[i] == '.') {
										if(token->valueType == JSON_VALUE_INT) {
											token->valueType = JSON_VALUE_FLOAT;
										} else {
											// Проверка на вторую "." в числе
											setError(line, col, str[i], JSON_ERR_ILLEGAL_SYMBOL, &parent);
											return 1;
										}
									}
									i++;
									col++;
								}
								// ВАЛИДАЦИЯ: числовое значение содержит символ не из множества [0-9, -, .] (JSON_ERR_ILLEGAL_SYMBOL)
								if( (str[i] == ',') ||
									(str[i] == '\n') ||
									(str[i] == ']') ||
									(str[i] == '}') ||
									(str[i] == ' ') ||
									(str[i] == '\r') ||
									(str[i] == '\t') ||
									((str[i] == '/') && ((str[i+1] == '/') || (str[i+1] == '*')))
									)
								{
									token->end = i;
									start = false;
									lastControlSymbol = 0;
									i--;
									col--;
									break;
								} else {
									setError(line, col, str[i], JSON_ERR_ILLEGAL_SYMBOL, &parent);
									return 1;
								}
							} else if(
								// null
								(str[i] == 'n') && (str[i+1] == 'u') && (str[i+2] == 'l') && (str[i+3] == 'l') &&
								(	(str[i+4] == ',') ||
									(str[i+4] == ']') ||
									(str[i+4] == '}') ||
									(str[i+4] == '\n') ||
									(str[i+4] == ' ') ||
									(str[i+4] == '\r') ||
									(str[i+4] == '\t') ||
									((str[i+4] == '/') && ((str[i+5] == '/') || (str[i+5] == '*')))
								))
							{
								token->valueType = JSON_VALUE_NULL;
								token->start = i;
								i += 3;
								col += 3;
								token->end = i+1;
								start = false;
								lastControlSymbol = 0;
								break;
							} else if(
								// true
								(str[i] == 't') && (str[i+1] == 'r') && (str[i+2] == 'u') && (str[i+3] == 'e') &&
								(	(str[i+4] == ',') ||
									(str[i+4] == ']') ||
									(str[i+4] == '}') ||
									(str[i+4] == '\n') ||
									(str[i+4] == ' ') ||
									(str[i+4] == '\r') ||
									(str[i+4] == '\t') ||
									((str[i+4] == '/') && ((str[i+5] == '/') || (str[i+5] == '*')))
								))
							{
								token->valueType = JSON_VALUE_BOOL;
								token->start = i;
								i += 3;
								col += 3;
								token->end = i+1;
								start = false;
								lastControlSymbol = 0;
								break;
							} else if(
								// false
								(str[i] == 'f') && (str[i+1] == 'a') && (str[i+2] == 'l') && (str[i+3] == 's') && (str[i+4] == 'e') &&
								(	(str[i+5] == ',') ||
									(str[i+5] == ']') ||
									(str[i+5] == '}') ||
									(str[i+5] == '\n') ||
									(str[i+5] == ' ') ||
									(str[i+5] == '\r') ||
									(str[i+5] == '\t') ||
									((str[i+5] == '/') && ((str[i+6] == '/') || (str[i+6] == '*')))
								))
							{
								token->valueType = JSON_VALUE_BOOL;
								token->start = i;
								i += 4;
								col += 4;
								token->end = i+1;
								start = false;
								lastControlSymbol = 0;
								break;
							} else {
								// ВАЛИДАЦИЯ: строковое значение без кавычек (JSON_ERR_STRING_WITHOUT_QUOTA)
								setError(line, col, '.', JSON_ERR_STRING_WITHOUT_QUOTA, &parent);
								return 1;
							}
						}
					} else {
						token->valueType = JSON_VALUE_STRING;
						lastControlSymbol = 0;
					}
				}
		}
	}
	// ВАЛИДАЦИЯ: Unexpected end of json (JSON_ERR_UNEXPECTED_END)
	if(level > 0) {
		setError(line, col, '.', JSON_ERR_UNEXPECTED_END, &parent);
		return 1;
	}

	free(parent);
	if((*jsonObj)->count > 0) {
		(*jsonObj)->count++;
		(*jsonObj)->nesting = maxNesting;
	}
	return 0;
}

/* перемещение указателя текущего элемента на новый элемент, или распределение памяти для порции новых элементов
 * внутренняя ф-ция
*/
_jsonToken_t* assignNewToken(_jsonObj_t **jsonObj, int *expectTokenCount, int pos, int parent)
{
	_jsonToken_t	*parentToken;	// родительский токен

	(*jsonObj)->count++;			// ID текущего токена
	if((*jsonObj)->count == *((int*)expectTokenCount)) {
		(*jsonObj)->token = (_jsonToken_t*)realloc((*jsonObj)->token, sizeof(_jsonToken_t) * (*((int*)expectTokenCount) <<= 1));
	}
	_jsonToken_t *token = ((*jsonObj)->token + (*jsonObj)->count);
	token->id = (*jsonObj)->count;	// ID текущего токена
	// если предыдущий токен - ключ, то это родитель
	// иначе, родителем может быть массив или объект
	token->parent = ((token-1)->type == JSON_KEY)?((*jsonObj)->count-1):parent;
	token->start = pos;
	token->end = 0;
	token->fChild = 0;
	token->lChild = 0;
	token->nextToken = 0;
	token->valueType = 0;

	parentToken = (*jsonObj)->token+token->parent;
	// сохранение id 1го дочернего элемента в родительском токене
	if(parentToken->fChild == 0) {
		parentToken->fChild = (*jsonObj)->count;
	}
	// сохранение id данного элемента в предыдущем элементе этого же уровня: parent->lChild
	// в parent->lChild безусловно пишем текущий элемент
	if(parentToken->lChild > 0) {
		((*jsonObj)->token+parentToken->lChild)->nextToken = (*jsonObj)->count;
	}
	parentToken->lChild = (*jsonObj)->count;

	return token;
}

void setError(int line, int col, char ch, int errNum, int **parent)
{
	char cCh[2];

	error.line = line;
	error.col = col-1;
	error.code = errNum;
	switch(errNum) {
		case JSON_ERR_UNEXPECTED_SYMBOL:
		case JSON_ERR_ILLEGAL_SYMBOL:
			cCh[0] = ch;
			cCh[1] = 0;
			strcpy(cErr, JSON_ERROR_LIST[errNum]);
			strcat(cErr, ": \"");
			strcat(cErr, cCh);
			strcat(cErr, "\"");
			error.message = cErr;
			break;
		default:
			error.message = JSON_ERROR_LIST[errNum];
	}
	free(*parent);
}

_jsonErr_t* getLastError()
{
	return &error;
}

// очистка занятой памяти, после того, как разобранный json уже не нужен
void clearFlatJsonObj(_jsonObj_t **jsonObj)
{
	if(jsonObj != NULL) {
		if((*jsonObj)->token != NULL) {
			free((*jsonObj)->token);
			(*jsonObj)->token = NULL;
		}
		free((*jsonObj));
		*jsonObj = NULL;
	}
}

/* получение значение элемента json'a по пути
 * внутренняя ф-ция
*/
_jsonToken_t* xPath(const char *path, _jsonObj_t *jsonObj)
{
	int				pathElem[jsonObj->nesting][2];		// [смещение, длина]
	int				len = strlen(path);
	int				i, pathCount = 0;
	_jsonToken_t	*token;

	if(jsonObj->nesting == 0)
		return (_jsonToken_t*) (0);

	pathElem[0][0] = pathElem[0][1] = 0;
	for(i=0; i<len; i++) {
		if(path[i] == '.') {
			pathElem[pathCount+1][0] = i+1;
			pathElem[++pathCount][1] = 0;
		}
		else
			pathElem[pathCount][1]++;
	}
	if(pathCount == 0) {
		pathElem[0][1] = len;
	}
	if((pathCount > jsonObj->nesting) || (pathElem[0][1] == 0))
		return (_jsonToken_t*) (0);						// ACHTUNG! нормальное описание ошибки !!!

	pathCount++;
	token = jsonObj->token + jsonObj->token->fChild;

	for(i=0; i<pathCount; i++) {
		// Ищем начиная с первого дочернего токена корневого элемента
		if((token->end - token->start) == pathElem[i][1]) {
			while (strncmp(path + pathElem[i][0], jsonObj->json + token->start, pathElem[i][1]) != 0) {
				if(token->nextToken > 0) {
					token = jsonObj->token + token->nextToken;
					if(token->type == JSON_OBJECT) {
						token = jsonObj->token + token->fChild;
					}
				} else {
					return (_jsonToken_t*)(0);			// ACHTUNG! нормальное описание ошибки !!!
				}
			}

			if(i == (pathCount-1)) {
				token = jsonObj->token + token->fChild;
				if(token->type == JSON_OBJECT) {
					token = jsonObj->token + token->fChild;
				}
				if(token->type == JSON_VALUE) {
					return token;
				} else {
					return (_jsonToken_t*)(0);			// ACHTUNG! нормальное описание ошибки !!!
				}
			} else {
				token = jsonObj->token + token->fChild;
				if(token->type == JSON_OBJECT) {
					token = jsonObj->token + token->fChild;
				}
				continue;
			}
		}
	}
	return (_jsonToken_t*) (0);						// ACHTUNG! нормальное описание ошибки !!!
}

/* получение указателя на значение элемента json'a по пути
 * внутренняя ф-ция
*/
void* getJsonVal(const char *key, _jsonObj_t *jsonObj) {
	void	*value;
	int		len;

	_jsonToken_t *token = xPath(key, jsonObj);
	if(token != (_jsonToken_t*)(0)) {
		len = token->end - token->start;
		value = (char*)malloc(len+4);
		strncpy(value, jsonObj->json + token->start, len);
		((char*)value)[len] = 0;
		return value;
	}
	return NULL;
}

/* получение строкового значение элемента json'a
 * json				cтрока содержащая путь к элементу, значение которого необходимо вернуть в виде СТРОКИ
 * jsonObj			указатель на _jsonObj_t, возвращённый предварительным вызовом jsonParser
 * return:			char*
 * После использования необходимо из клиентской программы освободить возвращённое значение!
*/
char* getJsonStr(const char *key, _jsonObj_t *jsonObj)
{
	return (char*)getJsonVal(key, jsonObj);
}

/* получение целочисленного значение элемента json'a
 * json				строка содержащая путь к элементу, значение которого необходимо вернуть в виде ЦЕЛОГО ЧИСЛА
 * jsonObj			указатель на _jsonObj_t, возвращённый предварительным вызовом jsonParser
 * return:			long long
 * КОСЯК! Если значение не найдено, то нельзя вернуть NULL!
*/
long long getJsonInt(const char *key, _jsonObj_t *jsonObj)
{
	char		*p = NULL;
	long long	ret;
	p = (char*)getJsonVal(key, jsonObj);
	ret = (p == NULL)?0:atoll(p);
	free(p);
	return ret;
}

/* получение вещественного значение элемента json'a
 * json				строка содержащая путь к элементу, значение которого необходимо вернуть в виде ЧИСЛА C ПЛАВАЮЩЕЙ ТОЧКОЙ
 * jsonObj			указатель на _jsonObj_t, возвращённый предварительным вызовом jsonParser
 * return:			long double
 * КОСЯК! Если значение не найдено, то нельзя вернуть NULL!
*/
long double getJsonDouble(const char *key, _jsonObj_t *jsonObj)
{
	char		*p = NULL;
	long double	ret;
	p = (char*)getJsonVal(key, jsonObj);
	ret = (p == NULL)?0:strtold(p, NULL);
	free(p);
	return ret;
}

/* Полезняшки и отладка
 */

void jsonFormat(_jsonObj_t *jsonObj)
{
	/*
	int		j, parentToken = 0;
	char	name[128];
	*/

	// таблица по всем токенам
	/*
	for(j=parentToken; j<jsonObj->count; j++) {
		name[(jsonObj->token + j)->end - (jsonObj->token + j)->start] = 0;
		snprintf(name, (jsonObj->token + j)->end - (jsonObj->token + j)->start + 1, "%s", (char*)(jsonObj->json+(jsonObj->token + j)->start));
		printf("Id %d pId %d cId %d nId %d name %s type %d\n",
			(jsonObj->token + j)->id, (jsonObj->token + j)->parent, (jsonObj->token + j)->fChild, (jsonObj->token + j)->nextToken, 
			name, (jsonObj->token + j)->type);
	}
	*/

	// дерево токенов
	printf("\n\n");
	tokenRecursive(jsonObj, jsonObj->token, 0, 0);
	printf("\n\n");
}

/* рекурсивный обход токенов для построения дерева
*/
void tokenRecursive(_jsonObj_t *jsonObj, _jsonToken_t *token, int level, int leftKey)
{
	processToken(jsonObj, token, level, leftKey);

	// токены того же уровня обходим без рекурсии
	// если у обрабатываемого токена того же уровня есть дочерние токены - тогда рекурсия
	while(token->nextToken > 0) {
		token = jsonObj->token + token->nextToken;
		processToken(jsonObj, token, level, leftKey);
	}
}

/* рисование одного токена в дереве
 * level - глубина вложенности для отступа при отрисовке дерева
 * type - признак того, что рисуем дерево, а не таблицу
 * leftKey - признак того, что переход на новую строку не выполнять, и слева уже располдожен ключ
*/
void processToken(_jsonObj_t *jsonObj, _jsonToken_t *token, int level, int leftKey)
{
	char	name[12800];		// ACHTUNG!!! Какой-то хардкодный хардкод!

	if(leftKey == 0) {
		printf("%*s",level*2, "");
	}

	name[token->end - token->start] = 0;
	snprintf(name, token->end - token->start + 1, "%s", (char*)(jsonObj->json + token->start));
	printf("id %d pId %d cId %d nId %d name %s type %d valueType %d\n",
		token->id, token->parent, token->fChild, token->nextToken, name, token->type, token->valueType);

	if(token->fChild != 0) {
		tokenRecursive(jsonObj, jsonObj->token + token->fChild, level+1, ((token->type == JSON_KEY) ? 1 : 0));
	}
}


/* Построение дерева в строку
*/
char* jsonAsString(_jsonObj_t *jsonObj)
{
	if(jsonString != NULL) {
		strfree2(&jsonString);
	}
	strinit2(&jsonString, 2048);

	// 4й параметр: уровень, с которого строится дерево
	// 5й параметр: признак необходимости отрисовки отступа слева
	tokenRecursive1(jsonObj, jsonObj->token, &jsonString, 0, 0, false);
	return jsonString->buff;
}

void tokenRecursive1(_jsonObj_t *jsonObj, _jsonToken_t *token, _string2_t **res, int level, int leftKey, bool nextToken)
{
	if((jsonObj->token + token->parent)->type != JSON_KEY) {
		nextToken = (token->nextToken > 0) ? true : false;
	}
	processToken1(jsonObj, token, res, level, leftKey, nextToken);

	// токены того же уровня обходим без рекурсии
	// если у обрабатываемого токена того же уровня есть дочерние токены - тогда рекурсия
	while(token->nextToken > 0) {
		token = jsonObj->token + token->nextToken;
		nextToken = (token->nextToken > 0) ? true : false;
		processToken1(jsonObj, token, res, level, leftKey, nextToken);
	}
}

void processToken1(_jsonObj_t *jsonObj, _jsonToken_t *token, _string2_t **res, int level, int leftKey, bool nextToken)
{
	if(leftKey == 0) {
		strncat2(res, (char*)NULL, level*2, ' ');
	}

	if(token->type == JSON_KEY) {
		strncat2(res, (char*)NULL, 1, '"');
		strncat2(res, (char*)(jsonObj->json + token->start), (token->end - token->start), 0);
		strncat2(res, "\": ", 3, 0);
	} else if(token->type == JSON_VALUE) {
		if(token->valueType == JSON_VALUE_STRING) {
			strncat2(res, (char*)NULL, 1, '"');
			strncat2(res, (char*)(jsonObj->json + token->start), (token->end - token->start), 0);
			strncat2(res, (char*)NULL, 1, '"');
		} else {
			strncat2(res, (char*)(jsonObj->json + token->start), (token->end - token->start), 0);
		}
	} else {
		strncat2(res, (char*)(jsonObj->json + token->start), (token->end - token->start), 0);
		strncat2(res, (char*)NULL, 1, '\n');
	}

	if(token->fChild != 0) {
		int level2 = (leftKey == 1) ? level : level + 1;
		tokenRecursive1(jsonObj, jsonObj->token + token->fChild, res, level2, ((token->type == JSON_KEY) ? 1 : 0), nextToken);
	}

	// отрисовка json'a
	if((token->type == JSON_OBJECT) || (token->type == JSON_ARRAY)) {
		if(leftKey == 1) {
			level--;
		}
		strncat2(res, (char*)NULL, 2*level, ' ');
		if(token->type == JSON_OBJECT) {
			strncat2(res, (char*)NULL, 1, '}');
		} else {
			strncat2(res, (char*)NULL, 1, ']');
		}
	}
	if(nextToken && (token->type != JSON_KEY)) {
		strncat2(res, (char*)NULL, 1, ',');
	}
	if(token->type != JSON_KEY) {
		strncat2(res, (char*)NULL, 1, '\n');
	}
}

