// One medium, one direction, one thread.
//
// Every stream this project carries - outgoing video, outgoing audio, incoming
// audio - gets its own GstPipeline driven by its own GMainLoop on its own
// thread. Audio and video are never synchronised, so there is nothing to gain
// from sharing a pipeline and three concrete things to lose:
//
//   * Latency is negotiated per pipeline as the MAX over all sinks. An audio
//     sink reporting 20 ms of device buffering would raise the video sink's
//     configured latency by the same amount, and vice versa.
//   * A pipeline has one clock. Live audio capture and the video pacer would be
//     slaved to the same clock even though nothing needs them aligned.
//   * State changes and errors are pipeline-wide. A microphone that fails to
//     open would take the video stream down with it.
//
// Separate pipelines remove all three. The cost is that each worker needs its
// own main context, because bus watches and timers added with the plain
// g_timeout_add()/gst_bus_add_watch() family land on the default context, which
// belongs to whichever thread happens to be running it.

#ifndef MEDIA_WORKER_H
#define MEDIA_WORKER_H

#include <gst/gst.h>
#include <glib.h>

typedef struct _MpWorker MpWorker;

// Return TRUE to say the message is fully handled and suppress the default
// handling below it.
typedef gboolean (*MpBusHook)(MpWorker *worker, GstMessage *message,
                              gpointer user_data);

struct _MpWorker {
    gchar *name;
    GstElement *pipeline;
    GMainContext *context;
    GMainLoop *loop;
    GThread *thread;

    MpBusHook bus_hook;
    gpointer bus_hook_data;

    GPtrArray *sources;      // GSource* attached to context, released on stop

    // Set from the bus watch, read by the supervisor. A worker that fails does
    // not take the process down: video keeps flowing when the microphone dies.
    gint failed;
    gchar *error_text;

    // Called (on the worker's thread) the first time this worker fails, so the
    // owner can decide whether that stream was essential.
    void (*on_failure)(MpWorker *worker, gpointer user_data);
    gpointer on_failure_data;

    gint started;
};

static MpWorker* mp_worker_new(const gchar *name) {
    MpWorker *w = g_new0(MpWorker, 1);
    w->name = g_strdup(name);
    w->context = g_main_context_new();
    w->loop = g_main_loop_new(w->context, FALSE);
    w->sources = g_ptr_array_new();
    return w;
}

static void mp_worker_fail(MpWorker *w, const gchar *text) {
    if (g_atomic_int_compare_and_exchange(&w->failed, 0, 1)) {
        w->error_text = g_strdup(text);
        if (w->on_failure) {
            w->on_failure(w, w->on_failure_data);
        }
    }
}

static gboolean mp_worker_bus_cb(GstBus *bus, GstMessage *message, gpointer user_data) {
    (void)bus;
    MpWorker *w = (MpWorker *)user_data;

    if (w->bus_hook && w->bus_hook(w, message, w->bus_hook_data)) {
        return TRUE;
    }

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError *error = NULL;
            gchar *debug = NULL;
            gst_message_parse_error(message, &error, &debug);
            g_printerr("[%s] error from %s: %s\n", w->name,
                       GST_OBJECT_NAME(message->src), error->message);
            if (debug) {
                g_printerr("[%s] debug: %s\n", w->name, debug);
            }
            mp_worker_fail(w, error->message);
            g_error_free(error);
            g_free(debug);
            g_main_loop_quit(w->loop);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError *error = NULL;
            gchar *debug = NULL;
            gst_message_parse_warning(message, &error, &debug);
            g_print("[%s] warning from %s: %s\n", w->name,
                    GST_OBJECT_NAME(message->src), error->message);
            g_error_free(error);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS:
            g_print("[%s] end of stream\n", w->name);
            g_main_loop_quit(w->loop);
            break;
        default:
            break;
    }
    return TRUE;
}

// Hand the worker its pipeline. The bus watch is attached to this worker's own
// context, so messages are delivered on this worker's thread and nowhere else.
static void mp_worker_set_pipeline(MpWorker *w, GstElement *pipeline,
                                   MpBusHook hook, gpointer hook_data) {
    w->pipeline = pipeline;
    w->bus_hook = hook;
    w->bus_hook_data = hook_data;

    GstBus *bus = gst_element_get_bus(pipeline);
    GSource *source = gst_bus_create_watch(bus);
    g_source_set_callback(source, (GSourceFunc)mp_worker_bus_cb, w, NULL);
    g_source_attach(source, w->context);
    g_ptr_array_add(w->sources, source);
    gst_object_unref(bus);
}

// Periodic callback on this worker's thread. Used for the per-second statistics
// and for polling the DTLS handshake result.
static void mp_worker_add_timeout(MpWorker *w, guint interval_ms,
                                  GSourceFunc callback, gpointer data) {
    GSource *source = g_timeout_source_new(interval_ms);
    g_source_set_callback(source, callback, data, NULL);
    g_source_attach(source, w->context);
    g_ptr_array_add(w->sources, source);
}

static gpointer mp_worker_thread_func(gpointer user_data) {
    MpWorker *w = (MpWorker *)user_data;
    // Anything this thread creates that wants a context - GIO async work, glib
    // timers - should use the worker's, not the global default.
    g_main_context_push_thread_default(w->context);
    g_main_loop_run(w->loop);
    g_main_context_pop_thread_default(w->context);
    return NULL;
}

// Start the loop thread first, then go to PLAYING, so the ASYNC_DONE and any
// early error is seen by a running watch rather than sitting in the queue.
static gboolean mp_worker_start(MpWorker *w) {
    if (!w->pipeline) {
        return FALSE;
    }
    gchar *thread_name = g_strdup_printf("mw-%s", w->name);
    w->thread = g_thread_new(thread_name, mp_worker_thread_func, w);
    g_free(thread_name);

    if (gst_element_set_state(w->pipeline, GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE) {
        g_printerr("[%s] failed to start the pipeline\n", w->name);
        mp_worker_fail(w, "state change to PLAYING failed");
        g_main_loop_quit(w->loop);
        g_thread_join(w->thread);
        w->thread = NULL;
        return FALSE;
    }
    g_atomic_int_set(&w->started, 1);
    return TRUE;
}

static void mp_worker_stop(MpWorker *w) {
    if (!w) {
        return;
    }
    if (w->loop) {
        g_main_loop_quit(w->loop);
    }
    if (w->thread) {
        g_thread_join(w->thread);
        w->thread = NULL;
    }
    if (w->pipeline) {
        gst_element_set_state(w->pipeline, GST_STATE_NULL);
    }
    for (guint i = 0; i < w->sources->len; i++) {
        GSource *source = (GSource *)g_ptr_array_index(w->sources, i);
        g_source_destroy(source);
        g_source_unref(source);
    }
    g_ptr_array_set_size(w->sources, 0);
    g_atomic_int_set(&w->started, 0);
}

static void mp_worker_free(MpWorker *w) {
    if (!w) {
        return;
    }
    mp_worker_stop(w);
    if (w->pipeline) {
        gst_object_unref(w->pipeline);
        w->pipeline = NULL;
    }
    g_ptr_array_free(w->sources, TRUE);
    g_main_loop_unref(w->loop);
    g_main_context_unref(w->context);
    g_free(w->error_text);
    g_free(w->name);
    g_free(w);
}

#endif  // MEDIA_WORKER_H
