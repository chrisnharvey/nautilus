/* nautilus-batch-rename.c
 *
 * Copyright (C) 2016 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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
#include "nautilus-files-view.h"
#include "nautilus-error-reporting.h"
#include "nautilus-batch-rename-utilities.h"

#include <glib/gprintf.h>
#include <string.h>

#define ADD_TEXT_ENTRY_SIZE 550
#define REPLACE_ENTRY_SIZE  275
#define MAX_DISPLAY_LEN 50

struct _NautilusBatchRename
{
        GtkDialog                parent;

        GtkWidget               *grid;

        GtkWidget               *add_text_options;
        GtkWidget               *cancel_button;
        GtkWidget               *conflict_listbox;
        GtkWidget               *error_label;
        GtkWidget               *expander;
        GtkWidget               *name_entry;
        GtkWidget               *rename_button;
        GtkWidget               *rename_modes;
        GtkWidget               *find_label;
        GtkWidget               *left_stack;
        GtkWidget               *label_stack;
        GtkWidget               *right_stack;
        GtkWidget               *replace_entry;
        GtkWidget               *replace_label;
        GtkWidget               *replace_box;

        GList                   *listbox_rows;//adauga aici rows de la listbox pt a fi sterse

        GList                   *selection;
        NautilusBatchRenameModes mode;
        NautilusFilesView       *view;

        GtkWidget               *expander_label;
};

static void     batch_rename_dialog_on_closed           (GtkDialog              *dialog);
static void     file_names_widget_entry_on_changed      (NautilusBatchRename    *dialog);

G_DEFINE_TYPE (NautilusBatchRename, nautilus_batch_rename, GTK_TYPE_DIALOG);

static GList*
batch_rename_get_new_names (NautilusBatchRename *dialog)
{
        GList *result = NULL;
        GList *selection;
        gchar *entry_text;
        gchar *replace_text;

        selection = dialog->selection;

        entry_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->name_entry)));
        replace_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->replace_entry)));

        result = get_new_names_list (dialog->mode, selection, entry_text, replace_text);

        g_free (entry_text);

        result = g_list_reverse (result);

        return result;
}

static void
rename_files_on_names_accepted (NautilusBatchRename *dialog,
                                GList               *new_names)
{
        /* do the actual rename here */
        GList *l1;
        GList *l2;
        GList *selection;
        NautilusFile *file;

        selection = dialog->selection;

        for (l1 = selection, l2 = new_names; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next) {
                file = NAUTILUS_FILE (l1->data);

                /* Put it on the queue for reveal after the view acknowledges the change */
                //do this sometime...
                /*g_hash_table_insert (dialog->view->details->pending_reveal,
                                     file,
                                     GUINT_TO_POINTER (FALSE));*/

                nautilus_rename_file (file, l2->data, NULL, NULL);
        }

        batch_rename_dialog_on_closed (GTK_DIALOG (dialog));
}

static void
batch_rename_add_text_changed (GtkComboBoxText *widget,
                               NautilusBatchRename *dialog)
{
        gchar* active_item;

        active_item = gtk_combo_box_text_get_active_text (widget);

        if (strcmp (active_item, "Append") == 0)
                dialog->mode = NAUTILUS_BATCH_RENAME_APPEND;

        if (strcmp (active_item, "Prepend") == 0)
                dialog->mode = NAUTILUS_BATCH_RENAME_PREPEND;

        /* update display text */
        file_names_widget_entry_on_changed (dialog);
}

static void
switch_to_replace_mode (NautilusBatchRename *dialog)
{
        GValue width = G_VALUE_INIT;

        g_value_init (&width, G_TYPE_INT);
        g_value_set_int (&width, 3);

        gtk_stack_set_visible_child (GTK_STACK (dialog->left_stack), GTK_WIDGET (dialog->find_label));
        gtk_stack_set_visible_child (GTK_STACK (dialog->right_stack), GTK_WIDGET (dialog->replace_box));
        gtk_widget_show (GTK_WIDGET (dialog->replace_box));

        gtk_widget_grab_focus (dialog->name_entry);

        gtk_container_child_set_property (GTK_CONTAINER (dialog->grid), dialog->name_entry, "width",&width);

        gtk_widget_set_size_request (dialog->name_entry, REPLACE_ENTRY_SIZE, -1);
}

static void
switch_to_add_text_mode (NautilusBatchRename *dialog)
{
        GValue width = G_VALUE_INIT;

        g_value_init (&width, G_TYPE_INT);
        g_value_set_int (&width, 6);

        gtk_stack_set_visible_child (GTK_STACK (dialog->left_stack), GTK_WIDGET (dialog->add_text_options));
        gtk_widget_hide (GTK_WIDGET (dialog->replace_box));

        gtk_widget_grab_focus (dialog->name_entry);

        gtk_container_child_set_property (GTK_CONTAINER (dialog->grid), dialog->name_entry, "width",&width);

        gtk_widget_set_size_request (dialog->name_entry, ADD_TEXT_ENTRY_SIZE, -1);
}

static void
batch_rename_mode_changed (GtkComboBoxText *widget,
                           NautilusBatchRename *dialog)
{
        gchar* active_item;

        active_item = gtk_combo_box_text_get_active_text (widget);

        if (strcmp (active_item, "Replace") == 0) {
                dialog->mode = NAUTILUS_BATCH_RENAME_REPLACE;
                switch_to_replace_mode (dialog);
        }

        /* check whether before it was append or prepend */
        if (strcmp (active_item, "Add Text") == 0) {
                batch_rename_add_text_changed (GTK_COMBO_BOX_TEXT (dialog->add_text_options), dialog);

                switch_to_add_text_mode (dialog);
        }

        /* update display text */
        file_names_widget_entry_on_changed (dialog);
}

static void
activate_expander (GtkExpander *expander,
                   NautilusBatchRename *dialog)
{
        //GValue width = G_VALUE_INIT;

        //g_value_init (&width, G_TYPE_INT);

        //if (gtk_expander_get_expanded (GTK_EXPANDER (expander))) {

                //g_value_set_int (&width, 8);
                //gtk_container_child_set_property (GTK_CONTAINER (dialog->grid), dialog->label_stack, "width",&width);
        //} else {

                //g_value_set_int (&width, 5);
                //gtk_container_child_set_property (GTK_CONTAINER (dialog->grid), dialog->label_stack, "width",&width);
        //}
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
create_row_for_label (const gchar *text,
                      gboolean     show_separator)
{
        GtkWidget *row;
        GtkWidget *label;

        row = gtk_list_box_row_new ();

        g_object_set_data (G_OBJECT (row), "show-separator", GINT_TO_POINTER (show_separator));

        label = g_object_new (GTK_TYPE_LABEL,
                              "label", text,
                              "hexpand", TRUE,
                              "xalign", 0.0,
                              "margin-start", 6,
                              NULL);

        gtk_list_box_row_set_selectable (GTK_LIST_BOX_ROW (row), FALSE);

        gtk_container_add (GTK_CONTAINER (row), label);
        gtk_widget_show_all (row);

        return row;
}

static void
fill_conflict_listbox (NautilusBatchRename *dialog,
                       GList               *conflict_list)
{
        GtkWidget *row;
        GList *l;

        /* clear rows from listbox (if there are any) */
        if (dialog->listbox_rows != NULL)
                for (l = dialog->listbox_rows; l != NULL; l = l->next) {
                        gtk_container_remove (GTK_CONTAINER (dialog->conflict_listbox),
                                              GTK_WIDGET (l->data));
                }

        g_list_free (dialog->listbox_rows);
        dialog->listbox_rows = NULL;

        row = create_row_for_label ("These files already exist:", FALSE);

        gtk_container_add (GTK_CONTAINER (dialog->conflict_listbox), row);

        /* add rows to a list so that they can be removed when there appears
         * a new conflict */
        dialog->listbox_rows = g_list_prepend (dialog->listbox_rows,
                                                 (gpointer) row);

        for (l = conflict_list; l != NULL; l = l->next) {

                row = create_row_for_label ((gchar*) l->data, l == conflict_list);

                gtk_container_add (GTK_CONTAINER (dialog->conflict_listbox), row);

                dialog->listbox_rows = g_list_prepend (dialog->listbox_rows,
                                                 (gpointer) row);
        }
}

static void
file_names_widget_entry_on_changed (NautilusBatchRename *dialog)
{
        gchar *entry_text;
        gchar *replace_text;
        gchar *file_name;
        GList *new_names;
        GList *duplicates;
        gchar *display_text = NULL;
        NautilusFile *file;
        gboolean singe_conflict;
        //GValue width = G_VALUE_INIT;

        if(dialog->selection == NULL)
                return;

        new_names = batch_rename_get_new_names(dialog);
        duplicates = list_has_duplicates (dialog->view, new_names, dialog->selection);

        if (duplicates != NULL)
                singe_conflict = (duplicates->next == NULL) ? TRUE:FALSE;

        file_name = NULL;

        /* check if there are name conflicts and display them if they exist */
        if (duplicates != NULL) {
                gtk_widget_set_sensitive (dialog->rename_button, FALSE);

                /* check if there is more than one conflict */
                if (!singe_conflict)
                        gtk_expander_set_label (GTK_EXPANDER (dialog->expander),
                                                "Multiple file conflicts");
                else {
                        file_name = concat ("File conflict: ", duplicates->data);

                        if (strlen (file_name) >= MAX_DISPLAY_LEN) {
                        gtk_label_set_label (GTK_LABEL (dialog->expander_label), file_name);

                        gtk_expander_set_label_widget (GTK_EXPANDER (dialog->expander),
                                                       dialog->expander_label);

                        gtk_widget_show (dialog->expander_label);
                        } else {
                               gtk_label_set_label (GTK_LABEL (dialog->error_label), file_name);
                        }
                }

                if (!singe_conflict || strlen (file_name) >= MAX_DISPLAY_LEN) {
                        gtk_expander_set_expanded (GTK_EXPANDER (dialog->expander), FALSE);
                        gtk_stack_set_visible_child (GTK_STACK (dialog->label_stack),
                                                     GTK_WIDGET (dialog->expander));
                }

                /* add name conflicts to the listbox */
                fill_conflict_listbox (dialog, duplicates);

                g_free (file_name);

                return;
        }
        else
                /* re-enable the rename button if there are no more name conflicts */
                if (duplicates == NULL && !gtk_widget_is_sensitive (dialog->rename_button)) {
                        gtk_expander_set_expanded (GTK_EXPANDER (dialog->expander), FALSE);

                        gtk_widget_set_sensitive (dialog->rename_button, TRUE);

                        gtk_stack_set_visible_child (GTK_STACK (dialog->label_stack), GTK_WIDGET (dialog->error_label));

                        //g_value_init (&width, G_TYPE_INT);
                        //g_value_set_int (&width, 8);
                        //gtk_container_child_set_property (GTK_CONTAINER (dialog->grid), dialog->label_stack, "width",&width);


                }

        /* Update label that shows an example of the renaming result */
        file = NAUTILUS_FILE (dialog->selection->data);

        file_name = g_strdup (nautilus_file_get_name (file));
        entry_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->name_entry)));
        replace_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->replace_entry)));

        if (entry_text == NULL ) {

                gtk_label_set_label (GTK_LABEL (dialog->error_label), file_name);
                g_free (file_name);

                return;
        }

        display_text = get_new_display_name (dialog->mode, file_name, entry_text, replace_text);

        gtk_label_set_label (GTK_LABEL (dialog->error_label), display_text);

        g_list_free (new_names);
        g_list_free (duplicates);

        g_free (entry_text);
        g_free (file_name);
        g_free (replace_text);
        g_free (display_text);
}

static void
batch_rename_dialog_on_closed (GtkDialog *dialog)
{
        gtk_window_close (GTK_WINDOW (dialog));
}

static void
file_names_widget_on_activate (NautilusBatchRename *dialog)
{
        GList *new_names;

        if (!gtk_widget_is_sensitive (dialog->rename_button))
                return;

        new_names = batch_rename_get_new_names(dialog);

        /* if names are all right call rename_files_on_names_accepted*/
        rename_files_on_names_accepted (dialog, new_names);
        g_list_free (new_names);
}

static void
nautilus_batch_rename_class_init (NautilusBatchRenameClass *klass)
{
        GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        dialog_class->close = batch_rename_dialog_on_closed;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/ui/nautilus-batch-rename-dialog.ui");

        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, grid);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, add_text_options);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, cancel_button);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, conflict_listbox);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, error_label);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, expander);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, name_entry);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, rename_button);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, rename_modes);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, find_label);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, left_stack);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, label_stack);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, right_stack);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, replace_label);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, replace_entry);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, replace_box);

        gtk_widget_class_bind_template_callback (widget_class, file_names_widget_entry_on_changed);
        gtk_widget_class_bind_template_callback (widget_class, batch_rename_dialog_on_closed);
        gtk_widget_class_bind_template_callback (widget_class, batch_rename_mode_changed);
        gtk_widget_class_bind_template_callback (widget_class, file_names_widget_on_activate);
        gtk_widget_class_bind_template_callback (widget_class, batch_rename_add_text_changed);
}

GtkWidget*
nautilus_batch_rename_new (NautilusFilesView *view)
{
        NautilusBatchRename *dialog;

        dialog = g_object_new (NAUTILUS_TYPE_BATCH_RENAME, NULL);

        dialog->selection = nautilus_view_get_selection (NAUTILUS_VIEW (view));
        dialog->view = view;

        gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                      GTK_WINDOW (nautilus_files_view_get_window (view)));

        gtk_widget_grab_focus (dialog->name_entry);

        gtk_label_set_ellipsize (GTK_LABEL (dialog->error_label), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars (GTK_LABEL (dialog->error_label), MAX_DISPLAY_LEN);

        gtk_label_set_ellipsize (GTK_LABEL (dialog->expander_label), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars (GTK_LABEL (dialog->expander_label), MAX_DISPLAY_LEN - 1);

        gtk_widget_set_vexpand (dialog->rename_button, FALSE);

        /* update display text */
        file_names_widget_entry_on_changed (dialog);

        return GTK_WIDGET (dialog);
}

static void
nautilus_batch_rename_init (NautilusBatchRename *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        gtk_combo_box_set_active (GTK_COMBO_BOX (self->add_text_options), 1);      
        gtk_combo_box_set_active (GTK_COMBO_BOX (self->rename_modes), 0);

        gtk_list_box_set_header_func (GTK_LIST_BOX (self->conflict_listbox),
                                (GtkListBoxUpdateHeaderFunc) listbox_header_func,
                                self,
                                NULL);

        g_signal_connect (self->expander, "activate", G_CALLBACK (activate_expander), self);

        self->expander_label = gtk_label_new ("");

        self->mode = NAUTILUS_BATCH_RENAME_PREPEND;
}