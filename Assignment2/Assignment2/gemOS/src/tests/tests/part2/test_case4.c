#include<ulib.h>

int main (u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {

    int strace_fd = create_trace_buffer(O_RDWR);
    int rdwr_fd = create_trace_buffer(O_RDWR);	    
	u64 strace_buff[4096];
	int read_buff[4096];
	
	start_strace(strace_fd, FULL_TRACING);
	int read_ret = read(rdwr_fd, read_buff, 10);
	if(read_ret != 0){
        printf("1.Test case failed\n");
        return -1;
    }
    for(int i=0;i<32;i+=2)
    {
        strace(i+2,0);
    }
    int x = strace(40, 0);
    if(x != -1)
    {
        printf("09. Testcase failed\n");
    }
	end_strace();
	strace(16,0);
    start_strace(strace_fd, FILTERED_TRACING);
    char *buff = mmap(0, 4146, PROT_WRITE | PROT_READ, MAP_POPULATE);
    end_strace();
    int strace_ret = read_strace(strace_fd, strace_buff, 2);
	if(strace_ret != 56){
		printf("2.Test case failed\n");
		return -1;
	}
	if(strace_buff[0] != 24){
		printf("3.Test case failed\n");
		return -1;
	}
    if(strace_buff[1] != rdwr_fd)
    {
        printf("4. Test failed\n");
		return -1;
    }
	if((u64*)(strace_buff[2]) != (u64*)&read_buff){
		printf("5.Test case failed\n");
                return -1;
	}
    if(strace_buff[3] != 10)
    {
        printf("6. Test failed\n");
		return -1;
    }
    if(strace_buff[4] != 40)
    {
        printf("7. Test failed\n");
		return -1;
    }
    if(strace_buff[5] != 2)
    {
        printf("8. Test failed\n");
		return -1;
    }
    if(strace_buff[6] != 0)
    {
        printf("9. Test failed\n");
		return -1;
    }
    strace_ret = read_strace(strace_fd, strace_buff, 17);
	if(strace_ret != 424)
	{
		printf("100. Test failed\n");
	}
    int j = 0;
    for(int k=2;k<=30;k+=2)
    {
        if(strace_buff[j++] != 40)
        {
             printf("%d - 10. Test failed\n", k);
			 return -1;
        }
        if(strace_buff[j++] != k+2)
        {
             printf("%d - 11. Test failed\n", k);
			 return -1;
        }
        if(strace_buff[j++] != 0)
        {
             printf("%d - 12. Test failed\n", k);
			 return -1;
        }
    }
    if(strace_buff[j++] != 40)
    {
        printf("13. Test failed\n");
		return -1;
    }
    if(strace_buff[j++] != 40)
    {
        printf("14. Test failed\n");
		return -1;
    }
    if(strace_buff[j++] != 0)
    {
        printf("15. Test failed\n");
		return -1;
    }
    if(strace_buff[j++] != 16)
    {
        printf("16. Test failed\n");
		return -1;
    }
    if(strace_buff[j++] != 0)
    {
        printf("17. Test failed\n");
		return -1;
    }
	if(strace_buff[j++] != 4146)
    {
        printf("18. Test failed\n");
		return -1;
    }
	if(strace_buff[j++] != 3)
    {
        printf("19. Test failed\n");
		return -1;
    }
	if(strace_buff[j++] != 2)
    {
        printf("20. Test failed\n");
		return -1;
    }
	printf("Test case passed\n");
        return 0;
}
