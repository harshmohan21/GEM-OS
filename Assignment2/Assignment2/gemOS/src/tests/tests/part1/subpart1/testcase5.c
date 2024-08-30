#include <ulib.h>

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{

	int fd = create_trace_buffer(O_READ);
	// char buff[4146];
	char buff2[4146];
	char *buff = mmap(0, 4146, PROT_WRITE | PROT_READ, MAP_POPULATE);

	for (int i = 0, j = 0; i < 4146; i++)
	{
		j = i % 26;
		buff[i] = 'A' + j;
	}

	int ret = write(fd, buff, 4096);
	if (ret != -1)
	{
		printf("1.Test case failed\n");
		return -1;
	}

	
	printf("Test case passed\n");
	return 0;
}