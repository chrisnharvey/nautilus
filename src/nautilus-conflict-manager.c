#include <gio/gio.h>
#include <glib/gi18n.h>

#include "nautilus-file.h"
#include "nautilus-file-operations.h"
#include "nautilus-conflict-manager.h"
#include "nautilus-file-conflict-dialog.h"

typedef struct FileConflictDialogData {
        GFile *src_name;
        GFile *dest_name;
        GFile *dest_dir_name;

        GtkWindow *parent;

        FileConflictResponse *response;

        NautilusFile *source;
        NautilusFile *destination;
        NautilusFile *dest_dir;

        NautilusFileConflictDialog *dialog;

        NautilusFileListCallback files_list_ready_cb;

        NautilusFileListHandle *handle;
        gulong src_handler_id;
        gulong dest_handler_id;
        /* Dialogs are ran from operation threads, which need to be blocked until
         * the user gives a response
         */
        gboolean completed;
        GMutex mutex;
        GCond cond;
} FileConflictDialogData;

static void
set_copy_move_dialog_text (FileConflictDialogData *data)
{
        g_autofree gchar *primary_text = NULL;
        g_autofree gchar *secondary_text = NULL;
        const gchar *message_extra;
        time_t src_mtime;
        time_t dest_mtime;
        g_autofree gchar *message = NULL;
        g_autofree gchar *dest_name;
        g_autofree gchar *dest_dir_name;
        gboolean source_is_dir;
        gboolean dest_is_dir;

        src_mtime = nautilus_file_get_mtime (data->source);
        dest_mtime = nautilus_file_get_mtime (data->destination);

        dest_name = nautilus_file_get_display_name (data->destination);
        dest_dir_name = nautilus_file_get_display_name (data->dest_dir);

        source_is_dir = nautilus_file_is_directory (data->source);
        dest_is_dir = nautilus_file_is_directory (data->destination);

        if (dest_is_dir) {
                if (source_is_dir) {
                        primary_text = g_strdup_printf
                                (_("Merge folder “%s”?"),
                                 dest_name);

                        message_extra =
                                _("Merging will ask for confirmation before replacing any files in "
                                  "the folder that conflict with the files being copied.");

                        if (src_mtime > dest_mtime) {
                                message = g_strdup_printf (
                                        _("An older folder with the same name already exists in “%s”."),
                                        dest_dir_name);
                        } else if (src_mtime < dest_mtime) {
                                message = g_strdup_printf (
                                        _("A newer folder with the same name already exists in “%s”."),
                                        dest_dir_name);
                        } else {
                                message = g_strdup_printf (
                                        _("Another folder with the same name already exists in “%s”."),
                                        dest_dir_name);
                        }
                } else {
                        primary_text = g_strdup_printf
                                (_("Replace folder “%s”?"), dest_name);
                        message_extra =
                                _("Replacing it will remove all files in the folder.");
                        message = g_strdup_printf
                                (_("A folder with the same name already exists in “%s”."),
                                 dest_dir_name);
                }
        } else {
                primary_text = g_strdup_printf
                        (_("Replace file “%s”?"), dest_name);

                message_extra = _("Replacing it will overwrite its content.");

                if (src_mtime > dest_mtime) {
                        message = g_strdup_printf (
                                _("An older file with the same name already exists in “%s”."),
                                dest_dir_name);
                } else if (src_mtime < dest_mtime) {
                        message = g_strdup_printf (
                                _("A newer file with the same name already exists in “%s”."),
                                dest_dir_name);
                } else {
                        message = g_strdup_printf (
                                _("Another file with the same name already exists in “%s”."),
                                dest_dir_name);
                }
        }

        secondary_text = g_strdup_printf ("%s\n%s", message, message_extra);

        nautilus_file_conflict_dialog_set_text (data->dialog,
                                                primary_text,
                                                secondary_text);
}

static void
set_extract_dialog_text (FileConflictDialogData *data)
{

        g_autofree gchar *primary_text = NULL;
        g_autofree gchar *secondary_text = NULL;
        const gchar *message_extra;
        g_autofree gchar *message = NULL;
        g_autofree gchar *dest_name;
        g_autofree gchar *dest_dir_name;

        dest_name = nautilus_file_get_display_name (data->destination);
        dest_dir_name = nautilus_file_get_display_name (data->dest_dir);

        if (g_file_equal (data->src_name, data->dest_name)) {
                primary_text = g_strdup_printf (_("Replace source archive “%s”?"), dest_name);
                message = g_strdup_printf
                        (_("The archive has the same name as the extract destination."));
                message_extra =
                        _("Replacing it will overwrite its contents once the operation is complete.");
        } else if (nautilus_file_is_directory (data->destination)) {
                primary_text = g_strdup_printf (_("Replace folder “%s”?"), dest_name);
                message = g_strdup_printf
                        (_("A folder with the same name as the extract destination already exists in “%s”."),
                         dest_dir_name);
                message_extra =
                        _("Replacing it will remove all files in the folder.");
        } else {
                primary_text = g_strdup_printf (_("Replace file “%s”?"), dest_name);
                message = g_strdup_printf
                        (_("A file with the same name as the extract destination already exists in “%s”."),
                         dest_dir_name);
                message_extra = _("Replacing it will overwrite its contents.");
        }

        secondary_text = g_strdup_printf ("%s\n%s", message, message_extra);

        nautilus_file_conflict_dialog_set_text (data->dialog,
                                                primary_text,
                                                secondary_text);
}

static void
set_images (FileConflictDialogData *data)
{
        GdkPixbuf *source_pixbuf;
        GdkPixbuf *destination_pixbuf;

        destination_pixbuf = nautilus_file_get_icon_pixbuf (data->destination,
                                                            NAUTILUS_CANVAS_ICON_SIZE_SMALL,
                                                            TRUE,
                                                            1,
                                                            NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS);

        source_pixbuf = nautilus_file_get_icon_pixbuf (data->source,
                                                       NAUTILUS_CANVAS_ICON_SIZE_SMALL,
                                                       TRUE,
                                                       1,
                                                       NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS);

        nautilus_file_conflict_dialog_set_images (data->dialog,
                                                  destination_pixbuf,
                                                  source_pixbuf);

        g_object_unref (destination_pixbuf);
        g_object_unref (source_pixbuf);
}

static void
set_file_labels (FileConflictDialogData *data)
{
        GString *destination_label;
        GString *source_label;
        gboolean source_is_dir;
        gboolean dest_is_dir;
        gboolean should_show_type;
        g_autofree char *destination_mime_type = NULL;
        g_autofree char *destination_date = NULL;
        g_autofree char *destination_size = NULL;
        g_autofree char *destination_type = NULL;
        g_autofree char *source_date = NULL;
        g_autofree char *source_size = NULL;
        g_autofree char *source_type = NULL;

        source_is_dir = nautilus_file_is_directory (data->source);
        dest_is_dir = nautilus_file_is_directory (data->destination);

        destination_mime_type = nautilus_file_get_mime_type (data->destination);
        should_show_type = !nautilus_file_is_mime_type (data->source,
                                                        destination_mime_type);

        destination_date = nautilus_file_get_string_attribute (data->destination,
                                                               "date_modified");
        destination_size = nautilus_file_get_string_attribute (data->destination,
                                                               "size");

        if (should_show_type) {
                destination_type = nautilus_file_get_string_attribute (data->destination,
                                                                       "type");
        }

        destination_label = g_string_new (NULL);
        if (dest_is_dir) {
                g_string_append_printf (destination_label, "<b>%s</b>\n", _("Original folder"));
                g_string_append_printf (destination_label, "%s %s\n", _("Items:"), destination_size);
        }
        else {
                g_string_append_printf (destination_label, "<b>%s</b>\n", _("Original file"));
                g_string_append_printf (destination_label, "%s %s\n", _("Size:"), destination_size);
        }

        if (should_show_type) {
                g_string_append_printf (destination_label, "%s %s\n", _("Type:"), destination_type);
        }

        g_string_append_printf (destination_label, "%s %s", _("Last modified:"), destination_date);

        source_date = nautilus_file_get_string_attribute (data->source,
                                                          "date_modified");
        source_size = nautilus_file_get_string_attribute (data->source,
                                                          "size");

        if (should_show_type) {
                source_type = nautilus_file_get_string_attribute (data->source,
                                                                  "type");
        }

        source_label = g_string_new (NULL);
        if (source_is_dir) {
                g_string_append_printf (source_label, "<b>%s</b>\n", dest_is_dir ? _("Merge with") : _("Replace with"));
                g_string_append_printf (source_label, "%s %s\n", _("Items:"), source_size);
        }
        else {
                g_string_append_printf (source_label, "<b>%s</b>\n", _("Replace with"));
                g_string_append_printf (source_label, "%s %s\n", _("Size:"), source_size);
        }

        if (should_show_type) {
                g_string_append_printf (source_label, "%s %s\n", _("Type:"), source_type);
        }

        g_string_append_printf (source_label, "%s %s", _("Last modified:"), source_date);

        nautilus_file_conflict_dialog_set_file_labels (data->dialog,
                                                       destination_label->str,
                                                       source_label->str);

        g_string_free (destination_label, TRUE);
        g_string_free (source_label, TRUE);
}

static void
set_conflict_name (FileConflictDialogData *data)
{
        g_autofree gchar *edit_name;

        edit_name = nautilus_file_get_edit_name (data->destination);

        nautilus_file_conflict_dialog_set_conflict_name (data->dialog,
                                                         edit_name);
}

static void
set_replace_button_label (FileConflictDialogData *data)
{
        gboolean source_is_dir, dest_is_dir;

        source_is_dir = nautilus_file_is_directory (data->source);
        dest_is_dir = nautilus_file_is_directory (data->destination);

        if (source_is_dir && dest_is_dir) {
                nautilus_file_conflict_dialog_set_replace_button_label (data->dialog,
                                                                        _("Merge"));
        }
}

static void
file_icons_changed (NautilusFile           *file,
                    FileConflictDialogData *data)
{
        set_images (data);
}

static void
copy_move_file_list_ready_cb (GList    *files,
                              gpointer  user_data)
{
        FileConflictDialogData *data = user_data;
        g_autofree gchar *title;

        data->handle = NULL;

        if (nautilus_file_is_directory (data->source)) {
                title = g_strdup (nautilus_file_is_directory (data->destination) ?
                                  _("Merge Folder") :
                                  _("File and Folder conflict"));
        } else {
                title = g_strdup (nautilus_file_is_directory (data->destination) ?
                                  _("File and Folder conflict") :
                                  _("File conflict"));
        }

        gtk_window_set_title (GTK_WINDOW (data->dialog), title);

        set_copy_move_dialog_text (data);

        set_images (data);

        set_file_labels (data);

        set_conflict_name (data);

        set_replace_button_label (data);

        nautilus_file_monitor_add (data->source, data, NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);
        nautilus_file_monitor_add (data->destination, data, NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);

        data->src_handler_id = g_signal_connect (data->source, "changed",
                          G_CALLBACK (file_icons_changed), data);
        data->dest_handler_id = g_signal_connect (data->destination, "changed",
                          G_CALLBACK (file_icons_changed), data);
}

static void
extract_file_list_ready_cb (GList    *files,
                            gpointer  user_data)
{
        FileConflictDialogData *data = user_data;

        data->handle = NULL;

        gtk_window_set_title (GTK_WINDOW (data->dialog), _("Extract conflict"));

        set_extract_dialog_text (data);

        set_images (data);

        set_file_labels (data);

        set_conflict_name (data);

        nautilus_file_conflict_dialog_disable_skip (data->dialog);
        nautilus_file_conflict_dialog_disable_apply_to_all (data->dialog);

        nautilus_file_monitor_add (data->source, data, NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);
        nautilus_file_monitor_add (data->destination, data, NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);

        data->src_handler_id = g_signal_connect (data->source, "changed",
                          G_CALLBACK (file_icons_changed), data);
        data->dest_handler_id = g_signal_connect (data->destination, "changed",
                          G_CALLBACK (file_icons_changed), data);
}

static void
build_dialog_appearance (FileConflictDialogData *data)
{
        GList *files = NULL;

        files = g_list_prepend (files, data->source);
        files = g_list_prepend (files, data->destination);
        files = g_list_prepend (files, data->dest_dir);

        nautilus_file_list_call_when_ready (files,
                                            NAUTILUS_FILE_ATTRIBUTES_FOR_ICON,
                                            &data->handle, data->files_list_ready_cb, data);
        g_list_free (files);
}

static gboolean
run_file_conflict_dialog (gpointer user_data)
{
        FileConflictDialogData *data = user_data;
        int response_id;

        g_mutex_lock (&data->mutex);

        data->source = nautilus_file_get (data->src_name);
        data->destination = nautilus_file_get (data->dest_name);
        data->dest_dir = nautilus_file_get (data->dest_dir_name);

        data->dialog = nautilus_file_conflict_dialog_new (data->parent);

        build_dialog_appearance (data);

        response_id = gtk_dialog_run (GTK_DIALOG (data->dialog));

        if (data->handle != NULL) {
                nautilus_file_list_cancel_call_when_ready (data->handle);
        }

        if (data->src_handler_id) {
                g_signal_handler_disconnect (data->source, data->src_handler_id);
                nautilus_file_monitor_remove (data->source, data);
        }

        if (data->dest_handler_id) {
                g_signal_handler_disconnect (data->destination, data->dest_handler_id);
                nautilus_file_monitor_remove (data->destination, data);
        }

        if (response_id == CONFLICT_RESPONSE_RENAME) {
                data->response->new_name =
                        nautilus_file_conflict_dialog_get_new_name (data->dialog);
        } else if (response_id != GTK_RESPONSE_CANCEL ||
                   response_id != GTK_RESPONSE_NONE) {
                   data->response->apply_to_all =
                           nautilus_file_conflict_dialog_get_apply_to_all (data->dialog);
        }

        data->response->id = response_id;
        data->completed = TRUE;

        gtk_widget_destroy (GTK_WIDGET (data->dialog));

        g_cond_signal (&data->cond);
        g_mutex_unlock (&data->mutex);

        nautilus_file_unref (data->source);
        nautilus_file_unref (data->destination);
        nautilus_file_unref (data->dest_dir);

        return FALSE;
}

static FileConflictResponse *
file_conflict_ask_user_action (FileConflictDialogData *data)
{
        FileConflictResponse *response;

        response = g_slice_new0 (FileConflictResponse);
        response->new_name = NULL;
        data->response = response;

        data->completed = FALSE;
        g_mutex_init (&data->mutex);
        g_cond_init (&data->cond);

        g_mutex_lock (&data->mutex);

        g_main_context_invoke (NULL,
                               run_file_conflict_dialog,
                               data);
        /* Block the current thread until the dialog has been run on the main
         * thread
         */
        while (!data->completed) {
                g_cond_wait (&data->cond, &data->mutex);
        }

        g_mutex_unlock (&data->mutex);
        g_mutex_clear (&data->mutex);
        g_cond_clear (&data->cond);

        g_slice_free (FileConflictDialogData, data);

        return response;
}

FileConflictResponse *
copy_move_file_conflict_ask_user_action (GtkWindow *parent_window,
                                         GFile     *src_name,
                                         GFile     *dest_name,
                                         GFile     *dest_dir_name)
{
        FileConflictDialogData *data;

        data = g_slice_new0 (FileConflictDialogData);
        data->parent = parent_window;
        data->src_name = src_name;
        data->dest_name = dest_name;
        data->dest_dir_name = dest_dir_name;

        data->files_list_ready_cb = copy_move_file_list_ready_cb;

        return file_conflict_ask_user_action (data);
}

FileConflictResponse *
extract_file_conflict_ask_user_action (GtkWindow *parent_window,
                                       GFile     *src_name,
                                       GFile     *dest_name,
                                       GFile     *dest_dir_name)
{
        FileConflictDialogData *data;

        data = g_slice_new0 (FileConflictDialogData);
        data->parent = parent_window;
        data->src_name = src_name;
        data->dest_name = dest_name;
        data->dest_dir_name = dest_dir_name;

        data->files_list_ready_cb = extract_file_list_ready_cb;

        return file_conflict_ask_user_action (data);
}
