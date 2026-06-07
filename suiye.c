#include "suiye.h"

static void SetColor(int color) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(h, color);
}

#define WHITE   7
#define RED     12
#define GREEN   10
#define YELLOW  14

static const char* keywords[KEY_COUNT] = {
    "MAKE","SET","SHOW","INPUT","ADD","SUB","MULX","DIVX",
    "JUMP","IF","ELSE","WHEN","LOOP","DO","STOP","SKIP",
    "CALL","LOAD","SAVE","OPEN","CLOSE","READ","WRITE",
    "WIN","MSG","API","SYSCALL","WAIT","RAND","CLS",
    "LEN","CAT","COPY","DEL","EXIT","RET","RUN","TIMES"
};

static const char* sye_list[SYE_COUNT] = {
    "user32","kernel32","msgbox","sleep","loadlib","getproc",
    "createwin","messagebox","exitprocess","getlasterror",
    "writefile","readfile","createfile","closehandle",
    "gettickcount","playsound","shellexec","findwindow"
};

void SuiYe_Create(SuiYe* sy) {
    memset(sy, 0, sizeof(SuiYe));
    sy->current_line = 1;
    srand((unsigned int)time(NULL));
    
    // 初始化所有token为EOF，彻底避免垃圾值
    for (int i = 0; i < MAX_TOKENS; i++) {
        sy->tokens[i].type = SY_TOK_EOF;
        sy->tokens[i].line = 1;
        memset(sy->tokens[i].text, 0, STR_SIZE);
    }
}

static SuiVar* get_var(SuiYe* sy, const char* name) {
    // 禁止使用关键字作为变量名
    for (int i = 0; i < KEY_COUNT; i++) {
        if (!strcmp(name, keywords[i])) {
            SetColor(RED);
            printf("\n[ERROR] Line %d: '%s' is a reserved keyword\n", sy->current_line, name);
            printf("Fix: Rename your variable to something else\n");
            SetColor(WHITE);
            exit(1);
        }
    }
    
    for (int i = 0; i < sy->var_cnt; i++) {
        if (!strcmp(sy->vars[i].name, name))
            return &sy->vars[i];
    }
    strcpy(sy->vars[sy->var_cnt].name, name);
    return &sy->vars[sy->var_cnt++];
}

static void reg_api(SuiYe* sy, const char* name, FARPROC addr) {
    strcpy(sy->apis[sy->api_cnt].name, name);
    sy->apis[sy->api_cnt].addr = addr;
    sy->api_cnt++;
}

void SuiYe_LoadSYE(SuiYe* sy, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return;

    char buf[STR_SIZE];
    while (fgets(buf, STR_SIZE, f)) {
        char* p = buf;
        while (isspace((unsigned char)*p)) p++;
        if (*p != '&') continue;
        p++;

        char name[STR_SIZE] = {0};
        int i = 0;
        while (!isspace((unsigned char)*p) && *p)
            name[i++] = *p++;

        for (int n = 0; n < SYE_COUNT; n++) {
            if (!strcmp(name, sye_list[n])) {
                switch (n) {
                    case 0: sy->dlls[sy->dll_cnt++] = LoadLibraryA("user32.dll"); break;
                    case 1: sy->dlls[sy->dll_cnt++] = LoadLibraryA("kernel32.dll"); break;
                    case 2: reg_api(sy, "MessageBoxA", GetProcAddress(sy->dlls[0], "MessageBoxA")); break;
                    case 3: reg_api(sy, "Sleep", GetProcAddress(sy->dlls[1], "Sleep")); break;
                    case 8: reg_api(sy, "ExitProcess", GetProcAddress(sy->dlls[1], "ExitProcess")); break;
                }
                break;
            }
        }
    }
    fclose(f);
}

static double expr(SuiYe* sy);

static double factor(SuiYe* sy) {
    SuiToken* t = &sy->tokens[sy->pos];
    double v = 0;

    if (t->type == SY_TOK_NUM) {
        v = t->value;
        sy->pos++;
    } else if (t->type == SY_TOK_ID) {
        v = get_var(sy, t->text)->value;
        sy->pos++;
    } else if (t->type == SY_TOK_LPAREN) {
        sy->pos++;
        v = expr(sy);
        if (sy->tokens[sy->pos].type == SY_TOK_RPAREN)
            sy->pos++;
    }
    return v;
}

static double term(SuiYe* sy) {
    double v = factor(sy);
    while (1) {
        SuiTokenType t = sy->tokens[sy->pos].type;
        if (t == SY_TOK_MUL) {
            sy->pos++;
            v *= factor(sy);
        } else if (t == SY_TOK_DIV) {
            sy->pos++;
            v /= factor(sy);
        } else break;
    }
    return v;
}

static double expr(SuiYe* sy) {
    double v = term(sy);
    while (1) {
        SuiTokenType t = sy->tokens[sy->pos].type;
        if (t == SY_TOK_PLUS) {
            sy->pos++;
            v += term(sy);
        } else if (t == SY_TOK_MINUS) {
            sy->pos++;
            v -= term(sy);
        } else break;
    }
    return v;
}

static int cond(SuiYe* sy) {
    double a = expr(sy);
    SuiToken* op = &sy->tokens[sy->pos++];
    double b = expr(sy);
    
    if (op->type == SY_TOK_GT) return a > b;
    if (op->type == SY_TOK_LT) return a < b;
    if (op->type == SY_TOK_EQ) return a == b;
    if (op->type == SY_TOK_NE) return a != b;
    return 0;
}

// 【终极修复】词法分析器，彻底解决空字符/空行问题
void SuiYe_Tokenize(SuiYe* sy, const char* code) {
    int c = 0, t = 0;
    sy->current_line = 1;
    sy->pos = 0;

    while (code[c] != '\0' && t < MAX_TOKENS - 1) {
        // 跳过所有空白字符
        while (isspace((unsigned char)code[c])) {
            if (code[c] == '\n') sy->current_line++;
            c++;
        }

        if (code[c] == '\0') break;

        // 处理注释
        if (code[c] == '|' && code[c+1] == '|') {
            while (code[c] != '\0' && code[c] != '\n') c++;
            continue;
        }

        // 处理关键字和标识符
        if (isalpha((unsigned char)code[c])) {
            int s = c;
            while (isalnum((unsigned char)code[c])) c++;
            int len = c - s;
            if (len <= 0) continue;

            char tmp[STR_SIZE] = {0};
            strncpy(tmp, code + s, len);

            SuiTokenType ty = SY_TOK_ID;
            for (int i = 0; i < KEY_COUNT; i++) {
                if (!strcmp(tmp, keywords[i])) {
                    ty = SY_TOK_MAKE + i;
                    break;
                }
            }

            sy->tokens[t].type = ty;
            sy->tokens[t].line = sy->current_line;
            strcpy(sy->tokens[t].text, tmp);
            t++;
            continue;
        }

        // 处理数字
        if (isdigit((unsigned char)code[c]) || code[c] == '.') {
            int s = c;
            while (isdigit((unsigned char)code[c]) || code[c] == '.') c++;
            int len = c - s;
            if (len <= 0) continue;

            char tmp[STR_SIZE] = {0};
            strncpy(tmp, code + s, len);
            sy->tokens[t].type = SY_TOK_NUM;
            sy->tokens[t].line = sy->current_line;
            sy->tokens[t].value = atof(tmp);
            t++;
            continue;
        }

        // 处理字符串
        if (code[c] == '"') {
            c++;
            int s = c;
            while (code[c] != '\0' && code[c] != '"') c++;
            int len = c - s;
            if (len > 0) {
                sy->tokens[t].type = SY_TOK_STR;
                sy->tokens[t].line = sy->current_line;
                strncpy(sy->tokens[t].text, code + s, len);
                t++;
            }
            if (code[c] == '"') c++;
            continue;
        }

        // 处理运算符
        switch (code[c]) {
            case '+': sy->tokens[t].type = SY_TOK_PLUS; break;
            case '-': sy->tokens[t].type = SY_TOK_MINUS; break;
            case '*': sy->tokens[t].type = SY_TOK_MUL; break;
            case '/': sy->tokens[t].type = SY_TOK_DIV; break;
            case '=': 
                if (code[c+1] == '=') {
                    sy->tokens[t].type = SY_TOK_EQ;
                    c++;
                } else {
                    sy->tokens[t].type = SY_TOK_ASSIGN;
                }
                break;
            case '!':
                if (code[c+1] == '=') {
                    sy->tokens[t].type = SY_TOK_NE;
                    c++;
                }
                break;
            case '>': sy->tokens[t].type = SY_TOK_GT; break;
            case '<': sy->tokens[t].type = SY_TOK_LT; break;
            case '(': sy->tokens[t].type = SY_TOK_LPAREN; break;
            case ')': sy->tokens[t].type = SY_TOK_RPAREN; break;
            default: c++; continue; // 跳过所有未知字符
        }

        sy->tokens[t].line = sy->current_line;
        t++;
        c++;
    }

    // 确保最后一个token是EOF
    sy->tokens[t].type = SY_TOK_EOF;
    sy->tokens[t].line = sy->current_line;
}

static int run(SuiYe* sy);

static int run_block(SuiYe* sy) {
    while (1) {
        SuiTokenType t = sy->tokens[sy->pos].type;
        if (t == SY_TOK_STOP || t == SY_TOK_ELSE || t == SY_TOK_EOF) break;
        if (run(sy) != 0) return -1;
    }
    return 0;
}

static int run(SuiYe* sy) {
    SuiToken* cur = &sy->tokens[sy->pos];

    // 【终极防护】跳过任何无效token
    if (cur->type == SY_TOK_EOF) return 1;
    if (cur->type == SY_TOK_STOP || cur->type == SY_TOK_ELSE) return 0;
    if (strlen(cur->text) == 0 && cur->type != SY_TOK_NUM) {
        sy->pos++;
        return 0;
    }

    // 变量定义
    if (cur->type == SY_TOK_MAKE) {
        sy->pos++;
        SuiVar* v = get_var(sy, sy->tokens[sy->pos].text);
        sy->pos += 2;
        v->value = expr(sy);
        return 0;
    }

    // 变量赋值
    if (cur->type == SY_TOK_SET) {
        sy->pos++;
        SuiVar* v = get_var(sy, sy->tokens[sy->pos].text);
        sy->pos += 2;
        v->value = expr(sy);
        return 0;
    }

    // 输出
    if (cur->type == SY_TOK_SHOW) {
        sy->pos++;
        if (sy->tokens[sy->pos].type == SY_TOK_STR) {
            printf("%s\n", sy->tokens[sy->pos].text);
            sy->pos++;
        } else {
            printf("%g\n", expr(sy));
        }
        return 0;
    }

    // 用户输入
    if (cur->type == SY_TOK_INPUT) {
        sy->pos++;
        SuiVar* v = get_var(sy, sy->tokens[sy->pos].text);
        printf("Enter value for %s: ", sy->tokens[sy->pos].text);
        scanf("%lf", &v->value);
        sy->pos++;
        return 0;
    }

    // 变量运算
    if (cur->type == SY_TOK_ADD) {
        sy->pos++;
        SuiVar* v = get_var(sy, sy->tokens[sy->pos].text);
        sy->pos += 2;
        v->value += expr(sy);
        return 0;
    }

    if (cur->type == SY_TOK_SUB) {
        sy->pos++;
        SuiVar* v = get_var(sy, sy->tokens[sy->pos].text);
        sy->pos += 2;
        v->value -= expr(sy);
        return 0;
    }

    if (cur->type == SY_TOK_MULX) {
        sy->pos++;
        SuiVar* v = get_var(sy, sy->tokens[sy->pos].text);
        sy->pos += 2;
        v->value *= expr(sy);
        return 0;
    }

    if (cur->type == SY_TOK_DIVX) {
        sy->pos++;
        SuiVar* v = get_var(sy, sy->tokens[sy->pos].text);
        sy->pos += 2;
        v->value /= expr(sy);
        return 0;
    }

    // 等待
    if (cur->type == SY_TOK_WAIT) {
        sy->pos++;
        Sleep((DWORD)expr(sy));
        return 0;
    }

    // 清屏
    if (cur->type == SY_TOK_CLS) {
        system("cls");
        sy->pos++;
        return 0;
    }

    // 弹窗
    if (cur->type == SY_TOK_MSG) {
        sy->pos++;
        MessageBoxA(0, sy->tokens[sy->pos].text, "SuiYe", 0);
        sy->pos++;
        return 0;
    }

    // 显示时间
    if (cur->type == SY_TOK_TIMES) {
        sy->pos++;
        time_t now = time(NULL);
        struct tm* ti = localtime(&now);
        printf("[%02d:%02d:%02d] Current system time\n",
            ti->tm_hour, ti->tm_min, ti->tm_sec);
        return 0;
    }

    // 随机数
    if (cur->type == SY_TOK_RAND) {
        sy->pos++;
        SuiVar* v = get_var(sy, sy->tokens[sy->pos].text);
        sy->pos += 2;
        int max = (int)expr(sy);
        v->value = rand() % max;
        return 0;
    }

    // 退出程序
    if (cur->type == SY_TOK_EXIT) {
        sy->pos++;
        exit(0);
        return 0;
    }

    // 循环
    if (cur->type == SY_TOK_LOOP) {
        sy->pos++;
        int cnt = (int)expr(sy);
        if (sy->tokens[sy->pos].type == SY_TOK_DO) sy->pos++;
        int mark = sy->pos;

        for (int i = 0; i < cnt; i++) {
            sy->pos = mark;
            if (run_block(sy) != 0) return -1;
        }

        if (sy->tokens[sy->pos].type == SY_TOK_STOP)
            sy->pos++;
        return 0;
    }

    // 条件判断
    if (cur->type == SY_TOK_IF) {
        sy->pos++;
        int ok = cond(sy);
        if (sy->tokens[sy->pos].type == SY_TOK_WHEN) sy->pos++;

        if (ok) {
            run_block(sy);
        } else {
            while (sy->tokens[sy->pos].type != SY_TOK_ELSE &&
                   sy->tokens[sy->pos].type != SY_TOK_STOP &&
                   sy->tokens[sy->pos].type != SY_TOK_EOF) {
                sy->pos++;
            }
        }

        if (sy->tokens[sy->pos].type == SY_TOK_ELSE) {
            sy->pos++;
            if (!ok) run_block(sy);
        }

        if (sy->tokens[sy->pos].type == SY_TOK_STOP)
            sy->pos++;
        return 0;
    }

    // 错误提示
    SetColor(RED);
    printf("\n[ERROR] Line %d: Unknown command '%s'\n", cur->line, cur->text);
    printf("Fix: Check spelling or refer to the syntax manual\n");
    SetColor(WHITE);
    return -1;
}

int SuiYe_Run(SuiYe* sy) {
    while (sy->tokens[sy->pos].type != SY_TOK_EOF) {
        if (run(sy) != 0) return -1;
    }
    return 0;
}

char* SuiYe_ReadFile(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char* buf = (char*)malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = 0;
    fclose(f);
    return buf;
}

int main(int argc, char* argv[]) {
    SetColor(WHITE);
    if (argc < 2) {
        SetColor(YELLOW);
        printf("Usage: SuiYe.exe script.sys\n");
        SetColor(WHITE);
        system("pause");
        return 0;
    }

    SuiYe sy;
    SuiYe_Create(&sy);
    SuiYe_LoadSYE(&sy, "api.sye");

    char* code = SuiYe_ReadFile(argv[1]);
    if (!code) {
        SetColor(RED);
        printf("[ERROR] Cannot read file: %s\n", argv[1]);
        SetColor(WHITE);
        system("pause");
        return 1;
    }

    SuiYe_Tokenize(&sy, code);
    int result = SuiYe_Run(&sy);
    free(code);

    if (result == 0) {
        SetColor(GREEN);
        printf("\n== Execution finished successfully ==\n");
    } else {
        SetColor(RED);
        printf("\n== Execution failed ==\n");
    }

    SetColor(WHITE);
    system("pause");
    return result;
}