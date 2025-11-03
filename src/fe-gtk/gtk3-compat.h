#ifndef HEXCHAT_GTK3_COMPAT_H
#define HEXCHAT_GTK3_COMPAT_H

/* Try to include GTK headers with fallback */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

/* Include system headers first */
#include <glib.h>
#include <glib/gi18n.h>

/* Gettext macros */
#ifndef N_
#define N_(String) (String)
#endif

#ifndef _
#define _(String) gettext(String)
#endif

/* GTK includes */
#ifdef _WIN32
#  ifdef _MSC_VER
#    pragma warning(push, 0)
#    include <windows.h>
#    pragma warning(pop)
#  endif
#  include <gtk/gtk.h>
#  include <gdk/gdkwin32.h>
#  include <gdk/gdk.h>
#  include <glib-object.h>
#  include <pango/pango.h>
#else
#  include <gtk/gtk.h>
#  include <gdk/gdkx.h>
#  include <gdk/gdk.h>
#  include <glib-object.h>
#  include <pango/pango.h>
#endif

/* GTK3 compatibility layer */
#if GTK_CHECK_VERSION(3,0,0) || !defined(GTK_MAJOR_VERSION) || (GTK_MAJOR_VERSION >= 3)

/* Basic widget macros */
#define gtk_hbox_new(X,Y) gtk_box_new(GTK_ORIENTATION_HORIZONTAL, Y)
#define gtk_vbox_new(X,Y) gtk_box_new(GTK_ORIENTATION_VERTICAL, Y)
#define gtk_hpaned_new() gtk_paned_new(GTK_ORIENTATION_HORIZONTAL)
#define gtk_vpaned_new() gtk_paned_new(GTK_ORIENTATION_VERTICAL)
#define gtk_hseparator_new() gtk_separator_new(GTK_ORIENTATION_HORIZONTAL)
#define gtk_vseparator_new() gtk_separator_new(GTK_ORIENTATION_VERTICAL)
#define gtk_hbutton_box_new() gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL)
#define gtk_vscrollbar_new(adj) gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, adj)
#define gtk_table_set_col_spacing(table, col, spacing) gtk_grid_set_column_spacing(GTK_GRID(table), spacing)

/* GTK2 compatibility flags */
#define GTK_SHRINK 0
#define GTK_FILL 4
#define GTK_EXPAND 1

typedef GtkAttachOptions GtkAttachOption;

/* Style and theming */
static GtkCssProvider *css_provider = NULL;

/**
 * setup_apply_css:
 * @widget: A #GtkWidget
 * @provider: A #GtkCssProvider
 *
 * Applies CSS styling to a widget and all its children.
 */
static void setup_apply_css(GtkWidget *widget, GtkStyleProvider *provider) {
    GtkWidget *child;
    
    if (!GTK_IS_WIDGET(widget) || gtk_widget_get_realized(widget))
        return;
    
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gtk_style_context_add_provider(context, provider, G_MAXUINT);
    
    if (GTK_IS_CONTAINER(widget)) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList *l = children; l != NULL; l = l->next) {
            setup_apply_css(GTK_WIDGET(l->data), provider);
        }
        g_list_free(children);
    }
}

/**
 * gtk_rc_parse_string:
 * @rc_string: CSS string to parse
 *
 * Parses a string containing CSS style information for theming.
 */
static void gtk_rc_parse_string(const gchar *rc_string) {
    if (!css_provider) {
        css_provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
    }
    
    g_autoptr(GError) error = NULL;
    if (!gtk_css_provider_load_from_data(css_provider, rc_string, -1, &error)) {
        g_warning("Failed to load CSS: %s", error ? error->message : "Unknown error");
    }
}

/**
 * gtk_widget_modify_base:
 * @widget: A #GtkWidget
 * @state: A #GtkStateType
 * @color: A #GdkColor or %NULL
 *
 * Sets the base color for a widget in a particular state.
 */
static void gtk_widget_modify_base(GtkWidget *widget, GtkStateType state, const GdkColor *color) {
    g_return_if_fail(GTK_IS_WIDGET(widget));
    
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gchar *class_name = g_strdup_printf("gtk2-compat-%p", widget);
    
    gtk_style_context_add_class(context, class_name);
    
    GString *css = g_string_new("");
    g_string_append_printf(css, ".%s {", class_name);
    
    if (color) {
        gdouble red = (gdouble)color->red / 65535.0;
        gdouble green = (gdouble)color->green / 65535.0;
        gdouble blue = (gdouble)color->blue / 65535.0;
        
        g_string_append_printf(css, 
            "background-color: %f %f %f; ", 
            red, green, blue);
    }
    
    g_string_append(css, "}");
    
    gtk_rc_parse_string(css->str);
    
    g_string_free(css, TRUE);
    g_free(class_name);
}

/**
 * gtk_widget_modify_text:
 * @widget: A #GtkWidget
 * @state: A #GtkStateType
 * @color: A #GdkColor or %NULL
 *
 * Sets the text color for a widget in a particular state.
 */
static void gtk_widget_modify_text(GtkWidget *widget, GtkStateFlags state, const GdkColor *color) {
    g_return_if_fail(GTK_IS_WIDGET(widget));
    
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gchar *class_name = g_strdup_printf("gtk2-text-compat-%p", widget);
    
    gtk_style_context_add_class(context, class_name);
    
    GString *css = g_string_new("");
    g_string_append_printf(css, ".%s {", class_name);
    
    if (color) {
        gdouble red = (gdouble)color->red / 65535.0;
        gdouble green = (gdouble)color->green / 65535.0;
        gdouble blue = (gdouble)color->blue / 65535.0;
        
        g_string_append_printf(css, 
            "color: %f %f %f; ", 
            red, green, blue);
    }
    
    g_string_append(css, "}");
    
    gtk_rc_parse_string(css->str);
    
    g_string_free(css, TRUE);
    g_free(class_name);
}

/**
 * gtk_widget_modify_font:
 * @widget: A #GtkWidget
 * @font_desc: A #PangoFontDescription or %NULL
 *
 * Sets the font for a widget.
 */
static void gtk_widget_modify_font(GtkWidget *widget, PangoFontDescription *font_desc) {
    g_return_if_fail(GTK_IS_WIDGET(widget));
    
    if (!font_desc)
        return;
    
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gchar *class_name = g_strdup_printf("gtk2-font-compat-%p", widget);
    
    gtk_style_context_add_class(context, class_name);
    
    gchar *font_string = pango_font_description_to_string(font_desc);
    
    GString *css = g_string_new("");
    g_string_append_printf(css, 
        ".%s { font: %s; }", 
        class_name, 
        font_string ? font_string : "");
    
    gtk_rc_parse_string(css->str);
    
    g_string_free(css, TRUE);
    g_free(font_string);
    g_free(class_name);
}

/* Stock items to icon names */
#define GTK_STOCK_OK "gtk-ok"
#define GTK_STOCK_CANCEL "gtk-cancel"
#define GTK_STOCK_OPEN "document-open"
#define GTK_STOCK_SAVE "document-save"
#define GTK_STOCK_CLOSE "window-close"
#define GTK_STOCK_DELETE "edit-delete"
#define GTK_STOCK_CLEAR "edit-clear"
#define GTK_STOCK_ADD "list-add"
#define GTK_STOCK_REMOVE "list-remove"
#define GTK_STOCK_EDIT "gtk-edit"
#define GTK_STOCK_REFRESH "view-refresh"
#define GTK_STOCK_ABOUT "help-about"
#define GTK_STOCK_PREFERENCES "preferences-system"
#define GTK_STOCK_QUIT "application-exit"
#define GTK_STOCK_YES "gtk-yes"
#define GTK_STOCK_NO "gtk-no"
#define GTK_STOCK_APPLY "gtk-apply"
#define GTK_STOCK_HELP "help-contents"
#define GTK_STOCK_FIND "edit-find"
#define GTK_STOCK_FIND_AND_REPLACE "edit-find-replace"
#define GTK_STOCK_JUMP_TO "go-jump"
#define GTK_STOCK_GO_UP "go-up"
#define GTK_STOCK_GO_DOWN "go-down"
#define GTK_STOCK_HOME "go-home"
#define GTK_STOCK_REVERT_TO_SAVED "document-revert"

#endif /* GTK_CHECK_VERSION(3,0,0) */

#endif /* HEXCHAT_GTK3_COMPAT_H */
