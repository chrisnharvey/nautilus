
#include "nautilus-batch-rename.h"
#include "nautilus-batch-rename-utilities.h"
#include "nautilus-file.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdarg.h>
#include <eel/eel-vfs-extensions.h>

#define MAX_DISPLAY_LEN 40
#define MAX_FILTER_LEN 500

typedef struct {
        NautilusFile *file;
        gint         *position;
} CreateDateElem;

typedef struct {
        NautilusBatchRename *dialog;
        GHashTable          *hash_table;

        GList *selection_metadata;

        gboolean have_creation_date;
        gboolean have_equipment;
        gboolean have_season;
        gboolean have_episode_nr;
        gboolean have_track_nr;
        gboolean have_artist_name;
} QueryData;

static void cursor_callback (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data);

void
string_free (gpointer mem)
{
        if (mem != NULL)
                g_string_free (mem, TRUE);
}

static GString*
batch_rename_replace (gchar *string,
                      gchar *substr,
                      gchar *replacement)
{
        GString *new_string;
        gchar **splitted_string;
        gint i, n_splits;

        new_string = g_string_new ("");

        if (substr == NULL || replacement == NULL) {
                g_string_append (new_string, string);

                return new_string;
        }

        if (strcmp (substr, "") == 0) {
                g_string_append (new_string, string);

                return new_string;
        }

        splitted_string = g_strsplit (string, substr, -1);
        if (splitted_string == NULL) {
                g_string_append (new_string, string);

                return new_string;
        }

        n_splits = g_strv_length (splitted_string);

        for (i = 0; i < n_splits; i++) {
            g_string_append (new_string, splitted_string[i]);

            if (i != n_splits - 1)
                g_string_append (new_string, replacement);
        }

        g_strfreev (splitted_string);

        return new_string;
}

static gchar*
get_metadata (GList *selection_metadata,
              gchar *file_name,
              gchar *metadata)
{
        GList *l;
        FileMetadata *file_metadata;

        for (l = selection_metadata; l != NULL; l = l->next) {
                file_metadata = l->data;
                if (g_strcmp0 (file_name, file_metadata->file_name->str) == 0) {
                        if (g_strcmp0 (metadata, "creation_date") == 0 &&
                            file_metadata->creation_date != NULL &&
                            g_strcmp0 (file_metadata->creation_date->str, ""))
                                return file_metadata->creation_date->str;

                        if (g_strcmp0 (metadata, "equipment") == 0 &&
                            file_metadata->equipment != NULL &&
                            g_strcmp0 (file_metadata->equipment->str, ""))
                                return file_metadata->equipment->str;

                        if (g_strcmp0 (metadata, "season") == 0 &&
                            file_metadata->season != NULL &&
                            g_strcmp0 (file_metadata->season->str, ""))
                                return file_metadata->season->str;

                        if (g_strcmp0 (metadata, "episode_nr") == 0 &&
                            file_metadata->episode_nr != NULL &&
                            g_strcmp0 (file_metadata->episode_nr->str, ""))
                                return file_metadata->episode_nr->str;

                        if (g_strcmp0 (metadata, "track_nr") == 0 &&
                            file_metadata->track_nr != NULL &&
                            g_strcmp0 (file_metadata->track_nr->str, ""))
                                return file_metadata->track_nr->str;

                        if (g_strcmp0 (metadata, "artist_name") == 0 &&
                            file_metadata->artist_name != NULL &&
                            g_strcmp0 (file_metadata->artist_name->str, ""))
                                return file_metadata->artist_name->str;
                }
        }

        return NULL;
}

static GString*
batch_rename_format (NautilusFile *file,
                     GList        *tags_list,
                     GList        *selection_metadata,
                     gint          count)
{
        GList *l;
        GString *tag, *new_name;
        gboolean added_tag;
        gint start_offset, end_offset;
        g_autofree gchar *file_name, *extension;
        gchar *metadata, **splitted_date, *name;

        file_name = nautilus_file_get_display_name (file);
        extension = nautilus_file_get_extension (file);

        eel_filename_get_rename_region (file_name,
                                        &start_offset, &end_offset);

        new_name = g_string_new ("");

        for (l = tags_list; l != NULL; l = l->next) {
                tag = l->data;
                added_tag = FALSE;

                if (!added_tag && g_strcmp0 (tag->str, "[Original file name]") == 0) {
                        name = nautilus_file_get_display_name (file);
                        new_name = g_string_append_len (new_name,
                                                        name,
                                                        end_offset);
                        added_tag = TRUE;
                        g_free (name);
                }

                if (!added_tag && g_strcmp0 (tag->str, "[1, 2, 3]") == 0) {
                        g_string_append_printf (new_name, "%d", count);
                        added_tag = TRUE;
                }

                if (!added_tag && g_strcmp0 (tag->str, "[01, 02, 03]") == 0) {
                        if (count < 10)
                                g_string_append_printf (new_name, "0%d", count);
                        else
                                g_string_append_printf (new_name, "%d", count);
                        added_tag = TRUE;
                }

                if (!added_tag && g_strcmp0 (tag->str, "[001, 002, 003]") == 0) {
                        if (count < 10)
                                g_string_append_printf (new_name, "00%d", count);
                        else
                                if (count < 100)
                                        g_string_append_printf (new_name, "0%d", count);
                                else
                                        g_string_append_printf (new_name, "%d", count);
                        added_tag = TRUE;
                }

                if (!added_tag && g_strcmp0 (tag->str, "[Camera model]") == 0) {
                        metadata = get_metadata (selection_metadata, file_name, "equipment");

                        if (metadata != NULL) {
                                new_name = g_string_append (new_name, metadata);
                                added_tag = TRUE;
                        }
                }

                if (!added_tag && g_strcmp0 (tag->str, "[Date taken]") == 0) {
                        metadata = get_metadata (selection_metadata, file_name, "creation_date");

                        if (metadata != NULL) {
                                splitted_date = g_strsplit (metadata, "T", -1);

                                new_name = g_string_append (new_name, splitted_date[0]);
                                added_tag = TRUE;

                                g_strfreev (splitted_date);
                        }
                }

                if (!added_tag && g_strcmp0 (tag->str, "[Season nr]") == 0) {
                        metadata = get_metadata (selection_metadata, file_name, "season");

                        if (metadata != NULL) {
                                new_name = g_string_append (new_name, metadata);
                                added_tag = TRUE;
                        }
                }

                if (!added_tag && g_strcmp0 (tag->str, "[Episode nr]") == 0) {
                        metadata = get_metadata (selection_metadata, file_name, "episode_nr");

                        if (metadata != NULL) {
                                new_name = g_string_append (new_name, metadata);
                                added_tag = TRUE;
                        }
                }

                if (!added_tag && g_strcmp0 (tag->str, "[Track nr]") == 0) {
                        metadata = get_metadata (selection_metadata, file_name, "track_nr");

                        if (metadata != NULL) {
                                new_name = g_string_append (new_name, metadata);
                                added_tag = TRUE;
                        }
                }

                if (!added_tag && g_strcmp0 (tag->str, "[Artist name]") == 0) {
                        metadata = get_metadata (selection_metadata, file_name, "artist_name");

                        if (metadata != NULL) {
                                new_name = g_string_append (new_name, metadata);
                                added_tag = TRUE;
                        }
                }

                if (!added_tag)
                        new_name = g_string_append (new_name, tag->str);
        }

        if (g_strcmp0 (new_name->str, "") == 0)
                new_name = g_string_append (new_name, file_name);
        else
                if (extension != NULL)
                        new_name = g_string_append (new_name, extension);

        return new_name;
}

GList*
get_new_names_list (NautilusBatchRenameMode      mode,
                    GList                       *selection,
                    GList                       *tags_list,
                    GList                       *selection_metadata,
                    gchar                       *entry_text,
                    gchar                       *replace_text)
{
        GList *l;
        GList *result;
        GString *file_name;
        NautilusFile *file;
        gchar *name;
        gint count;

        result = NULL;
        count = 1;
        file_name = g_string_new ("");

        for (l = selection; l != NULL; l = l->next) {
                file = NAUTILUS_FILE (l->data);

                name = nautilus_file_get_name (file);
                g_string_append (file_name, name);

                /* get the new name here and add it to the list*/
                if (mode == NAUTILUS_BATCH_RENAME_FORMAT) {
                        result = g_list_prepend (result,
                                                 batch_rename_format (file,
                                                                      tags_list,
                                                                      selection_metadata,
                                                                      count++));
                }

                if (mode == NAUTILUS_BATCH_RENAME_REPLACE)
                        result = g_list_prepend (result,
                                                 batch_rename_replace (file_name->str,
                                                                       entry_text,
                                                                       replace_text));
                
                g_string_erase (file_name, 0, -1);
                g_free (name);
        }

        g_string_free (file_name, TRUE);

        return result;
}

/* If there is a file that generates a conflict, there is a case where that isn't
 * actually a conflict. This case is when the file that generates the conflict is
 * in the selection and this file changed it's name */
gboolean
file_name_changed (GList        *selection,
                   GList        *new_names,
                   GString      *old_name,
                   gchar        *parent_uri)
{
        GList *l1, *l2;
        NautilusFile *selection_file;
        gchar *name1;
        GString *new_name;
        gchar *selection_parent_uri;

        l2 = new_names;

        for (l1 = selection; l1 != NULL; l1 = l1->next) {
                selection_file = NAUTILUS_FILE (l1->data);
                name1 = nautilus_file_get_name (selection_file);

                selection_parent_uri = nautilus_file_get_parent_uri (selection_file);

                if (g_strcmp0 (name1, old_name->str) == 0) {
                        new_name = l2->data;

                        /* if the name didn't change, then there's a conflict */
                        if (g_string_equal (old_name, new_name) &&
                            (parent_uri == NULL || g_strcmp0 (parent_uri, selection_parent_uri) == 0))
                                return FALSE;


                        /* if this file exists and it changed it's name, then there's no
                         * conflict */
                        return TRUE;
                }

                l2 = l2->next;

                g_free (selection_parent_uri);
        }

        /* such a file doesn't exist so there actually is a conflict */
        return FALSE;
}

static void
got_files_callback (NautilusDirectory *directory, GList *files, gpointer callback_data)
{
        check_conflict_for_file (NAUTILUS_BATCH_RENAME (callback_data),
                                 directory,
                                 files);
}

GList*
list_has_duplicates (NautilusBatchRename *dialog,
                     NautilusDirectory   *model,
                     GList               *new_names,
                     GList               *selection,
                     GList               *parents_list,
                     gboolean             same_parent,
                     GCancellable        *cancellable)
{
        GList *directory_files, *l1, *l2, *l3, *result;
        NautilusFile *file1, *file2;
        GString *file_name1, *file_name2, *file_name3, *new_name;
        NautilusDirectory *parent;
        gboolean is_renameable_desktop_file, have_conflict;
        gchar *name;

        result = NULL;

        file_name1 = g_string_new ("");
        file_name2 = g_string_new ("");

        if (!same_parent) {
                for (l1 = parents_list; l1 != NULL; l1 = l1->next) {
                        parent = nautilus_directory_get_by_uri (l1->data);

                        nautilus_directory_call_when_ready (parent,
                                                            NAUTILUS_FILE_ATTRIBUTE_INFO,
                                                            TRUE,
                                                            got_files_callback,
                                                            dialog);

                }
            return NULL;
        }

        directory_files = nautilus_directory_get_file_list (model);
        l3 = selection;

        for (l1 = new_names; l1 != NULL; l1 = l1->next) {
                if (g_cancellable_is_cancelled (cancellable)) {
                        g_list_free_full (result, g_free);
                        g_list_free_full (new_names, string_free);
                        break;
                }

                file1 = NAUTILUS_FILE (l3->data);
                new_name = l1->data;
                is_renameable_desktop_file = nautilus_file_is_mime_type (file1, "application/x-desktop");

                have_conflict = FALSE;

                if (!is_renameable_desktop_file && strstr (new_name->str, "/") != NULL) {
                        result = g_list_prepend (result, strdup (new_name->str));

                        continue;
                }

                name = nautilus_file_get_name (file1);
                g_string_erase (file_name1, 0, -1);
                g_string_append (file_name1, name);

                g_free (name);

                /* check for duplicate only if the name has changed */
                if (!g_string_equal (new_name, file_name1)) {
                        /* check with already existing files */
                        for (l2 = directory_files; l2 != NULL; l2 = l2->next) {
                                file2 = NAUTILUS_FILE (l2->data);

                                name = nautilus_file_get_name (file2);
                                g_string_erase (file_name2, 0, -1);
                                g_string_append (file_name2, name);
                                g_free (name);

                                if (g_string_equal (new_name, file_name2) &&
                                    !file_name_changed (selection, new_names, new_name, NULL)) {
                                        result = g_list_prepend (result, strdup (new_name->str));
                                        have_conflict = TRUE;

                                        break;
                                }
                        }

                        /* check with files that will result from the batch rename, unless
                         * this file already has a conflict */
                        if (!have_conflict)
                                for (l2 = new_names; l2 != NULL; l2 = l2->next) {
                                        file_name3 = l2->data;

                                        if (l1 != l2 && g_string_equal (new_name, file_name3)) {
                                                result = g_list_prepend (result, strdup (new_name->str));

                                                break;
                                        }
                                }
                }

                l3 = l3->next;
        }

        g_string_free (file_name1, TRUE);
        g_string_free (file_name2, TRUE);
        nautilus_file_list_free (directory_files);

        if (!g_cancellable_is_cancelled (cancellable))
                g_list_free_full (new_names, string_free);

        return g_list_reverse (result);
}

gint
compare_files_by_name_ascending (gconstpointer a,
                                 gconstpointer b)
{
        NautilusFile *file1;
        NautilusFile *file2;

        file1 = NAUTILUS_FILE (a);
        file2 = NAUTILUS_FILE (b);

        return nautilus_file_compare_for_sort (file1,file2,
                                               NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
                                               FALSE, FALSE);
}

gint
compare_files_by_name_descending (gconstpointer a,
                                  gconstpointer b)
{
        NautilusFile *file1;
        NautilusFile *file2;

        file1 = NAUTILUS_FILE (a);
        file2 = NAUTILUS_FILE (b);

        return nautilus_file_compare_for_sort (file1,file2,
                                               NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
                                               FALSE, TRUE);
}

gint
compare_files_by_first_modified (gconstpointer a,
                                 gconstpointer b)
{
        NautilusFile *file1;
        NautilusFile *file2;

        file1 = NAUTILUS_FILE (a);
        file2 = NAUTILUS_FILE (b);

        return nautilus_file_compare_for_sort (file1,file2,
                                               NAUTILUS_FILE_SORT_BY_MTIME,
                                               FALSE, FALSE);
}

gint
compare_files_by_last_modified (gconstpointer a,
                                gconstpointer b)
{
        NautilusFile *file1;
        NautilusFile *file2;

        file1 = NAUTILUS_FILE (a);
        file2 = NAUTILUS_FILE (b);

        return nautilus_file_compare_for_sort (file1,file2,
                                               NAUTILUS_FILE_SORT_BY_MTIME,
                                               FALSE, TRUE);
}

gint
compare_files_by_first_created (gconstpointer a,
                                gconstpointer b)
{
        CreateDateElem *elem1;
        CreateDateElem *elem2;

        elem1 = (CreateDateElem*) a;
        elem2 = (CreateDateElem*) b;

        return *(elem1->position) - *(elem2->position);
}

gint
compare_files_by_last_created (gconstpointer a,
                               gconstpointer b)
{
        CreateDateElem *elem1;
        CreateDateElem *elem2;

        elem1 = (CreateDateElem*) a;
        elem2 = (CreateDateElem*) b;

        return *(elem2->position) - *(elem1->position);
}

GList*
nautilus_batch_rename_sort (GList       *selection,
                            SortingMode  mode,
                            GHashTable  *hash_table)
{
        GList *l,*l2;
        NautilusFile *file;
        GList *createDate_list, *createDate_list_sorted;
        gchar *name;

        if (mode == ORIGINAL_ASCENDING)
                return g_list_sort (selection, compare_files_by_name_ascending);

        if (mode == ORIGINAL_DESCENDING) {
                return g_list_sort (selection, compare_files_by_name_descending);
        }

        if (mode == FIRST_MODIFIED) {
            return g_list_sort (selection, compare_files_by_first_modified);
        }

        if (mode == LAST_MODIFIED) {
            return g_list_sort (selection, compare_files_by_last_modified);
        }

        if (mode == FIRST_CREATED || mode == LAST_CREATED) {
                createDate_list = NULL;

                for (l = selection; l != NULL; l = l->next) {
                        CreateDateElem *elem;
                        elem = g_malloc (sizeof (NautilusFile*) + sizeof (gint*));

                        file = NAUTILUS_FILE (l->data);

                        name = nautilus_file_get_name (file);
                        elem->file = file;
                        elem->position = (gint*) g_hash_table_lookup (hash_table, name);
                        g_free (name);

                        createDate_list = g_list_prepend (createDate_list, elem);
                }

                if (mode == FIRST_CREATED)
                        createDate_list_sorted = g_list_sort (createDate_list,
                                                              compare_files_by_first_created);
                else
                        createDate_list_sorted = g_list_sort (createDate_list,
                                                              compare_files_by_last_created);

                for (l = selection, l2 = createDate_list_sorted; l2 != NULL; l = l->next, l2 = l2->next) {
                        CreateDateElem *elem = l2->data;
                        l->data = elem->file;
                }

                g_list_free (createDate_list);
        }

        return selection;
}

static void
cursor_next (QueryData           *data,
             TrackerSparqlCursor *cursor)
{
        tracker_sparql_cursor_next_async (cursor,
                                          NULL,
                                          cursor_callback,
                                          data);
}

static void
cursor_callback (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
        GHashTable *hash_table;
        TrackerSparqlCursor *cursor;
        gboolean success;
        gint *value;
        QueryData *data;
        GError *error;
        const gchar *file_name;
        GList *l;
        FileMetadata *metadata;

        error = NULL;
        metadata = NULL;

        cursor = TRACKER_SPARQL_CURSOR (object);
        data = user_data;
        hash_table = data->hash_table;

        success = tracker_sparql_cursor_next_finish (cursor, result, &error);
        if (!success) {
                g_clear_error (&error);
                g_clear_object (&cursor);


                query_finished (data->dialog, data->hash_table, data->selection_metadata);

                return;
        }

        /* creation date used for sorting criteria */
        if (tracker_sparql_cursor_get_string (cursor, 1, NULL) == NULL) {
                if (hash_table != NULL)
                        g_hash_table_destroy (hash_table);

                data->hash_table = NULL;
                data->have_creation_date = FALSE;
        } else {
                if (data->have_creation_date){
                        value = g_malloc (sizeof(int));
                        *value = g_hash_table_size (hash_table);

                        g_hash_table_insert (hash_table,
                             strdup (tracker_sparql_cursor_get_string (cursor, 0, NULL)),
                             value);
                }
        }
        file_name = tracker_sparql_cursor_get_string (cursor, 0, NULL);
        for (l = data->selection_metadata; l != NULL; l = l->next) {
                metadata = l->data;

                if (g_strcmp0 (file_name, metadata->file_name->str) == 0)
                        break;
        }

        /* Metadata to be used in file name
         * creation date */
        if (data->have_creation_date && tracker_sparql_cursor_get_string (cursor, 1, NULL) == NULL) {
                data->have_creation_date = FALSE;

                for (l = data->selection_metadata; l != NULL; l = l->next) {
                        metadata = l->data;

                        g_string_free (metadata->creation_date, TRUE);
                        metadata->creation_date = NULL;
                }
        } else {
                if (data->have_creation_date)
                        g_string_append (metadata->creation_date,
                                         tracker_sparql_cursor_get_string (cursor, 1, NULL));
        }

        /* equipment */
        if (data->have_equipment && tracker_sparql_cursor_get_string (cursor, 2, NULL) == NULL) {
                data->have_equipment = FALSE;

                for (l = data->selection_metadata; l != NULL; l = l->next) {
                        metadata = l->data;

                        g_string_free (metadata->equipment, TRUE);
                        metadata->equipment = NULL;
                }
        } else {
                if (data->have_equipment)
                        g_string_append (metadata->equipment,
                                         tracker_sparql_cursor_get_string (cursor, 2, NULL));
        }

        /* season nr */
        if (data->have_season && tracker_sparql_cursor_get_string (cursor, 3, NULL) == NULL) {
                data->have_season = FALSE;

                for (l = data->selection_metadata; l != NULL; l = l->next) {
                        metadata = l->data;

                        g_string_free (metadata->season, TRUE);
                        metadata->season = NULL;
                }
        } else {
                if (data->have_season)
                        g_string_append (metadata->season,
                                         tracker_sparql_cursor_get_string (cursor, 3, NULL));
        }

        /* episode nr */
        if (data->have_episode_nr && tracker_sparql_cursor_get_string (cursor, 4, NULL) == NULL) {
                data->have_episode_nr = FALSE;

                for (l = data->selection_metadata; l != NULL; l = l->next) {
                        metadata = l->data;

                        g_string_free (metadata->episode_nr, TRUE);
                        metadata->episode_nr = NULL;
                }
        } else {
                if (data->have_episode_nr)
                        g_string_append (metadata->episode_nr,
                                         tracker_sparql_cursor_get_string (cursor, 4, NULL));
        }

        /* track number */
        if (data->have_track_nr && tracker_sparql_cursor_get_string (cursor, 5, NULL) == NULL) {
                data->have_track_nr = FALSE;
                for (l = data->selection_metadata; l != NULL; l = l->next) {
                        metadata = l->data;

                        g_string_free (metadata->track_nr, TRUE);
                        metadata->track_nr = NULL;
                }
        } else {
                if (data->have_track_nr)
                        g_string_append (metadata->track_nr,
                                         tracker_sparql_cursor_get_string (cursor, 5, NULL));
        }

        /* artist name */
        if (data->have_artist_name && tracker_sparql_cursor_get_string (cursor, 6, NULL) == NULL) {
                data->have_artist_name = FALSE;

                for (l = data->selection_metadata; l != NULL; l = l->next) {
                        metadata = l->data;

                        g_string_free (metadata->artist_name, TRUE);
                        metadata->artist_name = NULL;
                }
        } else {
                if (data->have_artist_name)
                        g_string_append (metadata->artist_name,
                                         tracker_sparql_cursor_get_string (cursor, 6, NULL));
        }

        /* Get next */
        cursor_next (data, cursor);
}

static void
query_callback (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
        TrackerSparqlConnection *connection;
        TrackerSparqlCursor *cursor;
        QueryData *data;
        GError *error;

        error = NULL;

        connection = TRACKER_SPARQL_CONNECTION (object);
        data = user_data;

        cursor = tracker_sparql_connection_query_finish (connection,
                                                         result,
                                                         &error);

        if (error != NULL) {
                g_error_free (error);

                query_finished (data->dialog, data->hash_table, data->selection_metadata);
        } else {
                cursor_next (data, cursor);
        }
}

void
check_metadata_for_selection (NautilusBatchRename *dialog,
                              GList               *selection)
{
        TrackerSparqlConnection *connection;
        GString *query;
        GHashTable *hash_table;
        GList *l;
        NautilusFile *file;
        GError *error;
        QueryData *data;
        gchar *file_name;
        FileMetadata *metadata;
        GList *selection_metadata;

        error = NULL;
        selection_metadata = NULL;

        query = g_string_new ("SELECT nfo:fileName(?file) nie:contentCreated(?file) "
                              "nfo:model(nfo:equipment(?file)) nmm:season(?file) nmm:episodeNumber(?file) "
                              "nmm:trackNumber(?file) nmm:artistName(?file) "
                              "WHERE { ?file a nfo:FileDataObject. ");

        g_string_append_printf (query,
                                "FILTER(tracker:uri-is-parent('%s', nie:url(?file))) ",
                                nautilus_file_get_parent_uri (NAUTILUS_FILE (selection->data)));

        for (l = selection; l != NULL; l = l->next) {
                file = NAUTILUS_FILE (l->data);
                file_name = nautilus_file_get_name (file);

                if (l == selection)
                        g_string_append_printf (query,
                                                "FILTER (nfo:fileName(?file) = '%s' ",
                                                file_name);
                else
                        g_string_append_printf (query,
                                                "|| nfo:fileName(?file) = '%s' ",
                                                file_name);

                metadata = g_malloc (9 * sizeof (GString*));
                metadata->file_name = g_string_new (file_name);
                metadata->creation_date = g_string_new ("");
                metadata->equipment = g_string_new ("");
                metadata->season = g_string_new ("");
                metadata->episode_nr = g_string_new ("");
                metadata->track_nr = g_string_new ("");
                metadata->artist_name = g_string_new ("");

                selection_metadata = g_list_append (selection_metadata, metadata);

                g_free (file_name);
        }

        g_string_append (query, ")} ORDER BY ASC(nie:contentCreated(?file))");

        connection = tracker_sparql_connection_get (NULL, &error);
        if (!connection) {
                g_error_free (error);

                return;
        }

        hash_table = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            (GDestroyNotify) g_free,
                                            (GDestroyNotify) g_free);

        data = g_malloc (sizeof (NautilusBatchRename*) +
                         sizeof (GHashTable*) +
                         sizeof (GList*) +
                         9 * sizeof (gboolean));
        data->hash_table = hash_table;
        data->dialog = dialog;
        data->selection_metadata = selection_metadata;

        data->have_season = TRUE;
        data->have_creation_date = TRUE;
        data->have_season = TRUE;
        data->have_artist_name = TRUE;
        data->have_track_nr = TRUE;
        data->have_equipment = TRUE;
        data->have_episode_nr = TRUE;

        /* Make an asynchronous query to the store */
        tracker_sparql_connection_query_async (connection,
                                               query->str,
                                               NULL,
                                               query_callback,
                                               data);

        g_object_unref (connection);
        g_string_free (query, TRUE);
}

gboolean
selection_has_single_parent (GList *selection)
{
        GList *l;
        NautilusFile *file;
        GString *parent_name1, *parent_name2;

        file = NAUTILUS_FILE (selection->data);

        parent_name2 = g_string_new ("");
        parent_name1 = g_string_new ("");
        g_string_append (parent_name1, nautilus_file_get_parent_uri (file));

        for (l = selection->next; l != NULL; l = l->next) {
                file = NAUTILUS_FILE (l->data);

                g_string_erase (parent_name2, 0, -1);
                g_string_append (parent_name2, nautilus_file_get_parent_uri (file));

                if (!g_string_equal (parent_name1, parent_name2))
                        return FALSE;
        }

        g_string_free (parent_name1, TRUE);
        g_string_free (parent_name2, TRUE);

        return TRUE;
}

GList*
distinct_file_parents (GList *selection)
{
        GList *result, *l1, *l2;
        NautilusFile *file;
        gboolean exists;
        gchar *parent_uri;

        result = NULL;

        for (l1 = selection; l1 != NULL; l1 = l1->next) {
                exists = FALSE;

                file = NAUTILUS_FILE (l1->data);
                parent_uri = nautilus_file_get_parent_uri (file);

                for (l2 = result; l2 != NULL; l2 = l2->next)
                        if (g_strcmp0 (parent_uri, l2->data) == 0) {
                                exists = TRUE;
                                break;
                        }

                if (!exists) {
                        result = g_list_prepend (result, parent_uri);
                } else {
                        g_free (parent_uri);
                }
        }

        return result;
}