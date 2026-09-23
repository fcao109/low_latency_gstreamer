// Version-tolerant element property helpers, shared by every binary.
//
// Element properties differ between GStreamer releases (nvh264enc gained
// "zerolatency" after 1.16, enum values for "preset"/"rc-mode" have been
// renumbered, opusenc's "frame-size" is an enum of strings). Setting a missing
// property emits a warning and setting a stale integer can silently select the
// wrong mode, so probe before assigning and resolve enums by nickname rather
// than by number.
//
// server.cpp and client.cpp still carry their own copies of the same helpers.
// That duplication is deliberate: the proven UDP video path keeps the exact
// code it was measured with, and only the code added for audio and encryption
// uses the shared versions here.

#ifndef MEDIA_PROPS_H
#define MEDIA_PROPS_H

#include <gst/gst.h>
#include <glib.h>

static GParamSpec* mp_find_prop(GstElement *element, const gchar *name) {
    if (!element || !name) {
        return NULL;
    }
    return g_object_class_find_property(G_OBJECT_GET_CLASS(element), name);
}

// Assign a numeric property regardless of whether the plugin declares it as
// int/uint/int64/uint64, clamping to the range it advertises. Plugins disagree
// on the exact numeric type of properties such as "bitrate" and "bframes", and
// passing the wrong width through g_object_set() is undefined behaviour.
static gboolean mp_set_number_prop(GstElement *element, const gchar *name,
                                   gint64 value) {
    GParamSpec *spec = mp_find_prop(element, name);
    if (!spec) {
        return FALSE;
    }
    if (G_IS_PARAM_SPEC_INT(spec)) {
        GParamSpecInt *s = G_PARAM_SPEC_INT(spec);
        g_object_set(element, name,
                     (gint)CLAMP(value, (gint64)s->minimum, (gint64)s->maximum), NULL);
    } else if (G_IS_PARAM_SPEC_UINT(spec)) {
        GParamSpecUInt *s = G_PARAM_SPEC_UINT(spec);
        if (value < 0) value = 0;
        g_object_set(element, name,
                     (guint)CLAMP((guint64)value, (guint64)s->minimum,
                                  (guint64)s->maximum), NULL);
    } else if (G_IS_PARAM_SPEC_INT64(spec)) {
        GParamSpecInt64 *s = G_PARAM_SPEC_INT64(spec);
        g_object_set(element, name, CLAMP(value, s->minimum, s->maximum), NULL);
    } else if (G_IS_PARAM_SPEC_UINT64(spec)) {
        GParamSpecUInt64 *s = G_PARAM_SPEC_UINT64(spec);
        if (value < 0) value = 0;
        g_object_set(element, name,
                     (guint64)CLAMP((guint64)value, s->minimum, s->maximum), NULL);
    } else if (G_IS_PARAM_SPEC_DOUBLE(spec)) {
        GParamSpecDouble *s = G_PARAM_SPEC_DOUBLE(spec);
        g_object_set(element, name,
                     CLAMP((gdouble)value, s->minimum, s->maximum), NULL);
    } else {
        return FALSE;
    }
    return TRUE;
}

static gboolean mp_set_bool_prop(GstElement *element, const gchar *name,
                                 gboolean value) {
    if (!mp_find_prop(element, name)) {
        return FALSE;
    }
    g_object_set(element, name, value, NULL);
    return TRUE;
}

static gboolean mp_set_string_prop(GstElement *element, const gchar *name,
                                   const gchar *value) {
    if (!mp_find_prop(element, name) || !value) {
        return FALSE;
    }
    g_object_set(element, name, value, NULL);
    return TRUE;
}

// Assign an enum/flags property using the first nickname the installed plugin
// recognises. Returns the nickname that was applied, or NULL if none matched.
static const gchar* mp_set_enum_prop(GstElement *element, const gchar *name,
                                     const gchar *const *nicknames) {
    GParamSpec *spec = mp_find_prop(element, name);
    if (!spec || !nicknames) {
        return NULL;
    }
    if (G_TYPE_IS_ENUM(spec->value_type)) {
        GEnumClass *enum_class = G_ENUM_CLASS(g_type_class_ref(spec->value_type));
        const gchar *applied = NULL;
        for (gint i = 0; nicknames[i] != NULL && applied == NULL; i++) {
            GEnumValue *match = g_enum_get_value_by_nick(enum_class, nicknames[i]);
            if (match) {
                g_object_set(element, name, match->value, NULL);
                applied = nicknames[i];
            }
        }
        g_type_class_unref(enum_class);
        return applied;
    }
    // Flags and other serialisable types go through GStreamer's string parser.
    if (nicknames[0]) {
        gst_util_set_object_arg(G_OBJECT(element), name, nicknames[0]);
        return nicknames[0];
    }
    return NULL;
}

static gboolean mp_have_element(const gchar *factory_name) {
    GstElementFactory *factory = gst_element_factory_find(factory_name);
    if (!factory) {
        return FALSE;
    }
    gst_object_unref(factory);
    return TRUE;
}

// Create the first of the listed factories that exists, so a pipeline can
// prefer a better element and still run where it is not installed.
static GstElement* mp_make_first(const gchar *const *factories,
                                 const gchar *element_name,
                                 const gchar **chosen) {
    for (gint i = 0; factories && factories[i]; i++) {
        GstElement *element = gst_element_factory_make(factories[i], element_name);
        if (element) {
            if (chosen) {
                *chosen = factories[i];
            }
            return element;
        }
    }
    if (chosen) {
        *chosen = NULL;
    }
    return NULL;
}

// Read the timestamp out of an RTP packet header (bytes 4-7, big endian). Done
// by hand to avoid a dependency on the gstrtp library for one field. Still
// correct for SRTP: encryption covers the payload, not the header.
static gboolean mp_rtp_timestamp_of(GstBuffer *buffer, guint32 *timestamp) {
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        return FALSE;
    }
    gboolean ok = FALSE;
    if (map.size >= 12) {
        *timestamp = GST_READ_UINT32_BE(map.data + 4);
        ok = TRUE;
    }
    gst_buffer_unmap(buffer, &map);
    return ok;
}

// Sink wrappers such as autovideosink and autoaudiosink only create their real
// sink during the state change, and proxy "sync" but not "qos"/"max-lateness".
// Walk in and stop the real sink dropping data, otherwise it sends QoS upstream
// and the decoder starts skipping - which looks exactly like packet loss from
// the outside and is very easy to misdiagnose.
static void mp_relax_sink_dropping(GstElement *element, gboolean *applied) {
    if (GST_IS_BIN(element)) {
        GstIterator *it = gst_bin_iterate_elements(GST_BIN(element));
        GValue item = G_VALUE_INIT;
        while (gst_iterator_next(it, &item) == GST_ITERATOR_OK) {
            mp_relax_sink_dropping(GST_ELEMENT(g_value_get_object(&item)), applied);
            g_value_reset(&item);
        }
        g_value_unset(&item);
        gst_iterator_free(it);
        return;
    }
    GObjectClass *klass = G_OBJECT_GET_CLASS(element);
    if (g_object_class_find_property(klass, "qos")) {
        g_object_set(element, "qos", FALSE, NULL);
        *applied = TRUE;
    }
    if (g_object_class_find_property(klass, "max-lateness")) {
        g_object_set(element, "max-lateness", (gint64)-1, NULL);
    }

    // Read back rather than trusting the set. A sink that keeps QoS enabled makes
    // the decoder throw away data to catch up.
    if (*applied) {
        gboolean qos = TRUE;
        g_object_get(element, "qos", &qos, NULL);
        g_print("Sink tuned: %s qos=%s\n", GST_ELEMENT_NAME(element),
                qos ? "TRUE (data will be dropped!)" : "false");
    }
}

#endif  // MEDIA_PROPS_H
