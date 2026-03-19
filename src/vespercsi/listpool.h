// Template list based object pool library based on macros with double-linked lists.

/* MIT License
 *
 * Copyright (c) 2024-2026 TechnicalUserX
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef LISTPOOL_H
#define LISTPOOL_H

#include <stdint.h>
#include <string.h>

#define LISTPOOL_DECLARE_SIZE(NAME) \
typedef uint64_t NAME##_listpool_size_t;

#define LISTPOOL_DECLARE_STATUS(NAME, NAME_CAPITAL) \
typedef enum NAME##_listpool_status_t { \
	NAME_CAPITAL##_LISTPOOL_STATUS_SUCCESS = 0, \
	NAME_CAPITAL##_LISTPOOL_STATUS_NULL_VALUE, \
	NAME_CAPITAL##_LISTPOOL_STATUS_EMPTY, \
	NAME_CAPITAL##_LISTPOOL_STATUS_NO_MEMORY \
}NAME##_listpool_status_t;

#define LISTPOOL_DECLARE_MEMORY(NAME,T) \
typedef struct NAME##_listpool_memory_t { \
	T data; \
	struct NAME##_listpool_memory_t* prev; \
	struct NAME##_listpool_memory_t* next; \
}NAME##_listpool_memory_t;

#define LISTPOOL_DECLARE_POOL(NAME,SIZE) \
typedef struct NAME##_listpool_t { \
	NAME##_listpool_memory_t* front; \
	NAME##_listpool_memory_t* back; \
	NAME##_listpool_size_t size; \
	NAME##_listpool_memory_t object_memory[SIZE]; \
	NAME##_listpool_memory_t* free_memory; \
}NAME##_listpool_t;

/**
 *	Not calling this function before use of pool is undefined behavior 
 */
#define LISTPOOL_DECLARE_INIT(NAME) \
NAME##_listpool_status_t NAME##_listpool_init(NAME##_listpool_t* pool); \

#define LISTPOOL_DEFINE_INIT(NAME,NAME_CAPITAL,SIZE) \
NAME##_listpool_status_t NAME##_listpool_init(NAME##_listpool_t* pool){ \
	if(pool == NULL){ \
		return NAME_CAPITAL##_LISTPOOL_STATUS_NULL_VALUE; \
	} \
	pool->front = NULL; \
	pool->back = NULL; \
	pool->size = 0; \
	pool->free_memory = pool->object_memory; \
	NAME##_listpool_memory_t* iterator = pool->free_memory; \
	for(NAME##_listpool_size_t i = 1; i < SIZE; i++){ \
		iterator->next = &pool->object_memory[i]; \
		iterator->prev = NULL; \
		iterator = iterator->next; \
	} \
	iterator->next = NULL; \
	iterator->prev = NULL; \
	return NAME_CAPITAL##_LISTPOOL_STATUS_SUCCESS; \
}

#define LISTPOOL_DECLARE_ALLOC(NAME) \
NAME##_listpool_memory_t* NAME##_listpool_memory_alloc(NAME##_listpool_memory_t** free_memory);

#define LISTPOOL_DEFINE_ALLOC(NAME) \
NAME##_listpool_memory_t* NAME##_listpool_memory_alloc(NAME##_listpool_memory_t** free_memory){ \
	if(free_memory == NULL){ \
		return NULL; \
	} \
	if(*free_memory == NULL){ \
		return NULL; \
	} \
	NAME##_listpool_memory_t* new_free_memory = *free_memory; \
	if((*free_memory)->next == NULL ){ \
		(*free_memory) = NULL; \
	}else{ \
		(*free_memory) = (*free_memory)->next; \
	} \
	return new_free_memory; \
}

#define LISTPOOL_DECLARE_FREE(NAME) \
void NAME##_listpool_memory_free(NAME##_listpool_memory_t** free_memory, NAME##_listpool_memory_t* object_memory);

#define LISTPOOL_DEFINE_FREE(NAME) \
void NAME##_listpool_memory_free(NAME##_listpool_memory_t** free_memory, NAME##_listpool_memory_t* object_memory){ \
	if(object_memory == NULL || free_memory == NULL){ \
		return; \
	} \
	object_memory->prev = NULL; \
	object_memory->next = (*free_memory); \
	(*free_memory) = object_memory; \
}

#define LISTPOOL_DECLARE_POP_BACK(NAME,T) \
NAME##_listpool_status_t NAME##_listpool_pop_back(NAME##_listpool_t* pool, T* data);

#define LISTPOOL_DEFINE_POP_BACK(NAME,NAME_CAPITAL,T) \
NAME##_listpool_status_t NAME##_listpool_pop_back(NAME##_listpool_t* pool, T* data){	\
	if(pool == NULL || data == NULL){\
		return NAME_CAPITAL##_LISTPOOL_STATUS_NULL_VALUE;\
	}\
	if(pool->size == 0){\
		return NAME_CAPITAL##_LISTPOOL_STATUS_EMPTY;\
	}\
	*data = pool->back->data;\
	NAME##_listpool_memory_t* free_this_memory = pool->back;\
	if(pool->size == 1){\
		pool->back = NULL;\
		pool->front = NULL;\
	}else{\
		pool->back->prev->next = NULL;	\
		pool->back = pool->back->prev;\
	}\
	NAME##_listpool_memory_free(&(pool->free_memory),free_this_memory);\
	pool->size--;\
	return NAME_CAPITAL##_LISTPOOL_STATUS_SUCCESS; \
}

#define LISTPOOL_DECLARE_POP_FRONT(NAME,T) \
NAME##_listpool_status_t NAME##_listpool_pop_front(NAME##_listpool_t* pool, T* data);

#define LISTPOOL_DEFINE_POP_FRONT(NAME,NAME_CAPITAL,T) \
NAME##_listpool_status_t NAME##_listpool_pop_front(NAME##_listpool_t* pool, T* data){\
	if(pool == NULL || data == NULL){\
		return NAME_CAPITAL##_LISTPOOL_STATUS_NULL_VALUE;\
	}\
	if(pool->size == 0){\
		return NAME_CAPITAL##_LISTPOOL_STATUS_EMPTY;\
	}\
	*data = pool->front->data;\
	NAME##_listpool_memory_t* free_this_memory = pool->front;\
	if(pool->size == 1){\
		pool->back = NULL;\
		pool->front = NULL;\
	}else{\
		pool->front->next->prev = NULL;\
		pool->front = pool->front->next;\
	}\
	NAME##_listpool_memory_free(&(pool->free_memory),free_this_memory);\
	pool->size--;\
	return NAME_CAPITAL##_LISTPOOL_STATUS_SUCCESS;\
}

#define LISTPOOL_DECLARE_PUSH_BACK(NAME,T) \
NAME##_listpool_status_t NAME##_listpool_push_back(NAME##_listpool_t* pool, T* data);

#define LISTPOOL_DEFINE_PUSH_BACK(NAME,NAME_CAPITAL,T) \
NAME##_listpool_status_t NAME##_listpool_push_back(NAME##_listpool_t* pool, T* data){\
	if(pool == NULL || data == NULL){\
		return NAME_CAPITAL##_LISTPOOL_STATUS_NULL_VALUE;\
	}\
	NAME##_listpool_memory_t* new_memory = NAME##_listpool_memory_alloc(&(pool->free_memory));\
	if(new_memory == NULL){\
		return NAME_CAPITAL##_LISTPOOL_STATUS_NO_MEMORY;\
	}\
	new_memory->data = *data;\
	new_memory->next = NULL;\
	new_memory->prev = pool->back;\
	if(pool->size == 0){\
		pool->front = new_memory;\
		pool->back = new_memory;\
	}else{\
		pool->back->next = new_memory;\
		pool->back = new_memory;\
	}\
	pool->size++;\
	return NAME_CAPITAL##_LISTPOOL_STATUS_SUCCESS;\
}

#define LISTPOOL_DECLARE_PUSH_FRONT(NAME,T) \
NAME##_listpool_status_t NAME##_listpool_push_front(NAME##_listpool_t* pool, T* data);

#define LISTPOOL_DEFINE_PUSH_FRONT(NAME,NAME_CAPITAL,T) \
NAME##_listpool_status_t NAME##_listpool_push_front(NAME##_listpool_t* pool, T* data){\
	if(pool == NULL || data == NULL){\
		return NAME_CAPITAL##_LISTPOOL_STATUS_NULL_VALUE;\
	}\
	NAME##_listpool_memory_t* new_memory = NAME##_listpool_memory_alloc(&(pool->free_memory));\
	if(new_memory == NULL){\
		return NAME_CAPITAL##_LISTPOOL_STATUS_NO_MEMORY;\
	}\
	new_memory->data = *data;\
	new_memory->next = pool->front;\
	new_memory->prev = NULL;\
	if(pool->size == 0){\
		pool->front = new_memory;\
		pool->back = new_memory;\
	}else{\
		pool->front->prev = new_memory;\
		pool->front = new_memory;\
	}\
	pool->size++;\
	return NAME_CAPITAL##_LISTPOOL_STATUS_SUCCESS;\
}


#define LISTPOOL_DECLARE_REMOVE(NAME) \
NAME##_listpool_status_t NAME##_listpool_remove(NAME##_listpool_t* pool, NAME##_listpool_memory_t* memory);

#define LISTPOOL_DEFINE_REMOVE(NAME, NAME_CAPITAL) \
NAME##_listpool_status_t NAME##_listpool_remove(NAME##_listpool_t* pool, NAME##_listpool_memory_t* memory){ \
	if(pool == NULL || memory == NULL){\
		return NAME_CAPITAL##_LISTPOOL_STATUS_NULL_VALUE;\
	}\
	NAME##_listpool_memory_t* free_this_memory = memory;\
	if(pool->size == 0){\
		return NAME_CAPITAL##_LISTPOOL_STATUS_EMPTY;\
	}\
	if(pool->size == 1){ \
        pool->front = NULL; \
        pool->back = NULL; \
	}else if(pool->front == memory){\
		pool->front->next->prev = NULL;\
		pool->front = pool->front->next;\
	}else if(pool->back == memory){ \
		pool->back->prev->next = NULL; \
		pool->back = pool->back->prev; \
	}else{\
		memory->next->prev = memory->prev;\
		memory->prev->next = memory->next;\
	} \
	NAME##_listpool_memory_free(&(pool->free_memory),free_this_memory);\
	pool->size--;\
	return NAME_CAPITAL##_LISTPOOL_STATUS_SUCCESS;\
}

#define LISTPOOL_DECLARE_CLEAR(NAME) \
NAME##_listpool_status_t NAME##_listpool_clear(NAME##_listpool_t* pool);

#define LISTPOOL_DEFINE_CLEAR(NAME, NAME_CAPITAL) \
NAME##_listpool_status_t NAME##_listpool_clear(NAME##_listpool_t* pool){ \
    if(pool == NULL){ \
        return NAME_CAPITAL##_LISTPOOL_STATUS_NULL_VALUE; \
    } \
    if(pool->size == 0){ \
        return NAME_CAPITAL##_LISTPOOL_STATUS_SUCCESS; \
    } \
    pool->back->next = pool->free_memory; \
    pool->free_memory = pool->front; \
    pool->front = NULL; \
    pool->back = NULL; \
    pool->size = 0; \
    return NAME_CAPITAL##_LISTPOOL_STATUS_SUCCESS; \
}


#define LISTPOOL_TEMPLATE_DECLARATION(NAME,NAME_CAPITAL,TYPE,SIZE) \
	LISTPOOL_DECLARE_SIZE(NAME); \
	LISTPOOL_DECLARE_STATUS(NAME,NAME_CAPITAL); \
	LISTPOOL_DECLARE_MEMORY(NAME,TYPE); \
	LISTPOOL_DECLARE_POOL(NAME,SIZE); \
	LISTPOOL_DECLARE_INIT(NAME); \
	LISTPOOL_DECLARE_ALLOC(NAME); \
	LISTPOOL_DECLARE_FREE(NAME); \
	LISTPOOL_DECLARE_POP_BACK(NAME,TYPE); \
	LISTPOOL_DECLARE_POP_FRONT(NAME,TYPE); \
	LISTPOOL_DECLARE_PUSH_BACK(NAME,TYPE); \
	LISTPOOL_DECLARE_PUSH_FRONT(NAME,TYPE); \
	LISTPOOL_DECLARE_REMOVE(NAME); \
	LISTPOOL_DECLARE_CLEAR(NAME);


#define LISTPOOL_TEMPLATE_DEFINITION(NAME,NAME_CAPITAL,TYPE,SIZE) \
	LISTPOOL_DEFINE_INIT(NAME,NAME_CAPITAL,SIZE); \
	LISTPOOL_DEFINE_ALLOC(NAME); \
	LISTPOOL_DEFINE_FREE(NAME); \
	LISTPOOL_DEFINE_POP_BACK(NAME,NAME_CAPITAL,TYPE); \
	LISTPOOL_DEFINE_POP_FRONT(NAME,NAME_CAPITAL,TYPE); \
	LISTPOOL_DEFINE_PUSH_BACK(NAME,NAME_CAPITAL,TYPE); \
	LISTPOOL_DEFINE_PUSH_FRONT(NAME,NAME_CAPITAL,TYPE); \
	LISTPOOL_DEFINE_REMOVE(NAME, NAME_CAPITAL); \
	LISTPOOL_DEFINE_CLEAR(NAME, NAME_CAPITAL);
#endif
