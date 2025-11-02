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

#define GDK_DISABLE_DEPRECATED 1
#define GTK_DISABLE_DEPRECATED 1

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* GTK3 compatibility */
#ifndef GTK_ALIGN_START
#define GTK_ALIGN_START 0
#define GTK_ALIGN_CENTER 1
#define GTK_ALIGN_END 2
#endif

#include "fe-gtk.h"
#include "../common/hexchat.h"
#include "../common/fe.h"
#include "gtkutil.h"

/* Forward declarations */
void notify_save(void);

#include "../common/userlist.h"
#include "../common/outbound.h"
#include "gtkutil.h"
#include "maingui.h"
/* palette.h included via fe-gtk.h */
#include "notifygui.h"


/* model for the notify treeview */
enum
{
	USER_COLUMN,
	STATUS_COLUMN,
	SERVER_COLUMN,
	SEEN_COLUMN,
	COLOUR_COLUMN,
	NPS_COLUMN, 	/* struct notify_per_server * */
	N_COLUMNS
};


static GtkWidget *notify_window = 0;
static GtkWidget *notify_button_opendialog;
static GtkWidget *notify_button_remove;
static GtkWidget *notify_button_add;

static GtkWidget *notify_treeview;

static void
notify_closegui (void)
{
	notify_window = 0;
	notify_save ();
}

/* Need this to be able to set the foreground colour property of a row
 * from a GdkColor * in the model  -Vince
 */
static void
notify_treecell_property_mapper (GtkTreeViewColumn *col, GtkCellRenderer *cell,
                                 GtkTreeModel *model, GtkTreeIter *iter,
                                 gpointer data)
{
	gchar *text;
	GdkRGBA *color;
	int model_column = GPOINTER_TO_INT (data);
	gchar *color_str = NULL;

	gtk_tree_model_get (GTK_TREE_MODEL (model), iter, 
	                    COLOUR_COLUMN, &color,
	                    model_column, &text, -1);

	if (color) {
		color_str = g_strdup_printf("rgba(%d,%d,%d,%.2f)",
		    (int)(color->red * 255),
		    (int)(color->green * 255),
		    (int)(color->blue * 255),
		    color->alpha);
	}

	g_object_set (G_OBJECT (cell), "text", text, NULL);
	if (color_str) {
		g_object_set (G_OBJECT (cell), "foreground-rgba", color_str, NULL);
		g_free (color_str);
	}
	g_free (text);
}

static void
notify_row_cb (GtkTreeSelection *sel, GtkTreeView *view)
{
	GtkTreeIter iter;
	struct notify_per_server *servnot;

	if (gtkutil_treeview_get_selected (view, &iter, NPS_COLUMN, &servnot, -1))
	{
		gtk_widget_set_sensitive (notify_button_opendialog, servnot ? servnot->ison : 0);
		gtk_widget_set_sensitive (notify_button_remove, TRUE);
		return;
	}

	gtk_widget_set_sensitive (notify_button_opendialog, FALSE);
	gtk_widget_set_sensitive (notify_button_remove, FALSE);
}

static GtkWidget *
notify_treeview_new (GtkWidget *box)
{
	GtkWidget *treeview;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkListStore *store;

	treeview = gtk_tree_view_new ();
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), TRUE);
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (treeview), TRUE);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Name"));
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer, "text", 0);
	gtk_tree_view_column_add_attribute (column, renderer, "foreground-rgba", 4);
	gtk_tree_view_column_set_sort_column_id (column, 0);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Status"), renderer,
												"text", 1,
												NULL);
	gtk_tree_view_column_set_sort_column_id (column, 1);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Network"), renderer,
												"text", 2,
												NULL);
	gtk_tree_view_column_set_sort_column_id (column, 2);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Last Seen"), renderer,
												"text", 3,
												NULL);
	gtk_tree_view_column_set_sort_column_id (column, 3);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

	store = gtk_list_store_new (5, 
		G_TYPE_STRING,  // Name
		G_TYPE_STRING,  // Status
		G_TYPE_STRING,  // Network
		G_TYPE_STRING,  // Last Seen
		GDK_TYPE_RGBA); // Color as GdkRGBA

	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store), 0,
								   (GtkTreeIterCompareFunc) g_utf8_collate, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
								   GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
								   GTK_SORT_ASCENDING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
	g_object_unref (store);

	g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview))),
					"changed", G_CALLBACK (notify_row_cb), treeview);

	gtk_widget_show (treeview);

	gtk_container_add (GTK_CONTAINER (box), treeview);

	return treeview;
}

void
notify_gui_update (void)
{
	struct notify *notify;
	struct notify_per_server *servnot;
	GSList *list = notify_list;
	GSList *slist;
	gchar *name, *status, *server, *seen;
	gchar agobuf[64];
	gboolean valid;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GdkRGBA colors[5];
	gint servcount;
	time_t lastseen, now = time (NULL);

	if (!notify_treeview)
		return;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (notify_treeview));
	valid = gtk_tree_model_get_iter_first (model, &iter);

	// Convert GdkColor to GdkRGBA
	gdk_rgba_parse (&colors[0], prefs.gui_list_color);
	gdk_rgba_parse (&colors[1], prefs.gui_list_bg);
	gdk_rgba_parse (&colors[2], prefs.gui_list_fg);
	gdk_rgba_parse (&colors[3], prefs.gui_list_bg_alert);
	gdk_rgba_parse (&colors[4], prefs.gui_list_fg_alert);

	while (list)
	{
		notify = list->data;

		if (!notify->server_list)
		{
			/* Offline - just one line */
			name = notify->name;
			server = notify->networks ? notify->networks : "";
			status = _("Offline");

			if (notify->lastseen)
			{
				gint lastseenminutes = (now - notify->lastseen) / 60;
				if (lastseenminutes < 2)
					g_snprintf (agobuf, sizeof (agobuf), _("Just now"));
				else if (lastseenminutes < 60)
					g_snprintf (agobuf, sizeof (agobuf), _("%d minutes ago"), lastseenminutes);
				else if (lastseenminutes < 120)
					g_snprintf (agobuf, sizeof (agobuf), _("An hour ago"));
				else
					g_snprintf (agobuf, sizeof (agobuf), _("%d hours ago"), lastseenminutes / 60);
				seen = agobuf;
			} else {
				seen = "";
			}

			if (!valid)
				gtk_list_store_append (GTK_LIST_STORE (model), &iter);

			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				0, name,
				1, status,
				2, server,
				3, seen,
				4, &colors[4],  // Alert color for offline users
				-1);

			if (valid)
				valid = gtk_tree_model_iter_next (model, &iter);
		}
		else
		{
			servcount = 0;
			slist = notify->server_list;

			while (slist)
			{
				servnot = slist->data;
				if (servcount > 0)
					name = "";
				else
					name = notify->name;

				server = server_get_network (servnot->server, TRUE);
				status = _("Online");

				if (servnot->lastseen)
				{
					gint lastseenminutes = (now - servnot->lastseen) / 60;
					if (lastseenminutes < 2)
						g_snprintf (agobuf, sizeof (agobuf), _("Just now"));
					else if (lastseenminutes < 60)
						g_snprintf (agobuf, sizeof (agobuf), _("%d minutes ago"), lastseenminutes);
					else if (lastseenminutes < 120)
						g_snprintf (agobuf, sizeof (agobuf), _("An hour ago"));
					else
						g_snprintf (agobuf, sizeof (agobuf), _("%d hours ago"), lastseenminutes / 60);
					seen = agobuf;
				} else {
					seen = "";
				}

				if (!valid)
					gtk_list_store_append (GTK_LIST_STORE (model), &iter);

				gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					0, name,
					1, status,
					2, server,
					3, seen,
					4, &colors[3],  // Normal color for online users
					-1);

				if (valid)
					valid = gtk_tree_model_iter_next (model, &iter);

				servcount++;
				slist = slist->next;
			}
		}

		list = list->next;
	}

	/* Remove any remaining rows */
	while (valid)
	{
		gboolean cont = gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		if (!cont)
			break;
	}
}

static void
notify_opendialog_clicked (GtkWidget * igad)
{
	GtkTreeView *view;
	GtkTreeIter iter;
	struct notify_per_server *servnot;

	view = notify_treeview;
	if (gtkutil_treeview_get_selected (view, &iter, NPS_COLUMN, &servnot, -1))
	{
		if (servnot)
			open_query (servnot->server, servnot->notify->name, TRUE);
	}
}

static void
notify_remove_clicked (GtkWidget * igad)
{
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	gboolean found = FALSE;
	char *name;

	view = notify_treeview;
	if (gtkutil_treeview_get_selected (view, &iter, USER_COLUMN, &name, -1))
	{
		model = gtk_tree_view_get_model (view);
		found = (*name != 0);
		while (!found)	/* the real nick is some previous node */
		{
			g_free (name); /* it's useless to us */
			if (!path)
				path = gtk_tree_model_get_path (model, &iter);
			if (!gtk_tree_path_prev (path))	/* arrgh! no previous node! */
			{
				g_warning ("notify list state is invalid\n");
				break;
			}
			if (!gtk_tree_model_get_iter (model, &iter, path))
				break;
			gtk_tree_model_get (model, &iter, USER_COLUMN, &name, -1);
			found = (*name != 0);
		}
		if (path)
			gtk_tree_path_free (path);
		if (!found)
			return;

		/* ok, now we can remove it */
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		notify_deluser (name);
		g_free (name);
	}
}

static void
notifygui_add_cb (GtkDialog *dialog, gint response, gpointer entry)
{
	char *networks;
	char *text;

	text = (char *)gtk_entry_get_text (GTK_ENTRY (entry));
	if (text[0] && response == GTK_RESPONSE_ACCEPT)
	{
		networks = (char*)gtk_entry_get_text (GTK_ENTRY (g_object_get_data (G_OBJECT (entry), "net")));
		if (g_ascii_strcasecmp (networks, "ALL") == 0 || networks[0] == 0)
			notify_adduser (text, NULL);
		else
			notify_adduser (text, networks);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
notifygui_add_enter (GtkWidget *entry, GtkWidget *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
}

void
fe_notify_ask (char *nick, char *networks)
{
	GtkWidget *dialog;
	GtkWidget *entry;
	GtkWidget *label;
	GtkWidget *wid;
	GtkWidget *table;
	char *msg = _("Enter nickname to add:");
	char buf[256];

	dialog = gtk_dialog_new_with_buttons (msg, NULL, GTK_DIALOG_MODAL,
										GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
										GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
										NULL);
	if (parent_window)
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent_window));
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);

	table = gtk_grid_new ();
	gtk_container_set_border_width (GTK_CONTAINER (table), 12);
	gtk_grid_set_row_spacing (GTK_GRID (table), 3);
	gtk_grid_set_column_spacing (GTK_GRID (table), 8);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), table);

	label = gtk_label_new (msg);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);

	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), nick);
	g_signal_connect (G_OBJECT (entry), "activate",
						 	G_CALLBACK (notifygui_add_enter), dialog);
	gtk_widget_set_hexpand (entry, TRUE);
	gtk_grid_attach (GTK_GRID (table), entry, 1, 0, 1, 1);

	g_signal_connect (G_OBJECT (dialog), "response",
						   G_CALLBACK (notifygui_add_cb), entry);

	label = gtk_label_new (_("Notify on these networks:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);

	wid = gtk_entry_new ();
	g_object_set_data (G_OBJECT (entry), "net", wid);
	g_signal_connect (G_OBJECT (wid), "activate",
						 	G_CALLBACK (notifygui_add_enter), dialog);
	gtk_entry_set_text (GTK_ENTRY (wid), networks ? networks : "ALL");
	gtk_widget_set_hexpand (wid, TRUE);
	gtk_grid_attach (GTK_GRID (table), wid, 1, 2, 1, 1);

	label = gtk_label_new (NULL);
	g_snprintf (buf, sizeof (buf), "<i><span size=\"smaller\">%s</span></i>", _("Comma separated list of networks is accepted."));
	gtk_label_set_markup (GTK_LABEL (label), buf);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (GTK_GRID (table), label, 1, 3, 1, 1);

	gtk_widget_show_all (dialog);
}

static void
notify_add_clicked (GtkWidget * igad)
{
	fe_notify_ask ("", NULL);
}

void
notify_opengui (void)
{
	GtkWidget *vbox, *hbox, *button, *sw, *content_area;
	GtkWidget *button_box;

	if (notify_window)
	{
		gtk_window_present (GTK_WINDOW (notify_window));
		return;
	}

	notify_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (notify_window), _("User List"));
	gtk_window_set_default_size (GTK_WINDOW (notify_window), 500, 300);
	gtk_window_set_position (GTK_WINDOW (notify_window), GTK_WIN_POS_CENTER);
	gtk_container_set_border_width (GTK_CONTAINER (notify_window), 5);

	content_area = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_add (GTK_CONTAINER (notify_window), content_area);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
								GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
								GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (content_area), sw, TRUE, TRUE, 0);

	notify_treeview = notify_treeview_new (sw);
	gtk_container_add (GTK_CONTAINER (sw), notify_treeview);

	button_box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (button_box), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (button_box), 5);
	gtk_box_pack_end (GTK_BOX (content_area), button_box, FALSE, FALSE, 0);

	// Replace stock items with text and icons
	button = gtk_button_new_with_mnemonic (_("_Close"));
	g_signal_connect_swapped (G_OBJECT (button), "clicked",
					  G_CALLBACK (gtk_widget_destroy), notify_window);
	gtk_box_pack_end (GTK_BOX (button_box), button, FALSE, FALSE, 0);

	button = gtk_button_new_with_mnemonic (_("_Remove"));
	g_signal_connect (G_OBJECT (button), "clicked",
					  G_CALLBACK (notify_remove_clicked), NULL);
	gtk_box_pack_end (GTK_BOX (button_box), button, FALSE, FALSE, 0);

	button = gtk_button_new_with_mnemonic (_("_Add"));
	g_signal_connect (G_OBJECT (button), "clicked",
					  G_CALLBACK (notify_add_clicked), NULL);
	gtk_box_pack_end (GTK_BOX (button_box), button, FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (notify_window), "delete-event",
				  G_CALLBACK (gtk_widget_hide_on_delete), NULL);

	notify_gui_update ();

	gtk_widget_show (notify_window);
}
