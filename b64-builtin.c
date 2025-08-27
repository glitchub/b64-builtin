#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// various bash headers
#include "config.h" // first!
#include "builtins.h"
#include "shell.h"
#include "bashgetopt.h"
#include "common.h"
#define sigjmp_buf jmp_buf

static int b64_builtin(WORD_LIST *list)
{
    int opt, decode = 0;
    reset_internal_getopt();
    while ((opt = internal_getopt(list, "de")) != -1)
    {
        switch (opt)
        {
            case 'd':
                decode = 1;
                break;
            case 'e':
                decode = 0;
                break;
            CASE_HELPOPT;
            default:
                builtin_usage();
                return (EX_USAGE);
        }
    }
    list = loptend;

    int f = 0; // stdin
    if (list) {
        f = open(list->word->word, O_RDONLY);
        if (f < 0)
        {
            builtin_error("%s: %s", list->word->word, strerror(errno));
            return EXECUTION_FAILURE;
        }
    }

    // The base64 character set

    if (decode)
    {
        const char dec[] = {
          // -1 = ignore (whitespace), otherwise decoded value 0 - 63
          // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 00
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 10
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, // 20
            52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, // 30
            -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, // 40
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, // 50
            -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, // 60
            41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, // 70
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 80
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 90
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // A0
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // B0
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // C0
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // D0
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // E0
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // F0
        };

        unsigned char in[4096*4], out[4096*3];
        int got, state = 0, next = 0;
        while ((got = read(f, in, sizeof(in))) > 0)
        {
            int o = 0;
            for (int i = 0; i < got; i++)
            {
                int d = dec[in[i]];
                if (d >= 0) switch(state)
                {
                    case 0: next = d << 2; state = 1; break;
                    case 1: out[o++] = next | d >> 4; next = d << 4; state = 2; break;
                    case 2: out[o++] = next | d >> 2; next = d << 6; state = 3; break;
                    case 3: out[o++] = next | d; state = 0; break;
                }
            }
            if (o) write(1, out, o);
        }
    } else
    {
        const char enc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        unsigned char in[4096*3], out[4096*4];
        int got, state = 0, next = 0;
        while ((got = read(f, in, sizeof(in))) > 0)
        {
            int o = 0;
            for (int i = 0; i < got; i++)
            {
                unsigned char c = in[i];
                switch(state)
                {
                    case 0: out[o++] = enc[c >> 2]; next = (c & 3) << 4; state = 1; break;
                    case 1: out[o++] = enc[next | (c >> 4)]; next = (c & 15) << 2; state = 2; break;
                    case 2: out[o++] = enc[next | (c >> 6)]; out[o++] = enc[c & 63]; state = 0; break;
                }
            }
            write(1, out, o);
        }
        if (state)
        {
            write(1, &enc[next], 1);
            write(1, "=", 1);
            if (state == 1) write(1, "=", 1);
        }
    }
    return EXECUTION_SUCCESS;
}

static char *b64_doc[] = {
    "Base64 encode/decode the given file, or standard input, to standard output.",
    "",
    "Options:",
    "  -d       decode base64 text to binary",
    "  -e       encode binary to base64 text (default)",
    "",
    "Encoded base64 is output as a single line without terminating CR. Unexpected characters",
    "encountered during decode are ignored.",
    "",
    "Exit Status:",
    "Returns success unless an invalid option or filename is given.",
    NULL
};

struct builtin b64_struct = {
    "b64",
    b64_builtin,
    BUILTIN_ENABLED,
    b64_doc,
    "b64 [-de] [file]",
    0
};
