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
#include "nautilus-rename-utilities.h"

#include <glib/gprintf.h>
#include <string.h>

#define ADD_TEXT_ENTRY_SIZE 550
#define REPLACE_ENTRY_SIZE  275
#define MAX_DISPLAY_LEN 40

struct _NautilusBatchRename
{
        GtkDialog                parent;

        GtkWidget               *grid;

        GtkWidget               *add_text_options;
        GtkWidget               *cancel_button;
        GtkWidget               *error_label;
        GtkWidget               *name_entry;
        GtkWidget               *rename_button;
        GtkWidget               *rename_modes;
        GtkWidget               *find_label;
        GtkWidget               *left_stack;
        GtkWidget               *right_stack;
        GtkWidget               *replace_entry;
        GtkWidget               *replace_label;
        GtkWidget               *replace_box;


        GList                   *selection;
        NautilusBatchRenameModes mode;
        NautilusFilesView       *view;
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
file_names_widget_entry_on_changed (NautilusBatchRename *dialog)
{
        gchar *entry_text;
        gchar *replace_text;
        gchar *file_name;
        gchar *display_text = NULL;
        NautilusFile *file;

        if(dialog->selection == NULL)
                return;

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
        /* check if all names are all right i.e no conflicts, valid name, etc...*/

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
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, error_label);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, name_entry);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, rename_button);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, rename_modes);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, find_label);
        gtk_widget_class_bind_template_child (widget_class, NautilusBatchRename, left_stack);
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

        self->mode = NAUTILUS_BATCH_RENAME_PREPEND;
}