/* X-Chat
 * Copyright (C) 1998-2005 Peter Zelezny.
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

/* GTK3 compatibility */
#if GTK_CHECK_VERSION(3,0,0)
#define gtk_hbox_new(X,Y) gtk_box_new(GTK_ORIENTATION_HORIZONTAL, Y)
#define gtk_vbox_new(X,Y) gtk_box_new(GTK_ORIENTATION_VERTICAL, Y)
#define gtk_hpaned_new() gtk_paned_new(GTK_ORIENTATION_HORIZONTAL)
#define gtk_vpaned_new() gtk_paned_new(GTK_ORIENTATION_VERTICAL)
#define gtk_hseparator_new() gtk_separator_new(GTK_ORIENTATION_HORIZONTAL)
#define gtk_vseparator_new() gtk_separator_new(GTK_ORIENTATION_VERTICAL)
#define gdk_cursor_new(X) gdk_cursor_new_for_display(gdk_display_get_default(), X)
#define gtk_menu_popup(menu, parent_menu_shell, parent_menu_item, func, data, button, activate_time) \
    gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL)
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <cairo/cairo.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "../common/hexchat.h"
#include "../common/fe.h"
#include "../common/server.h"
#include "../common/hexchatc.h"
#include "../common/outbound.h"
#include "../common/inbound.h"
#include "../common/plugin.h"
#include "../common/modes.h"
#include "../common/url.h"
#include "../common/util.h"
#include "../common/text.h"
#include "../common/chanopt.h"
#include "../common/cfgfiles.h"

#include "fe-gtk.h"
#include "banlist.h"
#include "gtkutil.h"
#include "joind.h"
#include "palette.h"
#include "maingui.h"
#include "menu.h"
#include "fkeys.h"
#include "userlistgui.h"
#include "chanview.h"
#include "pixmaps.h"
#include "plugin-tray.h"
#include "xtext.h"
#include "sexy-spell-entry.h"
#include "gtkutil.h"

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#define GUI_SPACING (3)
#define GUI_BORDER (0)

enum
{
	POS_INVALID = 0,
	POS_TOPLEFT = 1,
	POS_BOTTOMLEFT = 2,
	POS_TOPRIGHT = 3,
	POS_BOTTOMRIGHT = 4,
	POS_TOP = 5,	/* for tabs only */
	POS_BOTTOM = 6,
	POS_HIDDEN = 7
};

/* two different types of tabs */
#define TAG_IRC 0		/* server, channel, dialog */
#define TAG_UTIL 1	/* dcc, notify, chanlist */

static void mg_create_entry (session *sess, GtkWidget *box);
static void mg_create_search (session *sess, GtkWidget *box);
static void mg_link_irctab (session *sess, int focus);

static session_gui static_mg_gui;
static session_gui *mg_gui = NULL;	/* the shared irc tab */
static int ignore_chanmode = FALSE;
static const char chan_flags[] = { 'c', 'n', 't', 'i', 'm', 'l', 'k' };

static chan *active_tab = NULL;	/* active tab */
GtkWidget *parent_window = NULL;			/* the master window */

GtkStyleContext *input_style;

static PangoAttrList *away_list = NULL;
static PangoAttrList *newdata_list = NULL;
static PangoAttrList *nickseen_list = NULL;
static PangoAttrList *newmsg_list = NULL;
static PangoAttrList *plain_list = NULL;

static PangoAttrList *
mg_attr_list_create (GdkRGBA *col, int size)
{
	PangoAttribute *attr;
	PangoAttrList *list;

	list = pango_attr_list_new ();

	if (col)
	{
		attr = pango_attr_foreground_new (
		    (guint16)(col->red * 65535.0 + 0.5),
		    (guint16)(col->green * 65535.0 + 0.5),
		    (guint16)(col->blue * 65535.0 + 0.5));
		attr->start_index = 0;
		attr->end_index = 0xffff;
		pango_attr_list_insert (list, attr);
	}

	if (size > 0)
	{
		attr = pango_attr_scale_new (size == 1 ? PANGO_SCALE_SMALL : PANGO_SCALE_X_SMALL);
		attr->start_index = 0;
		attr->end_index = 0xffff;
		pango_attr_list_insert (list, attr);
	}

	return list;
}

static void
mg_create_tab_colors (void)
{
	if (plain_list)
	{
		pango_attr_list_unref (plain_list);
		pango_attr_list_unref (newmsg_list);
		pango_attr_list_unref (newdata_list);
		pango_attr_list_unref (nickseen_list);
		pango_attr_list_unref (away_list);
	}

	plain_list = mg_attr_list_create (NULL, prefs.hex_gui_tab_small);
	newdata_list = mg_attr_list_create (&colors[COL_NEW_DATA], prefs.hex_gui_tab_small);
	nickseen_list = mg_attr_list_create (&colors[COL_HILIGHT], prefs.hex_gui_tab_small);
	newmsg_list = mg_attr_list_create (&colors[COL_NEW_MSG], prefs.hex_gui_tab_small);
	away_list = mg_attr_list_create (&colors[COL_AWAY], FALSE);
}

static void
set_window_urgency (GtkWidget *win, gboolean set)
{
#ifdef WIN32
	gtk_window_set_urgency_hint (GTK_WINDOW (win), set);
#else
	/* On non-Windows, use the window's urgency hint */
	gtk_window_set_urgency_hint (GTK_WINDOW (win), set);
#endif
}

static void
flash_window (GtkWidget *win)
{
#ifdef WIN32
	FLASHWINFO fw;

	fw.cbSize = sizeof (fw);
	fw.hwnd = (HWND) GDK_WINDOW_HWND (gtk_widget_get_window (win));
	fw.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
	fw.uCount = 5;
	fw.dwTimeout = 0;

	FlashWindowEx (&fw);
#endif
}

static void
unflash_window (GtkWidget *win)
{
	set_window_urgency (win, FALSE);
}

/* flash the taskbar button */

void
fe_flash_window (session *sess)
{
	if (fe_gui_info (sess, 0) != 1)	/* only do it if not focused */
		flash_window (sess->gui->window);
}

/* set a tab plain, red, light-red, or blue */

void
fe_set_tab_color (struct session *sess, tabcolor col)
{
	session_gui *gui = sess->gui;
	GdkRGBA *color = NULL;

	switch (col)
	{
	case COL_DEFAULT:
		color = &gui->tab_color_default;
		break;
	case COL_SELECTED:
		color = &gui->tab_color_selected;
		break;
	case COL_HIGHLIGHT:
		color = &gui->tab_color_highlight;
		break;
	case COL_NEW_DATA:
		color = &gui->tab_color_new_data;
		break;
	case COL_NEW_MESSAGE:
		color = &gui->tab_color_new_message;
		break;
	}

	GtkStyleContext *context = gtk_widget_get_style_context(gui->tab_label);
	GtkCssProvider *provider = gtk_css_provider_new();
	gchar *css = g_strdup_printf("GtkLabel { background-color: #%02x%02x%02x; }",
						 (int)(color->red * 255.0),
						 (int)(color->green * 255.0),
						 (int)(color->blue * 255.0));
	gtk_css_provider_load_from_data(provider, css, -1, NULL);
	gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
						  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(provider);
	g_free(css);
	gtk_widget_queue_draw (gui->tab_label);
}

static void
mg_focus (session *sess)
{
	if (sess->gui->is_tab)
		current_tab = sess;
	current_sess = sess;

	/* dirty trick to avoid auto-selection */
	SPELL_ENTRY_SET_EDITABLE (sess->gui->input_box, FALSE);
	gtk_widget_grab_focus (sess->gui->input_box);
	SPELL_ENTRY_SET_EDITABLE (sess->gui->input_box, TRUE);

	sess->server->front_session = sess;

	if (sess->server->server_session != NULL)
	{
		if (sess->server->server_session->type != SESS_SERVER)
			sess->server->server_session = sess;
	} else
	{
		sess->server->server_session = sess;
	}

	/* when called via mg_changui_new, is_tab might be true, but
		sess->res->tab is still NULL. */
	if (sess->res->tab)
		fe_set_tab_color (sess, FE_COLOR_NONE);
}

static int
mg_progressbar_update (GtkWidget *bar)
{
	static int type = 0;
	static gdouble pos = 0;

	pos += 0.05;
	if (pos >= 0.99)
	{
		if (type == 0)
		{
			type = 1;
			gtk_progress_bar_set_orientation ((GtkProgressBar *) bar,
														 GTK_PROGRESS_RIGHT_TO_LEFT);
		} else
		{
			type = 0;
			gtk_progress_bar_set_orientation ((GtkProgressBar *) bar,
														 GTK_PROGRESS_LEFT_TO_RIGHT);
		}
		pos = 0.05;
	}
	gtk_progress_bar_set_fraction ((GtkProgressBar *) bar, pos);
	return 1;
}

void
mg_progressbar_create (session_gui *gui)
{
	gui->bar = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (gui->nick_box), gui->bar, 0, 0, 0);
	gtk_widget_show (gui->bar);
	gui->bartag = fe_timeout_add (50, mg_progressbar_update, gui->bar);
}

void
mg_progressbar_destroy (session_gui *gui)
{
	fe_timeout_remove (gui->bartag);
	gtk_widget_destroy (gui->bar);
	gui->bar = 0;
	gui->bartag = 0;
}

/* switching tabs away from this one, so remember some info about it! */

static void
mg_unpopulate (session *sess)
{
	restore_gui *res;
	session_gui *gui;
	int i;

	gui = sess->gui;
	res = sess->res;

	res->input_text = g_strdup (SPELL_ENTRY_GET_TEXT (gui->input_box));
	res->topic_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (gui->topic_entry)));
	res->limit_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (gui->limit_entry)));
	res->key_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (gui->key_entry)));
	if (gui->laginfo)
		res->lag_text = g_strdup (gtk_label_get_text (GTK_LABEL (gui->laginfo)));
	if (gui->throttleinfo)
		res->queue_text = g_strdup (gtk_label_get_text (GTK_LABEL (gui->throttleinfo)));

	for (i = 0; i < NUM_FLAG_WIDS - 1; i++)
		res->flag_wid_state[i] = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gui->flag_wid[i]));

	res->old_ul_value = userlist_get_value (gui->user_tree);
	if (gui->lagometer)
		res->lag_value = gtk_progress_bar_get_fraction (
													GTK_PROGRESS_BAR (gui->lagometer));
	if (gui->throttlemeter)
		res->queue_value = gtk_progress_bar_get_fraction (
													GTK_PROGRESS_BAR (gui->throttlemeter));

	if (gui->bar)
	{
		res->c_graph = TRUE;	/* still have a graph, just not visible now */
		mg_progressbar_destroy (gui);
	}
}

static void
mg_restore_label (GtkWidget *label, char **text)
{
	if (!label)
		return;

	if (*text)
	{
		gtk_label_set_text (GTK_LABEL (label), *text);
		g_free (*text);
		*text = NULL;
	} else
	{
		gtk_label_set_text (GTK_LABEL (label), "");
	}
}

static void
mg_restore_entry (GtkWidget *entry, char **text)
{
	if (*text)
	{
		gtk_entry_set_text (GTK_ENTRY (entry), *text);
		g_free (*text);
		*text = NULL;
	} else
	{
		gtk_entry_set_text (GTK_ENTRY (entry), "");
	}
	gtk_editable_set_position (GTK_EDITABLE (entry), -1);
}

static void
mg_restore_speller (GtkWidget *entry, char **text)
{
	if (*text)
	{
		SPELL_ENTRY_SET_TEXT (entry, *text);
		g_free (*text);
		*text = NULL;
	} else
	{
		SPELL_ENTRY_SET_TEXT (entry, "");
	}
	SPELL_ENTRY_SET_POS (entry, -1);
}

void
mg_set_topic_tip (session *sess)
{
	char *text;

	switch (sess->type)
	{
	case SESS_CHANNEL:
		if (sess->topic)
		{
			text = g_strdup_printf (_("Topic for %s is: %s"), sess->channel,
						 sess->topic);
			gtk_widget_set_tooltip_text (sess->gui->topic_entry, text);
			g_free (text);
		} else
			gtk_widget_set_tooltip_text (sess->gui->topic_entry, _("No topic is set"));
		break;
	default:
		if (gtk_entry_get_text (GTK_ENTRY (sess->gui->topic_entry)) &&
			 gtk_entry_get_text (GTK_ENTRY (sess->gui->topic_entry))[0])
			gtk_widget_set_tooltip_text (sess->gui->topic_entry, (char *)gtk_entry_get_text (GTK_ENTRY (sess->gui->topic_entry)));
		else
			gtk_widget_set_tooltip_text (sess->gui->topic_entry, NULL);
	}
}

static void
mg_hide_empty_pane (GtkPaned *pane)
{
	if ((gtk_paned_get_child1 (pane) == NULL || !gtk_widget_get_visible (gtk_paned_get_child1 (pane))) &&
		(gtk_paned_get_child2 (pane) == NULL || !gtk_widget_get_visible (gtk_paned_get_child2 (pane))))
	{
		gtk_widget_hide (GTK_WIDGET (pane));
		return;
	}

	gtk_widget_show (GTK_WIDGET (pane));
}

static void
mg_hide_empty_boxes (session_gui *gui)
{
	/* hide empty vpanes - so the handle is not shown */
	mg_hide_empty_pane ((GtkPaned*)gui->vpane_right);
	mg_hide_empty_pane ((GtkPaned*)gui->vpane_left);
}

static void
mg_userlist_showhide (session *sess, int show)
{
	session_gui *gui = sess->gui;
	int handle_size;
	int right_size;
	GtkAllocation allocation;

	right_size = MAX (prefs.hex_gui_pane_right_size, prefs.hex_gui_pane_right_size_min);

	if (show)
	{
		gtk_widget_show (gui->user_box);
		gui->ul_hidden = 0;

		gtk_widget_get_allocation (gui->hpane_right, &allocation);
		gtk_widget_style_get (GTK_WIDGET (gui->hpane_right), "handle-size", &handle_size, NULL);
		gtk_paned_set_position (GTK_PANED (gui->hpane_right), allocation.width - (right_size + handle_size));
	}
	else
	{
		gtk_widget_hide (gui->user_box);
		gui->ul_hidden = 1;
	}

	mg_hide_empty_boxes (gui);
}

static gboolean
mg_is_userlist_and_tree_combined (void)
{
	if (prefs.hex_gui_tab_pos == POS_TOPLEFT && prefs.hex_gui_ulist_pos == POS_BOTTOMLEFT)
		return TRUE;
	if (prefs.hex_gui_tab_pos == POS_BOTTOMLEFT && prefs.hex_gui_ulist_pos == POS_TOPLEFT)
		return TRUE;

	if (prefs.hex_gui_tab_pos == POS_TOPRIGHT && prefs.hex_gui_ulist_pos == POS_BOTTOMRIGHT)
		return TRUE;
	if (prefs.hex_gui_tab_pos == POS_BOTTOMRIGHT && prefs.hex_gui_ulist_pos == POS_TOPRIGHT)
		return TRUE;

	return FALSE;
}

/* decide if the userlist should be shown or hidden for this tab */

void
mg_decide_userlist (session *sess, gboolean switch_to_current)
{
	/* when called from menu.c we need this */
	if (sess->gui == mg_gui && switch_to_current)
		sess = current_tab;

	if (prefs.hex_gui_ulist_hide)
	{
		mg_userlist_showhide (sess, FALSE);
		return;
	}

	switch (sess->type)
	{
	case SESS_SERVER:
	case SESS_DIALOG:
	case SESS_NOTICES:
	case SESS_SNOTICES:
		if (mg_is_userlist_and_tree_combined ())
			mg_userlist_showhide (sess, TRUE);	/* show */
		else
			mg_userlist_showhide (sess, FALSE);	/* hide */
		break;
	default:		
		mg_userlist_showhide (sess, TRUE);	/* show */
	}
}

static int ul_tag = 0;

static gboolean
mg_populate_userlist (session *sess)
{
	if (!sess)
		sess = current_tab;

	if (is_session (sess))
	{
		if (sess->type == SESS_DIALOG)
			mg_set_access_icon (sess->gui, NULL, sess->server->is_away);
		else
			mg_set_access_icon (sess->gui, get_user_icon (sess->server, sess->me), sess->server->is_away);
		userlist_show (sess);
		userlist_set_value (sess->gui->user_tree, sess->res->old_ul_value);
	}

	ul_tag = 0;
	return 0;
}

/* fill the irc tab with a new channel */

static void
mg_populate (session *sess)
{
	session_gui *gui = sess->gui;
	restore_gui *res = sess->res;
	int i, render = TRUE;
	guint16 vis = gui->ul_hidden;
	GtkAllocation allocation;

	switch (sess->type)
	{
	case SESS_DIALOG:
		/* show the dialog buttons */
		gtk_widget_show (gui->dialogbutton_box);
		/* hide the chan-mode buttons */
		gtk_widget_hide (gui->topicbutton_box);
		/* hide the userlist */
		mg_decide_userlist (sess, FALSE);
		/* shouldn't edit the topic */
		gtk_editable_set_editable (GTK_EDITABLE (gui->topic_entry), FALSE);
		/* might be hidden from server tab */
		if (prefs.hex_gui_topicbar)
			gtk_widget_show (gui->topic_bar);
		break;
	case SESS_SERVER:
		if (prefs.hex_gui_mode_buttons)
			gtk_widget_show (gui->topicbutton_box);
		/* hide the dialog buttons */
		gtk_widget_hide (gui->dialogbutton_box);
		/* hide the userlist */
		mg_decide_userlist (sess, FALSE);
		/* servers don't have topics */
		gtk_widget_hide (gui->topic_bar);
		break;
	default:
		/* hide the dialog buttons */
		gtk_widget_hide (gui->dialogbutton_box);
		if (prefs.hex_gui_mode_buttons)
			gtk_widget_show (gui->topicbutton_box);
		/* show the userlist */
		mg_decide_userlist (sess, FALSE);
		/* let the topic be editted */
		gtk_editable_set_editable (GTK_EDITABLE (gui->topic_entry), TRUE);
		if (prefs.hex_gui_topicbar)
			gtk_widget_show (gui->topic_bar);
	}

	/* move to THE irc tab */
	if (gui->is_tab)
		gtk_notebook_set_current_page (GTK_NOTEBOOK (gui->note_book), 0);

	/* xtext size change? Then don't render, wait for the expose caused
      by showing/hidding the userlist */
	gtk_widget_get_allocation (gui->user_box, &allocation);
	if (vis != gui->ul_hidden && allocation.width > 1)
		render = FALSE;

	gtk_xtext_buffer_show (GTK_XTEXT (gui->xtext), res->buffer, render);

	if (gui->is_tab)
		gtk_widget_set_sensitive (gui->menu, TRUE);

	/* restore all the GtkEntry's */
	mg_restore_entry (gui->topic_entry, &res->topic_text);
	mg_restore_speller (gui->input_box, &res->input_text);
	mg_restore_entry (gui->key_entry, &res->key_text);
	mg_restore_entry (gui->limit_entry, &res->limit_text);
	mg_restore_label (gui->laginfo, &res->lag_text);
	mg_restore_label (gui->throttleinfo, &res->queue_text);

	mg_focus (sess);
	fe_set_title (sess);

	/* this one flickers, so only change if necessary */
	if (strcmp (sess->server->nick, gtk_button_get_label (GTK_BUTTON (gui->nick_label))) != 0)
		gtk_button_set_label (GTK_BUTTON (gui->nick_label), sess->server->nick);

	/* this is slow, so make it a timeout event */
	if (!gui->is_tab)
	{
		mg_populate_userlist (sess);
	} else
	{
		if (ul_tag == 0)
			ul_tag = g_idle_add ((GSourceFunc)mg_populate_userlist, NULL);
	}

	fe_userlist_numbers (sess);

	/* restore all the channel mode buttons */
	ignore_chanmode = TRUE;
	for (i = 0; i < NUM_FLAG_WIDS - 1; i++)
	{
		/* Hide if mode not supported */
		if (sess->server && strchr (sess->server->chanmodes, chan_flags[i]) == NULL)
			gtk_widget_hide (sess->gui->flag_wid[i]);
		else
			gtk_widget_show (sess->gui->flag_wid[i]);

		/* Update state */
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gui->flag_wid[i]),
									res->flag_wid_state[i]);
	}
	ignore_chanmode = FALSE;

	if (gui->lagometer)
	{
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (gui->lagometer),
												 res->lag_value);
		if (res->lag_tip)
			gtk_widget_set_tooltip_text (gtk_widget_get_parent (sess->gui->lagometer), res->lag_tip);
	}
	if (gui->throttlemeter)
	{
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (gui->throttlemeter),
												 res->queue_value);
		if (res->queue_tip)
			gtk_widget_set_tooltip_text (gtk_widget_get_parent (sess->gui->throttlemeter), res->queue_tip);
	}

	/* did this tab have a connecting graph? restore it.. */
	if (res->c_graph)
	{
		res->c_graph = FALSE;
		mg_progressbar_create (gui);
	}

	/* menu items */
	menu_set_away (gui, sess->server->is_away);
	gtk_widget_set_sensitive (gui->menu_item[MENU_ID_AWAY], sess->server->connected);
	gtk_widget_set_sensitive (gui->menu_item[MENU_ID_JOIN], sess->server->end_of_motd);
	gtk_widget_set_sensitive (gui->menu_item[MENU_ID_DISCONNECT],
									  sess->server->connected || sess->server->recondelay_tag);

	mg_set_topic_tip (sess);

	plugin_emit_dummy_print (sess, "Focus Tab");
}

void
mg_bring_tofront_sess (session *sess)	/* IRC tab or window */
{
	if (sess->gui->is_tab)
		chan_focus (sess->res->tab);
	else
		gtk_window_present (GTK_WINDOW (sess->gui->window));
}

void
mg_bring_tofront (GtkWidget *vbox)	/* non-IRC tab or window */
{
	chan *ch;

	ch = g_object_get_data (G_OBJECT (vbox), "ch");
	if (ch)
		chan_focus (ch);
	else
		gtk_window_present (GTK_WINDOW (gtk_widget_get_toplevel (vbox)));
}

void
mg_switch_page (int relative, int num)
{
	if (mg_gui)
		chanview_move_focus (mg_gui->chanview, relative, num);
}

/* a toplevel IRC window was destroyed */

static void
mg_topdestroy_cb (GtkWidget *win, session *sess)
{
/*	printf("enter mg_topdestroy. sess %p was destroyed\n", sess);*/
	session_free (sess);	/* tell hexchat.c about it */
}

/* cleanup an IRC tab */

static void
mg_ircdestroy (session *sess)
{
	GSList *list;

	session_free (sess);	/* tell hexchat.c about it */

	if (mg_gui == NULL)
	{
/*		puts("-> mg_gui is already NULL");*/
		return;
	}

	list = sess_list;
	while (list)
	{
		sess = list->data;
		if (sess->gui->is_tab)
		{
/*			puts("-> some tabs still remain");*/
			return;
		}
		list = list->next;
	}

/*	puts("-> no tabs left, killing main tabwindow");*/
	gtk_widget_destroy (mg_gui->window);
	active_tab = NULL;
	mg_gui = NULL;
	parent_window = NULL;
}

static void
mg_tab_close_cb (GtkWidget *dialog, gint arg1, session *sess)
{
	GSList *list, *next;

	gtk_widget_destroy (dialog);
	if (arg1 == GTK_RESPONSE_OK && is_session (sess))
	{
		/* force it NOT to send individual PARTs */
		sess->server->sent_quit = TRUE;

		for (list = sess_list; list;)
		{
			next = list->next;
			if (((session *)list->data)->server == sess->server &&
				 ((session *)list->data) != sess)
				fe_close_window ((session *)list->data);
			list = next;
		}

		/* just send one QUIT - better for BNCs */
		sess->server->sent_quit = FALSE;
		fe_close_window (sess);
	}
}

void
mg_tab_close (session *sess)
{
	GtkWidget *dialog;
	GSList *list;
	int i;

	if (chan_remove (sess->res->tab, FALSE))
	{
		sess->res->tab = NULL;
		mg_ircdestroy (sess);
	}
	else
	{
		for (i = 0, list = sess_list; list; list = list->next)
		{
			session *s = (session*)list->data;
			if (s->server == sess->server && (s->type == SESS_CHANNEL || s->type == SESS_DIALOG))
				i++;
		}
		dialog = gtk_message_dialog_new (GTK_WINDOW (parent_window), 0,
						GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL,
						_("This server still has %d channels or dialogs associated with it. "
						  "Close them all?"), i);
		g_signal_connect (G_OBJECT (dialog), "response",
								G_CALLBACK (mg_tab_close_cb), sess);
		if (prefs.hex_gui_tab_layout)
		{
			gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
		}
		else
		{
			gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);		
		}
		gtk_widget_show (dialog);
	}
}

static void
mg_menu_destroy (GtkWidget *menu, gpointer userdata)
{
	gtk_widget_destroy (menu);
	g_object_unref (menu);
}

void
mg_create_icon_item (char *label, char *icon_name, GtkWidget *menu,
						void *callback, void *userdata)
{
	GtkWidget *item;
	GtkWidget *box;
	GtkWidget *image;
	GtkWidget *label_widget;

	item = gtk_menu_item_new();
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add(GTK_CONTAINER(item), box);

	if (icon_name && *icon_name) {
		image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
		gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
	}

	label_widget = gtk_label_new_with_mnemonic(label);
	gtk_label_set_use_underline(GTK_LABEL(label_widget), TRUE);
	gtk_box_pack_start(GTK_BOX(box), label_widget, TRUE, TRUE, 0);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(callback), userdata);
	gtk_widget_show_all(item);
}

static int
mg_count_networks (void)
{
	int cons = 0;
	GSList *list;

	for (list = serv_list; list; list = list->next)
	{
		if (((server *)list->data)->connected)
			cons++;
	}
	return cons;
}

static int
mg_count_dccs (void)
{
	GSList *list;
	struct DCC *dcc;
	int dccs = 0;

	list = dcc_list;
	while (list)
	{
		dcc = list->data;
		if ((dcc->type == TYPE_SEND || dcc->type == TYPE_RECV) &&
			 dcc->dccstat == STAT_ACTIVE)
			dccs++;
		list = list->next;
	}

	return dccs;
}

void
mg_open_quit_dialog (gboolean minimize_button)
{
	static GtkWidget *dialog = NULL;
	GtkWidget *dialog_vbox1;
	GtkWidget *table1;
	GtkWidget *image;
	GtkWidget *checkbutton1;
	GtkWidget *label;
	GtkWidget *dialog_action_area1;
	GtkWidget *button;
	char *text, *connecttext;
	int cons;
	int dccs;

	if (dialog)
	{
		gtk_window_present (GTK_WINDOW (dialog));
		return;
	}

	dccs = mg_count_dccs ();
	cons = mg_count_networks ();
	if (dccs + cons == 0 || !prefs.hex_gui_quit_dialog)
	{
		hexchat_exit ();
		return;
	}

	dialog = gtk_dialog_new ();
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 6);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Quit HexChat?"));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent_window));
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

	dialog_vbox1 = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_widget_show (dialog_vbox1);

	table1 = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (table1);
	gtk_box_pack_start (GTK_BOX (dialog_vbox1), table1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (table1), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table1), 12);
	gtk_table_set_col_spacings (GTK_TABLE (table1), 12);

	image = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_DIALOG);
	gtk_widget_show (image);
	gtk_table_attach (GTK_TABLE (table1), image, 0, 1, 0, 1,
							(GtkAttachOptions) (GTK_FILL),
							(GtkAttachOptions) (GTK_FILL), 0, 0);

	checkbutton1 = gtk_check_button_new_with_mnemonic (_("Don't ask next time."));
	gtk_widget_show (checkbutton1);
	gtk_table_attach (GTK_TABLE (table1), checkbutton1, 0, 2, 1, 2,
							(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
							(GtkAttachOptions) (0), 0, 4);

	connecttext = g_strdup_printf (_("You are connected to %i IRC networks."), cons);
	text = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s\n%s",
								_("Are you sure you want to quit?"),
								cons ? connecttext : "",
								dccs ? _("Some file transfers are still active.") : "");
	g_free (connecttext);
	label = gtk_label_new (text);
	g_free (text);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table1), label, 1, 2, 0, 1,
						(GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK | GTK_FILL),
						(GtkAttachOptions) (GTK_EXPAND | GTK_SHRINK), 0, 0);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (label), 0);
	gtk_label_set_yalign (GTK_LABEL (label), 0.5);

	dialog_action_area1 = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
	gtk_widget_show (dialog_action_area1);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area1),
										GTK_BUTTONBOX_END);

	if (minimize_button && gtkutil_tray_icon_supported (GTK_WINDOW(dialog)))
	{
		button = gtk_button_new_with_mnemonic (_("_Minimize to Tray"));
		gtk_widget_show (button);
		gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, 1);
	}

	button = gtk_button_new_from_stock ("gtk-cancel");
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
											GTK_RESPONSE_CANCEL);
	gtk_widget_grab_focus (button);

	button = gtk_button_new_from_stock ("gtk-quit");
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, 0);

	gtk_widget_show (dialog);

	switch (gtk_dialog_run (GTK_DIALOG (dialog)))
	{
	case 0:
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton1)))
			prefs.hex_gui_quit_dialog = 0;
		hexchat_exit ();
		break;
	case 1: /* minimize to tray */
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbutton1)))
		{
			prefs.hex_gui_tray_close = 1;
			/*prefs.hex_gui_quit_dialog = 0;*/
		}
		/* force tray icon ON, if not already */
		if (!prefs.hex_gui_tray)
		{
			prefs.hex_gui_tray = 1;
			tray_apply_setup ();
		}
		tray_toggle_visibility (TRUE);
		break;
	}

	gtk_widget_destroy (dialog);
	dialog = NULL;
}

void
mg_close_sess (session *sess)
{
	if (sess_list->next == NULL)
	{
		mg_open_quit_dialog (FALSE);
		return;
	}

	fe_close_window (sess);
}

static int
mg_chan_remove (chan *ch)
{
	/* remove the tab from chanview */
	chan_remove (ch, TRUE);
	/* any tabs left? */
	if (chanview_get_size (mg_gui->chanview) < 1)
	{
		/* if not, destroy the main tab window */
		gtk_widget_destroy (mg_gui->window);
		current_tab = NULL;
		active_tab = NULL;
		mg_gui = NULL;
		parent_window = NULL;
		return TRUE;
	}
	return FALSE;
}

/* destroy non-irc tab/window */

static void
mg_close_gen (chan *ch, GtkWidget *box)
{
	if (!ch)
		ch = g_object_get_data (G_OBJECT (box), "ch");
	if (ch)
	{
		/* remove from notebook */
		gtk_widget_destroy (box);
		/* remove the tab from chanview */
		mg_chan_remove (ch);
	} else
	{
		gtk_widget_destroy (gtk_widget_get_toplevel (box));
	}
}

/* the "X" close button has been pressed (tab-view) */

static void
mg_xbutton_cb (chanview *cv, chan *ch, int tag, gpointer userdata)
{
	if (tag == TAG_IRC)	/* irc tab */
		mg_close_sess (userdata);
	else						/* non-irc utility tab */
		mg_close_gen (ch, userdata);
}

static void
mg_link_gentab (chan *ch, GtkWidget *box)
{
	int num;
	GtkWidget *win;

	g_object_ref (box);

	num = gtk_notebook_page_num (GTK_NOTEBOOK (mg_gui->note_book), box);
	gtk_notebook_remove_page (GTK_NOTEBOOK (mg_gui->note_book), num);
	mg_chan_remove (ch);

	win = gtkutil_window_new (g_object_get_data (G_OBJECT (box), "title"), "",
									  GPOINTER_TO_INT (g_object_get_data (G_OBJECT (box), "w")),
									  GPOINTER_TO_INT (g_object_get_data (G_OBJECT (box), "h")),
									  2);
	/* so it doesn't try to chan_remove (there's no tab anymore) */
	g_object_steal_data (G_OBJECT (box), "ch");
	gtk_container_set_border_width (GTK_CONTAINER (box), 0);
	gtk_container_add (GTK_CONTAINER (win), box);
	gtk_widget_show (win);

	g_object_unref (box);
}

static void
mg_detach_tab_cb (GtkWidget *item, chan *ch)
{
	if (chan_get_tag (ch) == TAG_IRC)	/* IRC tab */
	{
		/* userdata is session * */
		mg_link_irctab (chan_get_userdata (ch), 1);
		return;
	}

	/* userdata is GtkWidget * */
	mg_link_gentab (ch, chan_get_userdata (ch));	/* non-IRC tab */
}

static void
mg_destroy_tab_cb (GtkWidget *item, chan *ch)
{
	/* treat it just like the X button press */
	mg_xbutton_cb (mg_gui->chanview, ch, chan_get_tag (ch), chan_get_userdata (ch));
}

static void
mg_color_insert (GtkWidget *item, gpointer userdata)
{
	char buf[32];
	char *text;
	int num = GPOINTER_TO_INT (userdata);

	if (num > 99)
	{
		switch (num)
		{
		case 100:
			text = "\002"; break;
		case 101:
			text = "\037"; break;
		case 102:
			text = "\035"; break;
		case 103:
			text = "\036"; break;
		default:
			text = "\017"; break;
		}
		key_action_insert (current_sess->gui->input_box, 0, text, 0, 0);
	} else
	{
		sprintf (buf, "\003%02d", num);
		key_action_insert (current_sess->gui->input_box, 0, buf, 0, 0);
	}
}

static void
mg_markup_item (GtkWidget *menu, char *text, int arg)
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_label ("");
	gtk_label_set_markup (GTK_LABEL (gtk_bin_get_child (GTK_BIN (item))), text);
	g_signal_connect (G_OBJECT (item), "activate",
							G_CALLBACK (mg_color_insert), GINT_TO_POINTER (arg));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

GtkWidget *
mg_submenu (GtkWidget *menu, char *text)
{
	GtkWidget *submenu, *item;

	item = gtk_menu_item_new_with_mnemonic (text);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	submenu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
	gtk_widget_show (submenu);

	return submenu;
}

static void
mg_create_color_menu (GtkWidget *menu, session *sess)
{
	GtkWidget *submenu;
	GtkWidget *subsubmenu;
	char buf[256];
	int i;

	submenu = mg_submenu (menu, _("Insert Attribute or Color Code"));

	mg_markup_item (submenu, _("<b>Bold</b>"), 100);
	mg_markup_item (submenu, _("<u>Underline</u>"), 101);
	mg_markup_item (submenu, _("<i>Italic</i>"), 102);
	mg_markup_item (submenu, _("<s>Strikethrough</s>"), 103);
	mg_markup_item (submenu, _("Normal"), 999);

	subsubmenu = mg_submenu (submenu, _("Colors 0-7"));

	for (i = 0; i < 8; i++)
	{
		sprintf (buf, "<tt><sup>%02d</sup> <span background=\"#%02x%02x%02x\">"
					"   </span></tt>",
				i, colors[i].red >> 8, colors[i].green >> 8, colors[i].blue >> 8);
		mg_markup_item (subsubmenu, buf, i);
	}

	subsubmenu = mg_submenu (submenu, _("Colors 8-15"));

	for (i = 8; i < 16; i++)
	{
		sprintf (buf, "<tt><sup>%02d</sup> <span background=\"#%02x%02x%02x\">"
					"   </span></tt>",
				i, colors[i].red >> 8, colors[i].green >> 8, colors[i].blue >> 8);
		mg_markup_item (subsubmenu, buf, i);
	}
}

static void
mg_set_guint8 (GtkCheckMenuItem *item, guint8 *setting)
{
	session *sess = current_sess;
	guint8 logging = sess->text_logging;

	*setting = SET_OFF;
	if (gtk_check_menu_item_get_active (item))
		*setting = SET_ON;

	/* has the logging setting changed? */
	if (logging != sess->text_logging)
		log_open_or_close (sess);

	chanopt_save (sess);
	chanopt_save_all (FALSE);
}

static void
mg_perchan_menu_item (char *label, GtkWidget *menu, guint8 *setting, guint global)
{
	guint8 initial_value = *setting;

	/* if it's using global value, use that as initial state */
	if (initial_value == SET_DEFAULT)
		initial_value = global;

	menu_toggle_item (label, menu, mg_set_guint8, setting, initial_value);
}

static void
mg_create_perchannelmenu (session *sess, GtkWidget *menu)
{
	GtkWidget *submenu;

	submenu = menu_quick_sub (_("_Settings"), menu, NULL, XCMENU_MNEMONIC, -1);

	mg_perchan_menu_item (_("_Log to Disk"), submenu, &sess->text_logging, prefs.hex_irc_logging);
	mg_perchan_menu_item (_("_Reload Scrollback"), submenu, &sess->text_scrollback, prefs.hex_text_replay);
	if (sess->type == SESS_CHANNEL)
	{
		mg_perchan_menu_item (_("Strip _Colors"), submenu, &sess->text_strip, prefs.hex_text_stripcolor_msg);
		mg_perchan_menu_item (_("_Hide Join/Part Messages"), submenu, &sess->text_hidejoinpart, prefs.hex_irc_conf_mode);
	}
}

static void
mg_create_alertmenu (session *sess, GtkWidget *menu)
{
	GtkWidget *submenu;
	int hex_balloon, hex_beep, hex_tray, hex_flash;


	switch (sess->type) {
		case SESS_DIALOG:
			hex_balloon = prefs.hex_input_balloon_priv;
			hex_beep = prefs.hex_input_beep_priv;
			hex_tray = prefs.hex_input_tray_priv;
			hex_flash = prefs.hex_input_flash_priv;
			break;
		default:
			hex_balloon = prefs.hex_input_balloon_chans;
			hex_beep = prefs.hex_input_beep_chans;
			hex_tray = prefs.hex_input_tray_chans;
			hex_flash = prefs.hex_input_flash_chans;
	}

	submenu = menu_quick_sub(_("_Extra Alerts"), menu, NULL, XCMENU_MNEMONIC, -1);

	mg_perchan_menu_item(_("Show Notifications"), submenu, &sess->alert_balloon, hex_balloon);

	mg_perchan_menu_item(_("Beep on _Message"), submenu, &sess->alert_beep, hex_beep);

	mg_perchan_menu_item(_("Blink Tray _Icon"), submenu, &sess->alert_tray, hex_tray);

	mg_perchan_menu_item(_("Blink Task _Bar"), submenu, &sess->alert_taskbar, hex_flash);
}

static void
mg_create_tabmenu (session *sess, GdkEventButton *event, chan *ch)
{
	GtkWidget *menu, *item;
	char buf[256];

	menu = gtk_menu_new ();

	if (sess)
	{
		char *name = g_markup_escape_text (sess->channel[0] ? sess->channel : _("<none>"), -1);
		g_snprintf (buf, sizeof (buf), "<span foreground=\"#3344cc\"><b>%s</b></span>", name);
		g_free (name);

		item = gtk_menu_item_new_with_label ("");
		gtk_label_set_markup (GTK_LABEL (gtk_bin_get_child (GTK_BIN (item))), buf);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);

		/* separator */
		menu_quick_item (0, 0, menu, XCMENU_SHADED, 0, 0);

		/* per-channel alerts */
		mg_create_alertmenu (sess, menu);

		/* per-channel settings */
		mg_create_perchannelmenu (sess, menu);

		/* separator */
		menu_quick_item (0, 0, menu, XCMENU_SHADED, 0, 0);

		if (sess->type == SESS_CHANNEL)
			menu_addfavoritemenu (sess->server, menu, sess->channel, TRUE);
		else if (sess->type == SESS_SERVER)
			menu_addconnectmenu (sess->server, menu);
	}

	mg_create_icon_item (_("_Detach"), GTK_STOCK_REDO, menu,
								mg_detach_tab_cb, ch);
	mg_create_icon_item (_("_Close"), GTK_STOCK_CLOSE, menu,
								mg_destroy_tab_cb, ch);
	if (sess && tabmenu_list)
		menu_create (menu, tabmenu_list, sess->channel, FALSE);
	if (sess)
		menu_add_plugin_items (menu, "\x4$TAB", sess->channel);

	if (event->window)
		gtk_menu_set_screen (GTK_MENU (menu), gdk_window_get_screen (event->window));
	g_object_ref (menu);
	g_object_ref_sink (menu);
	g_object_unref (menu);
	g_signal_connect (G_OBJECT (menu), "selection-done",
							G_CALLBACK (mg_menu_destroy), NULL);
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 0, event->time);
}

static gboolean
mg_tab_contextmenu_cb (chanview *cv, chan *ch, int tag, gpointer ud, GdkEventButton *event)
{
	/* middle-click to close a tab */
	if (((prefs.hex_gui_tab_middleclose && event->button == 2))
		&& event->type == GDK_BUTTON_PRESS)
	{
		mg_xbutton_cb (cv, ch, tag, ud);
		return TRUE;
	}

	if (event->button != 3)
		return FALSE;

	if (tag == TAG_IRC)
		mg_create_tabmenu (ud, event, ch);
	else
		mg_create_tabmenu (NULL, event, ch);

	return TRUE;
}

void
mg_dnd_drop_file (session *sess, char *target, char *uri)
{
	char *p, *data, *next, *fname;

	p = data = g_strdup (uri);
	while (*p)
	{
		next = strchr (p, '\r');
		if (g_ascii_strncasecmp ("file:", p, 5) == 0)
		{
			if (next)
				*next = 0;
			fname = g_filename_from_uri (p, NULL, NULL);
			if (fname)
			{
				/* dcc_send() expects utf-8 */
				p = g_filename_from_utf8 (fname, -1, 0, 0, 0);
				if (p)
				{
					dcc_send (sess, target, p, prefs.hex_dcc_max_send_cps, 0);
					g_free (p);
				}
				g_free (fname);
			}
		}
		if (!next)
			break;
		p = next + 1;
		if (*p == '\n')
			p++;
	}
	g_free (data);

}

static void
mg_dialog_dnd_drop (GtkWidget * widget, GdkDragContext * context, gint x,
						  gint y, GtkSelectionData * selection_data, guint info,
						  guint32 time, gpointer ud)
{
	if (current_sess->type == SESS_DIALOG)
		/* sess->channel is really the nickname of dialogs */
		mg_dnd_drop_file (current_sess, current_sess->channel, (char *)gtk_selection_data_get_data (selection_data));
}

/* add a tabbed channel */

static void
mg_add_chan (session *sess)
{
	GdkPixbuf *icon;
	char *name = _("<none>");

	if (sess->channel[0])
		name = sess->channel;

	switch (sess->type)
	{
	case SESS_CHANNEL:
		icon = pix_tree_channel;
		break;
	case SESS_SERVER:
		icon = pix_tree_server;
		break;
	default:
		icon = pix_tree_dialog;
	}

	sess->res->tab = chanview_add (sess->gui->chanview, name, sess->server, sess,
											 sess->type == SESS_SERVER ? FALSE : TRUE,
											 TAG_IRC, icon);
	if (plain_list == NULL)
		mg_create_tab_colors ();

	chan_set_color (sess->res->tab, plain_list);

	if (sess->res->buffer == NULL)
	{
		sess->res->buffer = gtk_xtext_buffer_new (GTK_XTEXT (sess->gui->xtext));
		gtk_xtext_set_time_stamp (sess->res->buffer, prefs.hex_stamp_text);
		sess->res->user_model = userlist_create_model (sess);
	}
}

static void
mg_userlist_button (GtkWidget * box, char *label, char *cmd,
						  int a, int b, int c, int d)
{
	GtkWidget *wid = gtk_button_new_with_label (label);
	g_signal_connect (G_OBJECT (wid), "clicked",
							G_CALLBACK (userlist_button_cb), cmd);
	gtk_table_attach_defaults (GTK_TABLE (box), wid, a, b, c, d);
	show_and_unfocus (wid);
}

static GtkWidget *
mg_create_userlistbuttons (GtkWidget *box)
{
	struct popup *pop;
	GSList *list = button_list;
	int a = 0, b = 0;
	GtkWidget *tab;

	tab = gtk_table_new (5, 2, FALSE);
	gtk_box_pack_end (GTK_BOX (box), tab, FALSE, FALSE, 0);

	while (list)
	{
		pop = list->data;
		if (pop->cmd[0])
		{
			mg_userlist_button (tab, pop->name, pop->cmd, a, a + 1, b, b + 1);
			a++;
			if (a == 2)
			{
				a = 0;
				b++;
			}
		}
		list = list->next;
	}

	return tab;
}

static void
mg_topic_cb (GtkWidget *entry, gpointer userdata)
{
	session *sess = current_sess;
	char *text;

	if (sess->channel[0] && sess->server->connected && sess->type == SESS_CHANNEL)
	{
		text = (char *)gtk_entry_get_text (GTK_ENTRY (entry));
		if (text[0] == 0)
			text = NULL;
		sess->server->p_topic (sess->server, sess->channel, text);
	} else
		gtk_entry_set_text (GTK_ENTRY (entry), "");
	/* restore focus to the input widget, where the next input will most
likely be */
	gtk_widget_grab_focus (sess->gui->input_box);
}

static void
mg_tabwindow_kill_cb (GtkWidget *win, gpointer userdata)
{
	GSList *list, *next;
	session *sess;

/*	puts("enter mg_tabwindow_kill_cb");*/
	hexchat_is_quitting = TRUE;

	/* see if there's any non-tab windows left */
	list = sess_list;
	while (list)
	{
		sess = list->data;
		next = list->next;
		if (!sess->gui->is_tab)
		{
			hexchat_is_quitting = FALSE;
/*			puts("-> will not exit, some toplevel windows left");*/
		} else
		{
			mg_ircdestroy (sess);
		}
		list = next;
	}

	current_tab = NULL;
	active_tab = NULL;
	mg_gui = NULL;
	parent_window = NULL;
}

static gboolean
mg_add_pane_signals (session_gui *gui)
{
	g_signal_connect (G_OBJECT (gui->hpane_right), "notify::position",
							G_CALLBACK (mg_rightpane_cb), gui);
	g_signal_connect (G_OBJECT (gui->hpane_left), "notify::position",
							G_CALLBACK (mg_leftpane_cb), gui);
	g_signal_connect (G_OBJECT (gui->vpane_left), "notify::position",
							G_CALLBACK (mg_vpane_cb), gui);
	g_signal_connect (G_OBJECT (gui->vpane_right), "notify::position",
							G_CALLBACK (mg_vpane_cb), gui);
	return FALSE;
}

static void
mg_create_center (session *sess, session_gui *gui, GtkWidget *box)
{
	GtkWidget *vbox, *hbox, *book;

	/* sep between top and bottom of left side */
	gui->vpane_left = gtk_vpaned_new ();

	/* sep between top and bottom of right side */
	gui->vpane_right = gtk_vpaned_new ();

	/* sep between left and xtext */
	gui->hpane_left = gtk_hpaned_new ();
	gtk_paned_set_position (GTK_PANED (gui->hpane_left), prefs.hex_gui_pane_left_size);

	/* sep between xtext and right side */
	gui->hpane_right = gtk_hpaned_new ();

	if (prefs.hex_gui_win_swap)
	{
		gtk_paned_pack2 (GTK_PANED (gui->hpane_left), gui->vpane_left, FALSE, TRUE);
		gtk_paned_pack1 (GTK_PANED (gui->hpane_left), gui->hpane_right, TRUE, TRUE);
	}
	else
	{
		gtk_paned_pack1 (GTK_PANED (gui->hpane_left), gui->vpane_left, FALSE, TRUE);
		gtk_paned_pack2 (GTK_PANED (gui->hpane_left), gui->hpane_right, TRUE, TRUE);
	}
	gtk_paned_pack2 (GTK_PANED (gui->hpane_right), gui->vpane_right, FALSE, TRUE);

	gtk_container_add (GTK_CONTAINER (box), gui->hpane_left);

	gui->note_book = book = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (book), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (book), FALSE);
	gtk_paned_pack1 (GTK_PANED (gui->hpane_right), book, TRUE, TRUE);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous (GTK_BOX (hbox), FALSE);
	gtk_paned_pack1 (GTK_PANED (gui->vpane_right), hbox, FALSE, TRUE);
	mg_create_userlist (gui, hbox);

	gui->user_box = hbox;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 3);
	gtk_box_set_homogeneous (GTK_BOX (vbox), FALSE);
	gtk_notebook_append_page (GTK_NOTEBOOK (book), vbox, NULL);
	mg_create_topicbar (sess, vbox);

	if (prefs.hex_gui_search_pos)
	{
		mg_create_search (sess, vbox);
		mg_create_textarea (sess, vbox);
	}
	else
	{
		mg_create_textarea (sess, vbox);
		mg_create_search (sess, vbox);
	}

	mg_create_entry (sess, vbox);

	mg_add_pane_signals (gui);
}

static void
mg_change_nick (int cancel, char *text, gpointer userdata)
{
	char buf[256];

	if (!cancel)
	{
		g_snprintf (buf, sizeof (buf), "nick %s", text);
		handle_command (current_sess, buf, FALSE);
	}
}

static void
mg_nickclick_cb (GtkWidget *button, gpointer userdata)
{
	fe_get_str (_("Enter new nickname:"), current_sess->server->nick,
					mg_change_nick, (void *) 1);
}

/* make sure chanview and userlist positions are sane */

static void
mg_sanitize_positions (int *cv, int *ul)
{
	if (prefs.hex_gui_tab_layout == 2)
	{
		/* treeview can't be on TOP or BOTTOM */
		if (*cv == POS_TOP || *cv == POS_BOTTOM)
			*cv = POS_TOPLEFT;
	}

	/* userlist can't be on TOP or BOTTOM */
	if (*ul == POS_TOP || *ul == POS_BOTTOM)
		*ul = POS_TOPRIGHT;

	/* can't have both in the same place */
	if (*cv == *ul)
	{
		*cv = POS_TOPRIGHT;
		if (*ul == POS_TOPRIGHT)
			*cv = POS_BOTTOMRIGHT;
	}
}

static void
mg_place_userlist_and_chanview_real (session_gui *gui, GtkWidget *userlist, GtkWidget *chanview)
{
	int unref_userlist = FALSE;
	int unref_chanview = FALSE;

	/* first, remove userlist/treeview from their containers */
	if (userlist && gtk_widget_get_parent (userlist))
	{
		g_object_ref (userlist);
		gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (userlist)), userlist);
		unref_userlist = TRUE;
	}

	if (chanview && gtk_widget_get_parent (chanview))
	{
		g_object_ref (chanview);
		gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (chanview)), chanview);
		unref_chanview = TRUE;
	}

	if (chanview)
	{
		/* incase the previous pos was POS_HIDDEN */
		gtk_widget_show (chanview);

		gtk_table_set_row_spacing (GTK_TABLE (gui->main_table), 1, 0);
		gtk_table_set_row_spacing (GTK_TABLE (gui->main_table), 2, 2);

		/* then place them back in their new positions */
		switch (prefs.hex_gui_tab_pos)
		{
		case POS_TOPLEFT:
			gtk_paned_pack1 (GTK_PANED (gui->vpane_left), chanview, FALSE, TRUE);
			break;
		case POS_BOTTOMLEFT:
			gtk_paned_pack2 (GTK_PANED (gui->vpane_left), chanview, FALSE, TRUE);
			break;
		case POS_TOPRIGHT:
			gtk_paned_pack1 (GTK_PANED (gui->vpane_right), chanview, FALSE, TRUE);
			break;
		case POS_BOTTOMRIGHT:
			gtk_paned_pack2 (GTK_PANED (gui->vpane_right), chanview, FALSE, TRUE);
			break;
		case POS_TOP:
			gtk_table_set_row_spacing (GTK_TABLE (gui->main_table), 1, GUI_SPACING-1);
			gtk_table_attach (GTK_TABLE (gui->main_table), chanview,
									1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
			break;
		case POS_HIDDEN:
			gtk_widget_hide (chanview);
			/* always attach it to something to avoid ref_count=0 */
			if (prefs.hex_gui_ulist_pos == POS_TOP)
				gtk_table_attach (GTK_TABLE (gui->main_table), chanview,
										1, 2, 3, 4, GTK_FILL, GTK_FILL, 0, 0);

			else
				gtk_table_attach (GTK_TABLE (gui->main_table), chanview,
										1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
			break;
		default:/* POS_BOTTOM */
			gtk_table_set_row_spacing (GTK_TABLE (gui->main_table), 2, 3);
			gtk_table_attach (GTK_TABLE (gui->main_table), chanview,
									1, 2, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
		}
	}

	if (userlist)
	{
		switch (prefs.hex_gui_ulist_pos)
		{
		case POS_TOPLEFT:
			gtk_paned_pack1 (GTK_PANED (gui->vpane_left), userlist, FALSE, TRUE);
			break;
		case POS_BOTTOMLEFT:
			gtk_paned_pack2 (GTK_PANED (gui->vpane_left), userlist, FALSE, TRUE);
			break;
		case POS_BOTTOMRIGHT:
			gtk_paned_pack2 (GTK_PANED (gui->vpane_right), userlist, FALSE, TRUE);
			break;
		/*case POS_HIDDEN:
			break;*/	/* Hide using the VIEW menu instead */
		default:/* POS_TOPRIGHT */
			gtk_paned_pack1 (GTK_PANED (gui->vpane_right), userlist, FALSE, TRUE);
		}
	}

	if (mg_is_userlist_and_tree_combined () && prefs.hex_gui_pane_divider_position != 0)
	{
		gtk_paned_set_position (GTK_PANED (gui->vpane_left), prefs.hex_gui_pane_divider_position);
		gtk_paned_set_position (GTK_PANED (gui->vpane_right), prefs.hex_gui_pane_divider_position);
	}

	if (unref_chanview)
		g_object_unref (chanview);
	if (unref_userlist)
		g_object_unref (userlist);

	mg_hide_empty_boxes (gui);
}

static void
mg_place_userlist_and_chanview (session_gui *gui)
{
	GtkOrientation orientation;
	GtkWidget *chanviewbox = NULL;
	int pos;

	mg_sanitize_positions (&prefs.hex_gui_tab_pos, &prefs.hex_gui_ulist_pos);

	if (gui->chanview)
	{
		pos = prefs.hex_gui_tab_pos;

		orientation = chanview_get_orientation (gui->chanview);
		if ((pos == POS_BOTTOM || pos == POS_TOP) && orientation == GTK_ORIENTATION_VERTICAL)
			chanview_set_orientation (gui->chanview, FALSE);
		else if ((pos == POS_TOPLEFT || pos == POS_BOTTOMLEFT || pos == POS_TOPRIGHT || pos == POS_BOTTOMRIGHT) && orientation == GTK_ORIENTATION_HORIZONTAL)
			chanview_set_orientation (gui->chanview, TRUE);
		chanviewbox = chanview_get_box (gui->chanview);
	}

	mg_place_userlist_and_chanview_real (gui, gui->user_box, chanviewbox);
}

void
mg_change_layout (int type)
{
	if (mg_gui)
	{
		/* put tabs at the bottom */
		if (type == 0 && prefs.hex_gui_tab_pos != POS_BOTTOM && prefs.hex_gui_tab_pos != POS_TOP)
			prefs.hex_gui_tab_pos = POS_BOTTOM;

		mg_place_userlist_and_chanview (mg_gui);
		chanview_set_impl (mg_gui->chanview, type);
	}
}

static void
mg_inputbox_rightclick (GtkEntry *entry, GtkWidget *menu)
{
	mg_create_color_menu (menu, NULL);
}

/* Search bar adapted from Conspire's by William Pitcock */

#define SEARCH_CHANGE		1
#define SEARCH_NEXT			2
#define SEARCH_PREVIOUS		3
#define SEARCH_REFRESH		4

static void
search_handle_event(int search_type, session *sess)
{
	textentry *last;
	const gchar *text = NULL;
	gtk_xtext_search_flags flags;
	GError *err = NULL;
	gboolean backwards = FALSE;

	/* When just typing show most recent first */
	if (search_type == SEARCH_PREVIOUS || search_type == SEARCH_CHANGE)
		backwards = TRUE;

	flags = ((prefs.hex_text_search_case_match == 1? case_match: 0) |
				(backwards? backward: 0) |
				(prefs.hex_text_search_highlight_all == 1? highlight: 0) |
				(prefs.hex_text_search_follow == 1? follow: 0) |
				(prefs.hex_text_search_regexp == 1? regexp: 0));

	if (search_type != SEARCH_REFRESH)
		text = gtk_entry_get_text (GTK_ENTRY(sess->gui->shentry));
	last = gtk_xtext_search (GTK_XTEXT (sess->gui->xtext), text, flags, &err);

	if (err)
	{
		gtk_entry_set_icon_from_stock (GTK_ENTRY (sess->gui->shentry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_DIALOG_ERROR);
		gtk_entry_set_icon_tooltip_text (GTK_ENTRY (sess->gui->shentry), GTK_ENTRY_ICON_SECONDARY, _(err->message));
		g_error_free (err);
	}
	else if (!last)
	{
		if (text && text[0] == 0) /* empty string, no error */
		{
			gtk_entry_set_icon_from_stock (GTK_ENTRY (sess->gui->shentry), GTK_ENTRY_ICON_SECONDARY, NULL);
		}
		else
		{
			/* Either end of search or not found, try again to wrap if only end */
			last = gtk_xtext_search (GTK_XTEXT (sess->gui->xtext), text, flags, &err);
			if (!last) /* Not found error */
			{
				gtk_entry_set_icon_from_stock (GTK_ENTRY (sess->gui->shentry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_DIALOG_ERROR);
				gtk_entry_set_icon_tooltip_text (GTK_ENTRY (sess->gui->shentry), GTK_ENTRY_ICON_SECONDARY, _("No results found."));
			}
		}
	}
	else
	{
		gtk_entry_set_icon_from_stock (GTK_ENTRY (sess->gui->shentry), GTK_ENTRY_ICON_SECONDARY, NULL);
	}
}

static void
search_handle_change(GtkWidget *wid, session *sess)
{
	search_handle_event(SEARCH_CHANGE, sess);
}

static void
search_handle_refresh(GtkWidget *wid, session *sess)
{
	search_handle_event(SEARCH_REFRESH, sess);
}

void
mg_search_handle_previous(GtkWidget *wid, session *sess)
{
	search_handle_event(SEARCH_PREVIOUS, sess);
}

void
mg_search_handle_next(GtkWidget *wid, session *sess)
{
	search_handle_event(SEARCH_NEXT, sess);
}

static void
search_set_option (GtkToggleButton *but, guint *pref)
{
	*pref = gtk_toggle_button_get_active(but);
	save_config();
}

void
mg_search_toggle(session *sess)
{
	if (gtk_widget_get_visible(sess->gui->shbox))
	{
		gtk_widget_hide(sess->gui->shbox);
		gtk_widget_grab_focus(sess->gui->input_box);
		gtk_entry_set_text(GTK_ENTRY(sess->gui->shentry), "");
	}
	else
	{
		/* Reset search state */
		gtk_entry_set_icon_from_stock (GTK_ENTRY (sess->gui->shentry), GTK_ENTRY_ICON_SECONDARY, NULL);

		/* Show and focus */
		gtk_widget_show(sess->gui->shbox);
		gtk_widget_grab_focus(sess->gui->shentry);
	}
}

static gboolean
search_handle_esc (GtkWidget *win, GdkEventKey *key, session *sess)
{
	if (key->keyval == GDK_KEY_Escape)
		mg_search_toggle(sess);
			
	return FALSE;
}

static void
mg_create_search(session *sess, GtkWidget *box)
{
	GtkWidget *entry, *label, *next, *previous, *highlight, *matchcase, *regex, *close;
	session_gui *gui = sess->gui;

	gui->shbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_box_set_homogeneous (GTK_BOX (gui->shbox), FALSE);
	gtk_box_pack_start(GTK_BOX(box), gui->shbox, FALSE, FALSE, 0);

	close = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (close), gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU));
	gtk_button_set_relief(GTK_BUTTON(close), GTK_RELIEF_NONE);
	gtk_widget_set_can_focus (close, FALSE);
	gtk_box_pack_start(GTK_BOX(gui->shbox), close, FALSE, FALSE, 0);
	g_signal_connect_swapped(G_OBJECT(close), "clicked", G_CALLBACK(mg_search_toggle), sess);

	label = gtk_label_new(_("Find:"));
	gtk_box_pack_start(GTK_BOX(gui->shbox), label, FALSE, FALSE, 0);

	gui->shentry = entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(gui->shbox), entry, FALSE, FALSE, 0);
	gtk_widget_set_size_request (gui->shentry, 180, -1);
	gui->search_changed_signal = g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(search_handle_change), sess);
	g_signal_connect (G_OBJECT (entry), "key_press_event", G_CALLBACK (search_handle_esc), sess);
	g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(mg_search_handle_next), sess);
	gtk_entry_set_icon_activatable (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, FALSE);
	gtk_entry_set_icon_tooltip_text (GTK_ENTRY (sess->gui->shentry), GTK_ENTRY_ICON_SECONDARY, _("Search hit end or not found."));

	previous = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (previous), gtk_image_new_from_icon_name ("go-previous", GTK_ICON_SIZE_MENU));
	gtk_button_set_relief(GTK_BUTTON(previous), GTK_RELIEF_NONE);
	gtk_widget_set_can_focus (previous, FALSE);
	gtk_box_pack_start(GTK_BOX(gui->shbox), previous, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(previous), "clicked", G_CALLBACK(mg_search_handle_previous), sess);

	next = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (next), gtk_image_new_from_icon_name ("go-next", GTK_ICON_SIZE_MENU));
	gtk_button_set_relief(GTK_BUTTON(next), GTK_RELIEF_NONE);
	gtk_widget_set_can_focus (next, FALSE);
	gtk_box_pack_start(GTK_BOX(gui->shbox), next, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(next), "clicked", G_CALLBACK(mg_search_handle_next), sess);

	highlight = gtk_check_button_new_with_mnemonic (_("_Highlight all"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(highlight), prefs.hex_text_search_highlight_all);
	gtk_widget_set_can_focus (highlight, FALSE);
	g_signal_connect (G_OBJECT (highlight), "toggled", G_CALLBACK (search_set_option), &prefs.hex_text_search_highlight_all);
	g_signal_connect (G_OBJECT (highlight), "toggled", G_CALLBACK (search_handle_refresh), sess);
	gtk_box_pack_start(GTK_BOX(gui->shbox), highlight, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text (highlight, _("Highlight all occurrences, and underline the current occurrence."));

	matchcase = gtk_check_button_new_with_mnemonic (_("Mat_ch case"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(matchcase), prefs.hex_text_search_case_match);
	gtk_widget_set_can_focus (matchcase, FALSE);
	g_signal_connect (G_OBJECT (matchcase), "toggled", G_CALLBACK (search_set_option), &prefs.hex_text_search_case_match);
	gtk_box_pack_start(GTK_BOX(gui->shbox), matchcase, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text (matchcase, _("Perform a case-sensitive search."));

	regex = gtk_check_button_new_with_mnemonic (_("_Regex"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(regex), prefs.hex_text_search_regexp);
	gtk_widget_set_can_focus (regex, FALSE);
	g_signal_connect (G_OBJECT (regex), "toggled", G_CALLBACK (search_set_option), &prefs.hex_text_search_regexp);
	gtk_box_pack_start(GTK_BOX(gui->shbox), regex, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text (regex, _("Regard search string as a regular expression."));
}

static void
mg_create_entry (session *sess, GtkWidget *box)
{
	GtkWidget *hbox, *but, *entry;
	session_gui *gui = sess->gui;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous (GTK_BOX (hbox), FALSE);
	gtk_box_pack_start (GTK_BOX (box), hbox, 0, 0, 0);

	gui->nick_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous (GTK_BOX (gui->nick_box), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), gui->nick_box, 0, 0, 0);

	gui->nick_label = but = gtk_button_new_with_label (sess->server->nick);
	gtk_button_set_relief (GTK_BUTTON (but), GTK_RELIEF_NONE);
	gtk_widget_set_can_focus (but, FALSE);
	gtk_box_pack_end (GTK_BOX (gui->nick_box), but, 0, 0, 0);
	g_signal_connect (G_OBJECT (but), "clicked",
							G_CALLBACK (mg_nickclick_cb), NULL);

	gui->input_box = entry = sexy_spell_entry_new ();
	sexy_spell_entry_set_checked ((SexySpellEntry *)entry, prefs.hex_gui_input_spell);
	sexy_spell_entry_set_parse_attributes ((SexySpellEntry *)entry, prefs.hex_gui_input_attr);

	gtk_entry_set_max_length (GTK_ENTRY (gui->input_box), 0);
	g_signal_connect (G_OBJECT (entry), "activate",
							G_CALLBACK (mg_inputbox_cb), gui);
	gtk_container_add (GTK_CONTAINER (hbox), entry);

	gtk_widget_set_name (entry, "hexchat-inputbox");
	g_signal_connect (G_OBJECT (entry), "key_press_event",
							G_CALLBACK (key_handle_key_press), NULL);
	g_signal_connect (G_OBJECT (entry), "focus_in_event",
							G_CALLBACK (mg_inputbox_focus), gui);
	g_signal_connect (G_OBJECT (entry), "populate_popup",
							G_CALLBACK (mg_inputbox_rightclick), NULL);
	g_signal_connect (G_OBJECT (entry), "word-check",
							G_CALLBACK (mg_spellcheck_cb), NULL);
	gtk_widget_grab_focus (entry);

	if (prefs.hex_gui_input_style)
		mg_apply_entry_style (entry);
}

static void
mg_create_textarea (session *sess, GtkWidget *box)
{
	GtkWidget *inbox, *vbox, *frame;
	GtkXText *xtext;
	session_gui *gui = sess->gui;
	static const GtkTargetEntry dnd_targets[] =
	{
		{"text/uri-list", 0, 1}
	};
	static const GtkTargetEntry dnd_dest_targets[] =
	{
		{"HEXCHAT_CHANVIEW", GTK_TARGET_SAME_APP, 75 },
		{"HEXCHAT_USERLIST", GTK_TARGET_SAME_APP, 75 }
	};

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_set_homogeneous (GTK_BOX (vbox), FALSE);
	gtk_container_add (GTK_CONTAINER (box), vbox);

	inbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_box_set_homogeneous (GTK_BOX (inbox), FALSE);
	gtk_container_add (GTK_CONTAINER (vbox), inbox);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (inbox), frame);

	gui->xtext = gtk_xtext_new (colors, TRUE);
	xtext = GTK_XTEXT (gui->xtext);
	gtk_xtext_set_max_indent (xtext, prefs.hex_text_max_indent);
	gtk_xtext_set_thin_separator (xtext, prefs.hex_text_thin_sep);
	gtk_xtext_set_urlcheck_function (xtext, mg_word_check);
	gtk_xtext_set_max_lines (xtext, prefs.hex_text_max_lines);
	gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (xtext));

	mg_update_xtext (GTK_WIDGET (xtext));

	g_signal_connect (G_OBJECT (xtext), "word_click",
							G_CALLBACK (mg_word_clicked), NULL);

	gui->vscrollbar = gtk_vscrollbar_new (GTK_XTEXT (xtext)->adj);
	gtk_box_pack_start (GTK_BOX (inbox), gui->vscrollbar, FALSE, TRUE, 0);

	gtk_drag_dest_set (gui->vscrollbar, 5, dnd_dest_targets, 2,
							 GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
	g_signal_connect (G_OBJECT (gui->vscrollbar), "drag_begin",
							G_CALLBACK (mg_drag_begin_cb), NULL);
	g_signal_connect (G_OBJECT (gui->vscrollbar), "drag_drop",
							G_CALLBACK (mg_drag_drop_cb), NULL);
	g_signal_connect (G_OBJECT (gui->vscrollbar), "drag_motion",
							G_CALLBACK (mg_drag_motion_cb), gui->vscrollbar);
	g_signal_connect (G_OBJECT (gui->vscrollbar), "drag_end",
							G_CALLBACK (mg_drag_end_cb), NULL);

	gtk_drag_dest_set (gui->xtext, GTK_DEST_DEFAULT_ALL, dnd_targets, 1,
							 GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
	g_signal_connect (G_OBJECT (gui->xtext), "drag_data_received",
							G_CALLBACK (mg_dialog_dnd_drop), NULL);
}

static GtkWidget *
mg_create_infoframe (GtkWidget *box)
{
	GtkWidget *frame, *label, *hbox;

	frame = gtk_frame_new (0);
	gtk_frame_set_shadow_type ((GtkFrame*)frame, GTK_SHADOW_OUT);
	gtk_container_add (GTK_CONTAINER (box), frame);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous (GTK_BOX (hbox), FALSE);
	gtk_container_add (GTK_CONTAINER (frame), hbox);

	label = gtk_label_new (NULL);
	gtk_container_add (GTK_CONTAINER (hbox), label);

	return label;
}

static void
mg_create_meters (session_gui *gui, GtkWidget *parent_box)
{
	GtkWidget *infbox, *wid, *box;

	gui->meter_box = infbox = box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
	gtk_box_set_homogeneous (GTK_BOX (box), FALSE);
	gtk_box_pack_start (GTK_BOX (parent_box), box, 0, 0, 0);

	if ((prefs.hex_gui_lagometer & 2) || (prefs.hex_gui_throttlemeter & 2))
	{
		infbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_box_set_homogeneous (GTK_BOX (infbox), FALSE);
		gtk_box_pack_start (GTK_BOX (box), infbox, 0, 0, 0);
	}

	if (prefs.hex_gui_lagometer & 1)
	{
		gui->lagometer = wid = gtk_progress_bar_new ();
#ifdef WIN32
		gtk_widget_set_size_request (wid, 1, 10);
#else
		gtk_widget_set_size_request (wid, 1, 8);
#endif

		wid = gtk_event_box_new ();
		gtk_container_add (GTK_CONTAINER (wid), gui->lagometer);
		gtk_box_pack_start (GTK_BOX (box), wid, 0, 0, 0);
	}
	if (prefs.hex_gui_lagometer & 2)
	{
		wid = mg_create_infoframe (infbox);
		gui->laginfo = wid;
		gtk_label_set_text ((GtkLabel *) wid, "Lag");
	}

	if (prefs.hex_gui_throttlemeter & 1)
	{
		gui->throttlemeter = wid = gtk_progress_bar_new ();
#ifdef WIN32
		gtk_widget_set_size_request (wid, 1, 10);
#else
		gtk_widget_set_size_request (wid, 1, 8);
#endif

		wid = gtk_event_box_new ();
		gtk_container_add (GTK_CONTAINER (wid), gui->throttlemeter);
		gtk_box_pack_start (GTK_BOX (box), wid, 0, 0, 0);
	}
	if (prefs.hex_gui_throttlemeter & 2)
	{
		wid = mg_create_infoframe (infbox);
		gui->throttleinfo = wid;
		gtk_label_set_text ((GtkLabel *) wid, "Throttle");
	}
}

void
mg_update_meters (session_gui *gui)
{
	gtk_widget_destroy (gui->meter_box);
	gui->lagometer = NULL;
	gui->laginfo = NULL;
	gui->throttlemeter = NULL;
	gui->throttleinfo = NULL;

	mg_create_meters (gui, gui->button_box_parent);
	gtk_widget_show_all (gui->meter_box);
}

static gboolean
mg_tabwin_focus_cb (GtkWindow * win, GdkEventFocus *event, gpointer userdata)
{
	current_sess = current_tab;
	if (current_sess)
	{
		gtk_xtext_check_marker_visibility (GTK_XTEXT (current_sess->gui->xtext));
		plugin_emit_dummy_print (current_sess, "Focus Window");
	}
	unflash_window (GTK_WIDGET (win));
	return FALSE;
}

static gboolean
mg_topwin_focus_cb (GtkWindow * win, GdkEventFocus *event, session *sess)
{
	current_sess = sess;
	if (!sess->server->server_session)
		sess->server->server_session = sess;
	gtk_xtext_check_marker_visibility(GTK_XTEXT (current_sess->gui->xtext));
	unflash_window (GTK_WIDGET (win));
	plugin_emit_dummy_print (sess, "Focus Window");
	return FALSE;
}

static void
mg_create_menu (session_gui *gui, GtkWidget *table, int away_state)
{
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new ();
	gtk_window_add_accel_group (GTK_WINDOW (gtk_widget_get_toplevel (table)),
										 accel_group);
	g_object_unref (accel_group);

	gui->menu = menu_create_main (accel_group, TRUE, away_state, !gui->is_tab,
											gui->menu_item);
	gtk_grid_attach (GTK_GRID (table), gui->menu, 0, 0, 3, 1);
	gtk_widget_set_hexpand (gui->menu, TRUE);
	gtk_widget_set_halign (gui->menu, GTK_ALIGN_FILL);
	gtk_widget_set_valign (gui->menu, GTK_ALIGN_START);
}

static void
mg_create_irctab (session *sess, GtkWidget *table)
{
	GtkWidget *vbox;
	session_gui *gui = sess->gui;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_set_homogeneous (GTK_BOX (vbox), FALSE);
	gtk_grid_attach (GTK_GRID (table), vbox, 1, 2, 1, 1);
	gtk_widget_set_hexpand (vbox, TRUE);
	gtk_widget_set_vexpand (vbox, TRUE);
	gtk_widget_set_halign (vbox, GTK_ALIGN_FILL);
	gtk_widget_set_valign (vbox, GTK_ALIGN_FILL);
	mg_create_center (sess, gui, vbox);
}

static void
mg_create_topwindow (session *sess)
{
	GtkWidget *win;
	GtkWidget *table;

	if (sess->type == SESS_DIALOG)
		win = gtkutil_window_new ("HexChat", NULL,
										  prefs.hex_gui_dialog_width, prefs.hex_gui_dialog_height, 0);
	else
		win = gtkutil_window_new ("HexChat", NULL,
										  prefs.hex_gui_win_width,
										  prefs.hex_gui_win_height, 0);
	sess->gui->window = win;
	gtk_container_set_border_width (GTK_CONTAINER (win), GUI_BORDER);
	gtk_widget_set_opacity (win, (prefs.hex_gui_transparency / 255.));

	g_signal_connect (G_OBJECT (win), "focus_in_event",
							G_CALLBACK (mg_topwin_focus_cb), sess);
	g_signal_connect (G_OBJECT (win), "destroy",
							G_CALLBACK (mg_topdestroy_cb), sess);
	g_signal_connect (G_OBJECT (win), "configure_event",
							G_CALLBACK (mg_configure_cb), sess);

	palette_alloc (win);

	table = gtk_grid_new ();
	/* spacing under the menubar */
	gtk_grid_set_row_spacing (GTK_GRID (table), GUI_SPACING);
	/* left and right borders */
	gtk_grid_set_column_spacing (GTK_GRID (table), 1);
	gtk_container_add (GTK_CONTAINER (win), table);

	mg_create_irctab (sess, table);
	mg_create_menu (sess->gui, table, sess->server->is_away);

	if (sess->res->buffer == NULL)
	{
		sess->res->buffer = gtk_xtext_buffer_new (GTK_XTEXT (sess->gui->xtext));
		gtk_xtext_buffer_show (GTK_XTEXT (sess->gui->xtext), sess->res->buffer, TRUE);
		gtk_xtext_set_time_stamp (sess->res->buffer, prefs.hex_stamp_text);
		sess->res->user_model = userlist_create_model (sess);
	}

	userlist_show (sess);

	gtk_widget_show_all (table);

	if (prefs.hex_gui_hide_menu)
		gtk_widget_hide (sess->gui->menu);

	/* Will be shown when needed */
	gtk_widget_hide (sess->gui->topic_bar);

	if (!prefs.hex_gui_ulist_buttons)
		gtk_widget_hide (sess->gui->button_box);

	if (!prefs.hex_gui_input_nick)
		gtk_widget_hide (sess->gui->nick_box);

	gtk_widget_hide(sess->gui->shbox);

	mg_decide_userlist (sess, FALSE);

	if (sess->type == SESS_DIALOG)
	{
		/* hide the chan-mode buttons */
		gtk_widget_hide (sess->gui->topicbutton_box);
	} else
	{
		gtk_widget_hide (sess->gui->dialogbutton_box);

		if (!prefs.hex_gui_mode_buttons)
			gtk_widget_hide (sess->gui->topicbutton_box);
	}

	mg_place_userlist_and_chanview (sess->gui);

	gtk_widget_show (win);
}

static gboolean
mg_tabwindow_de_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	GSList *list;
	session *sess;
	GtkWindow *win = GTK_WINDOW(gtk_widget_get_toplevel (widget));

	if (prefs.hex_gui_tray_close && gtkutil_tray_icon_supported (win) && tray_toggle_visibility (FALSE))
		return TRUE;

	/* check for remaining toplevel windows */
	list = sess_list;
	while (list)
	{
		sess = list->data;
		if (!sess->gui->is_tab)
			return FALSE;
		list = list->next;
	}

	mg_open_quit_dialog (TRUE);
	return TRUE;
}

#ifdef G_OS_WIN32
static GdkFilterReturn
mg_time_change (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	MSG *msg = (MSG*)xevent;

	if (msg->message == WM_TIMECHANGE)
	{
		_tzset();
	}

	return GDK_FILTER_CONTINUE;
}
#endif

static void
mg_create_tabwindow (session *sess)
{
	GtkWidget *win;
	GtkWidget *table;
#ifdef G_OS_WIN32
	GdkWindow *parent_win;
#endif

	win = gtkutil_window_new ("HexChat", NULL, prefs.hex_gui_win_width,
									  prefs.hex_gui_win_height, 0);
	sess->gui->window = win;
	gtk_window_move (GTK_WINDOW (win), prefs.hex_gui_win_left,
						  prefs.hex_gui_win_top);
	if (prefs.hex_gui_win_state)
		gtk_window_maximize (GTK_WINDOW (win));
	if (prefs.hex_gui_win_fullscreen)
		gtk_window_fullscreen (GTK_WINDOW (win));
	gtk_widget_set_opacity (win, (prefs.hex_gui_transparency / 255.));
	gtk_container_set_border_width (GTK_CONTAINER (win), GUI_BORDER);

	g_signal_connect (G_OBJECT (win), "delete_event",
						   G_CALLBACK (mg_tabwindow_de_cb), 0);
	g_signal_connect (G_OBJECT (win), "destroy",
							G_CALLBACK (mg_tabwindow_kill_cb), 0);
	g_signal_connect (G_OBJECT (win), "focus_in_event",
							G_CALLBACK (mg_tabwin_focus_cb), NULL);
	g_signal_connect (G_OBJECT (win), "configure_event",
							G_CALLBACK (mg_configure_cb), NULL);
	g_signal_connect (G_OBJECT (win), "window_state_event",
							G_CALLBACK (mg_windowstate_cb), NULL);

	palette_alloc (win);

	sess->gui->main_table = table = gtk_grid_new ();
gtk_grid_set_row_spacing (GTK_GRID (table), 2);
gtk_grid_set_column_spacing (GTK_GRID (table), 2);
	/* spacing under the menubar */
	gtk_table_set_row_spacing (GTK_TABLE (table), 0, GUI_SPACING);
	/* left and right borders */
	gtk_table_set_col_spacing (GTK_TABLE (table), 0, 1);
	gtk_table_set_col_spacing (GTK_TABLE (table), 1, 1);
	gtk_container_add (GTK_CONTAINER (win), table);

	mg_create_irctab (sess, table);
	mg_create_tabs (sess->gui);
	mg_create_menu (sess->gui, table, sess->server->is_away);

	mg_focus (sess);

	gtk_widget_show_all (table);

	if (prefs.hex_gui_hide_menu)
		gtk_widget_hide (sess->gui->menu);

	mg_decide_userlist (sess, FALSE);

	/* Will be shown when needed */
	gtk_widget_hide (sess->gui->topic_bar);

	if (!prefs.hex_gui_mode_buttons)
		gtk_widget_hide (sess->gui->topicbutton_box);

	if (!prefs.hex_gui_ulist_buttons)
		gtk_widget_hide (sess->gui->button_box);

	if (!prefs.hex_gui_input_nick)
		gtk_widget_hide (sess->gui->nick_box);

	gtk_widget_hide (sess->gui->shbox);

	mg_place_userlist_and_chanview (sess->gui);

	gtk_widget_show (win);

#ifdef G_OS_WIN32
	parent_win = gtk_widget_get_window (win);
	gdk_window_add_filter (parent_win, mg_time_change, NULL);
#endif
}

void
mg_apply_setup (void)
{
	GSList *list = sess_list;
	session *sess;
	int done_main = FALSE;

	mg_create_tab_colors ();

	while (list)
	{
		sess = list->data;
		gtk_xtext_set_time_stamp (sess->res->buffer, prefs.hex_stamp_text);
		((xtext_buffer *)sess->res->buffer)->needs_recalc = TRUE;
		if (!sess->gui->is_tab || !done_main)
			mg_place_userlist_and_chanview (sess->gui);
		if (sess->gui->is_tab)
			done_main = TRUE;
		list = list->next;
	}
}

static chan *
mg_add_generic_tab (char *name, char *title, void *family, GtkWidget *box)
{
	chan *ch;

	gtk_notebook_append_page (GTK_NOTEBOOK (mg_gui->note_book), box, NULL);
	gtk_widget_show (box);

	ch = chanview_add (mg_gui->chanview, name, NULL, box, TRUE, TAG_UTIL, pix_tree_util);
	chan_set_color (ch, plain_list);

	g_object_set_data_full (G_OBJECT (box), "title", g_strdup (title), g_free);
	g_object_set_data (G_OBJECT (box), "ch", ch);

	if (prefs.hex_gui_tab_newtofront)
		chan_focus (ch);

	return ch;
}

void
fe_buttons_update (session *sess)
{
	session_gui *gui = sess->gui;

	gtk_widget_destroy (gui->button_box);
	gui->button_box = mg_create_userlistbuttons (gui->button_box_parent);

	if (prefs.hex_gui_ulist_buttons)
		gtk_widget_show (sess->gui->button_box);
	else
		gtk_widget_hide (sess->gui->button_box);
}

void
fe_clear_channel (session *sess)
{
	char tbuf[CHANLEN+6];
	session_gui *gui = sess->gui;

	if (sess->gui->is_tab)
	{
		if (sess->waitchannel[0])
		{
			if (prefs.hex_gui_tab_trunc > 2 && g_utf8_strlen (sess->waitchannel, -1) > prefs.hex_gui_tab_trunc)
			{
				/* truncate long channel names */
				tbuf[0] = '(';
				strcpy (tbuf + 1, sess->waitchannel);
				g_utf8_offset_to_pointer(tbuf, prefs.hex_gui_tab_trunc)[0] = 0;
				strcat (tbuf, "..)");
			} else
			{
				sprintf (tbuf, "(%s)", sess->waitchannel);
			}
		}
		else
			strcpy (tbuf, _("<none>"));
		chan_rename (sess->res->tab, tbuf, prefs.hex_gui_tab_trunc);
	}

	if (!sess->gui->is_tab || sess == current_tab)
	{
		gtk_entry_set_text (GTK_ENTRY (gui->topic_entry), "");

		if (gui->op_xpm)
		{
			gtk_widget_destroy (gui->op_xpm);
			gui->op_xpm = 0;
		}
	} else
	{
		if (sess->res->topic_text)
		{
			g_free (sess->res->topic_text);
			sess->res->topic_text = NULL;
		}
	}
}

void
fe_set_nonchannel (session *sess, int state)
{
}

void
fe_dlgbuttons_update (session *sess)
{
	GtkWidget *box;
	session_gui *gui = sess->gui;

	gtk_widget_destroy (gui->dialogbutton_box);

	gui->dialogbutton_box = box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous (GTK_BOX (box), FALSE);
	gtk_box_pack_start (GTK_BOX (gui->topic_bar), box, 0, 0, 0);
	gtk_box_reorder_child (GTK_BOX (gui->topic_bar), box, 3);
	mg_create_dialogbuttons (box);

	gtk_widget_show_all (box);

	if (current_tab && current_tab->type != SESS_DIALOG)
		gtk_widget_hide (current_tab->gui->dialogbutton_box);
}

void
fe_update_mode_buttons (session *sess, char mode, char sign)
{
	int state, i;

	if (sign == '+')
		state = TRUE;
	else
		state = FALSE;

	for (i = 0; i < NUM_FLAG_WIDS - 1; i++)
	{
		if (chan_flags[i] == mode)
		{
			if (!sess->gui->is_tab || sess == current_tab)
			{
				ignore_chanmode = TRUE;
				if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sess->gui->flag_wid[i])) != state)
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (sess->gui->flag_wid[i]), state);
				ignore_chanmode = FALSE;
			} else
			{
				sess->res->flag_wid_state[i] = state;
			}
			return;
		}
	}
}

void
fe_set_nick (server *serv, char *newnick)
{
	GSList *list;
	session *sess;

	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
		{
			if (current_tab == sess || !sess->gui->is_tab)
				gtk_button_set_label (GTK_BUTTON (sess->gui->nick_label), newnick);
		}
		list = list->next;
	}
}

void
fe_set_away (server *serv)
{
	GSList *list;
	session *sess;

	while (list)
	{
		sess = list->data;
		if (sess->server == serv)
		{
			if (!sess->gui->is_tab || sess == current_tab)
			{
				menu_set_away (sess->gui, serv->is_away);
				/* gray out my nickname */
				mg_set_myself_away (sess->gui, serv->is_away);
			}
		}
		list = list->next;
	}
}

void
fe_set_channel (session *sess)
{
	if (sess->res->tab != NULL)
		chan_rename (sess->res->tab, sess->channel, prefs.hex_gui_tab_trunc);
}

void
mg_move_tab (session *sess, int delta)
{
	if (sess->gui->is_tab)
		chan_move (sess->res->tab, delta);
}

void
mg_move_tab_family (session *sess, int delta)
{
	if (sess->gui->is_tab)
		chan_move_family (sess->res->tab, delta);
}

void
mg_set_title (GtkWidget *vbox, char *title) /* for non-irc tab/window only */
{
	char *old;

	old = g_object_get_data (G_OBJECT (vbox), "title");
	if (old)
	{
		g_object_set_data_full (G_OBJECT (vbox), "title", g_strdup (title), g_free);
	} else
	{
		gtk_window_set_title (GTK_WINDOW (vbox), title);
	}
}

void
fe_server_callback (server *serv)
{
	joind_close (serv);

	if (serv->gui->chanlist_window)
		mg_close_gen (NULL, serv->gui->chanlist_window);

	if (serv->gui->rawlog_window)
		mg_close_gen (NULL, serv->gui->rawlog_window);

	g_free (serv->gui);
}

/* called when a session is being killed */

void
fe_session_callback (session *sess)
{
	gtk_xtext_buffer_free (sess->res->buffer);
	g_object_unref (G_OBJECT (sess->res->user_model));

	if (sess->res->banlist && sess->res->banlist->window)
		mg_close_gen (NULL, sess->res->banlist->window);

	g_free (sess->res->input_text);
	g_free (sess->res->topic_text);
	g_free (sess->res->limit_text);
	g_free (sess->res->key_text);
	g_free (sess->res->queue_text);
	g_free (sess->res->queue_tip);
	g_free (sess->res->lag_text);
	g_free (sess->res->lag_tip);

	if (sess->gui->bartag)
		fe_timeout_remove (sess->gui->bartag);

	if (sess->gui != &static_mg_gui)
		g_free (sess->gui);
	g_free (sess->res);
}

/* ===== DRAG AND DROP STUFF ===== */

static gboolean
is_child_of (GtkWidget *widget, GtkWidget *parent)
{
	while (widget)
	{
		if (gtk_widget_get_parent (widget) == parent)
			return TRUE;
		widget = gtk_widget_get_parent (widget);
	}
	return FALSE;
}

static void
mg_handle_drop (GtkWidget *widget, int y, int *pos, int *other_pos)
{
	int height;
	session_gui *gui = current_sess->gui;

	height = gdk_window_get_height (gtk_widget_get_window (widget));

	if (y < height / 2)
	{
		if (is_child_of (widget, gui->vpane_left))
			*pos = 1;	/* top left */
		else
			*pos = 3;	/* top right */
	}
	else
	{
		if (is_child_of (widget, gui->vpane_left))
			*pos = 2;	/* bottom left */
		else
			*pos = 4;	/* bottom right */
	}

	/* both in the same pos? must move one */
	if (*pos == *other_pos)
	{
		switch (*other_pos)
		{
		case 1:
			*other_pos = 2;
			break;
		case 2:
			*other_pos = 1;
			break;
		case 3:
			*other_pos = 4;
			break;
		case 4:
			*other_pos = 3;
			break;
		}
	}

	mg_place_userlist_and_chanview (gui);
}

static gboolean
mg_is_gui_target (GdkDragContext *context)
{
	char *target_name;

	if (!context || !gdk_drag_context_list_targets (context) || !gdk_drag_context_list_targets (context)->data)
		return FALSE;

	target_name = gdk_atom_name (gdk_drag_context_list_targets (context)->data);
	if (target_name)
	{
		/* if it's not HEXCHAT_CHANVIEW or HEXCHAT_USERLIST */
		/* we should ignore it. */
		if (target_name[0] != 'H')
		{
			g_free (target_name);
			return FALSE;
		}
		g_free (target_name);
	}

	return TRUE;
}

/* this begin callback just creates an nice of the source */

gboolean
mg_drag_begin_cb (GtkWidget *widget, GdkDragContext *context, gpointer userdata)
{
	int width, height;
	cairo_surface_t *surface;
	GdkPixbuf *pix, *pix2;
	GdkWindow *window;

	/* ignore file drops */
	if (!mg_is_gui_target (context))
		return FALSE;

	window = gtk_widget_get_window (widget);
	if (!window) {
		return FALSE;
	}

	width = gdk_window_get_width (window);
	height = gdk_window_get_height (window);

	/* In GTK3, we use gdk_pixbuf_get_from_window instead of gdk_pixbuf_get_from_drawable */
	pix = gdk_pixbuf_get_from_window (window, 0, 0, width, height);
	pix2 = gdk_pixbuf_scale_simple (pix, width * 4 / 5, height / 2, GDK_INTERP_HYPER);
	g_object_unref (pix);

	gtk_drag_set_icon_pixbuf (context, pix2, 0, 0);
	g_object_set_data (G_OBJECT (widget), "ico", pix2);

	return TRUE;
}

void
mg_drag_end_cb (GtkWidget *widget, GdkDragContext *context, gpointer userdata)
{
	/* ignore file drops */
	if (!mg_is_gui_target (context))
		return;

	g_object_unref (g_object_get_data (G_OBJECT (widget), "ico"));
}

/* drop complete */

gboolean
mg_drag_drop_cb (GtkWidget *widget, GdkDragContext *context, int x, int y, guint time, gpointer user_data)
{
	/* ignore file drops */
	if (!mg_is_gui_target (context))
		return FALSE;

	switch (gdk_drag_context_get_selected_action (context))
	{
	case GDK_ACTION_MOVE:
		/* from userlist */
		mg_handle_drop (widget, y, &prefs.hex_gui_ulist_pos, &prefs.hex_gui_tab_pos);
		break;
	case GDK_ACTION_COPY:
		/* from tree - we use GDK_ACTION_COPY for the tree */
		mg_handle_drop (widget, y, &prefs.hex_gui_tab_pos, &prefs.hex_gui_ulist_pos);
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

/* draw highlight rectangle in the destination */

gboolean
mg_drag_motion_cb (GtkWidget *widget, GdkDragContext *context, int x, int y, guint time, gpointer scbar)
{
	cairo_t *cr;
	GdkRGBA col = { 0, 0, 0, 0.5 };
	GdkWindow *draw;
	GtkAllocation allocation;
	int width, height;

	/* ignore file drops */
	if (!mg_is_gui_target (context))
		return FALSE;

	if (scbar)	/* scrollbar */
	{
		gtk_widget_get_allocation (widget, &allocation);
		draw = gtk_widget_get_window (widget);
	}
	else
	{
		allocation.x = allocation.y = 0;
		draw = gtk_widget_get_window (widget);
	}

	if (!draw)
		return FALSE;

	/* Get the window dimensions */
	width = gdk_window_get_width (draw);
	height = gdk_window_get_height (draw);

	/* Create a cairo context for drawing */
	cr = gdk_cairo_create (draw);
	if (!cr)
		return FALSE;

	/* Set the color and line width */
	gdk_cairo_set_source_rgba (cr, &col);
	cairo_set_line_width (cr, 2.0);

	/* Draw a rectangle around the window */
	cairo_rectangle (cr, 1, 1, width - 2, height - 2);
	cairo_stroke (cr);

	/* Determine which half of the widget we're in */
	int half = height / 2;
	int ox = allocation.x;
	int oy = allocation.y;

	/* Queue a redraw of the entire widget */
	gtk_widget_queue_draw (widget);

	/* Create a new cairo context for the highlight */
	cr = gdk_cairo_create (draw);
	if (!cr)
		return FALSE;

	/* Set the color and line width for the highlight */
	gdk_cairo_set_source_rgba (cr, &col);
	cairo_set_line_width (cr, 2.0);

	if (y < half)
	{
		/* Highlight the top half */
		cairo_rectangle (cr, 1 + ox, 1 + oy, width - 2, half - 1);
	}
	else
	{
		/* Highlight the bottom half */
		cairo_rectangle (cr, 1 + ox, oy + half, width - 2, height - half - 1);
	}

	/* Draw the highlight */
	cairo_stroke (cr);

	/* Clean up */
	cairo_destroy (cr);

	return TRUE;
}
