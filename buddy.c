/**
 * Buddy Allocator
 *
 * For the list library usage, see http://www.mcs.anl.gov/~kazutomo/list/
 */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/
#define USE_DEBUG 0

/**************************************************************************
 * Included Files
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "buddy.h"
#include "list.h"

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define MIN_ORDER 12
#define MAX_ORDER 20
#define INIT_ORDER -1

#define PAGE_SIZE (1<<MIN_ORDER)
/* page index to address */
#define PAGE_TO_ADDR(page_idx) (void *)((page_idx*PAGE_SIZE) + g_memory)

/* address to page index */
#define ADDR_TO_PAGE(addr) ((unsigned long)((void *)addr - (void *)g_memory) / PAGE_SIZE)

/* find buddy address */
#define BUDDY_ADDR(addr, o) (void *)((((unsigned long)addr - (unsigned long)g_memory) ^ (1<<o)) \
									 + (unsigned long)g_memory)

#if USE_DEBUG == 1
#  define PDEBUG(fmt, ...) \
	fprintf(stderr, "%s(), %s:%d: " fmt,			\
		__func__, __FILE__, __LINE__, ##__VA_ARGS__)
#  define IFDEBUG(x) x
#else
#  define PDEBUG(fmt, ...)
#  define IFDEBUG(x)
#endif

/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
	struct list_head list;
	int isUsed;
	int order;
	int index;
	char *memAdr;
	/* TODO: DECLARE NECESSARY MEMBER VARIABLES */
} page_t;

/**************************************************************************
 * Global Variables
 **************************************************************************/
/* free lists*/
struct list_head free_area[MAX_ORDER+1];

/* memory area */
char g_memory[1<<MAX_ORDER];

/* page structures */
page_t g_pages[(1<<MAX_ORDER)/PAGE_SIZE];

/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/

/**************************************************************************
* Get Proper Level for a Memory Request
**************************************************************************/
 int getProperLevel(int req)
 {
	 int i;
	 for(i=MIN_ORDER; i<MAX_ORDER; i++){
		 if(1<<i >= req){
			 return i;
		 }
	 }
	 printf("ERR: MEMREQ IS TOO BIG.\n");
	 return -1;
 }

//find the correct index of the free block after a split.
int find_index(int order){
	int index = 256/(MAX_ORDER - (order + 1));
	return index;
}

//Pass it the order we're at and the order we need
 void split(int currentOrder, int desiredOrder, int index){
	 printf("splitting on order: %d for index %d\n", currentOrder, index);
	 if(currentOrder == desiredOrder) {
		 return;
	 }
	 int order = currentOrder-1;
	 page_t* right_side = &g_pages[ADDR_TO_PAGE(BUDDY_ADDR(PAGE_TO_ADDR(index), order))];
		 //int ind = find_index(order);
	 list_add(&(right_side->list), &free_area[order]);
	 split(order, desiredOrder, index);
 }

/**************************************************************************
 * Local Functions
 **************************************************************************/

/**
 * Initialize the buddy system
 */
void buddy_init()
{
	int i;
	int n_pages = (1<<MAX_ORDER) / PAGE_SIZE;
	for (i = 0; i < n_pages; i++) {

		//Add to the g_pages array given the index i
		page_t new;
		new.isUsed = 0;
		new.index = i;
		new.order = INIT_ORDER;
		g_pages[i] = new;

		/* TODO: INITIALIZE PAGE STRUCTURES */
	}

	/* initialize freelist */
	for (i = MIN_ORDER; i <= MAX_ORDER; i++) {
		INIT_LIST_HEAD(&free_area[i]);
	}

	/* add the entire memory as a freeblock */
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]);

	/* set the 1024 chunk to highest order */
	g_pages[0].order = MAX_ORDER;
}

/**
 * Allocate a memory block.
 *
 * On a memory request, the allocator returns the head of a free-list of the
 * matching size (i.e., smallest block that satisfies the request). If the
 * free-list of the matching block size is empty, then a larger block size will
 * be selected. The selected (large) block is then splitted into two smaller
 * blocks. Among the two blocks, left block will be used for allocation or be
 * further splitted while the right block will be added to the appropriate
 * free-list.
 *
 * @param size size in bytes
 * @return memory block address
 */
void *buddy_alloc(int size)
{
	/*
	* Round size up to nearest power of 2 to find the first
	* level to check
	* Go to level n, check and see if free list has items in it,
	* if so, alloc memory block and return the address (index of g_page)
	*
	* If NOT, go to level n-1, go to free list, get a block, split it,
	* add the right block to the free list of level n and alloc the left block to
	* the memory request
	*/
	int order = getProperLevel(size);
	int freeOrder = order;

	while(list_empty(&free_area[freeOrder]) && (freeOrder < 21)){
		printf("Level %d has no free blocks, checking next level\n",freeOrder);
		freeOrder++;
		if(freeOrder==21){
			return NULL;
		}
	}
	printf("First free order is: %d\n",freeOrder);
	page_t* free_page = list_entry(free_area[freeOrder].next, page_t, list);
	int index = free_page->index;
	list_del_init(&(free_page->list));

	//split block at level n, remove from the free list
	free_page->isUsed = 1;
	free_page->order = order;
	split(freeOrder, order, index);

	return ((void*)PAGE_TO_ADDR(index));

	/* PSEUDOCODE
		if the list of free blocks at level n is empty
	    allocate a block at level n-1 (using this algorithm)
	    split the block into two blocks at level n
	    insert the two blocks into the list of free blocks for level n
		set the first block to alloated the list at level n and return it
	*/
}

/**
 * Free an allocated memory block.
 *
 * Whenever a block is freed, the allocator checks its buddy. If the buddy is
 * free as well, then the two buddies are combined to form a bigger block. This
 * process continues until one of the buddies is not free.
 *
 * @param addr memory block address to be freed
 */
void buddy_free(void *addr)
{
	/*
	* get the page_t struct of the page to free
	* based on the addr passed in
	*/
	page_t* page_to_free = &g_pages[ADDR_TO_PAGE(addr)];
	int currentOrder = page_to_free->order;
	char found = 0;
	while((currentOrder <= MAX_ORDER) && (found == 0)) {
		if(currentOrder == MAX_ORDER) {
			printf("order is maxed out.");
			page_to_free->order = currentOrder;
			list_add(&page_to_free->list, &free_area[currentOrder]);
		}
		page_t *buddy = &g_pages[ADDR_TO_PAGE(BUDDY_ADDR(PAGE_TO_ADDR(page_to_free->index), currentOrder))];
		struct list_head *tracker;
		list_for_each(tracker, &free_area[currentOrder]){
			if(buddy==list_entry(tracker,page_t,list)){
				buddy->isUsed = 0;
			}
		}
		if(buddy->isUsed == 0){
			list_del_init(&buddy->list);
			if(buddy<page_to_free){
				page_to_free = buddy;
			}
			currentOrder++;
		} else {
			found = 1;
		}
	}
	page_to_free->order = currentOrder;
	list_add(&page_to_free->list, &free_area[currentOrder]);
}//END BUDDY FREE

/**
 * Print the buddy system status---order oriented
 *
 * print free pages in each order.
 */
void buddy_dump()
{
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		list_for_each(pos, &free_area[o]) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}
