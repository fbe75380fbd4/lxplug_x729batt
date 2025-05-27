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

#include "batt_i2c.h"

#include "lxpanel/plugin.h"

#define CMON_INTERVAL 60000
#define VMON_INTERVAL 60000

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
} devicestatus_t;

/* Prototypes */
static void convert_alpha(guchar *dest_data, int dest_stride, guchar *src_data, int src_stride, int src_x, int src_y, int width, int height);
GdkPixbuf *gdk_pixbuf_get_from_surface(cairo_surface_t *surface, gint src_x, gint src_y, gint width, gint height);
static int init_battery(x729BattPlugin *pt);
static int update_battery(x729BattPlugin *pt, devicestatus_t *status);
static void draw_icon(x729BattPlugin *pt, int level);
static void update_icon(x729BattPlugin *pt);
static gboolean timer_event(x729BattPlugin *pt);
static gboolean vtimer_event(x729BattPlugin *pt);

/* gdk_pixbuf_get_from_surface function from GDK+3 */
static void convert_alpha(guchar *dest_data, int dest_stride, guchar *src_data, int src_stride, int src_x, int src_y, int width, int height)
{
    int x, y;

    src_data += src_stride * src_y + src_x * 4;

    for (y = 0; y < height; y++)
    {
        guint32 *src = (guint32 *)src_data;

        for (x = 0; x < width; x++)
        {
            guint alpha = src[x] >> 24;

            if (alpha == 0)
            {
                dest_data[x * 4 + 0] = 0;
                dest_data[x * 4 + 1] = 0;
                dest_data[x * 4 + 2] = 0;
            }
            else
            {
                dest_data[x * 4 + 0] = (((src[x] & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
                dest_data[x * 4 + 1] = (((src[x] & 0x00ff00) >> 8) * 255 + alpha / 2) / alpha;
                dest_data[x * 4 + 2] = (((src[x] & 0x0000ff) >> 0) * 255 + alpha / 2) / alpha;
            }
            dest_data[x * 4 + 3] = alpha;
        }

        src_data += src_stride;
        dest_data += dest_stride;
    }
}

/* TBD */
GdkPixbuf *gdk_pixbuf_get_from_surface(cairo_surface_t *surface, gint src_x, gint src_y, gint width, gint height)
{
    GdkPixbuf *dest = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);

    convert_alpha(gdk_pixbuf_get_pixels(dest),
                  gdk_pixbuf_get_rowstride(dest),
                  cairo_image_surface_get_data(surface),
                  cairo_image_surface_get_stride(surface),
                  0, 0,
                  width, height);

    cairo_surface_destroy(surface);
    return dest;
}

/* Initial check for battery */
static int init_battery(x729BattPlugin *pt)
{
    pt->batt = battery_get();
    if (pt->batt)
        return 1;

    return 0;
}

/* Update battery and voltage levels */
static int update_battery(x729BattPlugin *pt, devicestatus_t *status)
{
    *status = STATUS_UNKNOWN;
    battery *b = pt->batt;
    if (b)
    {
        battery_update(b);

        if (b->percentage <= 20)
            lxpanel_notify(pt->panel, "WARNING: Low battery detected!\nPlease verify your power supply.");

        *status = (devicestatus_t)b->devicestatus;
        return 1;
    }

    return 0;
}

/* Draw the icon in relevant colour and fill level */
static void draw_icon(x729BattPlugin *pt, int level)
{
    float r, g, b;
    int h, w, f, ic;

    // calculate dimensions based on icon size
    ic = panel_get_icon_size(pt->panel);
    w = ic < 36 ? 36 : ic;
    h = ((w * 10) / 36) * 2; // force it to be even
    if (h < 18)
        h = 18;
    if (h >= ic)
        h = ic - 2;

    // set color based on level
    if (level <= 20)
    {
        // RGB: 242,0,14
        r = 0.95;
        g = 0.0;
        b = 0.05;
    }
    else if (level <= 50)
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
    cairo_t *cr = cairo_create(surface);
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
    else if (level > 97)
        f = w - 12;
    else
    {
        f = (w - 12) * level;
        f /= 97;
        if (f > w - 12)
            f = w - 12;
    }
    cairo_set_source_rgb(cr, r, g, b);
    cairo_rectangle(cr, 5, 4, f, h - 8);
    cairo_fill(cr);

    // create a pixbuf from the cairo surface
    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, w, h);

    // copy the pixbuf to the icon resource
    g_object_ref_sink(pt->tray_icon);
    gtk_image_set_from_pixbuf(GTK_IMAGE(pt->tray_icon), pixbuf);

    g_object_unref(pixbuf);
    cairo_destroy(cr);
}

/* Read the current battery status and update the icon accordingly */
static void update_icon(x729BattPlugin *pt)
{
    float voltage;
    int capacity;
    devicestatus_t status;
    char str[255];

    if (!pt->timer)
        return;

    // Update battery capacity and voltage levels
    if (!update_battery(pt, &status) | (status == STATUS_UNKNOWN))
        return;

    battery *b = pt->batt;
    capacity = b->percentage;
    voltage = b->voltage;

    // fill the battery symbol and create the tooltip
    sprintf(str, "Capacity : %01d%%\nVoltage : %02.02fV", capacity, voltage);
    draw_icon(pt, capacity);

    // set the tooltip
    gtk_widget_set_tooltip_text(pt->tray_icon, str);
}

/* Updates battery icon after timeout. */
static gboolean timer_event(x729BattPlugin *pt)
{
    update_icon(pt);
    return TRUE;
}

/* Warns if low voltage from power supply is detected after timeout. */
static gboolean vtimer_event(x729BattPlugin *pt)
{
    float voltage = get_voltage();
    devicestatus_t status = (devicestatus_t)get_devicestatus();

    // Threshold for battery voltage
    if ((status == STATUS_PRESENT) & (voltage < 3.33))
    {
        lxpanel_notify(pt->panel, "WARNING: Low voltage detected!\nPlease verify your power supply.");
        return TRUE;
    }

    return FALSE;
}

/* Plugin destructor. */
static void x729batt_destructor(gpointer user_data)
{
    x729BattPlugin *pt = (x729BattPlugin *)user_data;

    /* Disconnect battery timer. */
    if (pt->timer)
        g_source_remove(pt->timer);

    /* Disconnect voltage timer. */
    if (pt->vtimer)
        g_source_remove(pt->vtimer);

    /* Deallocate memory */
    g_free(pt);
}

/* Plugin constructor. */
static GtkWidget *x729batt_constructor(LXPanel *panel, config_setting_t *settings)
{
    /* Allocate and initialize plugin context */
    x729BattPlugin *pt = g_new0(x729BattPlugin, 1);

    /* Allocate top level widget and set into plugin widget pointer. */
    pt->panel = panel;
    pt->settings = settings;
    pt->plugin = gtk_event_box_new();
    lxpanel_plugin_set_data(pt->plugin, pt, x729batt_destructor);

    /* Allocate icon as a child of top level */
    pt->tray_icon = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(pt->plugin), pt->tray_icon);

    if (init_battery(pt))
    {
        /* Start timed events to monitor status */
        pt->timer = g_timeout_add(CMON_INTERVAL, (GSourceFunc)timer_event, (gpointer)pt);

        /* Start timed events to monitor low voltage warnings */
        pt->vtimer = g_timeout_add(VMON_INTERVAL, (GSourceFunc)vtimer_event, (gpointer)pt);

        /* Display battery status */
        update_icon(pt);
    }
    else
    {
        pt->timer = 0;
        pt->vtimer = 0;
    }

    /* Show the widget and return */
    gtk_widget_show_all(pt->plugin);
    return pt->plugin;
}

/* TBD. */
FM_DEFINE_MODULE(lxpanel_gtk, x729batt)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = "x729 Battery & Voltage",
    .description = "Monitors x729 voltage and battery capacity.",

    // assigning our functions to provided pointers.
    .new_instance = x729batt_constructor};
