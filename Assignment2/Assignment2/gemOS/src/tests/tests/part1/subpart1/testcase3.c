#include<ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5){
    int fd = create_trace_buffer(O_RDWR);
	char buff[4096];
	char buff2[4096];

	for(int i = 0; i< 4096; i++){
		buff[i] = 'A';
	}

	int ret = write(fd, buff, 4096);
//	printf("ret value from write: %d\n", ret);
	if(ret != 4096){
		printf("1.Test case failed\n");
		return -1;
	}
    ret = write(fd, buff, 10);
    if(ret != 0){
        printf("%d", ret);
        printf("2. Case failed\n");
        return -1;
    }

	int ret2 = read(fd, buff2, 4096);
//	printf("ret value from write: %d\n", ret2);
	if(ret2 != 4096){
		printf("2.Test case failed\n");
		return -1;	
	}
	int ret3 = ustrncmp(buff, buff2, 10);
	if(ret3 != 0){
		printf("3.Test case failed\n");
		return -1;	
	}

    char buff3[4096];
    char buff4[4096];
    for(int i = 0; i<4096; i++){
        buff3[i] = 'B';
    }

    int ret4 = write(fd, buff3, 4096);
    if(ret4 != 4096){
        printf("4. %d failed\n", ret4);
        return -1;
    }
    int ret5 = read(fd, buff4, 4096);
    if(ret5 != 4096){
        printf("5. %d failed\n", ret5);
    }

    int ret6 = ustrncmp(buff3, buff4, 4096);
	if(ret6 != 0){
		printf("6.Test case failed\n");
		return -1;	
	}

    close(fd);
	printf("Test case passed\n");
	return 0;
}