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
 *
 * Overview:
 * ---------
 *
 * Please read Documentation/ckrm/cpu_sched for a general overview of
 * how the O(1) CKRM scheduler.
 *
 * ckrm_classqueue.h provides the definition to maintain the 
 * per cpu class runqueue.
 *   
 */

#ifndef _CKRM_CLASSQUEUE_H
#define _CKRM_CLASSQUEUE_H

#include <linux/list.h>

#define CLASSQUEUE_SIZE_SHIFT 	7
#define CLASSQUEUE_SIZE ( 1 << CLASSQUEUE_SIZE_SHIFT )
#define CQ_BITMAP_SIZE ((((CLASSQUEUE_SIZE+1+7)/8)+sizeof(long)-1)/sizeof(long))

/**
 * struct cq_prio_array: duplicates prio_array defined in sched.c 
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
 * classqueue can be thought of as runqueue of lrq's (per cpu object of
 * a CKRM class as task runqueue (instead of runqueue of tasks)
 * - a class's local lrq is enqueued into the local classqueue when a
 *   first task is enqueued lrq.
 * - a class's local lrq is removed from the local classqueue when the 
 *   last task is dequeued from the lrq.
 * - lrq's are ordered based on their priority (determined elsewhere)
 *   ( CKRM: caculated based on it's progress (cvt) and urgency (top_priority)
 */

struct classqueue_struct {
	int enabled;                   // support dynamic on/off
	unsigned long base;
	unsigned long base_offset;
	struct cq_prio_array array;
};

/** 
 * struct cq_node_struct:
 * - the link object between class local runqueue and classqueue
 * @list: links the class local runqueue to classqueue
 * @prio: class priority
 * @index: real index into the classqueue array, calculated based on priority
 */
struct cq_node_struct {
	struct list_head list;
	int prio;
	int index;
	/*
	 * set when the class jump out of the class queue window
	 * class with this value set should be repositioned whenever classqueue slides window
	 * real_prio is valid when need_repos is set
	 */
	int real_prio;
	int need_repos; 
};
typedef struct cq_node_struct cq_node_t;

static inline void cq_node_init(cq_node_t * node)
{
	node->prio = 0;
	node->index = -1;
	node->real_prio = 0;
	node->need_repos = 0;
	INIT_LIST_HEAD(&node->list);
}

/*if the class is in classqueue*/
static inline int cls_in_classqueue(cq_node_t * node)
{
	return !list_empty(&node->list);
}

/*initialize the data structure*/
int classqueue_init(struct classqueue_struct *cq, int enabled);

/*add the class to classqueue at given priority */
void classqueue_enqueue(struct classqueue_struct *cq, 
			cq_node_t * node, int prio);

/*remove the class from classqueue */
void classqueue_dequeue(struct classqueue_struct *cq, cq_node_t * node);

/*change the position of the class in classqueue*/
void classqueue_update_prio(struct classqueue_struct *cq, 
			    cq_node_t * node, int new_prio);

/*return the first class in classqueue*/
cq_node_t *classqueue_get_head(struct classqueue_struct *cq);

/*update the base priority of the classqueue*/
void classqueue_update_base(struct classqueue_struct *cq);

/**
 * class_compare_prio: compare the priority of this two nodes
 */
static inline int class_compare_prio(struct cq_node_struct* node1, 
				     struct cq_node_struct* node2)
{
	return ( node1->prio - node2->prio);
}

#endif
