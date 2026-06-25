// SPDX-License-Identifier: BSD-3-Clause

#include <sys/mman.h>
#include <unistd.h>
#include "osmem.h"
#include "block_meta.h"

#define MMAP_THRESHOLD (128 * 1024)
#define size_block_meta_with_pad (((sizeof(struct block_meta) % 8 != 0) + (sizeof(struct block_meta) / 8)) * 8)

struct block_meta *block_meta_head;
void *p;

// coding style

void *heap_prealloc(void)
{
	void *p = sbrk(MMAP_THRESHOLD);

	DIE(p == (void *) -1, "sbrk returned (void *) -1");
	void *payload = p + size_block_meta_with_pad;

	block_meta_head = p;
	block_meta_head->size = MMAP_THRESHOLD - size_block_meta_with_pad;
	block_meta_head->status = STATUS_FREE;
	block_meta_head->next = block_meta_head;
	block_meta_head->prev = block_meta_head;

	return payload;
}


void *find_best_block(size_t size)
{
	void *p = NULL;
	size_t Min = MMAP_THRESHOLD;
	struct block_meta *block_meta_head_temp = block_meta_head;

	do {
		size_t size_with_padding = (((block_meta_head_temp->size) % 8 != 0) + ((block_meta_head_temp->size) / 8)) * 8;

		if (block_meta_head_temp->status == STATUS_FREE && size_with_padding >= size && size_with_padding < Min) {
			Min = size_with_padding;
			p = block_meta_head_temp;
		}
		block_meta_head_temp = block_meta_head_temp->next;
	} while (block_meta_head_temp != block_meta_head);
	return p;
}

void *alloc_new_block(size_t size)
{
	size_t size_with_padding = (((size + size_block_meta_with_pad) % 8 != 0) +
								((size + size_block_meta_with_pad) / 8)) * 8;
	void *p = sbrk(size_with_padding);

	DIE(p == (void *) -1, "sbrk returned (void *) -1");
	void *payload = p + size_block_meta_with_pad;

	((struct block_meta *)p)->size = size;
	((struct block_meta *)p)->status = STATUS_ALLOC;
	struct block_meta *block_meta_head_temp = block_meta_head->prev;

	block_meta_head_temp->next = ((struct block_meta *)p);
	((struct block_meta *)p)->next = block_meta_head;
	((struct block_meta *)p)->prev = block_meta_head_temp;
	block_meta_head->prev = ((struct block_meta *)p);

	return payload;
}

void *split_block(struct block_meta *block_meta_head_temp, size_t size)
{
	size_t size_with_padding = (((size + size_block_meta_with_pad) % 8 != 0) +
								((size + size_block_meta_with_pad) / 8)) * 8;


	void *block_meta_head_temp2 = (void *)block_meta_head_temp + size_with_padding;
	((struct block_meta *)block_meta_head_temp2)->status = STATUS_FREE;
	size_t size_with_padding2 = (((size % 8 != 0) + (size / 8)) * 8);
	((struct block_meta *)block_meta_head_temp2)->size =
	 block_meta_head_temp->size - size_with_padding2 - size_block_meta_with_pad;
	((struct block_meta *)block_meta_head_temp2)->next = block_meta_head_temp->next;
	((struct block_meta *)block_meta_head_temp2)->prev = block_meta_head_temp;

	block_meta_head_temp->next->prev = ((struct block_meta *)block_meta_head_temp2);
	block_meta_head_temp->next = ((struct block_meta *)block_meta_head_temp2);

	block_meta_head_temp->status = STATUS_ALLOC;
	block_meta_head_temp->size = size;

	void *p = (void *) block_meta_head_temp + size_block_meta_with_pad;
	return p;
}

void coalesce(void)
{
	struct block_meta *block_meta_head_temp = block_meta_head;

	do {
		struct block_meta *block_meta_head_temp2 = block_meta_head_temp->next;

		if (block_meta_head_temp->status == STATUS_FREE && block_meta_head_temp2->status == STATUS_FREE &&
			block_meta_head_temp != block_meta_head_temp2 && block_meta_head_temp->next != block_meta_head) {
			size_t size_with_padding = (((block_meta_head_temp2->size + size_block_meta_with_pad) % 8 != 0) +
										((block_meta_head_temp2->size + size_block_meta_with_pad) / 8)) * 8;
			block_meta_head_temp->size += size_with_padding;
			block_meta_head_temp2->next->prev = block_meta_head_temp;
			block_meta_head_temp->next = block_meta_head_temp2->next;
			block_meta_head_temp = block_meta_head_temp->prev;
		}

		block_meta_head_temp = block_meta_head_temp->next;
	} while (block_meta_head_temp != block_meta_head);
}

void calloc_init(void *payload)
{
	struct block_meta *block_meta_head_temp = payload - size_block_meta_with_pad;

	for (size_t i = 0; i < block_meta_head_temp->size; i++) {
		void *payload_temp = payload + i;
		*(char *)payload_temp = 0;
	}
}

void copy_data(void *payload1, void *payload2, size_t size)
{
	for (size_t i = 0; i < size; i++)
		*(char *)(payload1 + i) = *(char *)(payload2+i);
}

size_t min(size_t a, size_t b)
{
	return (a < b) ? a : b;
}

void *alloc_new_realloc(struct block_meta *block_meta_head_temp, void *ptr,
						size_t size, size_t size_with_padding1, size_t size_with_padding2)
{
	if (block_meta_head->prev == block_meta_head_temp) {
		void *p = sbrk(size_with_padding1 - size_with_padding2);

		DIE(p == (void *) -1, "sbrk returned (void *) -1");
		block_meta_head_temp->size = size;
		return ptr;
	}
	void *p = os_malloc(size);

	copy_data(p, ptr, min(size, block_meta_head_temp->size));
	os_free(ptr);
	return p;
}


void *os_malloc(size_t size)
{
	/* TODO: Implement os_malloc */
	size_t size_with_padding = ((size % 8 != 0) + (size / 8)) * 8;

	if (size == 0) {
		return NULL;
	} else if (size_with_padding + size_block_meta_with_pad < MMAP_THRESHOLD) {
		if (p == NULL)
			p = heap_prealloc();
		coalesce();
		struct block_meta *block_meta_head_temp = find_best_block(size);

		if (block_meta_head_temp == NULL) {
			struct block_meta *block_meta_head_temp2 = block_meta_head->prev;

			while (block_meta_head_temp2->status == STATUS_MAPPED && block_meta_head_temp2 != block_meta_head)
				block_meta_head_temp2 = block_meta_head_temp2->prev;
			if (block_meta_head_temp2->status == STATUS_FREE) {
				size_t size_with_padding_prev = ((block_meta_head_temp2->size % 8 != 0) +
												(block_meta_head_temp2->size) / 8) * 8;
				size_t size_with_padding = (((size - size_with_padding_prev) % 8 != 0) + ((size - size_with_padding_prev) / 8)) * 8;
				void *p = sbrk(size_with_padding);

				DIE(p == (void *) -1, "sbrk returned (void *) -1");
				block_meta_head_temp2->size = size;
				block_meta_head_temp2->status = STATUS_ALLOC;
				void *payload = (void *)block_meta_head_temp2 + size_block_meta_with_pad;
				return payload;
			} else {
				return alloc_new_block(size);
			}
		} else {
			int size_with_padding = (((size - block_meta_head_temp->size + size_block_meta_with_pad) % 8 != 0) +
									((size - block_meta_head_temp->size + size_block_meta_with_pad) / 8)) * 8;
			if (size_with_padding >= 0) {
				block_meta_head_temp->status = STATUS_ALLOC;
				void *payload = (void *)block_meta_head_temp + size_block_meta_with_pad;
				return payload;
			}
			void *payload = split_block(block_meta_head_temp, size);
			return payload;
		}

	} else {
		size_t size_with_padding = (((size + size_block_meta_with_pad) % 8 != 0) +
									((size + size_block_meta_with_pad) / 8)) * 8;
		void *p = mmap(NULL, size_with_padding, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		DIE(p == (void *) -1, "mmap returned (void *) -1");
		((struct block_meta *) p)->size = size;
		((struct block_meta *) p)->status = STATUS_MAPPED;
		if (block_meta_head == NULL) {
			block_meta_head = p;
		} else {
			struct block_meta *block_meta_head_temp = block_meta_head->prev;

			block_meta_head_temp->next = p;
			((struct block_meta *) p)->prev = block_meta_head_temp;
		}
		((struct block_meta *) p)->next = block_meta_head;
		block_meta_head->prev = p;
		return (p + size_block_meta_with_pad);
	}
}

void os_free(void *ptr)
{
	/* TODO: Implement os_free */
	if (ptr == NULL)
		return;
	struct block_meta *p = ptr - size_block_meta_with_pad;

	if (p->status == STATUS_ALLOC)
		p->status = STATUS_FREE;
	if (p->status == STATUS_MAPPED) {
		if (p->next == p && p->prev == p) {
			p->next = NULL;
			p->prev = NULL;
			block_meta_head = NULL;
		} else {
			p->next->prev = p->prev;
			p->prev->next = p->next;
		}
		size_t size_with_padding = (((p->size + size_block_meta_with_pad) % 8 != 0) +
									((p->size + size_block_meta_with_pad) / 8)) * 8;
		int return_value = munmap(ptr-size_block_meta_with_pad, size_with_padding);

		DIE(return_value == -1, "munmap returned -1");
	}
}


void *os_calloc(size_t nmemb, size_t size)
{
	/* TODO: Implement os_calloc */
	size *= nmemb;
	size_t size_with_padding = ((size % 8 != 0) + (size / 8)) * 8;

	if (size == 0) {
		return NULL;
	} else if (size_with_padding + size_block_meta_with_pad < (size_t)getpagesize()) {
		if (p == NULL)
			p = heap_prealloc();
		coalesce();
		struct block_meta *block_meta_head_temp = find_best_block(size);

		if (block_meta_head_temp == NULL) {
			if (block_meta_head->prev->status == STATUS_FREE) {
				size_t size_with_padding_prev = ((block_meta_head->prev->size % 8 != 0) +
												(block_meta_head->prev->size) / 8) * 8;
				size_t size_with_padding = (((size - size_with_padding_prev) % 8 != 0) + ((size - size_with_padding_prev) / 8)) * 8;
				void *p = sbrk(size_with_padding);

				DIE(p == (void *) -1, "sbrk returned (void *) -1");
				block_meta_head->prev->size = size;
				block_meta_head->prev->status = STATUS_ALLOC;
				void *payload = (void *)block_meta_head->prev + size_block_meta_with_pad;

				calloc_init(payload);
				return payload;
			}
			void *p = alloc_new_block(size);

			calloc_init(p);
			return p;
		}
		int size_with_padding = (((size - block_meta_head_temp->size + size_block_meta_with_pad) % 8 != 0) +
								((size - block_meta_head_temp->size + size_block_meta_with_pad) / 8)) * 8;
		if (size_with_padding >= 0) {
			block_meta_head_temp->status = STATUS_ALLOC;
			void *payload = (void *)block_meta_head_temp + size_block_meta_with_pad;

			calloc_init(payload);
			return payload;
		}
		void *payload = split_block(block_meta_head_temp, size);

		calloc_init(payload);
		return payload;

	} else {
		size_t size_with_padding = (((size + size_block_meta_with_pad) % 8 != 0) +
									((size + size_block_meta_with_pad) / 8)) * 8;
		void *p = mmap(NULL, size_with_padding, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		calloc_init(p + size_block_meta_with_pad);
		DIE(p == (void *) -1, "mmap returned (void *) -1");
		((struct block_meta *) p)->size = size;
		((struct block_meta *) p)->status = STATUS_MAPPED;
		if (block_meta_head == NULL) {
			block_meta_head = p;
		} else {
			struct block_meta *block_meta_head_temp = block_meta_head->prev;

			block_meta_head_temp->next = p;
			((struct block_meta *) p)->prev = block_meta_head_temp;
		}
		((struct block_meta *) p)->next = block_meta_head;
		block_meta_head->prev = p;
		return (p + size_block_meta_with_pad);
	}
}

void *os_realloc(void *ptr, size_t size)
{
	/* TODO: Implement os_realloc */
	if (ptr == NULL) {
		return os_malloc(size);
	} else if (size == 0) {
		os_free(ptr);
		return NULL;
	}
	struct block_meta *block_meta_head_temp = ptr - size_block_meta_with_pad;
	int size_with_padding = (((size - block_meta_head_temp->size + size_block_meta_with_pad) % 8 != 0) +
							((size - block_meta_head_temp->size + size_block_meta_with_pad) / 8)) * 8;
	size_t size_with_padding1 = ((size % 8 != 0) + (size / 8)) * 8;
	size_t size_with_padding2 = ((block_meta_head_temp->size % 8 != 0) + (block_meta_head_temp->size / 8)) * 8;
	size_t size_with_padding3 = ((block_meta_head_temp->next->size % 8 != 0) +
								(block_meta_head_temp->next->size / 8)) * 8;
	if (block_meta_head_temp->status == STATUS_FREE) {
		return NULL;
	} else if (size > MMAP_THRESHOLD || block_meta_head_temp->status == STATUS_MAPPED) {
		void *p = os_malloc(size);

		copy_data(p, ptr, min(block_meta_head_temp->size, size));
		os_free(ptr);
		return p;
	} else if (size_with_padding >= 0) {
		if (size_with_padding > 32) {
			coalesce();
			if (block_meta_head_temp->next->status != STATUS_FREE ||
				size_with_padding2 + size_block_meta_with_pad + size_with_padding3 < size_with_padding1){
				return alloc_new_realloc(block_meta_head_temp, ptr, size, size_with_padding1, size_with_padding2);
			}
			block_meta_head_temp->size += (size_with_padding2 - block_meta_head_temp->size) +
											size_block_meta_with_pad + block_meta_head_temp->next->size;
			block_meta_head_temp->next->next->prev = block_meta_head_temp;
			block_meta_head_temp->next = block_meta_head_temp->next->next;
			return ptr;
		} else {
			return ptr;
		}
	} else {
		void *payload = split_block(block_meta_head_temp, size);
		return payload;
	}
}
