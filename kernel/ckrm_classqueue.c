/* kernel/ckrm_classqueue.c : implements the class queue
 *
 * Copyright (C) Haoqiang Zheng, IBM Corp. 2003
 *           (C) Hubertus Franke, IBM Corp. 2003
 *
 * Class queue functionality for CKRM cpu controller
 *
 * Latest version, more details at http://ckrm.sf.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

/* Changes
 *
 * Aug 28, 2003
 *        Created.
 * July 08, 2004
 *        classqueue now has a fixed size
 *        major clean up
 *        function/structure names are changed to more intuitive ones
 */
#include <linux/sched.h>
#include <linux/ckrm_classqueue.h>

#define cq_nr_member(cq) (cq->array.nr_active)
#define CLASSQUEUE_MASK   (CLASSQUEUE_SIZE - 1)  

/**
 * get_node_index - 
 *      translate the logical priority to the real index in the queue
 * 
 * validate the position
 * a valid prio is [cq->base,cq->base + size -1]
 * check whether node is supposed to be enqeued beyond above window and 
 * if so set the need_repos flag 
 */
static inline unsigned long get_node_index(struct classqueue_struct *cq, 
					   cq_node_t * node)
{
	unsigned long index;
	int max_prio;

	if (!cq_nr_member(cq))
		return 0;

	max_prio = cq->base + (CLASSQUEUE_SIZE - 1);
	if (unlikely(node->prio > max_prio)) {
		node->real_prio = node->prio;
		node->prio = max_prio;
		node->need_repos = 1;
	} else
		node->need_repos = 0;

	if (unlikely(node->prio < cq->base))
		node->prio = cq->base;

       	index = (cq->base_offset + (node->prio - cq->base)) ;
	return ( index & CLASSQUEUE_MASK );   // ensure its in limits
}

/**
 * initialize a class queue object
 */
int classqueue_init(struct classqueue_struct *cq, int enabled)
{
	int i;
	struct cq_prio_array *array;

	array = &cq->array;
	for (i = 0; i < CLASSQUEUE_SIZE; i++) {
		INIT_LIST_HEAD(array->queue + i);
		__clear_bit(i, array->bitmap);
	}
	// delimiter for bitsearch
	__set_bit(CLASSQUEUE_SIZE, array->bitmap);
	array->nr_active = 0;

	cq->base = 0;
	cq->base_offset = 0;
	cq->enabled = enabled;

	return 0;
}

/**
 *classqueue_enqueue - add the class to classqueue based on its prio
 */
void classqueue_enqueue(struct classqueue_struct *cq,
			cq_node_t * node, int prio)
{
	int index;

	//get real index
	if (cq_nr_member(cq)) {		
		index = get_node_index(cq, node);
	} else {		//the first one
		cq->base = prio;
		cq->base_offset = 0;
		index = 0;
	}

	//add to the queue
	list_add(&(node->list), &cq->array.queue[index]);
	__set_bit(index, cq->array.bitmap);
	cq->array.nr_active++;

	node->index = index;
	node->prio = prio;
}

void classqueue_dequeue(struct classqueue_struct *cq, cq_node_t * node)
{
	//delete from queue
	list_del_init(&(node->list));
	cq->array.nr_active--;

	//check clear the bitmap
	if (list_empty(&cq->array.queue[node->index]))
		__clear_bit(node->index, cq->array.bitmap);
}

void classqueue_update_prio(struct classqueue_struct *cq,
			    cq_node_t * node, int new_pos)
{
	int index;

	if (! cls_in_classqueue(node)) 
		return;

	node->prio = new_pos;
	index = get_node_index(cq, node);

	//remove from the original position
	list_del_init(&(node->list));
	if (list_empty(&cq->array.queue[node->index]))
		__clear_bit(node->index, cq->array.bitmap);
	
	//add to new positon, round robin for classes with same priority
	list_add_tail(&(node->list), &cq->array.queue[index]);
	__set_bit(index, cq->array.bitmap);	
	node->index = index;
}


static inline void __classqueue_update_base(struct classqueue_struct *cq, 
					    int new_base)
{
	int max_prio; 
	if (unlikely(new_base <= cq->base)) // base will never move back
		return; 
	if (unlikely(!cq_nr_member(cq))) {  
		cq->base_offset = 0;
		cq->base = new_base;        // is this necessary ??
		return;
	}
	    
	max_prio = cq->base + (CLASSQUEUE_SIZE - 1);
	if (unlikely(new_base > max_prio))
		new_base = max_prio;

       	cq->base_offset = (cq->base_offset + (new_base - cq->base)) & CLASSQUEUE_MASK; 
	cq->base = new_base;
}
 
/**
 *classqueue_get_min_prio: return the priority of the last node in queue
 *
 * this function can be called without runqueue lock held
 * return 0 if there's nothing in the queue
 */
static inline int classqueue_get_min_prio(struct classqueue_struct *cq)
{
	cq_node_t *result = NULL;
	int pos;

	/* 
	 * search over the bitmap to get the first class in the queue
	 */
	pos = find_next_bit(cq->array.bitmap, CLASSQUEUE_SIZE, cq->base_offset);
	//do circular search from the beginning
	if (pos >= CLASSQUEUE_SIZE) 
		pos = find_first_bit(cq->array.bitmap, CLASSQUEUE_SIZE);

	if (pos < CLASSQUEUE_SIZE) {
		result = list_entry(cq->array.queue[pos].next, cq_node_t, list);
		if (list_empty(&cq->array.queue[pos]))
			result = NULL;
	}
	if (result)
		return result->prio;
	else 
		return 0;
}

/**
 * this function must be called with runqueue lock held
 */
cq_node_t *classqueue_get_head(struct classqueue_struct *cq)
{
	cq_node_t *node;
	int pos;
	int index;
	int new_base;

search_again:
	node = NULL;
	/* 
	 * search over the bitmap to get the first class in the queue
	 */
	pos = find_next_bit(cq->array.bitmap, CLASSQUEUE_SIZE, cq->base_offset);
	//do circular search from the beginning
	if (pos >= CLASSQUEUE_SIZE) 
		pos = find_first_bit(cq->array.bitmap, CLASSQUEUE_SIZE);

	if (pos < CLASSQUEUE_SIZE) {
		//BUG_ON(list_empty(&cq->array.queue[pos]));
 		node = list_entry(cq->array.queue[pos].next, cq_node_t, list);
	}

	//check if the node need to be repositioned
	if (likely(! node || ! node->need_repos)) 
		return node;

	// We need to reposition this node in the class queue
	// BUG_ON(node->prio == node->real_prio);
	
	//remove from the original position
	list_del_init(&(node->list));
	if (list_empty(&cq->array.queue[node->index]))
	  __clear_bit(node->index, cq->array.bitmap);
	
	new_base = classqueue_get_min_prio(cq);
	node->prio = node->real_prio;
	
	if (! new_base)
		new_base  = node->real_prio;
	else if (node->real_prio < new_base)
		new_base  = node->real_prio;
	__classqueue_update_base(cq,new_base);
	
	index = get_node_index(cq, node);		
	//add to new positon, round robin for classes with same priority
	list_add_tail(&(node->list), &cq->array.queue[index]);
	__set_bit(index, cq->array.bitmap);	
	node->index = index;
	
	goto search_again;		
}

/**
 * Moving the end of queue forward
 * the new_base here is logical, we need to translate to the abosule position
 */
void classqueue_update_base(struct classqueue_struct *cq)
{
	int new_base;
	
	if (! cq_nr_member(cq)) {
		cq->base = 0;
		cq->base_offset = 0;
		return;
	}

	new_base = classqueue_get_min_prio(cq);
       	__classqueue_update_base(cq,new_base);
}
