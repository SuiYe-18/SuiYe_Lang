#ifndef SUIYE_H
#define SUIYE_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_TOKENS 4096
#define MAX_VARS   512
#define MAX_API    256
#define MAX_DLLS   16
#define SYE_COUNT  18
#define KEY_COUNT  38
#define STR_SIZE   256

typedef enum {
    SY_TOK_EOF,
    SY_TOK_NUM,
    SY_TOK_STR,
    SY_TOK_ID,

    SY_TOK_PLUS,
    SY_TOK_MINUS,
    SY_TOK_MUL,
    SY_TOK_DIV,
    SY_TOK_ASSIGN,
    SY_TOK_EQ,
    SY_TOK_NE,
    SY_TOK_GT,
    SY_TOK_LT,
    SY_TOK_LPAREN,
    SY_TOK_RPAREN,

    SY_TOK_MAKE,
    SY_TOK_SET,
    SY_TOK_SHOW,
    SY_TOK_INPUT,
    SY_TOK_ADD,
    SY_TOK_SUB,
    SY_TOK_MULX,
    SY_TOK_DIVX,
    SY_TOK_JUMP,
    SY_TOK_IF,
    SY_TOK_ELSE,
    SY_TOK_WHEN,
    SY_TOK_LOOP,
    SY_TOK_DO,
    SY_TOK_STOP,
    SY_TOK_SKIP,
    SY_TOK_CALL,
    SY_TOK_LOAD,
    SY_TOK_SAVE,
    SY_TOK_OPEN,
    SY_TOK_CLOSE,
    SY_TOK_READ,
    SY_TOK_WRITE,
    SY_TOK_WIN,
    SY_TOK_MSG,
    SY_TOK_API,
    SY_TOK_SYSCALL,
    SY_TOK_WAIT,
    SY_TOK_RAND,
    SY_TOK_CLS,
    SY_TOK_LEN,
    SY_TOK_CAT,
    SY_TOK_COPY,
    SY_TOK_DEL,
    SY_TOK_EXIT,
    SY_TOK_RET,
    SY_TOK_RUN,
    SY_TOK_TIMES
} SuiTokenType;

typedef struct {
    SuiTokenType type;
    char text[STR_SIZE];
    double value;
    int line;
} SuiToken;

typedef struct {
    char name[STR_SIZE];
    double value;
} SuiVar;

typedef struct {
    char name[STR_SIZE];
    FARPROC addr;
} SuiWinAPI;

typedef struct {
    SuiToken tokens[MAX_TOKENS];
    int pos;
    int current_line;
    SuiVar vars[MAX_VARS];
    int var_cnt;
    SuiWinAPI apis[MAX_API];
    int api_cnt;
    HMODULE dlls[MAX_DLLS];
    int dll_cnt;
} SuiYe;

#ifdef __cplusplus
extern "C" {
#endif

void SuiYe_Create(SuiYe* sy);
void SuiYe_LoadSYE(SuiYe* sy, const char* path);
void SuiYe_Tokenize(SuiYe* sy, const char* code);
int SuiYe_Run(SuiYe* sy);
char* SuiYe_ReadFile(const char* filename);

#ifdef __cplusplus
}
#endif

#endif