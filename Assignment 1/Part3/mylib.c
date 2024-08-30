// #include <stdio.h>

// void *memalloc(unsigned long size) 
// {
// 	printf("memalloc() called\n");
// 	return NULL;
// }

// int memfree(void *ptr)
// {
// 	printf("memfree() called\n");
// 	return 0;
// }	
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

// Define the metadata structure to keep track of allocated and free chunks
typedef struct Metadata {
    size_t size;
    struct Metadata* next;
} Metadata;

// Define the size of the memory chunk obtained from the OS (4MB)
#define CHUNK_SIZE (4 * 1024 * 1024)

// Global pointer to the first chunk of memory
Metadata* mem_start = NULL;

// Helper function to split a free chunk into allocated and free parts
void splitChunk(Metadata* chunk, size_t size) {
    Metadata* newFreeChunk = (Metadata*)((char*)chunk + size);
    newFreeChunk->size = chunk->size - size;
    newFreeChunk->next = chunk->next;
    chunk->size = size;
    chunk->next = newFreeChunk;
}

void* memalloc(unsigned long size) {
    // Check if size is zero or invalid
    if (size == 0) return NULL;

    // Make sure size is a multiple of 4MB
    size = ((size - 1) / CHUNK_SIZE + 1) * CHUNK_SIZE;

    // Search for a free chunk that fits the requested size
    Metadata* prev = NULL;
    Metadata* curr = mem_start;
    while (curr != NULL) {
        if (curr->size >= size) {
            // Found a free chunk that can satisfy the request
            if (curr->size > size) {
                splitChunk(curr, size);
            }
            // Mark the chunk as allocated
            curr->size = -curr->size;
            return (void*)(curr + 1);
        }
        prev = curr;
        curr = curr->next;
    }

    // If no free chunk is large enough, request a new chunk from the OS
    size_t chunk_size = size + sizeof(Metadata);
    Metadata* newChunk = (Metadata*)mmap(NULL, chunk_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (newChunk == MAP_FAILED) {
        return NULL; // Failed to allocate memory from OS
    }

    // Initialize the new chunk's metadata
    newChunk->size = -chunk_size;
    newChunk->next = NULL;

    // Add the new chunk to the end of the list
    if (prev == NULL) {
        mem_start = newChunk;
    } else {
        prev->next = newChunk;
    }

    // Return the allocated memory
    return (void*)(newChunk + 1);
}

int memfree(void* ptr) {
    // Check if the pointer is NULL
    if (ptr == NULL) return 0;

    // Calculate the metadata pointer
    Metadata* chunk = (Metadata*)((char*)ptr - sizeof(Metadata));

    // Mark the chunk as free
    chunk->size = -chunk->size;

    return 0;
}

// Example of how to use memalloc and memfree
int main() {
    // Allocate memory
    void* p1 = memalloc(100);
    void* p2 = memalloc(200);
    void* p3 = memalloc(400);

    // Free memory
    memfree(p1);
    memfree(p2);
    memfree(p3);

    return 0;
}
