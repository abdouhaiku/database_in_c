//
// Created by Abdou on 23/07/2026.
//

#include <stdio.h>
#include <string.h>

int main() {
    char buffer[] = "Hello   World\tC Programming";
    char *p = buffer;
    const char *delim = " \t\n";
    char *token;

    while ((token = strsep(&p, delim)) != NULL) {
        if (*token != '\0') { // Skip empty tokens from consecutive spaces
            printf("Token: %s\n", token);
        }
    }
    return 0;
}