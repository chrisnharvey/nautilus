/* nautilus-batch-rename.c
 *
 * Copyright (C) 2016 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nautilus-batch-rename.h"
#include "nautilus-file.h"
#include "nautilus-error-reporting.h"
#include "nautilus-batch-rename-utilities.h"

#include <glib/gprintf.h>
#include <glib.h>
#include <string.h>

#define ADD_TEXT_ENTRY_SIZE 550
#define REPLACE_ENTRY_SIZE  275
#define DIALOG_TITLE_LEN 25
#define TAG_UNAVAILABLE -2

struct _NautilusBatchRename
{
        GtkDialog                parent;

        GtkWidget               *grid;

        GtkWidget               *cancel_button;
        GtkWidget               *conflict_listbox;
        GtkWidget               *name_entry;
        GtkWidget               *rename_button;
        GtkWidget               *find_entry;
        GtkWidget               *mode_stack;
        GtkWidget               *replace_entry;
        GtkWidget               *format_mode_button;
        GtkWidget               *replace_mode_button;
        GtkWidget               *add_button;
        GtkWidget               *add_popover;
        GtkWidget               *numbering_order_label;
        GtkWidget               *numbering_label;
        GtkWidget               *scrolled_window;
        GtkWidget               *numbering_order_popover;
        GtkWidget               *numbering_order_button;
        GtkWidget               *conflict_box;
        GtkWidget               *conflict_label;
        GtkWidget               *conflict_down;
        GtkWidget               *conflict_up;

        GList                   *listbox_rows;
        GList                   *listbox_labels_new;
        GList                   *listbox_labels_old;

        GList                   *selection;
        GList                   *new_names;
        NautilusBatchRenameMode  mode;
        NautilusDirectory       *model;

        GActionGroup            *action_group;

        GMenu                   *numbering_order_menu;
        GMenu                   *add_tag_menu;

        GHashTable              *create_date;
        GList                   *selection_metadata;

        /* check if all files in selection have the same parent */
        gboolean                 same_parent;
        /* the number of the currently selected conflict */
        gint                     selected_conflict;
        /* total conflicts number */
        gint                     conflicts_number;

        gint                     checked_parents;
        GList                   *duplicates;
        GList                   *distinct_parents;
        GTask                   *task;
        GCancellable            *cancellable;
        gboolean                 checking_conflicts;

        GtkSizeGroup            *size_group1;
        GtkSizeGroup            *size_group2;

        /* starting tag position, -1 if tag is missing and
         * -2 if tag can't be added at all */
        gint                     original_name_tag;
        gint                     numbering_tag;
        gint                     creation_date_tag;
        gint                     equipment_tag;
        gint                     season_tag;
        gint                     episode_nr_tag;
        gint                     track_nr_tag;
        gint                     artist_name_tag;
};

static void     file_names_widget_entry_on_changed      (NautilusBatchRename    *dialog);

G_DEFINE_TYPE (NautilusBatchRename, nautilus_batch_rename, GTK_TYPE_DIALOG);

static void
numbering_order_changed (GSimpleAction       *action,
                         GVariant            *value,
                         gpointer             user_data)
{
        NautilusBatchRename *dialog;
        const gchar *target_name;

        dialog = NAUTILUS_BATCH_RENAME (user_data);

        target_name = g_variant_get_string (value, NULL);

        if (g_strcmp0 (target_name, "name-ascending") == 0) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                     "Original name (Ascending)");
                dialog->selection = nautilus_batch_rename_sort (dialog->selection,
                                                                ORIGINAL_ASCENDING,
                                                                NULL);
        }

        if (g_strcmp0 (target_name, "name-descending") == 0) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                     "Original name (Descending)");
                dialog->selection = nautilus_batch_rename_sort (dialog->selection,
                                                                ORIGINAL_DESCENDING,
                                                                NULL);
        }

        if (g_strcmp0 (target_name, "first-modified") == 0) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                     "First Modified");
                dialog->selection = nautilus_batch_rename_sort (dialog->selection,
                                                                FIRST_MODIFIED,
                                                                NULL);
        }

        if (g_strcmp0 (target_name, "last-modified") == 0) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                     "Last Modified");
                dialog->selection = nautilus_batch_rename_sort (dialog->selection,
                                                                LAST_MODIFIED,
                                                                NULL);
        }

        if (g_strcmp0 (target_name, "first-created") == 0) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                     "First Created");
                dialog->selection = nautilus_batch_rename_sort (dialog->selection,
                                                                FIRST_CREATED,
                                                                dialog->create_date);
        }

        if (g_strcmp0 (target_name, "last-created") == 0) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_order_label),
                                     "Last Created");
                dialog->selection = nautilus_batch_rename_sort (dialog->selection,
                                                                LAST_CREATED,
                                                                dialog->create_date);
        }

        g_simple_action_set_state (action, value);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->numbering_order_button), FALSE);

        /* update display text */
        file_names_widget_entry_on_changed (dialog);
}

static void
add_original_file_name_tag (GSimpleAction       *action,
                            GVariant            *value,
                            gpointer             user_data)
{
        NautilusBatchRename *dialog;
        gint *cursor_pos;

        dialog = NAUTILUS_BATCH_RENAME (user_data);
        cursor_pos = g_malloc (sizeof (int));

        g_object_get (dialog->name_entry, "cursor-position", cursor_pos, NULL);

        gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                  "[Original file name]",
                                  strlen ("[Original file name]"),
                                  cursor_pos);
        *cursor_pos += strlen ("[Original file name]");

        gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->name_entry));

        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

        g_free (cursor_pos);
}

static void
disable_action (NautilusBatchRename *dialog,
                gchar               *action_name)
{
        GAction *action;

        action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                             action_name);
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}

static void
add_metadata_tag (GSimpleAction       *action,
                  GVariant            *value,
                  gpointer             user_data)
{
        NautilusBatchRename *dialog;
        gchar *action_name;
        gint *cursor_pos;

        dialog = NAUTILUS_BATCH_RENAME (user_data);
        action_name = g_malloc (strlen ("add-numbering-tag-zero"));
        cursor_pos = g_malloc (sizeof (int));

        g_object_get (action, "name", &action_name, NULL);
        g_object_get (dialog->name_entry, "cursor-position", cursor_pos, NULL);

        if (g_strrstr (action_name, "creation-date")) {
                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          "[Date taken]",
                                          strlen ("[Date taken]"),
                                          cursor_pos);
                disable_action (dialog, "add-creation-date-tag");
        }

        if (g_strrstr (action_name, "equipment")) {
                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          "[Camera model]",
                                          strlen ("[Camera model]"),
                                          cursor_pos);
                disable_action (dialog, "add-equipment-tag");
        }

        if (g_strrstr (action_name, "season")) {
                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          "[Season nr]",
                                          strlen ("[Season nr]"),
                                          cursor_pos);
                disable_action (dialog, "add-season-tag");
        }

        if (g_strrstr (action_name, "episode")) {
                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          "[Episode nr]",
                                          strlen ("[Episode nr]"),
                                          cursor_pos);
                disable_action (dialog, "add-episode-tag");
        }

        if (g_strrstr (action_name, "track")) {
                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          "[Track nr]",
                                          strlen ("[Track nr]"),
                                          cursor_pos);
                disable_action (dialog, "add-track-number-tag");
        }

        if (g_strrstr (action_name, "artist")) {
                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          "[Artist name]",
                                          strlen ("[Artist name]"),
                                          cursor_pos);
                disable_action (dialog, "add-artist-name-tag");
        }

        gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->name_entry));

        g_free (action_name);
}

static void
add_numbering_tag (GSimpleAction       *action,
                   GVariant            *value,
                   gpointer             user_data)
{
        NautilusBatchRename *dialog;
        gchar *action_name;
        gint *cursor_pos;
        GAction *add_numbering_action;

        dialog = NAUTILUS_BATCH_RENAME (user_data);
        action_name = g_malloc (strlen ("add-numbering-tag-zero"));
        cursor_pos = g_malloc (sizeof (int));

        g_object_get (action, "name", &action_name, NULL);
        g_object_get (dialog->name_entry, "cursor-position", cursor_pos, NULL);

        if (g_strrstr (action_name, "zero"))
                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          "[1, 2, 3]",
                                          strlen ("[1, 2, 3]"),
                                          cursor_pos);

        if (g_strrstr (action_name, "one"))
                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          "[01, 02, 03]",
                                          strlen ("[01, 02, 03]"),
                                          cursor_pos);

        if (g_strrstr (action_name, "two"))
                gtk_editable_insert_text (GTK_EDITABLE (dialog->name_entry),
                                          "[001, 002, 003]",
                                          strlen ("[001, 002, 003]"),
                                          cursor_pos);

        add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                              "add-numbering-tag-zero");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), FALSE);
        add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                              "add-numbering-tag-one");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), FALSE);

        add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                              "add-numbering-tag-two");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), FALSE);

        gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->name_entry));

        g_free (action_name);
}

const GActionEntry dialog_entries[] = {
        { "numbering-order-changed", NULL, "s", "'name-ascending'",  numbering_order_changed },
        { "add-original-file-name-tag", add_original_file_name_tag },
        { "add-numbering-tag-zero", add_numbering_tag },
        { "add-numbering-tag-one", add_numbering_tag },
        { "add-numbering-tag-two", add_numbering_tag },
        { "add-creation-date-tag", add_metadata_tag },
        { "add-equipment-tag", add_metadata_tag },
        { "add-season-tag", add_metadata_tag },
        { "add-episode-tag", add_metadata_tag },
        { "add-video-album-tag", add_metadata_tag },
        { "add-track-number-tag", add_metadata_tag },
        { "add-artist-name-tag", add_metadata_tag },
        { "add-album-title-tag", add_metadata_tag },

};

gint compare_int (gconstpointer a,
                  gconstpointer b)
{
      return *(int*)a - *(int*)b;
}

static GList*
split_entry_text (NautilusBatchRename *dialog,
                  gchar               *entry_text)
{
        GString *string, *tag;
        GArray *tag_positions;
        gint tags, i, j;
        GList *result = NULL;

        tags = 0;
        j = 0;
        tag_positions = g_array_new (FALSE, FALSE, sizeof (gint));

        if (dialog->numbering_tag >= 0) {
                g_array_append_val (tag_positions, dialog->numbering_tag);
                tags++;
        }

        if (dialog->original_name_tag >= 0) {
                g_array_append_val (tag_positions, dialog->original_name_tag);
                tags++;
        }

        if (dialog->creation_date_tag >= 0) {
                g_array_append_val (tag_positions, dialog->creation_date_tag);
                tags++;
        }

        if (dialog->equipment_tag >= 0) {
                g_array_append_val (tag_positions, dialog->equipment_tag);
                tags++;
        }

        if (dialog->season_tag >= 0) {
                g_array_append_val (tag_positions, dialog->season_tag);
                tags++;
        }

        if (dialog->episode_nr_tag >= 0) {
                g_array_append_val (tag_positions, dialog->episode_nr_tag);
                tags++;
        }

        if (dialog->track_nr_tag >= 0) {
                g_array_append_val (tag_positions, dialog->track_nr_tag);
                tags++;
        }

        if (dialog->artist_name_tag >= 0) {
                g_array_append_val (tag_positions, dialog->artist_name_tag);
                tags++;
        }

        g_array_sort (tag_positions, compare_int);

        for (i = 0; i < tags; i++) {
                tag = g_string_new ("");

                tag = g_string_append_len (tag,
                                           entry_text + g_array_index (tag_positions, gint, i),
                                           3);

                string = g_string_new ("");

                string = g_string_append_len (string,
                                              entry_text + j,
                                              g_array_index (tag_positions, gint, i) - j);

                if (g_strcmp0 (string->str, ""))
                        result = g_list_append (result, string);

                if (g_strcmp0 (tag->str, "[Or") == 0) {
                        j = g_array_index (tag_positions, gint, i) + strlen ("[Original file name]");
                        tag = g_string_append (tag, "iginal file name]");
                }
                if (g_strcmp0 (tag->str, "[1,") == 0) {
                        j = g_array_index (tag_positions, gint, i) + strlen ("[1, 2, 3]");
                        tag = g_string_append (tag, " 2, 3]");
                }
                if (g_strcmp0 (tag->str, "[01") == 0) {
                        j = g_array_index (tag_positions, gint, i) + strlen ("[01, 02, 03]");
                        tag = g_string_append (tag, ", 02, 03]");
                }
                if (g_strcmp0 (tag->str, "[00") == 0) {
                        j = g_array_index (tag_positions, gint, i) + strlen ("[001, 002, 003]");
                        tag = g_string_append (tag, "1, 002, 003]");
                }
                if (g_strcmp0 (tag->str, "[Da") == 0) {
                        j = g_array_index (tag_positions, gint, i) + strlen ("[Date taken]");
                        tag = g_string_append (tag, "te taken]");
                }
                if (g_strcmp0 (tag->str, "[Ca") == 0) {
                        j = g_array_index (tag_positions, gint, i) + strlen ("[Camera model]");
                        tag = g_string_append (tag, "mera model]");
                }
                if (g_strcmp0 (tag->str, "[Se") == 0) {
                        j = g_array_index (tag_positions, gint, i) + strlen ("[Season nr]");
                        tag = g_string_append (tag, "ason nr]");
                }
                if (g_strcmp0 (tag->str, "[Ep") == 0) {
                        j = g_array_index (tag_positions, gint, i) + strlen ("[Episode nr]");
                        tag = g_string_append (tag, "isode nr]");
                }
                if (g_strcmp0 (tag->str, "[Tr") == 0) {
                        j = g_array_index (tag_positions, gint, i) + strlen ("[Track nr]");
                        tag = g_string_append (tag, "ack nr]");
                }
                if (g_strcmp0 (tag->str, "[Ar") == 0) {
                        j = g_array_index (tag_positions, gint, i) + strlen ("[Artist name]");
                        tag = g_string_append (tag, "tist name]");
                }
                result = g_list_append (result, tag);
        }
        string = g_string_new ("");
        string = g_string_append (string, entry_text + j);

        if (g_strcmp0 (string->str, ""))
                result = g_list_append (result, string);

        g_array_free (tag_positions, TRUE);
        return result;
}

static GList*
batch_rename_get_new_names (NautilusBatchRename *dialog)
{
        GList *result = NULL;
        GList *selection, *tags_list;
        g_autofree gchar *entry_text;
        g_autofree gchar *replace_text;

        selection = dialog->selection;
        tags_list = NULL;

        if (dialog->mode == NAUTILUS_BATCH_RENAME_REPLACE)
                entry_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->find_entry)));
        else
                entry_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->name_entry)));

        replace_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->replace_entry)));

        if (dialog->mode == NAUTILUS_BATCH_RENAME_REPLACE) {
                result = get_new_names_list (dialog->mode,
                                             selection,
                                             NULL,
                                             NULL,
                                             entry_text,
                                             replace_text);
        } else {
                /* get list of tags and regular text */
                tags_list = split_entry_text (dialog, entry_text);

                result = get_new_names_list (dialog->mode,
                                             selection,
                                             tags_list,
                                             dialog->selection_metadata,
                                             entry_text,
                                             replace_text);
        }

        result = g_list_reverse (result);

        if (tags_list != NULL)
                g_list_free_full (tags_list, string_free);

        return result;
}

static void
rename_files_on_names_accepted (NautilusBatchRename *dialog,
                                GList               *new_names)
{
        /* do the actual rename here */
        nautilus_file_batch_rename (dialog->selection, new_names, NULL, NULL);

        gtk_window_close (GTK_WINDOW (dialog));
}

static void
listbox_header_func (GtkListBoxRow         *row,
                     GtkListBoxRow         *before,
                     NautilusBatchRename   *dialog)
{
  gboolean show_separator;

  show_separator = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "show-separator"));

  if (show_separator)
    {
      GtkWidget *separator;

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (separator);

      gtk_list_box_row_set_header (row, separator);
    }
}

static GtkWidget*
create_row_for_label (NautilusBatchRename *dialog,
                      const gchar         *new_text,
                      const gchar         *old_text,
                      gboolean             show_separator)
{
        GtkWidget *row;
        GtkWidget *label_new;
        GtkWidget *label_old;
        GtkWidget *box;
        GtkWidget *icon;

        row = gtk_list_box_row_new ();

        g_object_set_data (G_OBJECT (row), "show-separator", GINT_TO_POINTER (show_separator));

        icon = gtk_image_new_from_icon_name ("media-playlist-consecutive-symbolic",
                                            GTK_ICON_SIZE_SMALL_TOOLBAR);

        box = g_object_new (GTK_TYPE_BOX,
                            "orientation",GTK_ORIENTATION_HORIZONTAL,
                            "hexpand", TRUE,
                            NULL);
        label_new = g_object_new (GTK_TYPE_LABEL,
                                  "label",new_text,
                                  "hexpand", TRUE,
                                  "selectable", TRUE,
                                  "xalign", 0.0,
                                  "margin-start", 6,
                                  NULL);

        label_old = g_object_new (GTK_TYPE_LABEL,
                                  "label",old_text,
                                  "hexpand", TRUE,
                                  "selectable", TRUE,
                                  "xalign", 0.0,
                                  "margin-start", 6,
                                  NULL);

        gtk_label_set_ellipsize (GTK_LABEL (label_new), PANGO_ELLIPSIZE_END);
        gtk_label_set_ellipsize (GTK_LABEL (label_old), PANGO_ELLIPSIZE_END);

        //gtk_size_group_add_widget (dialog->size_group1, label_new);
        //gtk_size_group_add_widget (dialog->size_group2, label_old);

        gtk_box_pack_end (GTK_BOX (box), label_new, TRUE, FALSE, 0);
        gtk_box_pack_end (GTK_BOX (box), icon, TRUE, FALSE, 0);
        gtk_box_pack_end (GTK_BOX (box), label_old, TRUE, FALSE, 0);
        gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), TRUE);

        dialog->listbox_labels_new = g_list_append (dialog->listbox_labels_new, label_new);
        dialog->listbox_labels_old = g_list_append (dialog->listbox_labels_old, label_old);

        gtk_container_add (GTK_CONTAINER (row), box);
        gtk_widget_show_all (row);

        return row;
}

static void
fill_display_listbox (NautilusBatchRename *dialog)
{
        GtkWidget *row;
        GList *l1;
        GList *l2;
        NautilusFile *file;
        GString *new_name;

        dialog->listbox_rows = NULL;
        dialog->size_group1 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        dialog->size_group2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

        /* add rows to a list so that they can be removed when the renaming
         * result changes */
        for (l1 = dialog->new_names, l2 = dialog->selection; l1 != NULL || l2 != NULL; l1 = l1->next, l2 = l2->next) {
                file = NAUTILUS_FILE (l2->data);
                new_name = l1->data;

                row = create_row_for_label (dialog, new_name->str, nautilus_file_get_name (file), TRUE);

                gtk_container_add (GTK_CONTAINER (dialog->conflict_listbox), row);

                dialog->listbox_rows = g_list_append (dialog->listbox_rows,
                                                      row);
        }
}

static gboolean
file_has_conflict (NautilusBatchRename *dialog,
                   GString             *new_name)
{
        GList *l;

        for (l = dialog->duplicates; l != NULL; l = l->next) {
                if (g_strcmp0 (l->data, new_name->str) == 0)
                        return TRUE;
        }

        return FALSE;
}

static void
select_nth_conflict (NautilusBatchRename *dialog)
{
        GList *l, *l2;
        GString *file_name, *display_text, *new_name;
        gint n, nth_conflict;
        NautilusFile *file;
        GtkAdjustment *adjustment;
        GtkAllocation allocation;

        nth_conflict = dialog->selected_conflict;
        n = nth_conflict;
        l = g_list_nth (dialog->duplicates, n);

        /* the conflict that has to be selected */
        file_name = g_string_new (l->data);
        display_text = g_string_new ("");

        n = 0;

        l2 = dialog->selection;

        for (l = dialog->new_names; l != NULL; l = l->next) {
                file = NAUTILUS_FILE (l2->data);

                new_name = l->data;

                /* g_strcmp0 is used for not selecting a file that doesn't change
                 * it's name */
                if (g_strcmp0 (new_name->str, nautilus_file_get_name (file)) &&
                    g_string_equal (file_name, new_name) &&
                    nth_conflict == 0)
                        break;

                /* a file can only have a conflict if it's name has changed */
                if (g_strcmp0 (new_name->str, nautilus_file_get_name (file)) &&
                    file_has_conflict (dialog, new_name))
                        nth_conflict--;

                n++;
                l2 = l2->next;
        }

        l = g_list_nth (dialog->listbox_rows, n);

        gtk_list_box_select_row (GTK_LIST_BOX (dialog->conflict_listbox),
                                 l->data);

        /* scroll to the selected row */
        adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (dialog->scrolled_window));
        gtk_widget_get_allocation (GTK_WIDGET (l->data), &allocation);
        gtk_adjustment_set_value (adjustment, (allocation.height + 1)*n);

        if (strstr (file_name->str, "/") == NULL)
                g_string_append_printf (display_text,
                                        "\"%s\" would conflict with an existing file.",
                                        file_name->str);
        else
                g_string_append_printf (display_text,
                                        "\"%s\" has unallowed character '/'.",
                                        file_name->str);

        gtk_label_set_label (GTK_LABEL (dialog->conflict_label),
                             display_text->str);
}

static void
move_next_conflict_down (NautilusBatchRename *dialog)
{
        dialog->selected_conflict++;

        if (dialog->selected_conflict == 1)
                gtk_widget_set_sensitive (dialog->conflict_up, TRUE);

        if (dialog->selected_conflict == dialog->conflicts_number - 1)
                gtk_widget_set_sensitive (dialog->conflict_down, FALSE);

        select_nth_conflict (dialog);
}

static void
move_next_conflict_up (NautilusBatchRename *dialog)
{
        dialog->selected_conflict--;

        if (dialog->selected_conflict == 0)
                gtk_widget_set_sensitive (dialog->conflict_up, FALSE);

        if (dialog->selected_conflict == dialog->conflicts_number - 2)
                gtk_widget_set_sensitive (dialog->conflict_down, TRUE);

        select_nth_conflict (dialog);
}

static void
update_listbox (NautilusBatchRename *dialog)
{
        GList *l1, *l2;
        NautilusFile *file;
        gchar *old_name;
        GtkLabel *label;
        GString *new_name;

        /* Update listbox that shows the result of the renaming for each file */
        for (l1 = dialog->new_names, l2 = dialog->listbox_labels_new; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next) {
                label = GTK_LABEL (l2->data);
                new_name = l1->data;

                gtk_label_set_label (label, new_name->str);
        }

        for (l1 = dialog->selection, l2 = dialog->listbox_labels_old; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next) {
                label = GTK_LABEL (l2->data);
                file = NAUTILUS_FILE (l1->data);

                old_name = nautilus_file_get_name (file);

                gtk_label_set_label (label, old_name);
        }


        /* check if there are name conflicts and display them if they exist */
        if (dialog->duplicates != NULL) {
                gtk_widget_set_sensitive (dialog->rename_button, FALSE);

                gtk_widget_show (dialog->conflict_box);

                dialog->selected_conflict = 0;
                dialog->conflicts_number = g_list_length (dialog->duplicates);

                select_nth_conflict (dialog);

                gtk_widget_set_sensitive (dialog->conflict_up, FALSE);

                if (g_list_length (dialog->duplicates) == 1)
                    gtk_widget_set_sensitive (dialog->conflict_down, FALSE);
                else
                    gtk_widget_set_sensitive (dialog->conflict_down, TRUE);
        } else {
                gtk_widget_hide (dialog->conflict_box);

                /* re-enable the rename button if there are no more name conflicts */
                if (dialog->duplicates == NULL && !gtk_widget_is_sensitive (dialog->rename_button))
                        gtk_widget_set_sensitive (dialog->rename_button, TRUE);
        }
}


void
check_conflict_for_file (NautilusBatchRename *dialog,
                         NautilusDirectory   *directory,
                         GList               *files)
{
        gchar *current_directory, *parent_uri, *name;
        NautilusFile *file1, *file2;
        GString *new_name, *file_name1, *file_name2;
        GList *l1, *l2, *l3;

        file_name1 = g_string_new ("");
        file_name2 = g_string_new ("");

        current_directory = nautilus_directory_get_uri (directory);

        for (l1 = dialog->selection, l2 = dialog->new_names; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next) {
                file1 = NAUTILUS_FILE (l1->data);

                g_string_erase (file_name1, 0, -1);
                name = nautilus_file_get_name (file1);
                g_string_append (file_name1, name);
                g_free (name);

                parent_uri = nautilus_file_get_parent_uri (file1);

                new_name = l2->data;

                /* check for duplicate only if the parent of the current file is
                 * the current directory and the name of the file has changed */
                if (g_strcmp0 (parent_uri, current_directory) == 0 &&
                    !g_string_equal (new_name, file_name1))
                         for (l3 = files; l3 != NULL; l3 = l3->next) {
                                file2 = NAUTILUS_FILE (l3->data);

                                g_string_erase (file_name2, 0, -1);
                                name = nautilus_file_get_name (file2);
                                g_string_append (file_name2, name);
                                g_free (name);

                                if (g_string_equal (new_name, file_name2) &&
                                    !file_name_changed (dialog->selection, dialog->new_names, new_name, parent_uri)) {
                                        dialog->duplicates = g_list_prepend (dialog->duplicates,
                                                                             strdup (new_name->str));
                                        break;
                                }
                        }
        }

        /* check if this is the last call of the callback. Update
         * the listbox with the conflicts if it is. */
        if (dialog->checked_parents == g_list_length (dialog->distinct_parents) - 1) {
                dialog->duplicates = g_list_reverse (dialog->duplicates);
        }

        dialog->checked_parents++;

        g_free (current_directory);
}

static void
list_has_duplicates_callback (GObject *object,
                              GAsyncResult *res,
                              gpointer user_data)
{
        NautilusBatchRename *dialog;

        dialog = NAUTILUS_BATCH_RENAME (object);

        dialog->checking_conflicts = FALSE;

        update_listbox (dialog);
}

static void
list_has_duplicates_async_thread (GTask        *task,
                                  gpointer      object,
                                  gpointer      task_data,
                                  GCancellable *cancellable)
{
        NautilusBatchRename *dialog;

        dialog = NAUTILUS_BATCH_RENAME (object);

        dialog->duplicates = list_has_duplicates (dialog,
                                                  dialog->model,
                                                  dialog->new_names,
                                                  dialog->selection,
                                                  dialog->distinct_parents,
                                                  dialog->same_parent,
                                                  cancellable);

        g_task_return_pointer (task, object, NULL);

}

static void
list_has_duplicates_async (NautilusBatchRename *dialog,
                           int                  io_priority,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
        if (dialog->checking_conflicts == TRUE)
                g_cancellable_cancel (dialog->cancellable);

        dialog->cancellable = g_cancellable_new ();

        dialog->checking_conflicts = TRUE;
        dialog->task = g_task_new (dialog, dialog->cancellable, callback, user_data);

        g_task_set_priority (dialog->task, io_priority);
        g_task_run_in_thread (dialog->task, list_has_duplicates_async_thread);

        g_object_unref (dialog->task);
}

static gint
check_tag (NautilusBatchRename *dialog,
           gchar               *tag_name,
           gchar               *action_name,
           gint                 old_position)
{
        GString *entry_text;
        GAction *action;
        gint position;

        entry_text = g_string_new (gtk_entry_get_text (GTK_ENTRY (dialog->name_entry)));
        position = old_position;

        if (g_strrstr (entry_text->str, tag_name) && old_position == -1) {
                action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                     action_name);
                g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
        }

        if (g_strrstr (entry_text->str, tag_name) == NULL && old_position >= 0) {
                action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                     action_name);
                g_simple_action_set_enabled (G_SIMPLE_ACTION (action), TRUE);

                position = -1;
        }

        if (g_strrstr (entry_text->str, tag_name)) {
                position = g_strrstr (entry_text->str, tag_name) - entry_text->str;
        }

        g_string_free (entry_text, TRUE);

        return position;
}

static void
check_numbering_tags (NautilusBatchRename *dialog)
{
        GString *entry_text;
        GAction *add_numbering_action;

        entry_text = g_string_new (gtk_entry_get_text (GTK_ENTRY (dialog->name_entry)));

        if ((g_strrstr (entry_text->str, "[1, 2, 3]") ||
            g_strrstr (entry_text->str, "[01, 02, 03]") ||
            g_strrstr (entry_text->str, "[001, 002, 003]")) &&
            dialog->numbering_tag == -1) {
                add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                      "add-numbering-tag-zero");
                g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), FALSE);

                add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                      "add-numbering-tag-one");
                g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), FALSE);

                add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                      "add-numbering-tag-two");
                g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), FALSE);
        }
        if (g_strrstr (entry_text->str, "[1, 2, 3]") == NULL &&
            g_strrstr (entry_text->str, "[01, 02, 03]") == NULL &&
            g_strrstr (entry_text->str, "[001, 002, 003]") == NULL &&
            dialog->numbering_tag >= 0) {
                add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                      "add-numbering-tag-zero");
                g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), TRUE);

                add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                      "add-numbering-tag-one");
                g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), TRUE);

                add_numbering_action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                                      "add-numbering-tag-two");
                g_simple_action_set_enabled (G_SIMPLE_ACTION (add_numbering_action), TRUE);

                dialog->numbering_tag = -1;
        }

        if (g_strrstr (entry_text->str, "[1, 2, 3]")) {
                dialog->numbering_tag = g_strrstr (entry_text->str, "[1, 2, 3]") - entry_text->str;
        }

        if (g_strrstr (entry_text->str, "[01, 02, 03]")) {
                dialog->numbering_tag = g_strrstr (entry_text->str, "[01, 02, 03]") - entry_text->str;
        }
        if (g_strrstr (entry_text->str, "[001, 002, 003]")) {
                dialog->numbering_tag = g_strrstr (entry_text->str, "[001, 002, 003]") - entry_text->str;
        }
        g_string_free (entry_text, TRUE);
}

static void
update_tags (NautilusBatchRename *dialog)
{
        dialog->original_name_tag = check_tag (dialog,
                                               "[Original file name]",
                                               "add-original-file-name-tag",
                                               dialog->original_name_tag);
        if (dialog->creation_date_tag != TAG_UNAVAILABLE)
                dialog->creation_date_tag = check_tag (dialog,
                                                       "[Date taken]",
                                                       "add-creation-date-tag",
                                                       dialog->creation_date_tag);
        if (dialog->equipment_tag != TAG_UNAVAILABLE)
                dialog->equipment_tag = check_tag (dialog,
                                                   "[Camera model]",
                                                   "add-equipment-tag",
                                                   dialog->equipment_tag);
        if (dialog->season_tag != TAG_UNAVAILABLE)
                dialog->season_tag = check_tag (dialog,
                                                "[Season nr]",
                                                "add-season-tag",
                                                dialog->season_tag);
        if (dialog->episode_nr_tag != TAG_UNAVAILABLE)
                dialog->episode_nr_tag = check_tag (dialog,
                                                    "[Episode nr]",
                                                    "add-episode-tag",
                                                    dialog->episode_nr_tag);
        if (dialog->track_nr_tag != TAG_UNAVAILABLE)
                dialog->track_nr_tag = check_tag (dialog,
                                                  "[Track nr]",
                                                  "add-track-number-tag",
                                                  dialog->track_nr_tag);
        if (dialog->artist_name_tag != TAG_UNAVAILABLE)
                dialog->artist_name_tag = check_tag (dialog,
                                                     "[Artist name]",
                                                     "add-artist-name-tag",
                                                     dialog->artist_name_tag);

        check_numbering_tags (dialog);
}

static void
file_names_widget_entry_on_changed (NautilusBatchRename *dialog)
{
        if (dialog->cancellable != NULL)
                g_cancellable_cancel (dialog->cancellable);

        if(dialog->selection == NULL)
                return;

        if (dialog->duplicates != NULL) {
                g_list_free_full (dialog->duplicates, g_free);
                dialog->duplicates = NULL;
        }

        if (dialog->new_names != NULL)
                g_list_free_full (dialog->new_names, string_free);

        update_tags (dialog);

        if (dialog->numbering_tag == -1) {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_label), "");
                gtk_widget_hide (dialog->numbering_order_button);
        } else {
                gtk_label_set_label (GTK_LABEL (dialog->numbering_label), "Automatic Numbering Order");
                gtk_widget_show (dialog->numbering_order_button);
        }

        dialog->new_names = batch_rename_get_new_names (dialog);
        dialog->checked_parents = 0;

        list_has_duplicates_async (dialog,
                                   G_PRIORITY_DEFAULT,
                                   list_has_duplicates_callback,
                                   NULL);
}

static void
file_names_widget_on_activate (NautilusBatchRename *dialog)
{
        GList *l;

        /* wait for checking conflicts to finish, to be sure that
         * the rename can actually take place */
        while (dialog->checking_conflicts){

        }

        if (!gtk_widget_is_sensitive (dialog->rename_button))
                return;

        /* clear rows from listbox */
        if (dialog->listbox_rows != NULL)
                for (l = dialog->listbox_rows; l != NULL; l = l->next)
                        gtk_container_remove (GTK_CONTAINER (dialog->conflict_listbox),
                                              GTK_WIDGET (l->data));

        g_list_free (dialog->listbox_rows);

        /* if names are all right call rename_files_on_names_accepted*/
        rename_files_on_names_accepted (dialog, dialog->new_names);
}

static void
batch_rename_mode_changed (NautilusBatchRename *dialog)
{
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->format_mode_button))) {
                gtk_stack_set_visible_child_name (GTK_STACK (dialog->mode_stack), "format");

                dialog->mode = NAUTILUS_BATCH_RENAME_FORMAT;

                gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->name_entry));

        } else {
                gtk_stack_set_visible_child_name (GTK_STACK (dialog->mode_stack), "replace");

                dialog->mode = NAUTILUS_BATCH_RENAME_REPLACE;

                gtk_entry_grab_focus_without_selecting (GTK_ENTRY (dialog->find_entry));
        }

        /* update display text */
        file_names_widget_entry_on_changed (dialog);

}

static void
add_button_clicked (NautilusBatchRename *dialog)
{
        if (gtk_widget_is_visible (dialog->add_popover))
                gtk_widget_set_visible (dialog->add_popover, FALSE);
        else
                gtk_widget_set_visible (dialog->add_popover, TRUE);
}

static void
add_popover_closed (NautilusBatchRename *dialog)
{
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->add_button), FALSE);
}

static void
numbering_order_button_clicked (NautilusBatchRename *dialog)
{
        if (gtk_widget_is_visible (dialog->numbering_order_popover))
                gtk_widget_set_visible (dialog->numbering_order_popover, FALSE);
        else
                gtk_widget_set_visible (dialog->numbering_order_popover, TRUE);
}

static void
numbering_order_popover_closed (NautilusBatchRename *dialog)
{
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->numbering_order_button), FALSE);
}

void
query_finished (NautilusBatchRename *dialog,
                GHashTable          *hash_table,
                GList               *selection_metadata)
{
        GMenuItem *first_created, *last_created;
        FileMetadata *metadata;

        /* for files with no metadata */
        if (hash_table != NULL && g_hash_table_size (hash_table) == 0)
                g_hash_table_destroy (hash_table);

        if (hash_table == NULL || g_hash_table_size (hash_table) == 0)
                dialog->create_date = NULL;
        else
                dialog->create_date = hash_table;

        if (dialog->create_date != NULL) {
                first_created = g_menu_item_new ("First Created",
                                                 "dialog.numbering-order-changed");

                g_menu_item_set_attribute (first_created, "target", "s", "first-created");

                g_menu_append_item (dialog->numbering_order_menu, first_created);

                last_created = g_menu_item_new ("Last Created",
                                                 "dialog.numbering-order-changed");

                g_menu_item_set_attribute (last_created, "target", "s", "last-created");

                g_menu_append_item (dialog->numbering_order_menu, last_created);

        }

        dialog->selection_metadata = selection_metadata;
        metadata = selection_metadata->data;

        if (metadata->creation_date == NULL || g_strcmp0 (metadata->creation_date->str, "") == 0) {
               disable_action (dialog, "add-creation-date-tag");
               dialog->creation_date_tag = TAG_UNAVAILABLE;
        } else {
                dialog->creation_date_tag = -1;
        }

        if (metadata->equipment == NULL || g_strcmp0 (metadata->equipment->str, "") == 0) {
               disable_action (dialog, "add-equipment-tag");
               dialog->equipment_tag = TAG_UNAVAILABLE;
        } else {
                dialog->equipment_tag = -1;
        }

        if (metadata->season == NULL || g_strcmp0 (metadata->season->str, "") == 0) {
               disable_action (dialog, "add-season-tag");
               dialog->season_tag = TAG_UNAVAILABLE;
        } else {
                dialog->creation_date_tag = -1;
        }

        if (metadata->episode_nr == NULL || g_strcmp0 (metadata->episode_nr->str, "") == 0) {
               disable_action (dialog, "add-episode-tag");
               dialog->episode_nr_tag = TAG_UNAVAILABLE;
        } else {
                dialog->episode_nr_tag = -1;
        }

        if (metadata->track_nr == NULL || g_strcmp0 (metadata->track_nr->str, "") == 0) {
               disable_action (dialog, "add-track-number-tag");
               dialog->track_nr_tag = TAG_UNAVAILABLE;
        } else {
                dialog->track_nr_tag = -1;
        }

        if (metadata->artist_name == NULL || g_strcmp0 (metadata->artist_name->str, "") == 0) {
               disable_action (dialog, "add-artist-name-tag");
               dialog->artist_name_tag = TAG_UNAVAILABLE;
        } else {
                dialog->artist_name_tag = -1;
        }
}

static void
nautilus_batch_rename_initialize_actions (NautilusBatchRename *dialog)
{
        GAction *action;

        dialog->action_group = G_ACTION_GROUP (g_simple_action_group_new ());

        g_action_map_add_action_entries (G_ACTION_MAP (dialog->action_group),
                                        dialog_entries,
                                        G_N_ELEMENTS (dialog_entries),
                                        dialog);
        gtk_widget_insert_action_group (GTK_WIDGET (dialog),
                                        "dialog",
                                        G_ACTION_GROUP (dialog->action_group));

        action = g_action_map_lookup_action (G_ACTION_MAP (dialog->action_group),
                                             "add-original-file-name-tag");
        g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);

        check_metadata_for_selection (dialog, dialog->selection);
}

static void
nautilus_batch_rename_finalize (GObject *object)
{
        NautilusBatchRename *dialog;
        GList *l;

        dialog = NAUTILUS_BATCH_RENAME (object);

        for (l = dialog->selection_metadata; l != NULL; l = l->next) {
                FileMetadata *metadata;

                metadata = l->data;

                if (metadata->file_name != NULL)
                        g_string_free (metadata->file_name, TRUE);
                if (metadata->creation_date != NULL)
                        g_string_free (metadata->creation_date, TRUE);
                if (metadata->equipment != NULL)
                        g_string_free (metadata->equipment, TRUE);
                if (metadata->season != NULL)
                        g_string_free (metadata->season, TRUE);
                if (metadata->episode_nr != NULL)
                        g_string_free (metadata->episode_nr, TRUE);
                if (metadata->track_nr != NULL)
                        g_string_free (metadata->track_nr, TRUE);
                if (metadata->artist_name != NULL)
                        g_string_free (metadata->artist_name, TRUE);
        }

        if (dialog->create_date != NULL)
                g_hash_table_destroy (dialog->create_date);

        g_list_free_full (dialog->distinct_parents, g_free);
        g_list_free_full (dialog->new_names, string_free);
        g_list_free_full (dialog->duplicates, g_free);

        g_object_unref (dialog->size_group1);
        g_object_unref (dialog->size_group2);

        G_OBJECT_CLASS (nautilus_batch_rename_parent_class)->finalize (object);
}

static void
nautilus_batch_rename_class_init (NautilusBatchRenameClass *klass)
{
        GtkWidgetClass *widget_class;
        GObjectClass *oclass;

        widget_class = GTK_WIDGET_CLASS (klass);
        oclass = G_OBJECT_CLASS (klass);

        oclass->finalize = nautilus_batch_rename_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-batch-rename-dialog.ui");

        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, grid);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, cancel_button);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, conflict_listbox);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, name_entry);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, rename_button);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, find_entry);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, replace_entry);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, mode_stack);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, replace_mode_button);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, format_mode_button);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, add_button);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, add_popover);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, numbering_order_label);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, scrolled_window);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, numbering_order_popover);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, numbering_order_button);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, numbering_order_menu);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, conflict_box);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, conflict_label);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, conflict_up);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, conflict_down);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, add_tag_menu);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, numbering_label);

        gtk_widget_class_bind_template_callback (widget_class, file_names_widget_entry_on_changed);
        gtk_widget_class_bind_template_callback (widget_class, file_names_widget_on_activate);
        gtk_widget_class_bind_template_callback (widget_class, batch_rename_mode_changed);
        gtk_widget_class_bind_template_callback (widget_class, add_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, add_popover_closed);
        gtk_widget_class_bind_template_callback (widget_class, numbering_order_button_clicked);
        gtk_widget_class_bind_template_callback (widget_class, numbering_order_popover_closed);
        gtk_widget_class_bind_template_callback (widget_class, move_next_conflict_up);
        gtk_widget_class_bind_template_callback (widget_class, move_next_conflict_down);
}

GtkWidget*
nautilus_batch_rename_new (GList *selection, NautilusDirectory *model, NautilusWindow *window)
{
        NautilusBatchRename *dialog;
        gint files_nr;
        GList *l;
        gchar *dialog_title;

        dialog = g_object_new (NAUTILUS_TYPE_BATCH_RENAME, "use-header-bar", TRUE, NULL);

        dialog->selection = selection;
        dialog->model = model;

        gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                      GTK_WINDOW (window));

        files_nr = 0;

        for (l = dialog->selection; l != NULL; l = l->next)
                files_nr++;

        dialog_title = g_malloc (DIALOG_TITLE_LEN);
        g_snprintf (dialog_title, DIALOG_TITLE_LEN, "Renaming %d files", files_nr);
        gtk_window_set_title (GTK_WINDOW (dialog), dialog_title);

        nautilus_batch_rename_initialize_actions (dialog);

        dialog->same_parent = selection_has_single_parent (dialog->selection);
        if (!dialog->same_parent)
                dialog->distinct_parents = distinct_file_parents (dialog->selection);
        else
                dialog->distinct_parents = NULL;

        /* update display text */
        file_names_widget_entry_on_changed (dialog);

        fill_display_listbox (dialog);

        g_free (dialog_title);

        return GTK_WIDGET (dialog);
}

static void
nautilus_batch_rename_init (NautilusBatchRename *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        gtk_list_box_set_header_func (GTK_LIST_BOX (self->conflict_listbox),
                                                    (GtkListBoxUpdateHeaderFunc) listbox_header_func,
                                                    self,
                                                    NULL);

        self->mode = NAUTILUS_BATCH_RENAME_FORMAT;

        gtk_popover_bind_model (GTK_POPOVER (self->numbering_order_popover),
                                G_MENU_MODEL (self->numbering_order_menu),
                                NULL);
        gtk_popover_bind_model (GTK_POPOVER (self->add_popover),
                                G_MENU_MODEL (self->add_tag_menu),
                                NULL);

        gtk_label_set_ellipsize (GTK_LABEL (self->conflict_label), PANGO_ELLIPSIZE_END);

        self->duplicates = NULL;
        self->new_names = NULL;
        self->listbox_rows = NULL;

        self->checking_conflicts = FALSE;

        self->original_name_tag = 0;
        self->numbering_tag = -1;
        gtk_entry_set_text (GTK_ENTRY (self->name_entry),"[Original file name]");
}