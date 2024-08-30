
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>


///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit) 
{
	struct exec_context *ctx = get_current_ctx();
	for (int i = 0; i < MAX_MM_SEGS; i++)
	{
		if (i == 3 && buff >= ctx->mms[i].start && buff+count<= ctx->mms[i].end )
		{
			return access_bit & ctx->mms[i].access_flags;
		}
		else if (buff >= ctx->mms[i].start && buff+count<= ctx->mms[i].next_free )
		{
			return access_bit & ctx->mms[i].access_flags;
		}
	}
	struct vm_area *vm_area = ctx->vm_area;
	while (vm_area != NULL)
	{
		if (buff >= vm_area->vm_start && buff +count<= vm_area->vm_end )
		{
			return access_bit & vm_area->access_flags;
		}
		vm_area = vm_area->vm_next;
	}

	return 0;
	// struct exec_context *ctx  = get_current_ctx();
	// // struct mm_segment mms[MAX_MM_SEGS] = ctx->mms;
  	// for (int i = 0; i < MAX_MM_SEGS; i++) {
  	//   if (buff >= ctx->mms[i].start && buff+count <= ctx->mms[i].end) {
  	//     if (ctx->mms[i].access_flags & access_bit) {
  	//       return 1;
  	//     }
  	//   }
  	// }
	// struct vm_area *vm_area = ctx->vm_area;
  	// while (vm_area != NULL) {
  	//   if (buff >= vm_area->vm_start && buff+count <= vm_area->vm_end) {
  	//     if (vm_area->access_flags & access_bit) {
  	//       return 1;
  	//     }
  	//   }
  	//   vm_area = vm_area->vm_next;
  	// }
  	// return 0;
}

long trace_buffer_close(struct file *filep)
{
	if (filep == NULL || filep->type != TRACE_BUFFER) {
    	return -EINVAL;
  	}
  	struct trace_buffer_info *trace_buffer = filep->trace_buffer;

  	os_page_free(USER_REG ,trace_buffer->buffer);
  	os_free(trace_buffer,sizeof(struct trace_buffer_info));
  	os_free(filep->fops , sizeof(struct fileops));
  	os_free(filep,sizeof(struct file));
	return 0;	
}

int trace_buffer_read(struct file *filep, char *buff, u32 count) {
  	if (filep == NULL || filep->type != TRACE_BUFFER) {
    	return -EINVAL;
  	}
	if (is_valid_mem_range((unsigned long)buff, count, O_WRITE) == 0)
	{
		return -EBADMEM;
	}
  	struct trace_buffer_info *trace_buffer = filep->trace_buffer;

  	// Check if the trace buffer is empty.
  	if (trace_buffer->isEmpty) {
  	  return 0;
  	}
	int bytes_read = 0;
	char* buffer = trace_buffer->buffer;
	for(int i=0; i < count; i++)
	{
		if((i+trace_buffer->read)%4096 == trace_buffer->write && i!=0) 
		{
			trace_buffer->isEmpty = 1;
			break;
		}
		buff[i]=buffer[(i+trace_buffer->read)%4096];
		bytes_read++;
	} 
	trace_buffer->read = (trace_buffer->read + bytes_read)%4096;
	if(bytes_read > 0 && trace_buffer->read == trace_buffer->write) trace_buffer->isEmpty = 1;
	return bytes_read;
}

int trace_buffer_write(struct file *filep, char *buff, u32 count)
{
	if (filep == NULL || filep->type != TRACE_BUFFER) {
	  return -EINVAL;
	}	
	if (is_valid_mem_range((unsigned long)buff, count, 1) == 0)
	{
		return -EBADMEM;
	}
	struct trace_buffer_info *trace_buffer = filep->trace_buffer;	
	int bytes_written = 0;
	char* buffer = trace_buffer->buffer;
	for(int i=0; i < count; i++)
	{
		if((i+trace_buffer->write)%4096 == trace_buffer->read && i!=0)
		break;
		buffer[(i+trace_buffer->write)%4096] = buff[i];
		bytes_written++;
	} 
	trace_buffer->write = (trace_buffer->write+ bytes_written)%4096;
	trace_buffer->isEmpty = 0;
	return bytes_written;
}

int sys_create_trace_buffer(struct exec_context *current, int mode)
{
	int fd;
  	for (fd = 0; fd < MAX_OPEN_FILES; fd++) {
  	  if (current->files[fd] == NULL) {
  	    break;
  	  }
  	}

  	if (fd == MAX_OPEN_FILES) {
  	  return -EINVAL;
  	}
  	struct file *file = os_alloc(sizeof(struct file));
  	if (file == NULL) {
  	  return -ENOMEM;
  	}

  	file->type = TRACE_BUFFER;
  	file->mode = O_RDWR;
  	file->offp = 0;
  	file->ref_count = 1;
  	file->inode = NULL;
  	file->trace_buffer = NULL;
  	file->fops = NULL;

  	struct trace_buffer_info *trace_buffer = os_alloc(sizeof(struct trace_buffer_info));
  	if (trace_buffer == NULL) {
  	  os_free(file,sizeof(struct file));
  	  return -ENOMEM;
  	}

  	trace_buffer->read = 0;
  	trace_buffer->write = 0;
  	trace_buffer->buffer = os_page_alloc(USER_REG);
	trace_buffer->isEmpty = 1;
  	if (trace_buffer->buffer == NULL) {
	    os_free(file , sizeof(struct file));
		os_free(trace_buffer , sizeof(struct trace_buffer_info));
  		return -ENOMEM;
  	}
  	file->trace_buffer = trace_buffer;
  	struct fileops *fops = os_alloc(sizeof(struct fileops));
  	if (fops == NULL) {
	    os_free(file , sizeof(struct file));
    	os_page_free(USER_REG ,trace_buffer->buffer);
		os_free(trace_buffer , sizeof(struct trace_buffer_info));
  	  	return -ENOMEM;
  	}

  	fops->read = trace_buffer_read;
  	fops->write = trace_buffer_write;
  	fops->close = trace_buffer_close;

  	file->fops = fops;
  	current->files[fd] = file;
  	return fd;
}

///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{
	struct exec_context *current = get_current_ctx();
	if (current == NULL)
		return -EINVAL;
	if (syscall_num == SYSCALL_START_STRACE || syscall_num == SYSCALL_END_STRACE)
		return 0;
	if (current->st_md_base->is_traced == 0)
		return 0;
	int arguments_num = 0;
	if (syscall_num == 2 || syscall_num == 10 || syscall_num == 11 || syscall_num == 13 || syscall_num == 15 || syscall_num == 20 || syscall_num == 21 || syscall_num == 22 || syscall_num == 38)
		arguments_num = 0;
	else if (syscall_num == 1 || syscall_num == 6 || syscall_num == 7 || syscall_num == 12 || syscall_num == 14 || syscall_num == 19 || syscall_num == 27 || syscall_num == 29)
		arguments_num = 1;
	else if (syscall_num == 4 || syscall_num == 8 || syscall_num == 9 || syscall_num == 17 || syscall_num == 23 || syscall_num == 28 || syscall_num == 37 || syscall_num == 40)
		arguments_num = 2;
	else if (syscall_num == 5 || syscall_num == 18 || syscall_num == 24 || syscall_num == 25 || syscall_num == 30 || syscall_num == 39 || syscall_num == 41)
		arguments_num = 3;
	else if (syscall_num == 16 || syscall_num == 35)
		arguments_num = 4;
	else
		return -EINVAL;
	int fd = current->st_md_base->strace_fd;
	int mode = current->st_md_base->tracing_mode;
	struct file *buffer_file = current->files[fd];
	struct trace_buffer_info *curr_trace_buffer = buffer_file->trace_buffer;
	char *buff = (char *)curr_trace_buffer->buffer;
	int writeOff = curr_trace_buffer->write;
	u64 arr[5] = {syscall_num , param1 , param2 , param3 , param4};
	if(mode == FULL_TRACING)
	{
		for (int i = 0; i <= arguments_num; i++)
		{
			for (int j = 0; j < 8; j++)
				buff[j + writeOff] = *((char *)((arr+i)) + j);

			writeOff = (writeOff + 8) % 4096;
		}
		curr_trace_buffer->write = writeOff;
		curr_trace_buffer->isEmpty = 0;
		return 0;
	}
	else if(mode == FILTERED_TRACING)
	{
		int flag = 0;
		struct strace_info *head = current->st_md_base->next;
		while (head != NULL)
		{
			if (head->syscall_num == syscall_num)
			{
				flag = 1;
				break;
			}
			head = head->next;
		}
		if (flag)
		{
			for (int i = 0; i <= arguments_num; i++)
			{
				for (int j = 0; j < 8; j++)
					buff[j + writeOff] = *((char *)((arr+i)) + j);
				
				writeOff = (writeOff + 8) % 4096;
			}
		curr_trace_buffer->write = writeOff;
		curr_trace_buffer->isEmpty = 0;
		}
	}
	return 0;
}


int sys_strace(struct exec_context *current, int syscall_num, int action)
{
	if (current == NULL)
		return -EINVAL;
	else if (action != ADD_STRACE && action != REMOVE_STRACE)
		return -EINVAL;
	else if (current->st_md_base == NULL)
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
		struct strace_info *head_pointer = current->st_md_base->next;
		struct strace_info *temp = NULL;
		if (head_pointer == NULL)
		{
			struct strace_info *first_syscall = os_alloc(sizeof(struct strace_info));
			first_syscall->syscall_num = syscall_num;
			first_syscall->next = NULL;
			current->st_md_base->next = first_syscall;
			current->st_md_base->last = first_syscall;
			current->st_md_base->count++;
			return 0;
		}
		else if(current->st_md_base->count == STRACE_MAX)
				return -EINVAL;
		else
		{
			while (head_pointer != NULL)
			{
				if (head_pointer->syscall_num == syscall_num)
				{
					return -EINVAL;
				}
				else
				{
					temp = head_pointer;
					head_pointer = head_pointer->next;
				}
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
		struct strace_info *head_pointer = current->st_md_base->next;
		if (head_pointer == NULL)
		{
			return -EINVAL;
		}
		else
		{
			struct strace_info *temp = NULL;
			while (head_pointer != NULL && head_pointer->syscall_num != syscall_num)
			{
				temp = head_pointer;
				head_pointer = head_pointer->next;
			}
			if (head_pointer == NULL)
			{
				return -EINVAL;
			}
			temp->next = head_pointer->next;
			os_free(head_pointer, sizeof(struct strace_info));
			current->st_md_base->count--;
			return 0;
		}
	}	
	return 0;
}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{
	if (filep == NULL) {
    	return -EINVAL;
  	}

  	if (buff == NULL) {
    	return -EINVAL;
  	}
	struct trace_buffer_info *curr_trace_buffer = filep->trace_buffer;
	char *trace_buff = curr_trace_buffer->buffer;
	int readOff = curr_trace_buffer->read;
	int bytes_read = 0;
	for (int i = 0; i < count; i++)
	{
		if(curr_trace_buffer->isEmpty) return bytes_read;
		u64 syscall_num = 0;
		int temp = trace_buffer_read(filep , buff+bytes_read , 8);
		if(temp<8)
		{
			return -EINVAL;
		}
		syscall_num = *(u64*)(buff+bytes_read);
		bytes_read += 8;
		int arguments_num;
		if (syscall_num == 2 || syscall_num == 10 || syscall_num == 11 || syscall_num == 13 || syscall_num == 15 || syscall_num == 20 || syscall_num == 21 || syscall_num == 22 || syscall_num == 38)
			arguments_num = 0;
		else if (syscall_num == 1 || syscall_num == 6 || syscall_num == 7 || syscall_num == 12 || syscall_num == 14 || syscall_num == 19 || syscall_num == 27 || syscall_num == 29)
			arguments_num = 1;
		else if (syscall_num == 4 || syscall_num == 8 || syscall_num == 9 || syscall_num == 17 || syscall_num == 23 || syscall_num == 28 || syscall_num == 37 || syscall_num == 40)
			arguments_num = 2;
		else if (syscall_num == 5 || syscall_num == 18 || syscall_num == 24 || syscall_num == 25 || syscall_num == 30 || syscall_num == 39 || syscall_num == 41)
			arguments_num = 3;
		else if (syscall_num == 16 || syscall_num == 35)
			arguments_num = 4;
		else
			return -EINVAL;
		for (int k = 0; k <arguments_num; k++)
		{
			if(trace_buffer_read(filep , buff+bytes_read , 8)<8)
			return -EINVAL;
			bytes_read += 8;
			readOff = (readOff + 8) % 4096;
		}
	}
	return bytes_read;
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{
  	// Check if the file descriptor is valid.
  	if (fd < 0 || fd >= MAX_OPEN_FILES) {
  	  	return -EINVAL;
  	}

  	// Check if the tracing mode is valid.
  	if (tracing_mode != FULL_TRACING && tracing_mode != FILTERED_TRACING) {
	  	return -EINVAL;
  	}

  	// Enable system call tracing for the current process.
	if (current->st_md_base == NULL)
	{
		struct strace_head *head = os_alloc(sizeof(struct strace_head));
		if (head == NULL)
		return -EINVAL;
		
		current->st_md_base = head;
		head->count = 0;
		head->is_traced = 1;
		head->strace_fd = fd;
		head->tracing_mode = tracing_mode;
		head->next = NULL;
		head->last = NULL;
		return 0;
	}
  	current->st_md_base->is_traced = 1;
  	current->st_md_base->strace_fd = fd;
  	current->st_md_base->tracing_mode = tracing_mode;
  	return 0;
}

int sys_end_strace(struct exec_context *current)
{
	if (current == NULL || current->st_md_base->is_traced == 0)
		return -EINVAL;
	struct strace_head *head_list = current->st_md_base;
	struct strace_info *curr = head_list->next;
	struct strace_info *next;
	while (curr != NULL)
	{
		next = curr->next;
		os_free(curr, sizeof(struct strace_info));
		curr = next;
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

//Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{
        return 0;
}


int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{
    return 0;
}

