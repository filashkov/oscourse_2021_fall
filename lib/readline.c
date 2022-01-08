#include <inc/stdio.h>
#include <inc/error.h>
#include <inc/types.h>
#include <inc/string.h>

#define BUFLEN 1024

static char buf[BUFLEN];

char *
readline(const char *prompt) {
    if (prompt) {
#if JOS_KERNEL
        cprintf("%s", prompt);
#else
        fprintf(1, "%s", prompt);
#endif
    }

    bool echo = iscons(0);

    for (size_t i = 0;;) {
        int c = getchar();

        if (c < 0) {
            if (c != -E_EOF)
                cprintf("read error: %i\n", c);
            return NULL;
        } else if ((c == '\b' || c == '\x7F')) {
            if (i) {
                if (echo) {
                    cputchar('\b');
                    cputchar(' ');
                    cputchar('\b');
                }
                i--;
            }
        } else if (c >= ' ') {
            if (i < BUFLEN - 1) {
                if (echo) {
                    cputchar(c);
                }
                buf[i++] = (char)c;
            }
        } else if (c == '\n' || c == '\r') {
            if (echo) {
                cputchar('\n');
            }
            buf[i] = 0;
            return buf;
        }
    }
}

/*
int
s2d(char* s) {
    int result = 0;
    int n = strlen(s);
    int sign = 1;
    if (s[0] == '-') {
        sign = -1;
    }
    for (int i = 0; i < n; i++) {
        if ((s[i] >= '0') && (s[i] <= '9')) {
            result *= 10;
            result += s[i] - '0';
        }
    }
    return sign * result;
}

unsigned int
s2u(char* s) {
    unsigned int result = 0;
    int n = strlen(s);
    for (int i = 0; i < n; i++) {
        if ((s[i] >= '0') && (s[i] <= '9')) {
            result *= 10;
            result += s[i] - '0';
        }
    }
    return result;
}
*/

long long
s2lld(const char* s) {
    long long result = 0;
    int n = strlen(s);
    int sign = 1;
    if (s[0] == '-') {
        sign = -1;
    }
    for (int i = 0; i < n; i++) {
        if ((s[i] >= '0') && (s[i] <= '9')) {
            result *= 10;
            result += s[i] - '0';
        }
    }
    return sign * result;
}

unsigned long long
s2llu(const char* s) {
    unsigned long long result = 0;
    int n = strlen(s);
    for (int i = 0; i < n; i++) {
        if ((s[i] >= '0') && (s[i] <= '9')) {
            result *= 10;
            result += s[i] - '0';
        }
    }
    return result;
}

/*
double
s2lf(const char* s) {
    double result = 0;
    int n = strlen(s);
    int sign = 1;
    if (s[0] == '-') {
        sign = -1;
    }
    bool was_dot = false;
    int i = 0;
    for (i = 0; i < n; i++) {
        if ((s[i] >= '0') && (s[i] <= '9')) {
            result *= 10;
            result += s[i] - '0';
        }
        if (s[i] == '.') {
            was_dot = true;
            break;
        }
    }
    i++;
    double fractal_part = 0;
    // int degree = n - i;
    double degree = 0.1;
    for (; i < n; i++) {
        if ((s[i] >= '0') && (s[i] <= '9')) {
            fractal_part += degree * (s[i] - '0');
            degree /= 10;
        }
    }

    result += fractal_part;
    return sign * result;
}
*/

void
readvalue(const char* format, void* value) {
    char* buf = readline(NULL);
    if (strcmp(format, "%s") == 0) {
        strcpy(value, buf);
    } else if (strcmp(format, "%d") == 0) {
        int result = s2lld(buf);
        memcpy(value, &result, sizeof(result));
    } else if ((strcmp(format, "%ld") == 0) || (strcmp(format, "%lld") == 0)) {
        long long result = s2lld(buf);
        memcpy(value, &result, sizeof(result));
    } else if (strcmp(format, "%u") == 0) {
        unsigned result = s2llu(buf);
        memcpy(value, &result, sizeof(result));
    } else if ((strcmp(format, "%lu") == 0) || (strcmp(format, "%llu") == 0)) {
        unsigned long long result = s2llu(buf);
        memcpy(value, &result, sizeof(result));
    } else if (strcmp(format, "%f") == 0) {
        //float result = s2lf(buf);
        //memcpy(value, &result, sizeof(result));
    } else if (strcmp(format, "%lf") == 0) {
        //double result = s2lf(buf);
        //memcpy(value, &result, sizeof(result));
    } else if (strcmp(format, "%c") == 0) {
        char result = buf[0];
        memcpy(value, &result, sizeof(result));
    }
}