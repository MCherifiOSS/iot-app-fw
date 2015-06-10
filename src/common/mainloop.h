/*
 * Copyright (c) 2012-2014, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel Corporation nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __IOT_MAINLOOP_H__
#define __IOT_MAINLOOP_H__

#include <signal.h>
#include <stdint.h>
#include <sys/poll.h>
#include <sys/epoll.h>

#include <iot/common/macros.h>

IOT_CDECL_BEGIN

/**
 * \addtogroup MurphCommonInfra
 * @{
 *
 * @file mainloop.h
 *
 * @brief Murphy mainloop implementation.
 *
 * The Murphy mainloop provides the basis for asynchronous single-threaded
 * event processing within Murphy. Most of the other services and APIs
 * offered by Murphy utilize either directly or indirectly the mainloop
 * abstraction.
 *
 * The Murphy mainloop provides abstractions for I/O watches, timers,
 * deferred callbacks, POSIX signal watches, and wakeup callbacks.
 * Additionally, the mainloop provides an abstraction of superloops,
 * a mechanims that allows to be pumped by an external mainloop.
 *
 * I/O watches trigger a callback when the file descriptor is in a certain
 * state. Among the available states are readability, writability, hangup,
 * and error conditions.
 *
 * Timers repeatedly trigger a callback at regular intervals. Timers can be
 * dynamically stopped, and restarted. The timer interval can be dynamically
 * changed.
 *
 * Deferred callbacks trigger a callback when the mainloop is done with
 * processing all pending events. Similarly to timers, deferred callbacks can
 * be dynamically stopped and restarted. Deferred callbacks don't starve the
 * mainloop. Each active deferred callback is triggered exactly once per every
 * mainloop iteration.
 *
 * POSIX signal watches trigger a callback when a specified signal has been
 * delivered to the process. It allows one to wait for signals in a unified
 * fashion with timers and I/O.
 *
 * Wakeup callbacks are triggered when the Murphy mainloop is woken up by
 * certain events or conditions. It allows one to piggyback extra processing
 * on an event that would have woken up the mainloop anyway. This might come
 * handy in battery-powered devices to conserve battery by concentrating
 * processing and usage of the CPU around events that would have woken up the
 * CPU anyway. Wakeup callbacks can be configured to forcibly wake up the CPU
 * if no other event of interest happens in a configurable amount of time.
 * Similarly, wakeup callbacks can be equipped with a configurable low-pass
 * filter that will prevent then from being called too often, if there is a
 * storm of events that surpasses the threshold.
 *
 * Superloops provide the opposite mechanism. They allow the Murphy mainloop
 * to be embedded into and pumped by 3rd-party mainloops.
 *
 * There are a number of superloop implementations readily available in the
 * Murphy source distribution for the most popular mainloop implementations,
 * including GMainloop, PulseAudio mainloop, EFL/ecore, Qt mainloop, and
 * wayland.
 *
 * Murphy also provides a simple publish-subscribe event mechanism, with
 * support for both synchronous and asynchronous event delivery. Arbitrary
 * data in various supported formats (iot_json_t, custom void *) can be
 * attached to events by the emitter. Events can be registered automatically
 * during startup, and later dynamically at any time. Events are referred to
 * by their well-known names or by their dynamically assigned integer
 * identifiers.
 */

/**
 * @brief Opaque Murphy mainloop type.
 */
typedef struct iot_mainloop_s iot_mainloop_t;

/**
 * @brief Murphy I/O watches.
 *
 * Murphy I/O watches provide a very simple, straightforward mechanism for
 * monitoring the state of a file descriptor. One creates an I/O watch for
 * a descriptor and a set of events of interest together with a callback and
 * an opaque user data pointer. The mainloop will trigger the callback whenever
 * any of the specified events happens.
 */

/**
 * @brief Opaque I/O watch type.
 */
typedef struct iot_io_watch_s iot_io_watch_t;

/**
 * @brief I/O events
 *
 * I/O events are used to specify what events the mainloop should monitor
 * for the file descriptor of the I/O watch. I/O events pretty much directly
 * correspond to the available poll(2)/epoll(7) events.
 */
typedef enum {
    IOT_IO_EVENT_NONE  = 0x0,
    IOT_IO_EVENT_IN    = EPOLLIN,
    IOT_IO_EVENT_PRI   = EPOLLPRI,
    IOT_IO_EVENT_OUT   = EPOLLOUT,
    IOT_IO_EVENT_RDHUP = EPOLLRDHUP,
    IOT_IO_EVENT_WRHUP = EPOLLHUP,
    IOT_IO_EVENT_HUP   = EPOLLRDHUP|EPOLLHUP,
    IOT_IO_EVENT_ERR   = EPOLLERR,
    IOT_IO_EVENT_INOUT = EPOLLIN|EPOLLOUT,
    IOT_IO_EVENT_ALL   = EPOLLIN|EPOLLPRI|EPOLLOUT|EPOLLRDHUP|EPOLLERR,
    /* event trigger modes */
    IOT_IO_TRIGGER_LEVEL = 0x1U << 25,
    IOT_IO_TRIGGER_EDGE  = EPOLLET,
    IOT_IO_TRIGGER_MASK  = IOT_IO_TRIGGER_LEVEL|IOT_IO_TRIGGER_EDGE
} iot_io_event_t;

/**
 * @brief I/O watch notification callback type.
 *
 * This is the type of the callback function that will be associated with
 * an I/O watch and triggered by the mainloop to deliver I/O event
 * notifications for the watch.
 *
 * @param [in] w          I/O watch the event is delivered for
 * @param [in] fd         file descriptor for which @w was created
 * @param [in] events     bitmask of events pending for @w
 * @param [in] user_data  opaque user data for @w
 */
typedef void (*iot_io_watch_cb_t)(iot_io_watch_t *w, int fd,
                                  iot_io_event_t events, void *user_data);

/**
 * @brief Register a new file descriptor to watch.
 *
 * Creates an I/O watch for the given file descriptor and the specified set
 * (bitmask) of events.
 *
 * @param [in] ml         mainloop to add I/O watch to
 * @param [in] fd         file descriptor to watch
 * @param [in] events     bitmask of events of interest
 * @param [in] cb         watch notification callback
 * @param [in] user_data  opaque notification user data
 *
 * @return Returns a new I/O watch or @NULL upon error.
 */
iot_io_watch_t *iot_add_io_watch(iot_mainloop_t *ml, int fd,
                                 iot_io_event_t events,
                                 iot_io_watch_cb_t cb, void *user_data);

/**
 * @brief Delete the given I/O watch.
 *
 * Deletes the given I/O watch and stops delivering notifications for the
 * associated file descriptor. Safe to be called from the watch notification
 * callback.
 *
 * @param [in] w  I/O watch to delete
 */
void iot_del_io_watch(iot_io_watch_t *w);

/**
 * @brief Get the mainloop associated with an I/O watch.
 *
 * Retrieve the mainloop for which the given I/O watch has been created.
 *
 * @param [in] w  I/O watch to get the mainloop for
 *
 * @return Return the mainloop associated with @w.
 */
iot_mainloop_t *iot_get_io_watch_mainloop(iot_io_watch_t *w);

/**
 * @brief Sets the default I/O watch trigger mode for the mainloop.
 *
 * Sets the default I/O watch behavior to either edge- or level-triggered
 * mode for the given mainloop.
 *
 * @param [in] ml    mainloop to set default mode for
 * @param [in] mode  @IOT_IO_TRIGGER_LEVEL or @IOT_IO_TRIGGER_EDGE
 *
 * @return Returns @TRUE upon success, @FALSE otherwise.
 */
int iot_set_io_event_mode(iot_mainloop_t *ml, iot_io_event_t mode);

/**
 * @brief Query the default I/O watch trigger mode for the mainloop.
 *
 * Queries whether the default I/O watch mode for the given mainloop is
 * level- or edge-triggered.
 *
 * @param [in] ml  the mainloop to query default mode for
 *
 * @return Returns the default mode, one of @IOT_IO_TRIGGER_LEVEL, or
 *         @IOT_IO_TRIGGER_EDGE.
 */
iot_io_event_t iot_get_io_event_mode(iot_mainloop_t *ml);

/**
 * @brief Convenience macros to fix naming convention.
 *
 * Here is a set of macros to patch up my apparent inability to provide a
 * consistently named set of functions. My apologies...
 */
#define iot_io_watch_add iot_add_io_watch
#define iot_io_watch_del iot_del_io_watch
#define iot_io_watch_get_mainloop iot_get_io_watch_mainloop
#define iot_io_event_mode_set iot_set_io_event_mode
#define iot_io_event_mode_get iot_get_io_event_mode

/**
 * @brief Murphy timers.
 *
 * Murphy timers provide a simple mechanism for the delayed execution of a
 * function or repeated execution of a function at certain intervals. A timer
 * is created with an interval, a callback to be triggered and an opaque user
 * data pointer to pass to the function. It is guaranteed that the function
 * will not be called more frequently than the specified interval. The mainloop
 * will try to minimize the difference between the specified and the actual
 * interval at which the callback is triggered.
 *
 * Timers can be dynamically stopped and restarted and the timer interval can
 * be dynamically changed. The timer interval resolution is 1 millisecond.
 */

/**
 * @brief Opaque Murphy timer type.
 */
typedef struct iot_timer_s iot_timer_t;

/**
 * @brief Timer notification callback type.
 *
 * This is the type of the callback function that will be associated with
 * a timer and will be triggered at the timer interval.
 *
 * @param [in] t          timer that has expired
 * @param [in] user_data  opaque user data associated with @t
 */
typedef void (*iot_timer_cb_t)(iot_timer_t *t, void *user_data);

/**
 * @brief Create a new Murphy timer.
 *
 * Create new timer for the given mainloop and the specified interval.
 *
 * @param [in] ml         mainloop to add the timer to
 * @param [in] msecs      timer interval, trigger @cb this often
 * @param [in] cb         callback to trigger
 * @param [in] user_data  opaque user data to pass to @cb
 *
 * @return Returns the newly created timer, or @NULL upon failure.
 */
iot_timer_t *iot_add_timer(iot_mainloop_t *ml, unsigned int msecs,
                           iot_timer_cb_t cb, void *user_data);

/**
 * @brief Modify the interval of the given timer.
 *
 * Update the given timer to be triggered at the new given iterval.
 *
 * @param [in] t      timer to modify
 * @param [in] msecs  new interval
 *
 * Use @IOT_TIMER_RESTART, or -1 for @msecs to simply restart the timer
 * without changing its interval.
 */
void iot_mod_timer(iot_timer_t *t, unsigned int msecs);

/**
 * @brief Macro to pass to @iot_mod_timer to simply restart a timer.
 */
#define IOT_TIMER_RESTART (unsigned int)-1

/**
 * @brief Delete the given timer.
 *
 * Deletes the given timer and stop delivering timer notifications for it.
 * Safe to be called from the timer notification callback.
 *
 * @param [in] t  timer to delete.
 */
void iot_del_timer(iot_timer_t *t);

/**
 * @brief Get the mainloop associated with a timer.
 *
 * Retrieve the mainloop for which the given timer has been created.
 *
 * @param [in] t  timer to get the mainloop for
 *
 * @return Return the mainloop associated with @t.
 */
iot_mainloop_t *iot_get_timer_mainloop(iot_timer_t *t);

/**
 * @brief Convenience macros to fix naming convention.
 *
 * Here is a set of macros to patch up my apparent inability to provide a
 * consistently named set of functions. My apologies...
 */
#define iot_timer_add iot_add_timer
#define iot_timer_del iot_del_timer
#define iot_timer_mod iot_mod_timer
#define iot_timer_get_mainloop iot_get_timer_mainloop


/**
 * @brief Murphy deferred callbacks.
 *
 * Murphy deferred callback provide a simple mechanism for delaying execution
 * of a function until all pending events of the mainloop has been processed.
 * A delayed callback will be called after all events have been processed and
 * delivered before the mainloop starts polling for new events.
 *
 * Deferred callbacks can be dynamically enabled and disabled. Deferred call-
 * backs cannot (easily) starve the mainloop, each active deferred callback
 * will be triggered exactly once per mainloop iteration.
 */

/**
 * @brief Opaque Murphy deferred callback type.
 */
typedef struct iot_deferred_s iot_deferred_t;

/**
 * @brief Deferred callback notification type.
 *
 * This is the type of the callback function that will be associated with
 * a deferred callback and will be triggered once per mainloop iteration if
 * it is enabled.
 *
 * @param [in] d          deferred callback
 * @param [in] user_data  opaque user data associated with @d
 */
typedef void (*iot_deferred_cb_t)(iot_deferred_t *d, void *user_data);

/**
 * @brief Create a new deferred callback.
 *
 * Create a new deferred callback for the given mainloop.
 *
 * @param [in] ml         mainloop to add the deferred callback to
 * @param [in] cb         callback to trigger
 * @param [in] user_data  opaque user data to pass to @cb
 *
 * @return Returns the newly created deferred callback, or @NULL upon failure.
 */
iot_deferred_t *iot_add_deferred(iot_mainloop_t *ml, iot_deferred_cb_t cb,
                                 void *user_data);

/**
 * @brief Remove a deferred callback.
 *
 * Delete the given deferred callback and stop triggering its associated
 * callback function. Safe to be called from an active callback.
 *
 * @param [in] d  defered callback to remove
 */
void iot_del_deferred(iot_deferred_t *d);

/**
 * @brief Disable a deferred callback.
 *
 * Disable the given deferred callback, don't trigger its notification
 * callback function until it is enabled again.
 *
 * @param [in] d  deferred callback to disable
 */
void iot_disable_deferred(iot_deferred_t *d);

/**
 * @brief Enable a deferred callback.
 *
 * Enable the given deferred callback, start triggering its notification
 * callback function until it removed or disabled again.
 *
 * @param [in] d  deferred callback to enabke
 */
void iot_enable_deferred(iot_deferred_t *d);

/**
 * @brief Get the mainloop associated with a deferred callback,
 *
 * Retrieve the mainloop for which the given deferred callback has been created.
 *
 * @param [in] d  deferred callback to get the mainloop for
 *
 * @return Return the mainloop associated with @d.
 */
iot_mainloop_t *iot_get_deferred_mainloop(iot_deferred_t *d);

/**
 * @brief Convenience macros to fix naming convention.
 *
 * Here is a set of macros to patch up my apparent inability to provide a
 * consistently named set of functions. My apologies...
 */
#define iot_deferred_add iot_add_deferred
#define iot_deferred_del iot_del_deferred
#define iot_deferred_enable iot_enable_deferred
#define iot_deferred_disable iot_disable_deferred
#define iot_deferred_get_mainloop iot_get_deferred_mainloop


/**
 * @brief Murphy signal handlers.
 *
 * Murphy signal handlers provide a mechanism to treat signals in an unified
 * manner to I/O and timers. Each signal handler created for a particular
 * signal will be called during the iteration of the mainloop, if that signal
 * has been delivered to the process running the mainloop since the previous
 * iteration.
 */

/**
 * @brief Opaque Murphy signal handler type.
 */
typedef struct iot_sighandler_s iot_sighandler_t;

/**
 * @brief Signal handler notification callback type.
 *
 * This is the type of the callback function that will be assocated with a
 * signal handler and triggered when the signal has been received.
 *
 * @param [in] h          signal handler
 * @param [in] signum     signal number of the received signal
 * @param [in] user_data  opaque user data associated with @h
 */
typedef void (*iot_sighandler_cb_t)(iot_sighandler_t *h, int signum,
                                    void *user_data);

/**
 * @brief Register a new signal handler.
 *
 * Create new signal handler for the given mainloop and signal number.
 *
 * @param [in] ml         mainloop to add signal handler to
 * @param [in] signum     signal number to add handler for
 * @param [in] user_data  opaque user data for @h
 *
 * @return Returns the newly created signal handler, or @NULL upon failure.
 */
iot_sighandler_t *iot_add_sighandler(iot_mainloop_t *ml, int signum,
                                     iot_sighandler_cb_t cb, void *user_data);

/**
 * @brief Unregister a signal handler.
 *
 * Unregister the given signal handler and stop delivering signal notifications
 * for the given signal to it. Safe to be called from the notification
 * callback.
 *
 * @param [in] h  signal handler to unregister
 */
void iot_del_sighandler(iot_sighandler_t *h);

/**
 * @brief Get the mainloop associated with a signal handler.
 *
 * Retrieve the mainloop for which the given signal handler has been created.
 *
 * @param [in] h  signal handler to get the mainloop for
 *
 * @return Return the mainloop associated with @h.
 */
iot_mainloop_t *iot_get_sighandler_mainloop(iot_sighandler_t *h);

/**
 * @brief Convenience macros to fix naming convention.
 *
 * Here is a set of macros to patch up my apparent inability to provide a
 * consistently named set of functions. My apologies...
 */
#define iot_sighandler_add iot_add_sighandler
#define iot_sighandler_del iot_del_sighandler
#define iot_sighandler_get_mainloop iot_get_sighandler_mainloop


/**
 * @brief Murphy wakeup callbacks.
 *
 * Murphy wakeup callbacks allow one to piggyback processing on events that
 * wake up the Murphy mainloop (timers, and I/O or signals) anyway but not
 * causing a wakeup by themselves. This can be useful in certain situations
 * on battery-powered devices that can frequently enter and come out of a
 * power-saving state. To avoid extra wakeups and for saving energy, one might
 * be able to spread certain non-time-critical processing across wakeups that
 * would happen anyway. Under some circumstances this can save a considerable
 * amount of energy.
 *
 * Wakeup callbacks can also be configured with a forced timeout and/or a
 * low-pass filter to control a minimum and a maximum frequency at which they
 * get triggered.
 */

/**
 * @brief Opaque Murphy wakeup callback type.
 */
typedef struct iot_wakeup_s iot_wakeup_t;

/**
 * @brief Wakeup events.
 *
 * Wakeup events specify under what conditions the wakeup callback should be
 * triggered.
 */
typedef enum {
    IOT_WAKEUP_EVENT_NONE  = 0x0,        /* no event */
    IOT_WAKEUP_EVENT_TIMER = 0x1,        /* woken up by timeout */
    IOT_WAKEUP_EVENT_IO    = 0x2,        /* woken up by I/O (or signal) */
    IOT_WAKEUP_EVENT_ANY   = 0x3,        /* mask of all selectable events */
    IOT_WAKEUP_EVENT_LIMIT = 0x4,        /* woken up by forced trigger */
} iot_wakeup_event_t;

/**
 * @brief Wakeup notification callback type.
 *
 * This is the type of the callback function that will be associated with
 * a wakeup callback and will be triggered under certain conditions.
 *
 * @param [in] w          the wakeup callback
 * @param [in] event      the event that caused the wakeup
 * @param [in] user_data  opaque user data associated with @w
 */
typedef void (*iot_wakeup_cb_t)(iot_wakeup_t *w, iot_wakeup_event_t event,
                                void *user_data);

/**
 * @brief Create a new Murphy wakeup callback.
 *
 * Create a new wakeup callback for the given mainloop and for the
 * specified events. If @lpf_msecs is specified it acts as a low-pass
 * filter on how frequently the notification callback can be triggered.
 * If @force_msecs is specified the mainloop will forcibly wake up (and
 * trigger the callback) at every @force_msecs millisecond intervals in
 * the absence of other events.
 *
 * @param [in] ml           mainloop to add the wakeup callback to
 * @param [in] events       bitmask of wakeup events to trigger this callback for
 * @param [in] lpf_msecs    low-pass filter, or -1
 * @param [in] force_msecs  forced callback interval, or -1
 * @param [in] cb           callback to trigger
 * @param [in] user_data    opaque user data to pass to @cb
 *
 * @return Returns the newly created wakeup callback, or @NULL upon failure.
 */
iot_wakeup_t *iot_add_wakeup(iot_mainloop_t *ml, iot_wakeup_event_t events,
                             unsigned int lpf_msecs, unsigned int force_msecs,
                             iot_wakeup_cb_t cb, void *user_data);

/**
 * @brief Macro to indicate the abscence of lpf_msecs or force_msecs.
 */
#define IOT_WAKEUP_NOLIMIT ((unsigned int)0)

/**
 * @brief Delete a wakeup callback.
 *
 * Remove the given wakeup callback from its mainloop. Safe to be called from
 * the notification callback.
 *
 * @param [in] w  the wakeup callback to remove
 */
void iot_del_wakeup(iot_wakeup_t *w);

/**
 * @brief Get the mainloop associated with a wakeup callback.
 *
 * Retrieve the mainloop for which the given wakeup callback has been created.
 *
 * @param [in] w  the wakeup callback to get the mainloop for
 *
 * @return Return the mainloop associated with @w.
 */

iot_mainloop_t *iot_get_wakeup_mainloop(iot_wakeup_t *w);

/**
 * @brief Convenience macros to fix naming convention.
 *
 * Here is a set of macros to patch up my apparent inability to provide a
 * consistently named set of functions. My apologies...
 */
#define iot_wakeup_add iot_add_wakeup
#define iot_wakeup_del iot_del_wakeup
#define iot_wakeup_get_mainloop iot_get_wakeup_mainloop


/**
 * @brief Murphy superloop.
 *
 * Murphy supoerloops serve an an abstraction to allow the Murphy mainloop
 * to be embedded or enslaved under the control of another mainloop. They
 * let the Murphy mainloop to be pumped by 'external' mainloops.
 *
 * This can come handy if one needs to integrate some Murphy-based component
 * into 3rd-party software or library which typically do not use a Murphy
 * mainloop. The most typical case is if that one needs to communicate with
 * Murphy from an existing external component (either directly or for instance
 * using one of the Murphy resource libraries) using a Murphy transport/IPC.
 * Murphy transports/IPC use a Murphy mainloop abstraction for I/O and timers.
 * In such a case, one can embed a Murphy mainloop under the real mainloop of
 * the hosting component, and pass that to the transport/IPC code to take care
 * of the communication details.
 *
 * Note that Murphy provides implementation for being embedded under many of
 * the popular mainloops, including GMainloop, PulseAudios mainloop, EFL/ecore
 * mainloop, Qt mainloop, Wayland mainloop, and the message pump of Crosswalk.
 */

/**
 * @brief Superloop operations.
 *
 * Superloop operations are essentially the basic primitives Murphy needs
 * an external mainloop to provide so that it can integrate itself under
 * that mainloop. The adaptation code which integrates Murphy to a particular
 * mainloop implementation needs to implement these using the APIs provided
 * by that external mainloop.
 *
 * Note that this interface might change in the future. Integrating to the
 * crosswalk infrastructure caused problems and changes which were not anti-
 * cipated  originally at all. They do not fit in nicely to the original
 * abstraction at all. We hope to address the resulting ugliness eventually
 * in an upcoming Murphy release.
 */
typedef struct {
    void *(*add_io)(void *glue_data, int fd, iot_io_event_t events,
                    void (*cb)(void *glue_data, void *id, int fd,
                               iot_io_event_t events, void *user_data),
                    void *user_data);
    void (*del_io)(void *glue_data, void *id);

    void *(*add_timer)(void *glue_data, unsigned int msecs,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data);
    void (*del_timer)(void *glue_data, void *id);
    void (*mod_timer)(void *glue_data, void *id, unsigned int msecs);

    void *(*add_defer)(void *glue_data,
                       void (*cb)(void *glue_data, void *id, void *user_data),
                       void *user_data);
    void  (*del_defer)(void *glue_data, void *id);
    void  (*mod_defer)(void *glue_data, void *id, int enabled);
    void  (*unregister)(void *glue_data);

    /*
     * Notes:
     *
     *     This is a band-aid attempt to get our mainloop run under the
     *     threaded event loop of xwalk which has strict limitations about
     *     what (type of) thread can access which functionality of the
     *     event loop. In particular, the I/O watch equivalent is limited
     *     for the I/O thread. In the current media element resource infra
     *     integration code of xwalk, the related code is run in the UI
     *     thread. This makes interacting from there with the daemon using
     *     the stock resource libraries (in particular, pumping the mainloop)
     *     non-straightforward.
     *
     *     The superloop glue code is supposed to use poll_events to trigger
     *     a nonblocking epoll_wait on our epoll fd to retrieve (and cache)
     *     pending epoll events from the kernel. poll_io is then used by us
     *     to retrieve pending epoll events from the glue code. The glue code
     *     needs to take care of any necessary locking to protect itself/us
     *     from potentially concurrent invocations of poll_io and poll_events
     *     from different threads.
     *
     *     The superloop abstraction now became really really ugly. poll_io
     *     and poll_events implicitly assume/know that add_io/del_io is only
     *     used for adding a single superloop I/O watch for our epoll fd.
     *     The I/O watch id in the functions below is passed around only for
     *     doublechecing this. Probably we should change the I/O watch part
     *     of the superloop API to be actually less generic and only usable
     *     for the epoll fd to avoid further confusion.
     */
    size_t (*poll_events)(void *id, iot_mainloop_t *ml, void **events);
    size_t (*poll_io)(void *glue_data, void *id, void *buf, size_t size);
} iot_superloop_ops_t;

/**
 * @brief Set a superloop to pump the given Murphy mainloop.
 *
 * Set the given superloop up as the pumping mainloop for @ml.
 *
 * @param [in] ml         mainloop to be pumped by the superloop
 * @param [in] ops        superloop control primitives
 * @param [in] loop_data  opaque superloop user data
 *
 * @return Returns @TRUE upon success, @FALSE otherwise.
 */
int iot_set_superloop(iot_mainloop_t *ml, iot_superloop_ops_t *ops,
                      void *loop_data);

/**
 * @brief Clear the superloop that pumps a Murphy mainloop.
 *
 * Clear the superloop effectively stopping it from pumping @ml.
 *
 * @param [in] ml  the mainloop to disassociate from its superloop
 *
 * @return Returns @TRUE upon success, @FALSE otherwise.
 */
int iot_clear_superloop(iot_mainloop_t *ml);


/**
 * @brief Unregister a mainloop from its superloop if it has one.
 *
 * This just an alias for @iot_clear_superloop.
 */
int iot_mainloop_unregister(iot_mainloop_t *ml);

/**
 * @brief Convenience macros to fix naming convention.
 *
 * Here is a set of macros to patch up my apparent inability to provide a
 * consistently named set of functions. My apologies...
 */
#define iot_superloop_set iot_set_superloop
#define iot_superloop_clear iot_clear_superloop


/**
 * @brief Murphy mainloop.
 *
 * The Murphy mainloop provides the basis for asynchronous single-threaded
 * event processing within Murphy. I/O watches, timers, deferred callbacks,
 * signal handlers and wakeup callbacks, as well as all object that use these
 * are inhrently operating within the context of a mainloop.
 */

/**
 * @brief Create a new mainloop.
 *
 * Creates and initializes a new Murphy mainloop.
 *
 * @return Return the new mainloop, or @NULL upon failure.
 */
iot_mainloop_t *iot_mainloop_create(void);

/**
 * @brief Destroy an existing mainloop.
 *
 * Destroys the given mainloop, freeing all associated I/O watches, timers,
 * deferred callbacks, signal handlers, etc.
 *
 * @param [in] ml  mainloop to destroy
 */
void iot_mainloop_destroy(iot_mainloop_t *ml);

/**
 * @brief Run a mainloop.
 *
 * Keep iterating the given mainloop until @iot_mainloop_quit is called
 * on it.
 *
 * @param [in] ml  the mainloop to run
 *
 * @return Returns the exit code @iot_mainloop_quit has been called with.
 */
int iot_mainloop_run(iot_mainloop_t *ml);

/**
 * @brief Prepare a mainloop for polling.
 *
 * Prepare the given mainloop for polling.
 *
 * @param [in] ml  mainloop to prepare
 *
 * @return Returns @TRUE on success, @FALSE otherwise.
 */
int iot_mainloop_prepare(iot_mainloop_t *ml);

/**
 * @brief Poll a mainloop.
 *
 * Poll the given mainloop, waiting for I/O, signals, or timers if @may_block
 * allows blocking.
 *
 * @param [in] ml         mainloop to prepare
 * @param [in] may_block  non-zero, if the mainloop is allowed to block in poll
 *
 * @return Returns @TRUE on success, @FALSE otherwise.
 */
int iot_mainloop_poll(iot_mainloop_t *ml, int may_block);

/**
 * @brief Dispatch pending events for a mainloop.
 *
 * Dispatch pending events within the given mainloop.
 *
 * @param [in] ml  mainloop to dispatch
 *
 * @return Returns @TRUE if @iot_mainloop_quit has not been called for @ml.
 */
int iot_mainloop_dispatch(iot_mainloop_t *ml);

/**
 * @brief Iterate a mainloop.
 *
 * Run a single prepare-poll-dispatch iteration cycle of the given mainloop.
 *
 * @param [in] ml  mainloop to iterate
 *
 * @return Returns @TRUE if @iot_mainloop_quit has not been called for @ml.
 */
int iot_mainloop_iterate(iot_mainloop_t *ml);

/**
 * @brief Request a mainloop to quit.
 *
 * Request the given mainloop to stop iterating and quit.
 *
 * @param [in] ml         mainloop to quit
 * @param [in] exit_code  exit code to pass to/return from the mainloop
 */
void iot_mainloop_quit(iot_mainloop_t *ml, int exit_code);


/**
 * @brief Murphy event bus and events.
 *
 * Murphy events provide a simple publish-subscribe mechanism with support
 * for both asynchronous and synchronous event delivery and for attaching
 * arbitrary data to emitted events. Events are registered dynamically and
 * they can be referred to either by their well-known names or by their
 * dynamically assigned integer identifiers.
 *
 * Event buses provide the means for grouping logically similar or related
 * events and also for isolating these from other types of events. Event
 * busses can be created dynamically and virtually there is no limit to the
 * number of allowed busses. There is a default global bus - also known as
 * the NULL bus - which is always available. The NULL bus is not associated
 * with any particular mainloop and hence only supports synchronous event
 * delivery. Other busses that are associated with a mainloop support both
 * synchronous and asynchronous event emission. Asynchronous events are
 * delivered from a deferred callback at the end of the mainloop iteration.
 *
 * Subscribing to events happens by specifying the event bus to listen on and
 * the bitmask of events of interest. Since the subscription is bus-specific,
 * although event identifiers are global, events that are emitted on other
 * busses are never delivered to a subscriber, even if the event identifier
 * matches the event mask.
 *
 * Data can be attached to an events in different formats. Currently, JSON
 * (iot_json_t), and custom emitter-specific formats are supported. For JSON
 * the infrastructure takes care of the lifecycle management of the attached
 * data. For custom data, currently the emitter needs to take care of it
 * (although this is probably subject to change in a future release).
 */

#include <iot/common/mask.h>


/**
 * @brief Opaque Murphy event watch type.
 */
typedef struct iot_event_watch_s iot_event_watch_t;

/**
 * @brief Event watch notification callback type.
 *
 * @param [in] w          watch being notified
 * @param [in] id         id of event being delivered
 * @param [in] format     format of attached event data
 * @param [in] data       attached event data
 * @param [in] user_data  opaque user data associated with @w
 */
typedef void (*iot_event_watch_cb_t)(iot_event_watch_t *w, uint32_t id,
                                     int format, void *data, void *user_data);

/**
 * @brief Opaque Murphy event bus type.
 */
typedef struct iot_event_bus_s iot_event_bus_t;

/**
 * @brief Event mask type, used for subscribing to events.
 */
typedef iot_mask_t iot_event_mask_t;


/**
 * @brief Event definition type.
 *
 * This type is used for defining events, ie. for registering event names.
 */
typedef struct {
    char     *name;                      /**< event name */
    uint32_t  id;                        /**< event id assigned by Murphy */
} iot_event_def_t;

/**
 * @brief Flags for event emission.
 */
typedef enum {
    IOT_EVENT_ASYNCHRONOUS  = 0x00,      /**< deliver asynchronously */
    IOT_EVENT_SYNCHRONOUS   = 0x01,      /**< deliver synchronously */
    IOT_EVENT_FORMAT_JSON   = 0x01 << 1, /**< attached data JSON */
    IOT_EVENT_FORMAT_CUSTOM = 0x02 << 1, /**< attached data of custom format */
    IOT_EVENT_FORMAT_MASK   = 0x03 << 1,
} iot_event_flag_t;

/**
 * @brief Macro to be used for the default synchronous global bus.
 */
#define IOT_GLOBAL_BUS NULL

/**
 * @brief Macro that can be used for the name of the default global bus.
 */
#define IOT_GLOBAL_BUS_NAME "global"

/**
 * @brief Reserved identifier to denote unknown events.
 */
#define IOT_EVENT_UNKNOWN 0

/**
 * @brief Reserved name for unknown events.
 */
#define IOT_EVENT_UNKNOWN_NAME "unknown"

/**
 * @brief Look up (or create) an event bus.
 *
 * Look up the event bus with the given name. Will create the named
 * event bus if it does not exist.
 *
 * @param [in] ml    mainloop associated with the bus
 * @param [in] name  name of the bus
 *
 * @return Returns the named event bus, or @NULL upon failure.
 */
iot_event_bus_t *iot_event_bus_get(iot_mainloop_t *ml, const char *name);
#define iot_event_bus_create iot_event_bus_get

/**
 * @brief Look up the identifier of a named event.
 *
 * Look up the identifier of the event with the given name. Will create
 * the named event if it does not exist.
 *
 * @param [in] name  name of the event to look up
 *
 * @return Returns the identifier of the named event, or @IOT_EVENT_UNKNOWN
 *         upon error.
 */
uint32_t iot_event_id(const char *name);
#define iot_event_register iot_event_id

/**
 * @brief Look up the name of the event for the given identifier.
 *
 * Look up the name of the event for the given name.
 *
 * @param [in] id  identifier of the event to look up
 *
 * @return Returns the name of the found event, or @IOT_EVENT_UNKNOWN_NAME
 *         upon error.
 */
const char *iot_event_name(uint32_t id);

/**
 * @brief Dump an event mask.
 *
 * Dump the names of the events set in mask into the given buffer.
 *
 * @param [in] mask  event mask to dump
 * @param [out] buf  buffer to dump mask into
 * @param [in] size  size of @buf
 *
 * @return Returns the buffer of dumped event mask.
 */
char *iot_event_dump_mask(iot_event_mask_t *mask, char *buf, size_t size);

/**
 * @brief Subscribe for a single event on a bus.
 *
 * Register an event watch for a single event on the given bus.
 *
 * @param [in] bus        event bus to subscribe on
 * @param [in] id         identifier of event to subscribe for
 * @param [in] cb         event notification callback
 * @param [in] user_data  opaque user data to pass to @cb
 *
 * @return Returns the newly created event watch, or @NULL upon failure.
 */
iot_event_watch_t *iot_event_add_watch(iot_event_bus_t *bus, uint32_t id,
                                       iot_event_watch_cb_t cb, void *user_data);

/**
 * @brief Subscribe for a set of events on a bus.
 *
 * Register for a set of events on the given bus.
 *
 * @param [in] bus        event bus to subscribe on
 * @param [in] mask       mask of event identifier to subscribe for
 * @param [in] cb         event notification callback
 * @param [in] user_data  opaque user data to pass to @cb
 *
 * @return Returns the newly created event watch, or @NULL upon failure.
 */
iot_event_watch_t *iot_event_add_watch_mask(iot_event_bus_t *bus,
                                            iot_event_mask_t *mask,
                                            iot_event_watch_cb_t cb,
                                            void *user_data);

/**
 * @brief Remove an event watch, unsubscribing from events.
 *
 * Remove the given event watch and unsubscribe from the associated events.
 *
 * @param [in] w  event watch to remove
 */
void iot_event_del_watch(iot_event_watch_t *w);

/**
 * @brief Emit an event on a bus
 *
 * Emit the event with the given identifier on the given bus (which can also
 * be the NULL bus). Attach the given data to the emitted event.
 *
 * @param [in] bus    bus to emit the event on, or @NULL for the global bus
 * @param [in] id     identifier of the event to emit
 * @param [in] flags  flags for event emission, specify whether the event
 *                    should be emitted sycnhronously and the format of @data
 * @param [in] data   data to attach to the event
 *
 * @return Returns 0 upon success, -1 upon failure in which case @errno
 *         is also set.
 */
int iot_event_emit(iot_event_bus_t *bus, uint32_t id, iot_event_flag_t flags,
                   void *data);

/**
 * @brief Convenience macro to emit an event with JSON data attached.
 *
 * Use this macro to emit an event with iot_json_t data attached.
 *
 * @param [in] bus    bus to emit event on
 * @param [in] id     event identifier
 * @param [in] flags  event emission flags (asynchronous vs. synchronous)
 * @param [in] data   iot_json_t data to attach
 *
 * @return Returns 0 upon success, -1 upon failure.
 */
#define iot_event_emit_json(bus, id, flags, data)                 \
    iot_event_emit((bus), (id), (flags) | IOT_EVENT_FORMAT_JSON, (data))

/**
 * @brief Convenience macro to emit an event with custom data attached.
 *
 * Use this macro to emit an event with custom data attached.
 *
 * @param [in] bus    bus to emit event on
 * @param [in] id     event identifier
 * @param [in] flags  event emission flags (asynchronous vs. synchronous)
 * @param [in] data   custom data to attach
 *
 * @return Returns 0 upon success, -1 upon failure.
 */
#define iot_event_emit_custom(bus, id, data, flags) \
    iot_event_emit((bus), (id), (data), (flags) | IOT_EVENT_FORMAT_CUSTOM)

/**
 * @brief Convenience macros for autoregistering a table of events.
 *
 * These macros can be used to automatically register a set of events
 * on startup.
 */

/**
 * @brief Macro to initialize a single event in a table passed to
 *        @IOT_REGISTER_EVENTS.
 *
 * @param [in] _name  event name
 * @param [in _id     id/index of event witin the table
 */
#define IOT_EVENT(_name, _id) [_id] = { .name = _name, .id = 0 }

/**
 * @brief Macro to automatically register a table of events.
 *
 * @param [in] _event_table  name of the event table to generate
 * @param [in] ...           event entries (use @IOT_EVENT for these)
 */
#define IOT_REGISTER_EVENTS(_event_table, ...)                  \
    static iot_event_def_t _event_table[] = {                   \
        __VA_ARGS__,                                            \
        { .name = NULL }                                        \
    };                                                          \
                                                                \
    IOT_INIT static void register_##_event_table(void)          \
    {                                                           \
        iot_event_def_t *e;                                     \
                                                                \
        for (e = _event_table; e->name != NULL; e++) {          \
            e->id = iot_event_register(e->name);                \
                                                                \
            if (e->id != IOT_EVENT_UNKNOWN)                     \
                iot_log_info("Event '%s' registered as 0x%x.",  \
                             e->name, e->id);                   \
            else                                                \
                iot_log_error("Failed to register event '%s'.", \
                              e->name);                         \
        }                                                       \
    }                                                           \
    struct __iot_allow_trailing_semicolon

/**
 * @}
 */

IOT_CDECL_END

#endif /* __IOT_MAINLOOP_H__ */
