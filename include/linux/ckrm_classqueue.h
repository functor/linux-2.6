/* include/linux/ckrm_classqueue.h : cpu control for CKRM
 *
 * Copyright (C) Haoqiang Zheng, IBM Corp. 2003
 *           (C) Hubertus Franke, IBM Corp. 2003
 * 
 * Circular queue functionality for CKRM cpu controller
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
 * July 07, 2004
 *   clean up, add comments     
 *   
 */

#ifndef _CKRM_CLASSQUEUE_H
#define _CKRM_CLASSQUEUE_H

#include <linux/list.h>

#define CLASSQUEUE_SIZE 128
#define CQ_BITMAP_SIZE ((((CLASSQUEUE_SIZE+1+7)/8)+sizeof(long)-1)/sizeof(long))

/**
 * struct cq_prio_array: duplicates prio_array defined in sched.c 
 *
 * I duplicate this data structure to make ckrm_classqueue implementation more modular
 */
struct cq_prio_array {
	int nr_active;
	unsigned long bitmap[CQ_BITMAP_SIZE];
	struct list_head queue[CLASSQUEUE_SIZE];
};

/**
 * struct classqueue_struct - a runqueue of class local runqueues
 * @array: priority array
 * @base: base priority
 * @base_offset: index in array for the base
 *
 * classqueue can be thought of as runqueue of classes (instead of runqueue of tasks)
 * as task runqueue, each processor has a classqueue
 * a class enters the classqueue when the first task in this class local runqueue shows up
 * a class enters the classqueue when the last task in the local runqueue leaves
 * class local runqueues are ordered based their priority
 *
 * status:
 *   hzheng: is 32bit base long enough?
 */
struct classqueue_struct {
	struct cq_prio_array array;
	unsigned long base;
	unsigned long base_offset;
};

/** 
 * struct cq_node_struct - the link object between class local runqueue and classqueue
 * @list: links the class local runqueue to classqueue
 * @prio: class priority, which is caculated based on it's progress (cvt) and urgency (top_priority)
 * @index: real index into the classqueue array, calculated based on priority
 *
 * NOTE: make sure list is empty when it's not in classqueue
 */
struct cq_node_struct {
	struct list_head list;
	int prio;
	int index;
};
typedef struct cq_node_struct cq_node_t;

typedef unsigned long long CVT_t;	// cummulative virtual time

static inline void cq_node_init(cq_node_t * node)
{
	node->prio = 0;
	node->index = -1;
	INIT_LIST_HEAD(&node->list);
}

/*if the class is in classqueue*/
static inline int cls_in_classqueue(cq_node_t * node)
{
	return !list_empty(&node->list);
}

/*initialize the data structure*/
int classqueue_init(struct classqueue_struct *cq);

/*add the class to classqueue*/
void classqueue_enqueue(struct classqueue_struct *cq, cq_node_t * node, int prio);

/**
 * classqueue_dequeue - remove the class from classqueue
 * 
 * internal:
 *   called when the last task is removed from the queue
 *   checked on load balancing and schedule
 *   hzheng: why don't I call it on class_dequeue_task?
 */
void classqueue_dequeue(struct classqueue_struct *cq, cq_node_t * node);

/*change the position of the class in classqueue*/
void classqueue_update_prio(struct classqueue_struct *cq, cq_node_t * node, int new_prio);

/*return the first class in classqueue*/
cq_node_t *classqueue_get_head(struct classqueue_struct *cq);

/*update the base priority of the classqueue*/
void classqueue_update_base(struct classqueue_struct *cq, int new_base);

/**
 * class_compare_prio: compare the priority of this two nodes
 */
static inline int class_compare_prio(struct cq_node_struct* node1, struct cq_node_struct* node2)
{
	return ( node1->prio - node2->prio);
}

#endif
