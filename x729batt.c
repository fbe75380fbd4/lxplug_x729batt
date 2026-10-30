/*
 *      x729batt.c
 *
 *      Copyright (c) 2024 Enrique Mejia.
 *      All rights reserved.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 */

#include "version.h"
#include "batt_i2c.h"

#include "lxpanel/plugin.h"

#define CMON_INTERVAL 60000
#define VMON_INTERVAL 60000
#define LOW_VOLTAGE_WARNING_VOLTS 3.33f
#define TOOLTIP_BUFFER_SIZE 255

enum
{
    LOW_BATTERY_PERCENT = 20,
    MEDIUM_BATTERY_PERCENT = 50,
    ICON_BASE_WIDTH = 36,
    ICON_HEIGHT_RATIO = 10,
    ICON_HEIGHT_EVEN_FACTOR = 2,
    ICON_MIN_HEIGHT = 18,
    ICON_FILL_FULL_PERCENT = 97,
    ICON_FILL_WIDTH_INSET = 12
};

/* Plug-in global data */
typedef struct
{
    GtkWidget *plugin;
    LXPanel *panel;
    GtkWidget *tray_icon;
    config_setting_t *settings;
    battery *batt;
    guint timer;
    guint vtimer;
} x729BattPlugin;

/* Device status */
typedef enum
{
    STATUS_UNKNOWN = 0,
    STATUS_PRESENT = 1
} device_status_t;

/* Power source */
typedef enum
{
    POW_ADAPTER = 0,
    POW_BATTERY = 1
} power_source_t;

/* Prototypes */
static gboolean convert_alpha(guchar *dest_data, int dest_stride, guchar *src_data, int src_stride, int src_x, int src_y, int width, int height);
static int init_battery(x729BattPlugin *plugin);
static int update_battery(x729BattPlugin *plugin, device_status_t *status);
static void draw_icon(x729BattPlugin *plugin, int level);
static void update_icon(x729BattPlugin *plugin);
static gboolean timer_event(x729BattPlugin *plugin);
static gboolean vtimer_event(x729BattPlugin *plugin);

/* Converts premultiplied Cairo pixels into the pixbuf's pixel layout. */
static gboolean convert_alpha(guchar *dest_data, int dest_stride, guchar *src_data, int src_stride, int src_x, int src_y, int width, int height)
{
    int x, y;
    gsize row_size, src_x_offset, src_y_offset;
    gsize last_src_row_offset, last_dest_row_offset;

    if (!dest_data || !src_data || dest_stride <= 0 || src_stride <= 0 ||
        src_x < 0 || src_y < 0 || width <= 0 || height <= 0)
        return FALSE;

    if ((gsize)width > G_MAXSIZE / 4)
        return FALSE;
    row_size = (gsize)width * 4;
    if (row_size > (gsize)dest_stride || row_size > (gsize)src_stride ||
        (gsize)src_x > G_MAXSIZE / 4)
        return FALSE;

    src_x_offset = (gsize)src_x * 4;
    if (src_x_offset > (gsize)src_stride - row_size ||
        (gsize)(height - 1) > G_MAXSIZE / (gsize)dest_stride ||
        (gsize)(height - 1) > G_MAXSIZE / (gsize)src_stride ||
        (gsize)src_y > G_MAXSIZE / (gsize)src_stride)
        return FALSE;

    src_y_offset = (gsize)src_stride * src_y;
    if (src_x_offset > G_MAXSIZE - src_y_offset)
        return FALSE;

    last_src_row_offset = (gsize)(height - 1) * src_stride;
    last_dest_row_offset = (gsize)(height - 1) * dest_stride;
    if (last_src_row_offset > G_MAXSIZE - src_y_offset - src_x_offset ||
        row_size > G_MAXSIZE - src_y_offset - src_x_offset - last_src_row_offset ||
        row_size > G_MAXSIZE - last_dest_row_offset)
        return FALSE;

    for (y = 0; y < height; y++)
    {
        const guint32 *src = (const guint32 *)(src_data + src_y_offset +
                                               (gsize)y * src_stride + src_x_offset);
        guchar *dest_row = dest_data + (gsize)y * dest_stride;

        for (x = 0; x < width; x++)
        {
            guint alpha = src[x] >> 24;
            gsize dest_offset = (gsize)x * 4;

            if (alpha == 0)
            {
                dest_row[dest_offset + 0] = 0;
                dest_row[dest_offset + 1] = 0;
                dest_row[dest_offset + 2] = 0;
            }
            else
            {
                dest_row[dest_offset + 0] = (((src[x] & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
                dest_row[dest_offset + 1] = (((src[x] & 0x00ff00) >> 8) * 255 + alpha / 2) / alpha;
                dest_row[dest_offset + 2] = (((src[x] & 0x0000ff) >> 0) * 255 + alpha / 2) / alpha;
            }
            dest_row[dest_offset + 3] = alpha;
        }
    }

    return TRUE;
}

/* Converts an image-surface region to a pixbuf and consumes the surface. */
static GdkPixbuf *pixbuf_from_cairo_surface(cairo_surface_t *surface, gint src_x, gint src_y, gint width, gint height)
{
    GdkPixbuf *dest;

    if (!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS ||
        cairo_surface_get_type(surface) != CAIRO_SURFACE_TYPE_IMAGE ||
        cairo_image_surface_get_format(surface) != CAIRO_FORMAT_ARGB32 ||
        width <= 0 || height <= 0 || src_x < 0 || src_y < 0 ||
        src_x > cairo_image_surface_get_width(surface) - width ||
        src_y > cairo_image_surface_get_height(surface) - height)
    {
        if (surface)
            cairo_surface_destroy(surface);
        return NULL;
    }

    cairo_surface_flush(surface);
    dest = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    if (!dest)
    {
        cairo_surface_destroy(surface);
        return NULL;
    }

    if (!convert_alpha(gdk_pixbuf_get_pixels(dest),
                       gdk_pixbuf_get_rowstride(dest),
                       cairo_image_surface_get_data(surface),
                       cairo_image_surface_get_stride(surface),
                       src_x, src_y, width, height))
    {
        g_object_unref(dest);
        dest = NULL;
    }

    cairo_surface_destroy(surface);
    return dest;
}

/* Reads initial battery data and stores it in the plugin context. */
static int init_battery(x729BattPlugin *plugin)
{
    plugin->batt = battery_get();
    if (plugin->batt)
        return 1;

    return 0;
}

/* Refreshes battery data and reports its device status. */
static int update_battery(x729BattPlugin *plugin, device_status_t *status)
{
    *status = STATUS_UNKNOWN;
    battery *b = plugin->batt;
    if (b)
    {
        if (!battery_update(b))
            return 0;

        if ((b->percentage <= LOW_BATTERY_PERCENT) &&
            ((power_source_t)b->power_source == POW_BATTERY))
            lxpanel_notify(plugin->panel, "WARNING: Low battery detected!\nPlease verify your power supply.");

        *status = (device_status_t)b->device_status;
        return 1;
    }

    return 0;
}

/* Draws the battery icon using the capacity-dependent color and fill level. */
static void draw_icon(x729BattPlugin *plugin, int level)
{
    float r, g, b;
    int h, w, f, ic;

    // calculate dimensions based on icon size
    ic = panel_get_icon_size(plugin->panel);
    if (ic <= 10)
        return;

    w = ic < ICON_BASE_WIDTH ? ICON_BASE_WIDTH : ic;
    gint64 height = ((gint64)w * ICON_HEIGHT_RATIO / ICON_BASE_WIDTH) *
                    ICON_HEIGHT_EVEN_FACTOR;
    if (height < ICON_MIN_HEIGHT)
        height = ICON_MIN_HEIGHT;
    if (height >= ic)
        height = ic - 2;
    if (height <= 8 || height > G_MAXINT)
        return;
    h = (gint)height;

    // set color based on level
    if (level <= LOW_BATTERY_PERCENT)
    {
        // RGB: 242,0,14
        r = 0.95;
        g = 0.0;
        b = 0.05;
    }
    else if (level <= MEDIUM_BATTERY_PERCENT)
    {
        // RGB: 255,158,1
        r = 1;
        g = 0.62;
        b = 0.0;
    }
    else
    {
        // RGB: 8,158,0
        r = 0.03;
        g = 0.62;
        b = 0.0;
    }

    // create and clear the drawing surface
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
    {
        cairo_surface_destroy(surface);
        return;
    }

    cairo_t *cr = cairo_create(surface);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS)
    {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return;
    }

    cairo_set_source_rgba(cr, 0, 0, 0, 0);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_fill(cr);

    // draw base icon on surface
    cairo_set_source_rgb(cr, 0.15, 0.59, 0.75); // RGB: 37,150,190
    cairo_rectangle(cr, 4, 1, w - 10, 1);
    cairo_rectangle(cr, 3, 2, w - 8, 1);
    cairo_rectangle(cr, 3, h - 3, w - 8, 1);
    cairo_rectangle(cr, 4, h - 2, w - 10, 1);
    cairo_rectangle(cr, 2, 3, 2, h - 6);
    cairo_rectangle(cr, w - 6, 3, 2, h - 6);
    cairo_rectangle(cr, w - 4, (h >> 1) - 3, 2, 6);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, r, g, b, 0.5);
    cairo_rectangle(cr, 3, 1, 1, 1);
    cairo_rectangle(cr, 2, 2, 1, 1);
    cairo_rectangle(cr, 2, h - 3, 1, 1);
    cairo_rectangle(cr, 3, h - 2, 1, 1);
    cairo_rectangle(cr, w - 6, 1, 1, 1);
    cairo_rectangle(cr, w - 5, 2, 1, 1);
    cairo_rectangle(cr, w - 5, h - 3, 1, 1);
    cairo_rectangle(cr, w - 6, h - 2, 1, 1);
    cairo_fill(cr);

    // fill the battery
    if (level < 0)
        f = 0;
    else if (level > ICON_FILL_FULL_PERCENT)
        f = w - ICON_FILL_WIDTH_INSET;
    else
    {
        f = (gint)(((gint64)(w - ICON_FILL_WIDTH_INSET) * level) /
                   ICON_FILL_FULL_PERCENT);
        if (f > w - ICON_FILL_WIDTH_INSET)
            f = w - ICON_FILL_WIDTH_INSET;
    }
    cairo_set_source_rgb(cr, r, g, b);
    cairo_rectangle(cr, 5, 4, f, h - 8);
    cairo_fill(cr);

    // create a pixbuf from the cairo surface
    GdkPixbuf *pixbuf = pixbuf_from_cairo_surface(surface, 0, 0, w, h);
    if (!pixbuf)
    {
        cairo_destroy(cr);
        return;
    }

    // copy the pixbuf to the icon resource
    g_object_ref_sink(plugin->tray_icon);
    gtk_image_set_from_pixbuf(GTK_IMAGE(plugin->tray_icon), pixbuf);

    g_object_unref(pixbuf);
    cairo_destroy(cr);
}

/* Refreshes the icon and tooltip from the latest battery readings. */
static void update_icon(x729BattPlugin *plugin)
{
    float voltage;
    int capacity;
    power_source_t power_source;
    device_status_t status;
    char str[TOOLTIP_BUFFER_SIZE];

    if (!plugin->timer)
        return;

    // Update battery capacity and voltage levels
    if (!update_battery(plugin, &status) || (status == STATUS_UNKNOWN))
        return;

    battery *b = plugin->batt;
    capacity = b->percentage;
    voltage = b->voltage;
    power_source = (power_source_t)b->power_source;

    // fill the battery symbol and create the tooltip
    if (power_source == POW_BATTERY)
        snprintf(str, sizeof(str), "Capacity : %01d%%\nVoltage : %02.02fV\nPower source : Battery", capacity, voltage);
    else if (power_source == POW_ADAPTER)
        snprintf(str, sizeof(str), "Capacity : %01d%%\nVoltage : %02.02fV\nPower source : AC adapter", capacity, voltage);
    else
        snprintf(str, sizeof(str), "Capacity : %01d%%\nVoltage : %02.02fV\nPower source : Unknown", capacity, voltage);
    draw_icon(plugin, capacity);

    // set the tooltip
    gtk_widget_set_tooltip_text(plugin->tray_icon, str);
}

/* Timer callback that refreshes the battery icon. */
static gboolean timer_event(x729BattPlugin *plugin)
{
    update_icon(plugin);
    return TRUE;
}

/* Timer callback that warns when battery voltage falls below its threshold. */
static gboolean vtimer_event(x729BattPlugin *plugin)
{
    float voltage = get_voltage();
    if (voltage < 0.0f)
        return TRUE;

    device_status_t status = (device_status_t)get_device_status();
    power_source_t power_source = (power_source_t)get_power_source();

    // Threshold for battery voltage
    if ((status == STATUS_PRESENT) && (voltage < LOW_VOLTAGE_WARNING_VOLTS) &&
        (power_source == POW_BATTERY))
    {
        lxpanel_notify(plugin->panel, "WARNING: Low voltage detected!\nPlease verify your power supply.");
        return TRUE;
    }

    return FALSE;
}

/* Removes plugin timers and releases its context. */
static void x729batt_destructor(gpointer user_data)
{
    x729BattPlugin *plugin = (x729BattPlugin *)user_data;

    /* Disconnect battery timer. */
    if (plugin->timer)
        g_source_remove(plugin->timer);

    /* Disconnect voltage timer. */
    if (plugin->vtimer)
        g_source_remove(plugin->vtimer);

    /* Deallocate memory */
    g_free(plugin);
}

/* Creates the plugin widget, initializes readings, and starts monitoring. */
static GtkWidget *x729batt_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* Allocate and initialize plugin context */
    x729BattPlugin *plugin = g_new0(x729BattPlugin, 1);

    /* Allocate top level widget and set into plugin widget pointer. */
    plugin->panel = panel;
    plugin->settings = settings;
    plugin->plugin = gtk_event_box_new();
    lxpanel_plugin_set_data(plugin->plugin, plugin, x729batt_destructor);

    /* Allocate icon as a child of top level */
    plugin->tray_icon = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(plugin->plugin), plugin->tray_icon);

    if (init_battery(plugin))
    {
        /* Start timed events to monitor status */
        plugin->timer = g_timeout_add(CMON_INTERVAL, (GSourceFunc)timer_event, (gpointer)plugin);

        /* Start timed events to monitor low voltage warnings */
        plugin->vtimer = g_timeout_add(VMON_INTERVAL, (GSourceFunc)vtimer_event, (gpointer)plugin);

        /* Display battery status */
        update_icon(plugin);
    }
    else
    {
        plugin->timer = 0;
        plugin->vtimer = 0;
    }

    /* Show the widget and return */
    gtk_widget_show_all(plugin->plugin);
    return plugin->plugin;
}

FM_DEFINE_MODULE(lxpanel_gtk, x729batt)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = "x729 Battery & Voltage",
    .description = "Monitors x729 voltage and battery capacity.",

    // assigning our functions to provided pointers.
    .new_instance = x729batt_constructor};
