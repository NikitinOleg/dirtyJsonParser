#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/limits.h>

#include "../lib/json/json.h"

// ID ошибки, номер теста (начинается с 1), признак ошибки в тесте, строка, столбец
int test[100][5] = {
		{0, 1, 0, 0, 0},		// JSON_ERR_NO_ERROR
		{0, 2, 0, 0, 0},		// JSON_ERR_NO_ERROR
		{1, 1, 1, 6, 1},		// JSON_ERR_OBJ_IN_OBJ
		{1, 2, 1, 24, 3},
		{1, 3, 1, 32, 3},
		{1, 4, 1, 38, 1},
		{2, 1, 1, 6, 4},		// JSON_ERR_SINGLE_KEY
		{2, 2, 1, 32, 5},
		{2, 3, 1, 37, 1},
		{2, 4, 1, 39, 0},
		{3, 1, 1, 41, 1},		// JSON_ERR_UNEXPECTED_END
		{3, 2, 1, 27, 15},
		{4, 1, 1, 17, 3},		// JSON_ERR_STRING_WITHOUT_QUOTA
		{4, 2, 1, 27, 13},
		{4, 3, 1, 34, 11},
		{5, 1, 1, 14, 15},		// JSON_ERR_ILLEGAL_SYMBOL
		{5, 2, 1, 15, 35},
		{6, 1, 1, 9, 0},		// JSON_ERR_UNEXPECTED_SYMBOL
		{6, 2, 1, 10, 1},
		{6, 3, 1, 11, 0},
		{6, 4, 1, 15, 29},
		{6, 5, 1, 17, 1},
		{6, 6, 1, 18, 0},
		{6, 7, 1, 30, 1},
		{6, 8, 1, 31, 1},
		{6, 9, 1, 32, 1},
		{6, 10, 1, 37, 11},
		{6, 11, 1, 41, 0},
		{6, 12, 1, 41, 2},

//		{6, 12, 1, 41, 0},
};
int testCount = 29;

int readFile(const char *fName, char **json);
void runAllTests();

int main(int argc, char **argv) {
	(void)(argc);
	(void)(argv);
	_jsonObj_t		*jsonObj;
	char			*js;
	int				res;

runAllTests();

	//if(readFile("./test/0/test_02.js", &js) > 0) {
	//if(readFile("./reg-contract-creditor-1.json", &js) > 0) {
	if(readFile("./work_test.json", &js) > 0) {
		res = jsonParser(js, &jsonObj, strlen(js));
		if(res == 1) {
			_jsonErr_t *err = getLastError();
			printf("Error! Line: %d, Col: %d, Message: %s\n", err->line, err->col, err->message);
		}
		//jsonFormat(jsonObj);
		char *aaa = jsonAsString(jsonObj);
		printf("%s\n", aaa);
		int fh = open("./hypothec_out.json", O_WRONLY | O_CREAT);
		write(fh, aaa, strlen(aaa));
		close(fh);

		free(js);
		clearFlatJsonObj(&jsonObj);
	}

return 0;

	res = jsonParser(js, &jsonObj, 0);
	if(res > 0) {
		_jsonErr_t *err = getLastError();
		printf("Error. Line: %d, Col: %d, Message: %s\n", err->line, err->col, err->message);
		return 0;
	}
	jsonFormat(jsonObj);

// ACHTUNG! Проверка полученного результата на NULL!!!
	char			*cRes;
	long long		lRes;
	long double		fRes;

	cRes = getJsonStr("a", jsonObj);
	if(cRes != 0) {
		printf("%s\n", cRes);
	}
	cRes = getJsonStr("n.key13.key23", jsonObj);
	printf("%s\n", cRes);
	cRes = getJsonStr("n.key11", jsonObj);
	printf("%s\n", cRes);
	lRes = getJsonInt("b.c", jsonObj);
	printf("%lld\n", lRes);
	fRes = getJsonDouble("b.d", jsonObj);
	printf("%.10Lf\n", fRes);
	clearFlatJsonObj(&jsonObj);

	return 0;
}

void runAllTests()
{
	char			fName[PATH_MAX];
	_jsonObj_t		*jsonObj;
	char			*js;
	int				res;

	for (int i=0; i<testCount; i++) {
		int errId = test[i][0];
		sprintf(fName, "./test/%d/test_%d%d.js", errId, errId, test[i][1]);
		if(readFile(fName, &js) == 0) {
			continue;
		}
		res = jsonParser(js, &jsonObj, 0);

		if(test[i][2] == 0) {
			if(res == 0) {
				printf("ERR ID: %d  Test: %d    Ok\n", errId, test[i][1]);
			} else {
				_jsonErr_t *err = getLastError();
				printf("ERR ID: %d  Test: %d    FAIL!\n", errId, test[i][1]);
				printf("Error! Line: %d, Col: %d, Message: %s\n", err->line, err->col, err->message);
			}
		}
		if(test[i][2] == 1) {
			if(res > 0) {
				_jsonErr_t *err = getLastError();
				if((err->line == test[i][3]) && (err->col == test[i][4])) {
					printf("ERR ID: %d  Test: %d    Ok\n", errId, test[i][1]);
				} else {
					printf("ERR ID: %d  Test: %d    FAIL!\n", errId, test[i][1]);
					printf("Error! Line: %d, Col: %d, Message: %s\n", err->line, err->col, err->message);
				}
			}
		}
		free(js);
		clearFlatJsonObj(&jsonObj);
	}
}

int readFile(const char *fName, char **json)
{
	struct stat		fStat;
	int				cfgFileLen;
	int				fh;

	stat(fName, &fStat);
	cfgFileLen = fStat.st_size;
	*json = (char*)malloc(cfgFileLen+8);
	fh = open(fName, O_RDONLY);
	if(fh == -1) {
		return 0;
	}
	read(fh, *json, fStat.st_size);
	(*json)[fStat.st_size] = 0;
	close(fh);
	return 1;
}

