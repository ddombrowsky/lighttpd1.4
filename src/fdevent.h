#ifndef _FDEVENT_H_
#define _FDEVENT_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "settings.h"
#include "bitset.h"

#if defined HAVE_STDINT_H
# include <stdint.h>
#elif defined HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#include <sys/types.h>

/* select event-system */

#if defined(HAVE_EPOLL_CTL) && defined(HAVE_SYS_EPOLL_H)
# define USE_LINUX_EPOLL
# include <sys/epoll.h>
#endif

/* MacOS 10.3.x has poll.h under /usr/include/, all other unixes
 * under /usr/include/sys/ */
#if defined HAVE_POLL && (defined(HAVE_SYS_POLL_H) || defined(HAVE_POLL_H))
# define USE_POLL
# ifdef HAVE_POLL_H
#  include <poll.h>
# else
#  include <sys/poll.h>
# endif
#endif

#if defined HAVE_SELECT
# ifdef __WIN32
#  include <winsock2.h>
# endif
# define USE_SELECT
# ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
# endif
#endif

#if defined HAVE_SYS_DEVPOLL_H && defined(__sun)
# define USE_SOLARIS_DEVPOLL
# include <sys/devpoll.h>
#endif

#if defined HAVE_SYS_EVENT_H && defined HAVE_KQUEUE
# define USE_FREEBSD_KQUEUE
# include <sys/event.h>
#endif

#if defined HAVE_SYS_PORT_H && defined HAVE_PORT_CREATE
# define USE_SOLARIS_PORT
# include <sys/port.h>
#endif

#if defined HAVE_LIBEV
# define USE_LIBEV
# include <ev.h>
#endif

struct server;

typedef handler_t (*fdevent_handler)(struct server *srv, void *ctx, int revents);

/* these are the POLL* values from <bits/poll.h> (linux poll)
 */

#define FDEVENT_IN     BV(0)
#define FDEVENT_PRI    BV(1)
#define FDEVENT_OUT    BV(2)
#define FDEVENT_ERR    BV(3)
#define FDEVENT_HUP    BV(4)
#define FDEVENT_NVAL   BV(5)

typedef enum { FD_EVENT_TYPE_UNSET = -1,
		FD_EVENT_TYPE_CONNECTION,
		FD_EVENT_TYPE_FCGI_CONNECTION,
		FD_EVENT_TYPE_DIRWATCH,
		FD_EVENT_TYPE_CGI_CONNECTION
} fd_event_t;

typedef enum { FDEVENT_HANDLER_UNSET,
		FDEVENT_HANDLER_SELECT,
		FDEVENT_HANDLER_POLL,
		FDEVENT_HANDLER_LINUX_SYSEPOLL,
		FDEVENT_HANDLER_SOLARIS_DEVPOLL,
		FDEVENT_HANDLER_FREEBSD_KQUEUE,
		FDEVENT_HANDLER_LIBEV
} fdevent_handler_t;


typedef struct _fdnode {
	fdevent_handler handler;
	void *ctx;
	void *handler_ctx;
	int fd;
	int events;
} fdnode;

/**
 * array of unused fd's
 *
 */

typedef struct {
	int *ptr;

	size_t used;
	size_t size;
} buffer_int;

/**
 * fd-event handler for select(), poll() and rt-signals on Linux 2.4
 *
 */
typedef struct fdevents {
	struct server *srv;
	fdevent_handler_t type;

	fdnode **fdarray;
	size_t maxfds;

#ifdef USE_LINUX_EPOLL
	int epoll_fd;
	struct epoll_event *epoll_events;
#endif
#ifdef USE_POLL
	struct pollfd *pollfds;

	size_t size;
	size_t used;

	buffer_int unused;
#endif
#ifdef USE_SELECT
	fd_set select_read;
	fd_set select_write;
	fd_set select_error;

	fd_set select_set_read;
	fd_set select_set_write;
	fd_set select_set_error;

	int select_max_fd;
#endif
#ifdef USE_SOLARIS_DEVPOLL
	int devpoll_fd;
	struct pollfd *devpollfds;
#endif
#ifdef USE_FREEBSD_KQUEUE
	int kq_fd;
	struct kevent *kq_results;
#endif
#ifdef USE_SOLARIS_PORT
	int port_fd;
#endif
#ifdef USE_LIBEV
	struct ev_loop *libev_loop;
#endif
	int (*reset)(struct fdevents *ev);
	void (*free)(struct fdevents *ev);

	int (*event_set)(struct fdevents *ev, int fde_ndx, int fd, int events);
	int (*event_del)(struct fdevents *ev, int fde_ndx, int fd);
	int (*event_get_revent)(struct fdevents *ev, size_t ndx);
	int (*event_get_fd)(struct fdevents *ev, size_t ndx);

	int (*event_next_fdndx)(struct fdevents *ev, int ndx);

	int (*poll)(struct fdevents *ev, int timeout_ms);

	int (*fcntl_set)(struct fdevents *ev, int fd);
} fdevents;

/**
 * Initialize a new list of fdevents.
 *
 * @param srv pointer to a server
 * @param maxfds The maximum number of file descriptors to allow.
 * @param type The type of handler to use (ex: FDEVENT_HANDLER_LINUX_SYSEPOLL).
 * @return The new, empty list of fdevents.
 */
fdevents *fdevent_init(struct server *srv, size_t maxfds, fdevent_handler_t type);

/**
 * If the registered fdevent handler supports it, reset the list.
 *
 * @param ev The list to reset (clear).
 * @return 0 on success (or unsupported).
 */
int fdevent_reset(fdevents *ev);

/**
 * Free the memory associated with an fdevent handler list.
 *
 * @param ev The list to free.
 */
void fdevent_free(fdevents *ev);


/**
 * Set a trigger for one or more events on a file descriptor. You should have called
 * fdevent_register() before calling this.
 *
 * @param ev The list of events which the file descriptor has already been registered on.
 * @param fde_ndx If provided and not -1, updates an existing event trigger. Otherwise,
 *   adds a new trigger and, if provided, receives its index.
 * @param fd The file descriptor to add the event trigger to.
 * @param events A bitmask of the types of events to trigger on.
 * @return 0 on success.
 */
int fdevent_event_set(fdevents *ev, int *fde_ndx, int fd, int events);

/**
 * Remove an event trigger.
 *
 * @param ev The list of events to remove from.
 * @param fde_ndx The index of the event trigger to remove.
 * @param fd The file descriptor to remove the trigger from.
 * @return -1 on success.
 */
int fdevent_event_del(fdevents *ev, int *fde_ndx, int fd);

/**
 * Get a bitmask of the event(s) which triggered.
 *
 * @param ev The list of events.
 * @param ndx The event index.
 * @return A bitmask containing the event(s) which triggered.
 */
int fdevent_event_get_revent(fdevents *ev, size_t ndx);

/**
 * Get the file descriptor associated with the triggered events.
 *
 * @param ev The event list.
 * @param ndx The event index.
 * @return The file descriptor associated with the trigger.
 */
int fdevent_event_get_fd(fdevents *ev, size_t ndx);

/**
 * Get the handler function associated with the given file descriptor.
 *
 * @param ev The event list.
 * @param fd The file descriptor.
 * @return The handler function registered to handle events from that file descriptor.
 */
fdevent_handler fdevent_get_handler(fdevents *ev, int fd);

/**
 * Get the context pointer associated with the given file descriptor.
 *
 * @param ev The event list.
 * @param fd The file descriptor.
 * @return The context pointer (registered with the handler function) for the descriptor.
 */
void * fdevent_get_context(fdevents *ev, int fd);


/**
 * Move on to the next event index.
 *
 * @param ev The event list.
 * @param ndx The last event index, or -1 to start from the beginning.
 * @return The next event index, or -1 if there are none left.
 */
int fdevent_event_next_fdndx(fdevents *ev, int ndx);


/**
 * Poll for new events.
 *
 * @param ev The event list.
 * @param timeout_ms The number of milliseconds to poll for.
 * @return The number of events which occurred during that time.
 */
int fdevent_poll(fdevents *ev, int timeout_ms);


/**
 * Register a handler function to be notified of events on a file descriptor.
 *
 * @param ev The event list.
 * @param fd The file descriptor.
 * @param handle The handler function.
 * @param ctx A pointer to some context, which will be passed to the function unaltered.
 * @return 0 on success.
 */
int fdevent_register(fdevents *ev, int fd, fdevent_handler handler, void *ctx);

/**
 * Unregister a handler from a file descriptor. You should have removed all triggers with
 * fdevent_event_del() before calling this.
 *
 * @param ev The event list.
 * @param fd The file descriptor.
 * @return 0 on success.
 */
int fdevent_unregister(fdevents *ev, int fd);

/**
 * Set all necessary fcntl options on the given file descriptor to make it usable by fdevent.
 *
 * @param ev The event list.
 * @param fd The file descriptor.
 * @return 0 on success.
 */
int fdevent_fcntl_set(fdevents *ev, int fd);

/** Initialize select-based fdevent handling on the given event list. */
int fdevent_select_init(fdevents *ev);

/** Initialize poll-based fdevent handling on the given event list. */
int fdevent_poll_init(fdevents *ev);

/** Initialize Linux rtsig-based fdevent handling on the given event list. */
int fdevent_linux_rtsig_init(fdevents *ev);

/** Initialize Linux epoll-based fdevent handling on the given event list. */
int fdevent_linux_sysepoll_init(fdevents *ev);

/** Initialize Solaris devpoll-based fdevent handling on the given event list. */
int fdevent_solaris_devpoll_init(fdevents *ev);

/** Initialize FreeBSD kqueue-based fdevent handling on the given event list. */
int fdevent_freebsd_kqueue_init(fdevents *ev);
int fdevent_libev_init(fdevents *ev);

#endif
