#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <ctype.h>

void mainLoop(char *str, bool file);

typedef struct
{
    char *name;
    char *value;
} EnvVar;

typedef struct
{
    char *name;
    struct tm time;
    int ret;
} Command;

enum retCommand
{
    quit,
    log,
    print,
    theme,
    $var
};

enum theme
{
    white,
    red,
    blue,
    green
};

const char *SHELL_NAME = "cshell$ ";

const char *INPUT_NAMES[5] = {"exit",
                              "log",
                              "print",
                              "theme",
                              "$"};
const char *THEME_NAMES[5] = {"white",
                              "red",
                              "blue",
                              "green"};
const char *THEME_CODES[5] = {"\033[37m",
                              "\033[31m",
                              "\033[34m",
                              "\033[32m"};
const char *CLEAR_CODE = "\033[0m";

char *currentThemeCode = "\033[37m";
int currentTheme = white;

const char *GENERIC_ERROR = "Missing keyword or command, or permission problem";
const char *THEME_ERROR = "Unsupported theme: ";
const char *VAR_ERROR = "Variable value expected";
const char *VAR_MISSING_ERROR_ONE = "Variable ";
const char *VAR_MISSING_ERROR_TWO = "doesn't exist";

/* Code for EnvVarArray*/
typedef struct
{
    EnvVar *array;
    size_t used;
    size_t size;
} EnvVarArray;

void initEnvArray(EnvVarArray *arr, size_t size)
{
    arr->array = (EnvVar *)calloc(size, sizeof(EnvVar));
    arr->used = 0;
    arr->size = size;
}

void pushEnvArray(EnvVarArray *arr, EnvVar elem)
{
    for (int i = 0; i < arr->used; i++)
    {
        if (arr->array[i].name == elem.name)
        {
            arr->array[arr->used].value = (char *)realloc(arr->array[arr->used].value, strlen(elem.value + 1));
            strcpy(arr->array[i].value, elem.value);
            return;
        }
    }

    if (arr->used == arr->size)
    {
        arr->size *= 2;
        arr->array = (EnvVar *)realloc(arr->array, arr->size * sizeof(EnvVar));
        memset(&arr->array[arr->used], 0, sizeof(EnvVar) * (arr->size - arr->used));
    }
    arr->array[arr->used].name = (char *)malloc(strlen(elem.name) + 1);
    strcpy(arr->array[arr->used].name, elem.name);

    arr->array[arr->used].value = (char *)malloc(strlen(elem.value) + 1);
    strcpy(arr->array[arr->used].value, elem.value);
    arr->used++;
}

void freeEnvArray(EnvVarArray *arr)
{
    for (int i = 0; i < arr->used; i++)
    {
        free(arr->array[i].name);
        free(arr->array[i].value);
        arr->array[i].name = NULL;
        arr->array[i].value = NULL;
    }

    free(arr->array);
    arr->array = NULL;
    arr->used = 0;
    arr->size = 0;
}

/* Code for CommandArray */
typedef struct
{
    Command *array;
    size_t used;
    size_t size;
} CommandArray;

//globals for command and environment var arrays
CommandArray commands;
EnvVarArray envVars;

void initCommandArray(CommandArray *arr, size_t size)
{
    arr->array = (Command *)malloc(size * sizeof(Command));
    arr->used = 0;
    arr->size = size;

    memset(&arr->array[0], 0, sizeof(Command) * size);
}

void pushCommandArray(CommandArray *arr, Command elem)
{
    if (arr->used == arr->size)
    {
        arr->size *= 2;
        arr->array = (Command *)realloc(arr->array, arr->size * sizeof(Command));
        memset(&arr->array[arr->used], 0, sizeof(Command) * (arr->size - arr->used));
    }
    arr->array[arr->used].name = (char *)malloc(strlen(elem.name) + 1);
    strcpy(arr->array[arr->used].name, elem.name);

    arr->array[arr->used].time = elem.time;

    arr->array[arr->used].ret = elem.ret;
    arr->used++;
}

int findEnvVarArray(EnvVarArray *arr, char *str)
{
    for (int i = 0; i < arr->used; i++)
    {
        if (strncmp(arr->array[i].name, str, strlen(str) - 1) == 0)
        {
            return i;
        }
    }
    return -1;
}

void freeCommandArray(CommandArray *arr)
{
    for (int i = 0; i < arr->used; i++)
    {
        free(arr->array[i].name);
        arr->array[i].name = NULL;
    }

    free(arr->array);
    arr->array = NULL;
    arr->used = 0;
    arr->size = 0;
}

void saveCommand(char *name, int ret, bool isVar)
{
    Command temp;
    temp.name = (char *)malloc(strlen(name) + 1);
    strcpy(temp.name, name);
    if (isVar)
    {
        temp.name[strlen(name) - 1] = '\0';
    }
    else
    {
        temp.name[strlen(name)] = '\0';
    }
    temp.ret = ret;
    time_t t = time(NULL);
    struct tm tempTime;
    localtime_r(&t, &tempTime);
    temp.time = tempTime;

    pushCommandArray(&commands, temp);
    free(temp.name);
}

int processInput(char *input)
{
    if (input[0] == 'e' ||
        input[0] == 'l' ||
        input[0] == 'p' ||
        input[0] == 't' ||
        input[0] == '$')
    {
        for (int i = 0; i < 5; i++)
        {
            char *ret = strstr(input, INPUT_NAMES[i]);
            if (ret != NULL)
            {
                return i;
            }
        }
    }
    return -1;
}

void printError(int errorCode, char *str)
{
    switch (errorCode)
    {
    case 0:
        if (currentTheme == white)
        {
            printf("%s\n", GENERIC_ERROR);
        }
        else
        {
            printf("%s%s%s\n", currentThemeCode, GENERIC_ERROR, CLEAR_CODE);
        }
        break;
    case 1:
        if (currentTheme == white)
        {
            printf("%s%s", THEME_ERROR, str);
        }
        else
        {
            printf("%s%s%s%s", currentThemeCode, THEME_ERROR, str, CLEAR_CODE);
        }
        if (str[0] == '\0')
        {
            printf("\n");
        }
        break;
    case 2:
        if (currentTheme == white)
        {
            printf("%s\n", VAR_ERROR);
        }
        else
        {
            printf("%s%s%s\n", currentThemeCode, VAR_ERROR, CLEAR_CODE);
        }
        break;
    case 3:;
        char *varName = malloc(strlen(str));
        strncpy(varName, str, strlen(str) - 1);
        if (currentTheme == white)
        {
            printf("%s%s %s\n", VAR_MISSING_ERROR_ONE, varName, VAR_MISSING_ERROR_TWO);
        }
        else
        {
            printf("%s%s%s %s%s\n", currentThemeCode, VAR_MISSING_ERROR_ONE, varName, VAR_MISSING_ERROR_TWO, CLEAR_CODE);
        }
        free(varName);
        break;
    default:
        break;
    }
}

int processTheme(char *str)
{
    char *args = &str[6];

    for (int i = 0; i < 4; i++)
    {
        char *color = strstr(args, THEME_NAMES[i]);
        if (color != NULL)
        {
            switch (i)
            {
            case white:
                if (args[6] == '\0')
                {
                    currentTheme = white;
                    currentThemeCode = (char *)THEME_CODES[white];
                    return 0;
                }
                else
                {
                    printError(1, args);
                    return 1;
                }
            case red:
                if (args[4] == '\0')
                {
                    currentTheme = red;
                    currentThemeCode = (char *)THEME_CODES[red];
                    return 0;
                }
                else
                {
                    printError(1, args);
                    return 1;
                }
            case blue:
                if (args[5] == '\0')
                {
                    currentTheme = blue;
                    currentThemeCode = (char *)THEME_CODES[blue];
                    return 0;
                }
                else
                {
                    printError(1, args);
                    return 1;
                }
            case green:
                if (args[6] == '\0')
                {
                    currentTheme = green;
                    currentThemeCode = (char *)THEME_CODES[green];
                    return 0;
                }
                else
                {
                    printError(1, args);
                    return 1;
                }

            default:
                break;
            }
        }
    }
    printError(1, args);
    return 1;
}

int processLog(char *str)
{
    char *args = &str[4];
    if (args[0] != '\0')
    {
        printError(0, " ");
        return 1;
    }
    if (currentTheme != white)
    {
        printf("%s", currentThemeCode);
    }

    for (int i = 0; i < commands.used; i++)
    {
        printf("%s", asctime(&commands.array[i].time));
        printf(" %s %d\n", commands.array[i].name, commands.array[i].ret);
    }
    return 0;
}

int processPrint(char *str)
{
    char *args = &str[6];
    int vars = 0;

    for (int i = 0; args[i] != '\0'; i++)
    {
        if (args[i] == '$')
        {
            vars++;
        }
    }

    if (vars == 0)
    {
        if (currentTheme != white)
        {
            printf("%s", currentThemeCode);
        }
        printf("%s", args);
        return 0;
    }
    else
    {
        int words = 1;
        int wordIndex = 0;
        for (int i = 0; args[i] != '\0'; i++)
        {
            if (args[i] == ' ')
            {
                words++;
            }
        }

        char **line;
        line = (char **)malloc(words * sizeof(char *));
        const char delim[2] = " ";
        char *token;

        token = strtok(args, delim);

        while (token != NULL)
        {
            line[wordIndex] = malloc((strlen(token) + 1) * sizeof(char));
            strcpy(line[wordIndex], token);
            wordIndex++;
            token = strtok(NULL, delim);
        }
        for (int i = 0; i < words; i++)
        {
            if (line[i][0] == '$')
            {
                char *name;
                name = malloc(strlen(line[i]));
                strcpy(name, line[i] + 1);
                int index = findEnvVarArray(&envVars, name);
                free(name);
                if (index == -1)
                {
                    printError(3, line[i]);
                    for (int i = 0; i < words; i++)
                    {
                        if (currentTheme == white)
                        {
                            printf("%s ", line[i]);
                        }
                        else
                        {
                            printf("%s%s%s ", currentThemeCode, line[i], CLEAR_CODE);
                        }
                    }
                    for (int i = 0; i < words; i++)
                    {
                        free(line[i]);
                        line[i] = NULL;
                    }
                    free(line);
                    line = NULL;
                    return -1;
                }
                else
                {
                    line[i] = realloc(line[i], (strlen(envVars.array[index].value) + 1));
                    strcpy(line[i], envVars.array[index].value);
                }
            }
        }

        for (int i = 0; i < words; i++)
        {
            if (currentTheme == white)
            {
                printf("%s ", line[i]);
            }
            else
            {
                printf("%s%s%s ", currentThemeCode, line[i], CLEAR_CODE);
            }
        }
        printf("\n");
        for (int i = 0; i < words; i++)
        {
            free(line[i]);
            line[i] = NULL;
        }
        free(line);
        line = NULL;
        return 0;
    }
    return 1;
}

int processVar(char *str)
{

    if (strlen(str) < 4)
    {
        printError(2, " ");
        return 1;
    }

    if (str[0] != '$')
    {
        printError(0, " ");
        return 1;
    }
    if (str[1] == ' ' || str[1] == '\0')
    {
        printError(2, " ");
        return 1;
    }
    int nameEnd = 0;
    int valueStart = 0;
    int valueEnd = 0;

    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '=')
        {
            if (nameEnd != 0)
            {
                printError(2, " ");
                return 1;
            }
            else
            {
                nameEnd = i;
                if (str[i + 1] == '\0' || str[i + 1] == ' ')
                {
                    printError(2, " ");
                    return 1;
                }
                else
                {
                    valueStart = i + 1;
                }
            }
        }

        if (str[i] == ' ')
        {
            if (nameEnd == 0 && valueEnd == 0)
            {
                printError(2, " ");
                return 1;
            }
        }

        if (str[i + 1] == '\0')
        {
            if (nameEnd != 0 && valueStart != 0)
            {
                valueEnd = i;
                break;
            }
            else
            {
                printError(2, " ");
                return 1;
            }
        }
    }
    EnvVar temp;
    int nameSize = nameEnd - 1;
    temp.name = (char *)malloc(nameSize + 1);
    strncpy(temp.name, &str[1], nameSize);
    temp.name[nameSize] = '\0';

    int valueSize = valueEnd - valueStart;
    temp.value = (char *)malloc(sizeof(char) * (valueSize + 1));
    strncpy(temp.value, &str[valueStart], valueSize + 1);
    temp.value[valueSize] = '\0';

    pushEnvArray(&envVars, temp);

    free(temp.name);
    free(temp.value);
    return 0;
}

int processSystem(char *str)
{
    printError(0, " ");
    return 1;
}

int main(int argc, char *argv[])
{
    if (argc > 2)
    {
        perror("ERROR, too many arguments");
        exit(1);
    }
    if (argc == 2)
    {
        FILE *file = fopen(argv[1], "r");
        if (file == NULL)
        {
            perror("Failed to open file");
            exit(1);
        }
        char buffer[256];
        initCommandArray(&commands, 10);
        initEnvArray(&envVars, 10);

        while (fgets(buffer, 256, file) != NULL)
        {
            mainLoop(buffer, true);
        }
        mainLoop("exit", false);
    }
    if (argc == 1)
    {
        char str[100];

        initCommandArray(&commands, 10);
        initEnvArray(&envVars, 10);

        while (1)
        {
            if (currentTheme == white)
            {
                printf("%s", SHELL_NAME);
            }
            else
            {
                printf("%s%s%s", currentThemeCode, SHELL_NAME, CLEAR_CODE);
            }
            fgets(str, sizeof(str), stdin);
            mainLoop(str, false);
        }
    }
    return 0;
}

void mainLoop(char *str, bool file)
{
    int ret = processInput(str);
    int status;
    switch (ret)
    {
    case -1: //could be non-built-in or error
        status = processSystem(str);
        str[strlen(str) - 1] = '\0';
        saveCommand(str, 0, false);
        break;
    case quit:
        if (str[5] == '\0')
        {
            if (currentTheme == white)
            {
                printf("Bye!");
            }
            else
            {
                printf("%sBye!%s", currentThemeCode, CLEAR_CODE);
            }
            freeCommandArray(&commands);
            freeEnvArray(&envVars);
            exit(0);
        }
        break;
    case log:;
        status = processLog(str);
        saveCommand("log", status, false);
        break;
    case print:
        status = processPrint(str);
        saveCommand("print", status, false);
        break;
    case theme:;
        status = processTheme(str);
        saveCommand("theme", status, false);
        break;
    case $var:
        status = processVar(str);
        if (status != -1)
        {
            if (file)
            {
                saveCommand(str, status, true);
            }
            else
            {
                saveCommand(str, status, true);
            }
        }
        break;

    default:
        break;
    }
}