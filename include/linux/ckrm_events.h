/*
 * ckrm_events.h - Class-based Kernel Resource Management (CKRM)
 *                 event handling
 *
 * Copyright (C) Hubertus Franke, IBM Corp. 2003,2004
 *           (C) Shailabh Nagar,  IBM Corp. 2003
 *           (C) Chandra Seetharaman, IBM Corp. 2003
 * 
 * 
 * Provides a base header file including macros and basic data structures.
 *
 * Latest version, more details at http://ckrm.sf.net
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/*
 * Changes
 *
 * 28 Aug 2003
 *        Created.
 * 06 Nov 2003
 *        Made modifications to suit the new RBCE module.
 * 10 Nov 2003
 *        Added callbacks_active and surrounding logic. Added task paramter
 *        for all CE callbacks.
 * 19 Nov 2004
 *        New Event callback structure
 */

#ifndef _LINUX_CKRM_EVENTS_H
#define _LINUX_CKRM_EVENTS_H

#ifdef CONFIG_CKRM

/*
 * Data structure and function to get the list of registered 
 * resource controllers.
 */

/*
 * CKRM defines a set of events at particular points in the kernel
 * at which callbacks registered by various class types are called
 */

enum ckrm_event {
	/*
	 * we distinguish these events types:
	 *
	 * (a) CKRM_LATCHABLE_EVENTS
	 *      events can be latched for event callbacks by classtypes
	 *
	 * (b) CKRM_NONLATACHBLE_EVENTS
	 *     events can not be latched but can be used to call classification
	 * 
	 * (c) event that are used for notification purposes
	 *     range: [ CKRM_EVENT_CANNOT_CLASSIFY .. )
	 */

	/* events (a) */

	CKRM_LATCHABLE_EVENTS,

	CKRM_EVENT_NEWTASK = CKRM_LATCHABLE_EVENTS,
	CKRM_EVENT_FORK,
	CKRM_EVENT_EXIT,
	CKRM_EVENT_EXEC,
	CKRM_EVENT_UID,
	CKRM_EVENT_GID,
	CKRM_EVENT_LOGIN,
	CKRM_EVENT_USERADD,
	CKRM_EVENT_USERDEL,
	CKRM_EVENT_LISTEN_START,
	CKRM_EVENT_LISTEN_STOP,
	CKRM_EVENT_APPTAG,

	/* events (b) */

	CKRM_NONLATCHABLE_EVENTS,

	CKRM_EVENT_RECLASSIFY = CKRM_NONLATCHABLE_EVENTS,

	/* events (c) */

	CKRM_NOTCLASSIFY_EVENTS,

	CKRM_EVENT_MANUAL = CKRM_NOTCLASSIFY_EVENTS,

	CKRM_NUM_EVENTS
};
#endif

#ifdef __KERNEL__
#ifdef CONFIG_CKRM

/*
 * CKRM event callback specification for the classtypes or resource controllers 
 *   typically an array is specified using CKRM_EVENT_SPEC terminated with 
 *   CKRM_EVENT_SPEC_LAST and then that array is registered using
 *   ckrm_register_event_set.
 *   Individual registration of event_cb is also possible
 */

typedef void (*ckrm_event_cb) (void *arg);

struct ckrm_hook_cb {
	ckrm_event_cb fct;
	struct ckrm_hook_cb *next;
};

struct ckrm_event_spec {
	enum ckrm_event ev;
	struct ckrm_hook_cb cb;
};

#define CKRM_EVENT_SPEC(EV,FCT) { CKRM_EVENT_##EV, \
					{ (ckrm_event_cb)FCT, NULL } }

int ckrm_register_event_set(struct ckrm_event_spec especs[]);
int ckrm_unregister_event_set(struct ckrm_event_spec especs[]);
int ckrm_register_event_cb(enum ckrm_event ev, struct ckrm_hook_cb *cb);
int ckrm_unregister_event_cb(enum ckrm_event ev, struct ckrm_hook_cb *cb);

extern void ckrm_invoke_event_cb_chain(enum ckrm_event ev, void *arg);

#define CKRM_DEF_CB(EV,fct)					\
static inline void ckrm_cb_##fct(void)				\
{								\
         ckrm_invoke_event_cb_chain(CKRM_EVENT_##EV,NULL);      \
}

#define CKRM_DEF_CB_ARG(EV,fct,argtp)					\
static inline void ckrm_cb_##fct(argtp arg)				\
{									\
         ckrm_invoke_event_cb_chain(CKRM_EVENT_##EV,(void*)arg);	\
}

#else /* !CONFIG_CKRM */

#define CKRM_DEF_CB(EV,fct)			\
static inline void ckrm_cb_##fct(void)  { }

#define CKRM_DEF_CB_ARG(EV,fct,argtp)		\
static inline void ckrm_cb_##fct(argtp arg) { }

#endif /* CONFIG_CKRM */

/*
 *   define the CKRM event functions 
 *               EVENT          FCT           ARG         
 */

/* forward declarations for function arguments */
struct task_struct;
struct sock;
struct user_struct;

CKRM_DEF_CB_ARG(FORK, fork, struct task_struct *);
CKRM_DEF_CB_ARG(EXEC, exec, const char *);
CKRM_DEF_CB(UID, uid);
CKRM_DEF_CB(GID, gid);
CKRM_DEF_CB(APPTAG, apptag);
CKRM_DEF_CB(LOGIN, login);
CKRM_DEF_CB_ARG(USERADD, useradd, struct user_struct *);
CKRM_DEF_CB_ARG(USERDEL, userdel, struct user_struct *);
CKRM_DEF_CB_ARG(LISTEN_START, listen_start, struct sock *);
CKRM_DEF_CB_ARG(LISTEN_STOP, listen_stop, struct sock *);

/* some other functions required */
#ifdef CONFIG_CKRM
extern void ckrm_init(void);
extern void ckrm_cb_newtask(struct task_struct *);
extern void ckrm_cb_exit(struct task_struct *);
#else
#define ckrm_init()		do { } while (0)
#define ckrm_cb_newtask(x)	do { } while (0)
#define ckrm_cb_exit(x)		do { } while (0)
#endif

extern int get_exe_path_name(struct task_struct *, char *, int);

#endif /* __KERNEL__ */
#endif /* _LINUX_CKRM_EVENTS_H */
