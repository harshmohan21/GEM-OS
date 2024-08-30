#include <context.h>
#include <memory.h>
#include <lib.h>
#include <entry.h>
#include <file.h>
#include <tracer.h>

///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit)
{
	int readCall = access_bit & 1;
	int writeCall = (access_bit >> 1) & 1;
	struct exec_context *ctx = get_current_ctx();

	for (int i = 0; i < MAX_MM_SEGS; i++)
	{
		if (buff >= ctx->mms[i].start && buff <= ctx->mms[i].next_free - 1)
		{
			if (writeCall)
			{
				if ((ctx->mms[i].access_flags >> 1) & 1)
				{
					return 1;
				}
				else
				{
					return 0;
				}
			}
			if (readCall)
			{
				if ((ctx->mms[i].access_flags) & 1)
				{
					return 1;
				}
				else
				{
					return 0;
				}
			}
		}
	}

	struct vm_area *p = ctx->vm_area;
	while (p != NULL)
	{
		if (buff >= p->vm_start && buff <= p->vm_end - 1)
		{
			if (writeCall)
			{
				if ((p->access_flags >> 1) & 1)
				{
					return 1;
				}
				else
				{
					return 0;
				}
			}
			if (readCall)
			{
				if ((p->access_flags) & 1)
				{
					return 1;
				}
				else
				{
					return 0;
				}
			}
		}
		p = p->vm_next;
	}

	return 0;
}

long trace_buffer_close(struct file *filep)
{
	os_page_free(USER_REG, filep->trace_buffer->buffer);
	os_free(filep->trace_buffer, sizeof(struct trace_buffer_info));
	os_free(filep->fops, sizeof(struct fileops));
	os_free(filep, sizeof(struct file));
	filep = NULL;
	return 0;
}

int trace_buffer_read(struct file *filep, char *buff, u32 count)
{
	if (filep->mode == 1 || filep->mode == 4 || filep->mode == 5)
	{
		return -EINVAL;
	}
	if (is_valid_mem_range(buff, count, 2) == 0)
	{
		return -EBADMEM;
	}
	int R = filep->trace_buffer->readOffset;
	int W = filep->trace_buffer->writeOffset;
	char *trace_buff = (char *)filep->trace_buffer->buffer;
	int read_capacity = (4096 + W - R) % 4096;
	if (read_capacity == 0 && filep->trace_buffer->isFull)
	{
		read_capacity = 4096;
	}
	else if (read_capacity == 0 && !filep->trace_buffer->isFull)
	{
		return 0;
	}
	int i;
	for (i = 0; i < count && i < read_capacity; i++)
	{
		buff[i] = trace_buff[R];
		R = (R + 1) % 4096;
	}
	filep->trace_buffer->readOffset = R;
	if (i == read_capacity)
	{
		filep->trace_buffer->isFull = 1;
	}
	return i;
}

int trace_buffer_write(struct file *filep, char *buff, u32 count)
{
	if (filep->mode == 2 || filep->mode == 4 || filep->mode == 6)
	{
		return -EINVAL;
	}

	if (is_valid_mem_range(buff, count, 1) == 0)
	{
		return -EBADMEM;
	}

	int R = filep->trace_buffer->readOffset;
	int W = filep->trace_buffer->writeOffset;
	char *trace_buff = (char *)filep->trace_buffer->buffer;
	int write_capacity = (4096 + R - W) % 4096;
	if (write_capacity == 0 && filep->trace_buffer->isFull)
	{
		return 0;
	}
	else if (write_capacity == 0 && !filep->trace_buffer->isFull)
	{
		write_capacity = 4096;
	}

	int i;
	for (i = 0; i < count && i < write_capacity; i++)
	{
		trace_buff[W] = buff[i];
		W = (W + 1) % 4096;
	}
	filep->trace_buffer->writeOffset = W;
	if (i == write_capacity)
	{
		filep->trace_buffer->isFull = 1;
	}
	return i;
}

int sys_create_trace_buffer(struct exec_context *current, int mode)
{
	int i;
	for (i = 0; i < MAX_OPEN_FILES; i++)
	{
		if (current->files[i] == NULL)
		{
			break;
		}
	}
	if (i == MAX_OPEN_FILES)
	{
		return -EINVAL;
	}

	struct file *new_file = os_alloc(sizeof(struct file));
	if (new_file == NULL)
	{
		return -ENOMEM;
	}

	new_file->type = TRACE_BUFFER;
	new_file->mode = mode;
	new_file->offp = 0; // Latest read or write offset
	new_file->ref_count = 1;
	new_file->inode = NULL;

	new_file->trace_buffer = os_alloc(sizeof(struct trace_buffer_info));
	if (new_file->trace_buffer == NULL)
	{
		os_free(new_file, sizeof(struct file)); // Free the previously allocated memory.
		return -ENOMEM;
	}

	new_file->trace_buffer->readOffset = 0;
	new_file->trace_buffer->writeOffset = 0;
	new_file->trace_buffer->isFull = 0;
	new_file->trace_buffer->buffer = os_page_alloc(USER_REG);
	if (new_file->trace_buffer->buffer == NULL)
	{
		// Memory allocation failure.
		os_free(new_file->trace_buffer, sizeof(struct trace_buffer_info)); // Free the allocated trace buffer.
		os_free(new_file, sizeof(struct file));														 // Free the allocated file.
		return -ENOMEM;
	}

	// Allocate and initialize the fileops object.
	struct fileops *new_fops = os_alloc(sizeof(struct fileops));
	if (new_fops == NULL)
	{
		os_page_free(USER_REG, new_file->trace_buffer->buffer);						 // Free the allocated buffer.
		os_free(new_file->trace_buffer, sizeof(struct trace_buffer_info)); // Free the allocated trace buffer.
		os_free(new_file, sizeof(struct file));														 // Free the allocated file.
		return -ENOMEM;
	}

	new_fops->read = trace_buffer_read;		// Assign the appropriate read function.
	new_fops->write = trace_buffer_write; // Assign the appropriate write function.
	new_fops->lseek = NULL;								// Implement lseek as required.
	new_fops->close = trace_buffer_close; // Implement close as required.

	// Assign the fileops object to the file.
	new_file->fops = new_fops;

	// Finally, return the file descriptor to the user.
	current->files[i] = new_file;
	return i;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////

int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{
	struct exec_context *current = get_current_ctx;
	if (current == NULL)
	{
		return -EINVAL;
	}
	if (syscall_num == SYSCALL_START_STRACE || syscall_num == SYSCALL_END_STRACE)
	{
		return 0;
	}
	int fd = current->st_md_base->strace_fd;
	int mode = current->st_md_base->tracing_mode;
	struct file *trace_buffer_file = current->files[fd];
	struct trace_buffer_info *curr_trace_buffer = trace_buffer_file->trace_buffer;
	int num_of_arguements = 0;
	if (current->st_md_base->is_traced == 0)
	{
		return 0;
	}

	if (syscall_num == 2 || syscall_num == 10 || syscall_num == 11 || syscall_num == 13 || syscall_num == 15 || syscall_num == 20 || syscall_num == 21 || syscall_num == 22 || syscall_num == 38)
	{
		// no arguements
		num_of_arguements = 0;
	}
	else if (syscall_num == 1 || syscall_num == 6 || syscall_num == 7 || syscall_num == 12 || syscall_num == 14 || syscall_num == 19 || syscall_num == 27 || syscall_num == 29)
	{
		// 1 arguement
		num_of_arguements = 1;
	}
	else if (syscall_num == 4 || syscall_num == 8 || syscall_num == 9 || syscall_num == 17 || syscall_num == 23 || syscall_num == 28 || syscall_num == 37 || syscall_num == 40)
	{
		// 2 arguements
		num_of_arguements = 2;
	}
	else if (syscall_num == 5 || syscall_num == 18 || syscall_num == 24 || syscall_num == 25 || syscall_num == 30 || syscall_num == 39 || syscall_num == 41)
	{
		// 3 arguements
		num_of_arguements = 3;
	}
	else if (syscall_num == 16 || syscall_num == 35)
	{
		// 4 arguements
		num_of_arguements = 4;
	}
	else
	{
		return -EINVAL;
	}
	char *buff = (char *)curr_trace_buffer->buffer;
	int readOff = curr_trace_buffer->readOffset;
	int writeOff = curr_trace_buffer->writeOffset;
	if (mode == FULL_TRACING)
	{
		for (int i = 0; i < num_of_arguements + 1; i++)
		{
			if (i == 0)
			{
				for (int j = 0; j < 8; j++)
				{
					buff[j + writeOff] = *((char *)(&syscall_num) + j);
				}
			}
			else if (i == 1)
			{
				for (int j = 0; j < 8; j++)
				{
					buff[j + writeOff] = *((char *)(&param1) + j);
				}
			}
			else if (i == 2)
			{
				for (int j = 0; j < 8; j++)
				{
					buff[j + writeOff] = *((char *)(&param2) + j);
				}
			}
			else if (i == 3)
			{
				for (int j = 0; j < 8; j++)
				{
					buff[j + writeOff] = *((char *)(&param3) + j);
				}
			}
			else if (i == 4)
			{
				for (int j = 0; j < 8; j++)
				{
					buff[j + writeOff] = *((char *)(&param4) + j);
				}
			}
			writeOff = (writeOff + 8) % 4096;
		}
		return 0;
	}
	else if (mode == FILTERED_TRACING)
	{
		int found = 0;
		struct strace_info *headpointer = current->st_md_base->next;
		while (headpointer != NULL)
		{
			if (headpointer->syscall_num == syscall_num)
			{
				found = 1;
				break;
			}
			headpointer = headpointer->next;
		}
		if (found)
		{
			for (int i = 0; i < num_of_arguements + 1; i++)
			{
				if (i == 0)
				{
					for (int j = 0; j < 8; j++)
					{
						buff[j + writeOff] = *((char *)(&syscall_num) + j);
					}
				}
				else if (i == 1)
				{
					for (int j = 0; j < 8; j++)
					{
						buff[j + writeOff] = *((char *)(&param1) + j);
					}
				}
				else if (i == 2)
				{
					for (int j = 0; j < 8; j++)
					{
						buff[j + writeOff] = *((char *)(&param2) + j);
					}
				}
				else if (i == 3)
				{
					for (int j = 0; j < 8; j++)
					{
						buff[j + writeOff] = *((char *)(&param3) + j);
					}
				}
				else if (i == 4)
				{
					for (int j = 0; j < 8; j++)
					{
						buff[j + writeOff] = *((char *)(&param4) + j);
					}
				}
				writeOff = (writeOff + 8) % 4096;
			}
			curr_trace_buffer->writeOffset = writeOff;
			return 0;
		}
		else
		{
			return 0;
		}
	}

}

int sys_strace(struct exec_context *current, int syscall_num, int action)
{
	if (current == NULL)
	{
		return -EINVAL;
	}
	if (action != ADD_STRACE && action != REMOVE_STRACE)
	{
		return -EINVAL;
	}
	if (current->st_md_base == NULL)
	{
		struct strace_head *head = os_alloc(sizeof(struct strace_head));
		current->st_md_base = head;
		head->count = 0;
		head->is_traced = 0;
		head->last = NULL;
		head->next = NULL;
	}
	if (action == ADD_STRACE)
	{
		struct strace_info *head_info = current->st_md_base->next;
		struct strace_info *temp = NULL;
		if (head_info == NULL)
		{
			struct strace_info *new_strace = os_alloc(sizeof(struct strace_info));
			new_strace->syscall_num = syscall_num;
			new_strace->next = NULL;
			current->st_md_base->next = new_strace;
			current->st_md_base->last = new_strace;
			current->st_md_base->count++;
			return 0;
		}
		else
		{
			while (head_info != NULL)
			{
				if (head_info->syscall_num == syscall_num)
				{
					return -EINVAL;
				}
				else
				{
					temp = head_info;
					head_info = head_info->next;
				}
			}
			if (current->st_md_base->count == STRACE_MAX)
			{
				return -EINVAL;
			}
			struct strace_info *new_strace = os_alloc(sizeof(struct strace_info));
			new_strace->syscall_num = syscall_num;
			new_strace->next = NULL;
			temp->next = new_strace;
			current->st_md_base->last = new_strace;
			current->st_md_base->count++;
			return 0;
		}
	}
	else if (action == REMOVE_STRACE)
	{
		struct strace_info *head_info = current->st_md_base->next;
		if (head_info == NULL)
		{
			return -EINVAL;
		}
		if (head_info->syscall_num == syscall_num)
		{
			current->st_md_base->next = head_info->next;
			current->st_md_base->count--;
			os_free(head_info, sizeof(struct strace_info));
			return 0;
		}
		else
		{
			struct strace_info *temp = NULL;
			while (head_info != NULL && head_info->syscall_num != syscall_num)
			{
				temp = head_info;
				head_info = head_info->next;
			}
			if (head_info == NULL)
			{
				return -EINVAL;
			}
			temp->next = head_info->next;
			os_free(head_info, sizeof(struct strace_info));
			current->st_md_base->count--;
			return 0;
		}
	}
}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{
	struct trace_buffer_info *curr_trace_buffer = filep->trace_buffer;
	char *trace_buff = curr_trace_buffer->buffer;
	int readOff = curr_trace_buffer->readOffset;
	int bytes_read = 0;
	for (int i = 0; i < count; i++)
	{
		u64 syscall_num = 0;
		for (int j = 0; j < 8; j++)
		{
			buff[bytes_read + j] = trace_buff[readOff + j];
			syscall_num = (syscall_num << 8) | trace_buff[readOff + j];
		}
		bytes_read += 8;
		readOff = (readOff + 8) % 4096;

		int num_of_arguements = 0;
		if (syscall_num == 2 || syscall_num == 10 || syscall_num == 11 || syscall_num == 13 || syscall_num == 15 || syscall_num == 20 || syscall_num == 21 || syscall_num == 22 || syscall_num == 38)
		{
			// no arguements
			num_of_arguements = 0;
		}
		else if (syscall_num == 1 || syscall_num == 6 || syscall_num == 7 || syscall_num == 12 || syscall_num == 14 || syscall_num == 19 || syscall_num == 27 || syscall_num == 29)
		{
			// 1 arguement
			num_of_arguements = 1;
		}
		else if (syscall_num == 4 || syscall_num == 8 || syscall_num == 9 || syscall_num == 17 || syscall_num == 23 || syscall_num == 28 || syscall_num == 37 || syscall_num == 40)
		{
			// 2 arguements
			num_of_arguements = 2;
		}
		else if (syscall_num == 5 || syscall_num == 18 || syscall_num == 24 || syscall_num == 25 || syscall_num == 30 || syscall_num == 39 || syscall_num == 41)
		{
			// 3 arguements
			num_of_arguements = 3;
		}
		else if (syscall_num == 16 || syscall_num == 35)
		{
			// 4 arguements
			num_of_arguements = 4;
		}
		for (int k = 0; k < num_of_arguements; k++)
		{
			for (int j = 0; j < 8; j++)
			{
				buff[bytes_read + j] = trace_buff[readOff + j];
			}
			bytes_read += 8;
			readOff = (readOff + 8) % 4096;
		}
	}
	return bytes_read;
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{
	if (current == NULL)
	{
		return -EINVAL;
	}
	if (tracing_mode != FULL_TRACING && tracing_mode != FILTERED_TRACING)
	{
		return -EINVAL;
	}
	if (current->st_md_base == NULL)
	{
		struct strace_head *head = os_alloc(sizeof(struct strace_head));
		if (head == NULL)
		{
			return -EINVAL; // memalloc failure
		}
		current->st_md_base = head;
		head->count = 0;
		head->is_traced = 1;
		head->strace_fd = fd;
		head->tracing_mode = tracing_mode;
		head->next = NULL;
		head->last = NULL;
		return 0;
	}
	else
	{
		struct strace_head *head = current->st_md_base;
		head->is_traced = 1;
		head->strace_fd = fd;
		head->tracing_mode = tracing_mode;
		return 0;
	}
}

int sys_end_strace(struct exec_context *current)
{
	if (current == NULL || current->st_md_base->is_traced == 0)
	{
		return -EINVAL;
	}
	struct strace_head *head_list = current->st_md_base;
	struct strace_info *curr = head_list->next;
	struct strace_info *nex;
	while (curr != NULL)
	{
		nex = curr->next;
		os_free(curr, sizeof(struct strace_info));
		curr = nex;
	}
	os_free(head_list, sizeof(struct strace_head));
	current->st_md_base = NULL;
	return 0;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////

long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{
	return 0;
}

// Fault handlerma
long handle_ftrace_fault(struct user_regs *regs)
{
	return 0;
}

int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{
	return 0;
}
// #include<ulib.h>

// int main (u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5) {

//         int fd = create_trace_buffer(O_RDWR);
// 	char buff[10];
// 	char buff2[10];

// 	for(int i = 0; i< 10; i++){
// 		buff[i] = 'A' + i;
// 	}

// 	int ret = write(fd, buff, 10);
// //	printf("ret value from write: %d\n", ret);
// 	if(ret != 10){
// 		printf("1.Test case failed\n");
// 		return -1;
// 	}

// 	int ret2 = read(fd, buff2, 10);
// //	printf("ret value from write: %d\n", ret2);
// 	if(ret2 != 10){
// 		printf("2.Test case failed\n");
// 		return -1;	
// 	}
// 	int ret3 = ustrncmp(buff, buff2, 10);
// 	if(ret3 != 0){
// 		printf("3.Test case failed\n");
// 		return -1;	
// 	}
//         close(fd);
// 	printf("Test case passed\n");
// 	return 0;
// }
