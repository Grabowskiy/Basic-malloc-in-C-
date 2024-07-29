#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <iostream>

typedef char ALIGN[16];

union memblock {
	struct {
		size_t size;
		unsigned is_free;
		union memblock *next;
	} s;
	/* force the memblock to be aligned to 16 bytes */
	ALIGN stub;
};
typedef union memblock memblock_t;

memblock_t *head = NULL, *tail = NULL;
pthread_mutex_t global_malloc_lock;

memblock_t *get_free_block(size_t size)
{
	memblock_t *current = head;
	while(current) {
		if (current->s.is_free && current->s.size >= size)
			return current;
		current = current->s.next;
	}
	return NULL;
}

void free(void *block)
{
	memblock_t *memblock, *tmp;

	void *programbreak;

	if (!block)
		return;
	pthread_mutex_lock(&global_malloc_lock);
	memblock = (memblock_t*)block - 1;

	programbreak = sbrk(0);

	if ((char*)block + memblock->s.size == programbreak) {
		if (head == tail) {
			head = tail = NULL;
		} else {
			tmp = head;
			while (tmp) {
				if(tmp->s.next == tail) {
					tmp->s.next = NULL;
					tail = tmp;
				}
				tmp = tmp->s.next;
			}
		}

		sbrk(0 - memblock->s.size - sizeof(memblock_t));

		pthread_mutex_unlock(&global_malloc_lock);
		return;
	}
	memblock->s.is_free = 1;
	pthread_mutex_unlock(&global_malloc_lock);
}

void *malloc(size_t size)
{
	size_t total_size;
	void *block;
	memblock_t *memblock;
	if (!size)
		return NULL;
	pthread_mutex_lock(&global_malloc_lock);
	memblock = get_free_block(size);
	if (memblock) {
		memblock->s.is_free = 0;
		pthread_mutex_unlock(&global_malloc_lock);
		return (void*)(memblock + 1);
	}
	
	total_size = sizeof(memblock_t) + size;
	block = sbrk(total_size);
	if (block == (void*) -1) {
		pthread_mutex_unlock(&global_malloc_lock);
		return NULL;
	}
	memblock = (memblock_t*)block;
	memblock->s.size = size;
	memblock->s.is_free = 0;
	memblock->s.next = NULL;
	if (!head)
		head = memblock;
	if (tail)
		tail->s.next = memblock;
	tail = memblock;
	pthread_mutex_unlock(&global_malloc_lock);
	return (void*)(memblock + 1);
}

void *calloc(size_t num, size_t nsize)
{
	size_t size;
	void *block;
	if (!num || !nsize)
		return NULL;
	size = num * nsize;
	/* check mul overflow */
	if (nsize != size / num)
		return NULL;
	block = malloc(size);
	if (!block)
		return NULL;
	memset(block, 0, size);
	return block;
}

void *realloc(void *block, size_t size)
{
	memblock_t *memblock;
	void *ret;
	if (!block || !size)
		return malloc(size);
	memblock = (memblock_t*)block - 1;
	if (memblock->s.size >= size)
		return block;
	ret = malloc(size);
	if (ret) {
		memcpy(ret, block, memblock->s.size);
		free(block);
	}
	return ret;
}


void print_mem_list(size_t& change_index)
{
	std::cout << change_index + 1 << "st change:" << std::endl;
	change_index++;
	memblock_t *current = head;
	printf("head = %p, tail = %p \n", (void*)head, (void*)tail);
	while(current) {
		printf("addr = %p, size = %zu, is_free=%u, next=%p\n",
			(void*)current, current->s.size, current->s.is_free, (void*)current->s.next);
		current = current->s.next;
	}
}

int main(){
	size_t change_index = 0;
	
	size_t* ptr = (size_t*)malloc(10 * sizeof(size_t));
	print_mem_list(change_index);
	
	realloc(ptr, sizeof(size_t)*20);
	print_mem_list(change_index);
	
	size_t* ptr2 = (size_t*)malloc(15 * sizeof(size_t));
	print_mem_list(change_index);
	
	free(ptr);
	print_mem_list(change_index);
	
	size_t* ptr3 = (size_t*)malloc(25 * sizeof(size_t));
	print_mem_list(change_index); 
}

