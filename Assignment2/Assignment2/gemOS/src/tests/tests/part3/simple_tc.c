#include<ulib.h>

static int func2(int a)
{
   return (2*a);
}

static int func1(int a, int b)
{
   int ret = 0;
   // ret = func2(a+b);
   ret = a+b;
   return ret;
}

static int func3(int a, int b, int c)
{
    int ret = 0;
    ret = a+b+c;
    return ret;
}

int main(u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
    int ret = 0;
    int ftrace_fd = 0;
    u64 ftrace_buff[512];

    ftrace_fd = create_trace_buffer(O_RDWR);
    if(ftrace_fd != 3)
    {
	printf("0. Test case failed\n");
	return -1;
    }
    
    ret = ftrace((unsigned long)&func1, ADD_FTRACE, 2, ftrace_fd);
    if(ret != 0)
    { 
	printf("1. Test case failed\n");
	return -1;
    }

    ret = ftrace((unsigned long)&func1, ENABLE_FTRACE, 0, ftrace_fd);
    if(ret != 0)
    { 
	printf("2. Test case failed\n");
	return -1;
    }

    ret = ftrace((unsigned long)&func2, ADD_FTRACE, 1, ftrace_fd);
    if(ret != 0)
    { 
	printf("3. Test case failed\n");
	return -1;
    }

    ret = ftrace((unsigned long)&func2, ENABLE_FTRACE, 0, ftrace_fd);
    if(ret != 0)
    { 
	printf("4. Test case failed\n");
	return -1;
    }

    ret = ftrace((unsigned long)&func3, ADD_FTRACE, 3, ftrace_fd);
    if(ret != 0)
    { 
	printf("3. Test case failed\n");
	return -1;
    }

    ret = ftrace((unsigned long)&func3, ENABLE_FTRACE, 0, ftrace_fd);
    if(ret != 0)
    { 
	printf("4. Test case failed\n");
	return -1;
    }
    func1(10,20);
    func2(10);
    // func3(10,50,20);
    int read_ret = read_ftrace(ftrace_fd, ftrace_buff, 4);
    /*for(int i = 0; i<(read_ret/8); i++){
	printf("ftrace_buff[%d] : %x\n", i*8, ftrace_buff[i]);
    }*/
    printf("user %d\n", read_ret);
    if(read_ret != 40)
    { 
	printf("5. Test case failed\n");
	return -1;
    }

    if((u64*)(((u64*)ftrace_buff)[0]) != (u64*)(&func1))
    { 
	printf("6. Test case failed\n");
	return -1;
    }

    if(((u64*)ftrace_buff)[1] != 10)
    { 
	printf("7. Test case failed\n");
	return -1;
    }

    if(((u64*)ftrace_buff)[2] != 20)
    { 
	printf("8. Test case failed\n");
	return -1;
    }

    if((u64*)(((u64*)ftrace_buff)[3]) != (u64*)(&func2))
    { 
	printf("9. Test case failed\n");
	return -1;
    }

    if(((u64*)ftrace_buff)[4] != 10)
    { 
	printf("10. Test case failed\n");
	return -1;
    }
    

    ret = ftrace((unsigned long)&func1, REMOVE_FTRACE, 0, ftrace_fd);
    if(ret != 0)
    { 
	printf("11. Test case failed\n");
	return -1;
    }

    ret = ftrace((unsigned long)&func2, REMOVE_FTRACE, 0, ftrace_fd);
    if(ret != 0)
    { 
	printf("12. Test case failed\n");
	return -1;
    }

    close(ftrace_fd);

    printf("Test case passed\n");

    return 0;
}
  
