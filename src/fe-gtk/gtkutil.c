/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
#define _FILE_OFFSET_BITS 64 /* allow selection of large files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fe-gtk.h"

#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#if defined (WIN32) || defined (__APPLE__)
#include <pango/pangocairo.h>
#endif

/* GTK3 compatibility */
#if GTK_CHECK_VERSION(3,0,0)
#define gtk_hbox_new(X,Y) gtk_box_new(GTK_ORIENTATION_HORIZONTAL, Y)
#define gtk_vbox_new(X,Y) gtk_box_new(GTK_ORIENTATION_VERTICAL, Y)
#define gtk_hpaned_new() gtk_paned_new(GTK_ORIENTATION_HORIZONTAL)
#define gtk_vpaned_new() gtk_paned_new(GTK_ORIENTATION_VERTICAL)
#define gtk_hseparator_new() gtk_separator_new(GTK_ORIENTATION_HORIZONTAL)
#define gtk_vseparator_new() gtk_separator_new(GTK_ORIENTATION_VERTICAL)
#define gdk_cursor_new(X) gdk_cursor_new_for_display(gdk_display_get_default(), X)
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "../common/hexchat.h"
#include "../common/fe.h"
#include "../common/util.h"
#include "../common/cfgfiles.h"
#include "../common/hexchatc.h"
#include "../common/typedef.h"
#include "gtkutil.h"
#include "pixmaps.h"

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/* gtkutil.c, just some gtk wrappers */

extern void path_part (char *file, char *path, int pathlen);

struct file_req
{
	GtkWidget *dialog;
	void *userdata;
	filereqcallback callback;
	int flags;		/* FRF_* flags */
};

static void
gtkutil_file_req_destroy (GtkWidget * wid, struct file_req *freq)
{
	freq->callback (freq->userdata, NULL);
	g_free (freq);
}

static void
gtkutil_check_file (char *filename, struct file_req *freq)
{
	int axs = FALSE;

	GFile *file = g_file_new_for_path (filename);

	if (freq->flags & FRF_WRITE)
	{
		GFile *parent = g_file_get_parent (file);

		GFileInfo *fi = g_file_query_info (parent, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
		if (fi != NULL)
		{
			if (g_file_info_get_attribute_boolean (fi, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
			{
				axs = TRUE;
			}

			g_object_unref (fi);
		}

		g_object_unref (parent);
	}
	else
	{
		GFileInfo *fi = g_file_query_info (file, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, G_FILE_QUERY_INFO_NONE, NULL, NULL);

		if (fi != NULL)
		{
			if (g_file_info_get_file_type (fi) != G_FILE_TYPE_DIRECTORY || (freq->flags & FRF_CHOOSEFOLDER))
			{
				axs = TRUE;
			}

			g_object_unref (fi);
		}
	}

	g_object_unref (file);

	if (axs)
	{
		char *filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
		if (filename_utf8 != NULL)
		{
			freq->callback (freq->userdata, filename_utf8);
			g_free (filename_utf8);
		}
		else
		{
			fe_message ("Filename encoding is corrupt.", FE_MSG_ERROR);
		}
	}
	else
	{
		if (freq->flags & FRF_WRITE)
		{
			fe_message (_("Cannot write to that file."), FE_MSG_ERROR);
		}
		else
		{
			fe_message (_("Cannot read that file."), FE_MSG_ERROR);
		}
	}
}

static void
gtkutil_file_req_done (GtkWidget * wid, struct file_req *freq)
{
	GSList *files, *cur;
	GtkFileChooser *fs = GTK_FILE_CHOOSER (freq->dialog);

	if (freq->flags & FRF_MULTIPLE)
	{
		files = cur = gtk_file_chooser_get_filenames (fs);
		while (cur)
		{
			gtkutil_check_file (cur->data, freq);
			g_free (cur->data);
			cur = cur->next;
		}
		if (files)
			g_slist_free (files);
	}
	else
	{
		if (freq->flags & FRF_CHOOSEFOLDER)
		{
			gchar *filename = gtk_file_chooser_get_current_folder (fs);
			gtkutil_check_file (filename, freq);
			g_free (filename);
		}
		else
		{
			gchar *filename = gtk_file_chooser_get_filename (fs);
			gtkutil_check_file (gtk_file_chooser_get_filename (fs), freq);
			g_free (filename);
		}
	}

	/* this should call the "destroy" cb, where we free(freq) */
	gtk_widget_destroy (freq->dialog);
}

static void
gtkutil_file_req_response (GtkWidget *dialog, gint res, struct file_req *freq)
{
	switch (res)
	{
	case GTK_RESPONSE_ACCEPT:
		gtkutil_file_req_done (dialog, freq);
		break;

	case GTK_RESPONSE_CANCEL:
		/* this should call the "destroy" cb, where we free(freq) */
		gtk_widget_destroy (freq->dialog);
	}
}

void
gtkutil_file_req (GtkWindow *parent, const char *title, void *callback, void *userdata, char *filter, char *extensions,
						int flags)
{
	struct file_req *freq;
	GtkWidget *dialog;
	GtkFileFilter *filefilter;
	extern char *get_xdir_fs (void);
	char *token;
	char *tokenbuffer;

	if (flags & FRF_WRITE)
	{
		dialog = gtk_file_chooser_dialog_new (title, NULL,
												GTK_FILE_CHOOSER_ACTION_SAVE,
												"_Cancel", GTK_RESPONSE_CANCEL,
												"_Save", GTK_RESPONSE_ACCEPT,
												NULL);

		if (!(flags & FRF_NOASKOVERWRITE))
			gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
	}
	else
		dialog = gtk_file_chooser_dialog_new (title, NULL,
												GTK_FILE_CHOOSER_ACTION_OPEN,
												"_Cancel", GTK_RESPONSE_CANCEL,
												"_Open", GTK_RESPONSE_ACCEPT,
												NULL);

	if (filter && filter[0] && (flags & FRF_FILTERISINITIAL))
	{
		if (flags & FRF_WRITE)
		{
			char temp[1024];
			path_part (filter, temp, sizeof (temp));
			gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), temp);
			gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), file_part (filter));
		}
		else
			gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), filter);
	}
	else if (!(flags & FRF_RECENTLYUSED))
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), get_xdir ());

	if (flags & FRF_MULTIPLE)
		gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE);
	if (flags & FRF_CHOOSEFOLDER)
		gtk_file_chooser_set_action (GTK_FILE_CHOOSER (dialog), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

	if ((flags & FRF_EXTENSIONS || flags & FRF_MIMETYPES) && extensions != NULL)
	{
		filefilter = gtk_file_filter_new ();
		tokenbuffer = g_strdup (extensions);
		token = strtok (tokenbuffer, ";");

		while (token != NULL)
		{
			if (flags & FRF_EXTENSIONS)
				gtk_file_filter_add_pattern (filefilter, token);
			else
				gtk_file_filter_add_mime_type (filefilter, token);
			token = strtok (NULL, ";");
		}

		g_free (tokenbuffer);
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filefilter);
	}

	gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (dialog), get_xdir (), NULL);

	freq = g_new (struct file_req, 1);
	freq->dialog = dialog;
	freq->flags = flags;
	freq->callback = callback;
	freq->userdata = userdata;

	g_signal_connect (G_OBJECT (dialog), "response",
							G_CALLBACK (gtkutil_file_req_response), freq);
	g_signal_connect (G_OBJECT (dialog), "destroy",
						   G_CALLBACK (gtkutil_file_req_destroy), (gpointer) freq);

	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

	if (flags & FRF_MODAL)
	{
		g_assert (parent);
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	}

	gtk_widget_show (dialog);
}

static gboolean
gtkutil_esc_destroy (GtkWidget * win, GdkEventKey * key, gpointer userdata)
{
	GtkWidget *wid;

	/* Destroy the window of detached utils */
	if (!gtk_widget_is_toplevel (win))
	{
		if (gdk_window_get_type_hint (gtk_widget_get_window (win)) == GDK_WINDOW_TYPE_HINT_DIALOG)
			wid = gtk_widget_get_parent (win);
		else
			return FALSE;
	}
	else
		wid = win;

	if (key->keyval == GDK_KEY_Escape)
		gtk_widget_destroy (wid);
			
	return FALSE;
}

void
gtkutil_destroy_on_esc (GtkWidget *win)
{
	g_signal_connect (G_OBJECT (win), "key_press_event", G_CALLBACK (gtkutil_esc_destroy), win);
}

void
gtkutil_destroy (GtkWidget * igad, GtkWidget * dgad)
{
	gtk_widget_destroy (dgad);
}

static void
gtkutil_get_str_response (GtkDialog *dialog, gint arg1, gpointer entry)
{
	void (*callback) (int cancel, char *text, void *user_data);
	char *text;
	void *user_data;

	text = (char *) gtk_entry_get_text (GTK_ENTRY (entry));
	callback = g_object_get_data (G_OBJECT (dialog), "cb");
	user_data = g_object_get_data (G_OBJECT (dialog), "ud");

	switch (arg1)
	{
	case GTK_RESPONSE_REJECT:
		callback (TRUE, text, user_data);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_ACCEPT:
		callback (FALSE, text, user_data);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	}
}

static void
gtkutil_str_enter (GtkWidget *entry, GtkWidget *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
}

void
fe_get_str (char *msg, char *def, void *callback, void *userdata)
{
	GtkWidget *dialog;
	GtkWidget *entry;
	GtkWidget *hbox;
	GtkWidget *label;
	extern GtkWidget *parent_window;

	dialog = gtk_dialog_new_with_buttons (msg, NULL, 0,
										"_Cancel", GTK_RESPONSE_REJECT,
										"_OK", GTK_RESPONSE_ACCEPT,
										NULL);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent_window));
	gtk_box_set_homogeneous (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), TRUE);

	if (userdata == (void *)1)	/* nick box is usually on the very bottom, make it centered */
	{
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	}
	else
	{
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
	}

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);

	g_object_set_data (G_OBJECT (dialog), "cb", callback);
	g_object_set_data (G_OBJECT (dialog), "ud", userdata);

	entry = gtk_entry_new ();
	g_signal_connect (G_OBJECT (entry), "activate",
						 	G_CALLBACK (gtkutil_str_enter), dialog);
	gtk_entry_set_text (GTK_ENTRY (entry), def);
	gtk_box_pack_end (GTK_BOX (hbox), entry, 0, 0, 0);

	label = gtk_label_new (msg);
	gtk_box_pack_end (GTK_BOX (hbox), label, 0, 0, 0);

	g_signal_connect (G_OBJECT (dialog), "response",
						   G_CALLBACK (gtkutil_get_str_response), entry);

	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox);

	gtk_widget_show_all (dialog);
}

static void
gtkutil_get_number_response (GtkDialog *dialog, gint arg1, gpointer spin)
{
	void (*callback) (int cancel, int value, void *user_data);
	int num;
	void *user_data;

	num = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
	callback = g_object_get_data (G_OBJECT (dialog), "cb");
	user_data = g_object_get_data (G_OBJECT (dialog), "ud");

	switch (arg1)
	{
	case GTK_RESPONSE_REJECT:
		callback (TRUE, num, user_data);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_ACCEPT:
		callback (FALSE, num, user_data);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	}
}

static void
gtkutil_get_bool_response (GtkDialog *dialog, gint arg1, gpointer spin)
{
	void (*callback) (int value, void *user_data);
	void *user_data;

	callback = g_object_get_data (G_OBJECT (dialog), "cb");
	user_data = g_object_get_data (G_OBJECT (dialog), "ud");

	switch (arg1)
	{
	case GTK_RESPONSE_REJECT:
		callback (0, user_data);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_ACCEPT:
		callback (1, user_data);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	}
}

void
fe_get_int (char *msg, int def, void *callback, void *userdata)
{
	GtkWidget *dialog;
	GtkWidget *spin;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkAdjustment *adj;
	extern GtkWidget *parent_window;

	dialog = gtk_dialog_new_with_buttons (msg, NULL, 0,
										"_Cancel", GTK_RESPONSE_REJECT,
										"_OK", GTK_RESPONSE_ACCEPT,
										NULL);
	gtk_box_set_homogeneous (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent_window));

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);

	g_object_set_data (G_OBJECT (dialog), "cb", callback);
	g_object_set_data (G_OBJECT (dialog), "ud", userdata);

	spin = gtk_spin_button_new (NULL, 1, 0);
	adj = gtk_spin_button_get_adjustment ((GtkSpinButton*)spin);
	gtk_adjustment_set_lower (adj, 0);
	gtk_adjustment_set_upper (adj, 1024);
	gtk_adjustment_set_step_increment (adj, 1);
	gtk_adjustment_changed (adj);
	gtk_spin_button_set_value ((GtkSpinButton*)spin, def);
	gtk_box_pack_end (GTK_BOX (hbox), spin, 0, 0, 0);

	label = gtk_label_new (msg);
	gtk_box_pack_end (GTK_BOX (hbox), label, 0, 0, 0);

	g_signal_connect (G_OBJECT (dialog), "response",
						   G_CALLBACK (gtkutil_get_number_response), spin);

	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox);

	gtk_widget_show_all (dialog);
}

void
fe_get_bool (char *title, char *prompt, void *callback, void *userdata)
{
	GtkWidget *dialog;
	GtkWidget *prompt_label;
	extern GtkWidget *parent_window;

	dialog = gtk_dialog_new_with_buttons (title, NULL, 0,
		"_No", GTK_RESPONSE_REJECT,
		"_Yes", GTK_RESPONSE_ACCEPT,
		NULL);
	gtk_box_set_homogeneous (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent_window));


	g_object_set_data (G_OBJECT (dialog), "cb", callback);
	g_object_set_data (G_OBJECT (dialog), "ud", userdata);

	prompt_label = gtk_label_new (prompt);

	g_signal_connect (G_OBJECT (dialog), "response",
		G_CALLBACK (gtkutil_get_bool_response), NULL);

	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), prompt_label);

	gtk_widget_show_all (dialog);
}

GtkWidget *
gtkutil_button (GtkWidget *box, const char *icon_name, const char *tip, void *callback,
					 void *userdata, const char *labeltext)
{
	GtkWidget *button;
	GtkWidget *img = NULL;

	if (icon_name && *icon_name)
	{
#if GTK_CHECK_VERSION(3,0,0)
		/* In GTK3, create image from icon name */
		img = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
#else
		/* In GTK2, try to use stock items if possible */
		if (g_str_has_prefix(icon_name, "gtk-"))
		{
			GtkStockItem item;
			if (gtk_stock_lookup(icon_name, &item))
				img = gtk_image_new_from_stock(icon_name, GTK_ICON_SIZE_BUTTON);
		}
		if (!img)
			img = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
#endif

		if (labeltext && *labeltext)
		{
			button = gtk_button_new_with_mnemonic(labeltext);
			gtk_button_set_image(GTK_BUTTON(button), img);
		}
		else
		{
			button = gtk_button_new();
			gtk_container_add(GTK_CONTAINER(button), img);
		}
	}
	else
	{
		button = gtk_button_new_with_mnemonic(labeltext ? labeltext : "");
	}

	if (tip && *tip)
		gtk_widget_set_tooltip_text(button, tip);

	if (callback)
	{
		g_signal_connect(button, "clicked", G_CALLBACK(callback), userdata);
	}

	if (box)
	{
#if GTK_CHECK_VERSION(3,0,0)
		gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
#else
		gtk_container_add(GTK_CONTAINER(box), button);
#endif
	}

	return button;
}

void
gtkutil_label_new (char *text, GtkWidget * box)
{
	GtkWidget *label = gtk_label_new (text);
	gtk_container_add (GTK_CONTAINER (box), label);
	gtk_widget_show (label);
}

GtkWidget *
gtkutil_entry_new (int max, GtkWidget *box, void *callback,
						 gpointer userdata)
{
	GtkWidget *entry;

	entry = gtk_entry_new ();

	if (max > 0)
		gtk_entry_set_max_length (GTK_ENTRY (entry), max);

	if (callback)
		g_signal_connect (G_OBJECT (entry), "activate",
				  G_CALLBACK (callback), userdata);

	if (box)
	{
#if GTK_CHECK_VERSION(3,0,0)
		gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 0);
#else
		gtk_container_add (GTK_CONTAINER (box), entry);
#endif
	}

	return entry;
}

void
show_and_unfocus (GtkWidget * wid)
{
	gtk_widget_set_can_focus (wid, FALSE);
	gtk_widget_show (wid);
}

void
gtkutil_set_icon (GtkWidget *win)
{
	GdkPixbuf *pixbuf = get_pixmap ("hexchat.png");
	if (pixbuf)
	{
		gtk_window_set_icon (GTK_WINDOW (win), pixbuf);
		g_object_unref (pixbuf);
	}
}

extern GtkWidget *parent_window;	/* maingui.c */

GtkWidget *
gtkutil_window_new (char *title, char *role, int width, int height, int flags)
{
	GtkWidget *win;

	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	if (title)
		gtk_window_set_title (GTK_WINDOW (win), title);

#if !GTK_CHECK_VERSION(3,0,0)
	if (role)
		gtk_window_set_role (GTK_WINDOW (win), role);
#endif

	if (width > 0 && height > 0)
	{
		gtk_window_set_default_size (GTK_WINDOW (win), width, height);
	}

	if (flags & 1)
		gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_MOUSE);

	gtk_container_set_border_width (GTK_CONTAINER (win), 6);

	return win;
}

/* pass NULL as selection to paste to both clipboard & X11 text */
void
gtkutil_copy_to_clipboard (GtkWidget *widget, GdkAtom selection,
                           const gchar *str)
{
	GtkClipboard *clipboard;

	if (str == NULL || *str == 0)
		return;

	if (selection == GDK_NONE)
	{
#ifdef GDK_WINDOWING_X11
		if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
		{
			gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
					  str, -1);
		}
	}
}

/* Treeview util functions */

GtkWidget *
gtkutil_treeview_new (GtkWidget *box, GtkTreeModel *model,
                      GtkTreeCellDataFunc mapper, ...)
{
	GtkWidget *win, *view;
	GtkCellRenderer *renderer = NULL;
	GtkTreeViewColumn *col;
	va_list args;
	int col_id = 0;
	GType type;
	char *title, *attr;

	win = gtk_scrolled_window_new (0, 0);
	gtk_container_add (GTK_CONTAINER (box), win);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (win),
											  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_show (win);

	view = gtk_tree_view_new_with_model (model);
	/* the view now has a ref on the model, we can unref it */
	g_object_unref (G_OBJECT (model));
	gtk_container_add (GTK_CONTAINER (win), view);

	va_start (args, mapper);
	for (col_id = va_arg (args, int); col_id != -1; col_id = va_arg (args, int))
	{
		type = gtk_tree_model_get_column_type (model, col_id);
		switch (type)
		{
			case G_TYPE_BOOLEAN:
				renderer = gtk_cell_renderer_toggle_new ();
				attr = "active";
				break;
			case G_TYPE_STRING:	/* fall through */
			default:
				renderer = gtk_cell_renderer_text_new ();
				attr = "text";
				break;
		}

		title = va_arg (args, char *);
		if (mapper)	/* user-specified function to set renderer attributes */
		{
			col = gtk_tree_view_column_new_with_attributes (title, renderer, NULL);
			gtk_tree_view_column_set_cell_data_func (col, renderer, mapper,
			                                         GINT_TO_POINTER (col_id), NULL);
		} else
		{
			/* just set the typical attribute for this type of renderer */
			col = gtk_tree_view_column_new_with_attributes (title, renderer,
			                                                attr, col_id, NULL);
		}
		gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);
		if (title == NULL)
			gtk_tree_view_column_set_visible (col, FALSE);
	}

	va_end (args);

	return view;
}

gboolean
gtkutil_treemodel_string_to_iter (GtkTreeModel *model, gchar *pathstr, GtkTreeIter *iter_ret)
{
	GtkTreePath *path = gtk_tree_path_new_from_string (pathstr);
	gboolean success;

	success = gtk_tree_model_get_iter (model, iter_ret, path);
	gtk_tree_path_free (path);
	return success;
}

/*gboolean
gtkutil_treeview_get_selected_iter (GtkTreeView *view, GtkTreeIter *iter_ret)
{
	GtkTreeModel *store;
	GtkTreeSelection *select;
	
	select = gtk_tree_view_get_selection (view);
	return gtk_tree_selection_get_selected (select, &store, iter_ret);
}*/

gboolean
gtkutil_treeview_get_selected (GtkTreeView *view, GtkTreeIter *iter_ret, ...)
{
	GtkTreeModel *store;
	GtkTreeSelection *select;
	gboolean has_selected;
	va_list args;
	
	select = gtk_tree_view_get_selection (view);
	has_selected = gtk_tree_selection_get_selected (select, &store, iter_ret);

	if (has_selected) {
		va_start (args, iter_ret);
		gtk_tree_model_get_valist (store, iter_ret, args);
		va_end (args);
	}

	return has_selected;
}

gboolean
gtkutil_tray_icon_supported (GtkWindow *window)
{
#ifndef GDK_WINDOWING_X11
	return TRUE;
#else
	GdkScreen *screen = gtk_window_get_screen (window);
	GdkDisplay *display = gdk_screen_get_display (screen);
	int screen_number = gdk_screen_get_number (screen);
	Display *xdisplay = gdk_x11_display_get_xdisplay (display);
	char *selection_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d", screen_number);
	Atom selection_atom = XInternAtom (xdisplay, selection_name, False);
	Window tray_window = None;

	XGrabServer (xdisplay);

	tray_window = XGetSelectionOwner (xdisplay, selection_atom);

	XUngrabServer (xdisplay);
	XFlush (xdisplay);
	g_free (selection_name);

	return (tray_window != None);
#endif
}

#if defined (WIN32) || defined (__APPLE__)
gboolean
gtkutil_find_font (const char *fontname)
{
	int i;
	int n_families;
	const char *family_name;
	PangoFontMap *fontmap;
	PangoFontFamily *family;
	PangoFontFamily **families;

	fontmap = pango_cairo_font_map_get_default ();
	pango_font_map_list_families (fontmap, &families, &n_families);

	for (i = 0; i < n_families; i++)
	{
		family = families[i];
		family_name = pango_font_family_get_name (family);

		if (!g_ascii_strcasecmp (family_name, fontname))
		{
			g_free (families);
			return TRUE;
		}
	}

	g_free (families);
	return FALSE;
}
#endif
