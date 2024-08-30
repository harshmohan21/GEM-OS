#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
void prt(void)
{
    printf("Unable to execute \n");
    exit(-1);
}
bool is_Number(char* str1) {
    int n, m;
    while (*str1 == ' ') str1 ++;
    n = m = 0;
    if (*str1 == '+' || *str1 == '-') str1 ++;
    
    while (*str1 >= '0' && *str1 <= '9') {
        n ++;
        str1 ++;
    }
        
    while (*str1 == ' ') str1 ++;
    return *str1 == 0 ? true : false;
}
void tostring(char str[], int num)
{
    int i, rem, len = 0, n;
 
    n = num;
    while (n != 0)
    {
        len++;
        n /= 10;
    }
    for (i = 0; i < len; i++)
    {
        rem = num % 10;
        num = num / 10;
        str[len - (i + 1)] = rem + '0';
    }
    str[len] = '\0';
}
int main(int argc, char *argv[]) {
        if(argc < 2)
    {
        prt();
        exit(1);
    }
    if(!is_Number(argv[argc-1]))
    {
        // perror("Incorrect number");
        prt();
        exit(-1);
    }
    int y = atoi(argv[argc - 1]);
    if(y < 0)
    {
        prt();
        exit(-1);
    }
    int x = y*2;
    char *argvv[argc];
    argvv[argc - 1] = NULL;
    // char s[64];
    char s[20];
    tostring(s,x);
    // itoa(x,s,10);
    argvv[argc-2] = s;
    if(argc == 2)
    printf("%d \n", x);
    else 
    {
        int len = strlen(argv[1]);
        argvv[0] = (char*)malloc(strlen(argv[1])+3);
        strcat(argvv[0], "./");
        strcat(argvv[0],argv[1]);
        for(int j =2 ;j<argc -1 ;j++)
        {
            argvv[j-1] = argv[j];
        }
        execv(argvv[0], argvv);
        prt();
    }
    return 0;
}
