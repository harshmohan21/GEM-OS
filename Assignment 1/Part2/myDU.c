#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include<unistd.h>
#include <sys/wait.h>
void prt(void)
{
    printf("Unable to execute \n");
    exit(-1);
}
long long resolve_symlink(char *path);
long long calculateDirectorySize(const char *path)
{
    const char *root_directory = path;
    long long total_size =0;
    DIR* directory = opendir(root_directory);
    if(directory == NULL) {
        // perror("Directory not found");
        prt();
        return 1;
    }
    struct dirent* entry;
    while((entry = readdir(directory)) != NULL) 
    {
        if (strcmp(entry->d_name, ".") == 0 ) {
            total_size += 4096;
            continue;
        }
        else if(strcmp(entry->d_name, "..") == 0)
        continue;
        char entry_path[1024];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", root_directory, entry->d_name);
        struct stat entry_stat;
        if (stat(entry_path, &entry_stat) == -1) {
            // perror("lstat");
            prt();
            continue;
        }

        // if (S_ISDIR(entry_stat.st_mode))  **********dont use this it gives wrong results
        if (entry->d_type == DT_DIR) 
        {
            long long sz = 0;
            char *c = (char*)malloc(strlen(path) + strlen(entry->d_name) + 2);
            if(c==NULL){
                // perror("malloc");
                prt();
                exit(-1);
            }
            strcat(c, path);
            strcat(c,"/");
            strcat(c, entry->d_name);
            sz = calculateDirectorySize(c);
            total_size += sz;
        } 
        else if (entry->d_type == DT_REG) {
            total_size += entry_stat.st_size;
        } 
        else 
        {
            char *symlinkPath = (char *)malloc(strlen(path) + strlen(entry->d_name)+2);
            strcat(symlinkPath, path);
            strcat(symlinkPath,"/");
            strcat(symlinkPath, entry->d_name);
            total_size += resolve_symlink(symlinkPath);
        }
    }
    return total_size;
}
long long resolve_symlink(char* symlinkPath) {
    char targetPath[1024];
    char resolvedPath[1024];
    ssize_t bytesRead = readlink(symlinkPath, targetPath, sizeof(targetPath) - 1);
    if (bytesRead == -1) {
        // perror("readlink");
        prt();
        exit(-1);
    }
    targetPath[bytesRead] = '\0';
    for(int i=strlen(symlinkPath)-1;i>=0;i--){
        if(symlinkPath[i]=='/'){
            symlinkPath[i+1] = '\0';
            break;
        }
    }
    strcpy(resolvedPath,symlinkPath);
    strcat(resolvedPath,targetPath);
    struct stat target_stat;
    int x = lstat(resolvedPath, &target_stat);
    if (S_ISLNK(target_stat.st_mode)) {
        return resolve_symlink(resolvedPath);
    }
    else if(S_ISREG(target_stat.st_mode))
    {
        return target_stat.st_size;
    }
    else
        return calculateDirectorySize(resolvedPath);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <directory>\n", argv[0]);
        return 1;
    }
    int fd[2];
    if(pipe(fd) < 0)
    {
        prt();
        exit(-1);
    }

    const char *root_directory = argv[1];
    long long total_size =0;
    DIR* directory = opendir(root_directory);
    if(directory == NULL) {
        prt();
    }
    struct dirent* entry;
    
    while((entry = readdir(directory)) != NULL) 
    {
        if (strcmp(entry->d_name, ".") == 0 ) {
            total_size += 4096;
            continue;
        }
        else if(strcmp(entry->d_name, "..") == 0)
        continue;

        char entry_path[1024];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", root_directory, entry->d_name);

        struct stat entry_stat;
        if (stat(entry_path, &entry_stat) == -1) {
            prt();
            continue; 
        }

        if (entry->d_type == DT_DIR) {
            int pid; 
            long long sz = 0;
            pid = fork();
            if(pid < 0)
            {
                prt();
                exit(-1);
            }
            if(!pid)
            {
                close(fd[0]);
                char *c = (char*)malloc(strlen(argv[1]) + strlen(entry->d_name+2));
                strcat(c, argv[1]);
                strcat(c,"/");
                strcat(c, entry->d_name);
                sz = calculateDirectorySize(c);
                free(c);
                if(write(fd[1] , &sz , sizeof(long long)) == -1)
                {
                    prt();
                }
                exit(0);

            }
            if(pid)
            {
                long long x;
                wait(NULL);
                read(fd[0] , &x , sizeof(long long));
                total_size += x;
            }
        } 
        else if (entry->d_type == DT_REG) {
            total_size += entry_stat.st_size;
        } 
        else{
            char finalPath[1024]; 
            char *symlinkPath = (char *)malloc(strlen(argv[1]) + strlen(entry->d_name)+2);
            strcat(symlinkPath, argv[1]);
            strcat(symlinkPath,"/");
            strcat(symlinkPath, entry->d_name);
            total_size += resolve_symlink(symlinkPath);
        }
    }
    printf("%llu\n" , total_size);
    // prt();
    return 0;
}
