
#include "nautilus-batch-rename.h"
#include "nautilus-batch-rename-utilities.h"
#include "nautilus-file.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdarg.h>

#define MAX_DISPLAY_LEN 40
#define MAX_FILTER_LEN 500

typedef struct {
        NautilusFile *file;
        gint *position;
} CreateDateElem;

static gchar*
batch_rename_append (gchar *file_name,
                     gchar *entry_text)
{
        gchar *result;
        gint len;

        len = strlen (entry_text) + strlen (file_name) + 1;
        result = g_malloc (len);

        if (result == NULL) {
            return strdup (file_name);
        }
        g_snprintf (result, len, "%s%s", file_name, entry_text);

        return result;
}

static gchar*
batch_rename_prepend (gchar *file_name,
                      gchar *entry_text)
{
        gchar *result;
        gint len;

        len = strlen (entry_text) + strlen (file_name) + 1;
        result = g_malloc (len);

        if (result == NULL) {
            return strdup (file_name);
        }

        g_snprintf (result, len, "%s%s", entry_text, file_name);

        return result;
}

static gchar*
batch_rename_replace (gchar *string,
                      gchar *substr,
                      gchar *replacement)
{
        GString *new_string;
        gchar **splitted_string;
        gint i, n_splits;

        if (substr == NULL || replacement == NULL) {
                return strdup (string);
        }

        if (strcmp (substr, "") == 0) {
                return strdup (string);
        }

        splitted_string = g_strsplit (string, substr, -1);
        if (splitted_string == NULL)
            return string;

        n_splits = g_strv_length (splitted_string);

        new_string = g_string_new ("");

        i = 0;

        while (i < n_splits) {
            g_string_append (new_string, splitted_string[i]);

            if (i != n_splits - 1)
                g_string_append (new_string, replacement);
            i++;
        }

        return new_string->str;
}

gchar*
get_new_name (NautilusBatchRenameMode   mode,
              gchar                     *file_name,
              gchar                     *entry_text,
              ...)
{
        va_list args;
        gchar *result;

        result = NULL;

        if (mode == NAUTILUS_BATCH_RENAME_REPLACE) {

                va_start (args, entry_text);

                result = batch_rename_replace (file_name, entry_text, va_arg(args, gchar*));

                va_end (args);
        }

        if (mode == NAUTILUS_BATCH_RENAME_APPEND)
                result = batch_rename_append (file_name, entry_text);

        if (mode == NAUTILUS_BATCH_RENAME_PREPEND)
                result = batch_rename_prepend (file_name, entry_text);

        return result;
}

GList*
get_new_names_list (NautilusBatchRenameMode     mode,
                    GList                       *selection,
                    gchar                       *entry_text,
                    gchar                       *replace_text)
{
        GList *l;
        GList *result;
        gchar *file_name;
        NautilusFile *file;

        result = NULL;

        for (l = selection; l != NULL; l = l->next) {
                file = NAUTILUS_FILE (l->data);

                file_name = strdup (nautilus_file_get_name (file));

                /* get the new name here and add it to the list*/
                if (mode == NAUTILUS_BATCH_RENAME_PREPEND)
                        result = g_list_prepend (result,
                                                 batch_rename_prepend (file_name, entry_text));

                if (mode == NAUTILUS_BATCH_RENAME_APPEND)
                        result = g_list_prepend (result,
                                                 batch_rename_append (file_name, entry_text));

                if (mode == NAUTILUS_BATCH_RENAME_REPLACE)
                        result = g_list_prepend (result,
                                                 batch_rename_replace (file_name, entry_text, replace_text));
                
                g_free (file_name);
        }

        return result;
}

gchar*
get_new_display_name (NautilusBatchRenameMode     mode,
                      gchar                       *file_name,
                      gchar                       *entry_text,
                      gchar                       *replace_text)
{
        gchar *result;

        result = get_new_name (mode, file_name, entry_text, replace_text);

        return result;
}

GList*
list_has_duplicates (NautilusFilesView *view,
                     GList             *new_names,
                     GList             *old_names)
{
        GList *l1, *l2;
        GList *result;
        NautilusFile *file;
        gchar *file_name;

        result = NULL;

        for (l1 = new_names, l2 = old_names; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next) {
                file = NAUTILUS_FILE (l2->data);
                file_name = strdup (nautilus_file_get_name (file));

                if (strcmp (l1->data, file_name) != 0 && file_with_name_exists (view, l1->data) == TRUE) {
                        result = g_list_prepend (result,
                                                 l1->data);
                }

                g_free (file_name);
        }
        return result;
}

gint
compare_files_by_name_ascending (gconstpointer a,
                                 gconstpointer b)
{
        NautilusFile *f1;
        NautilusFile *f2;

        f1 = NAUTILUS_FILE (a);
        f2 = NAUTILUS_FILE (b);

        return nautilus_file_compare_for_sort (f1,f2,
                                               NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
                                               FALSE, FALSE);
}

gint
compare_files_by_name_descending (gconstpointer a,
                                  gconstpointer b)
{
        NautilusFile *f1;
        NautilusFile *f2;

        f1 = NAUTILUS_FILE (a);
        f2 = NAUTILUS_FILE (b);

        return nautilus_file_compare_for_sort (f1,f2,
                                               NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
                                               FALSE, TRUE);
}

gint
compare_files_by_first_modified (gconstpointer a,
                                 gconstpointer b)
{
        NautilusFile *f1;
        NautilusFile *f2;

        f1 = NAUTILUS_FILE (a);
        f2 = NAUTILUS_FILE (b);

        return nautilus_file_compare_for_sort (f1,f2,
                                               NAUTILUS_FILE_SORT_BY_MTIME,
                                               FALSE, FALSE);
}

gint
compare_files_by_last_modified (gconstpointer a,
                                gconstpointer b)
{
        NautilusFile *f1;
        NautilusFile *f2;

        f1 = NAUTILUS_FILE (a);
        f2 = NAUTILUS_FILE (b);

        return nautilus_file_compare_for_sort (f1,f2,
                                               NAUTILUS_FILE_SORT_BY_MTIME,
                                               FALSE, TRUE);
}

gint
compare_files_by_first_created (gconstpointer a,
                                gconstpointer b)
{
        return *(((CreateDateElem*) a)->position) - *(((CreateDateElem*) b)->position);
}

gint
compare_files_by_last_created (gconstpointer a,
                               gconstpointer b)
{
        return *(((CreateDateElem*) b)->position) - *(((CreateDateElem*) a)->position);
}

GList*
nautilus_batch_rename_sort (GList *selection,
                            SortingMode mode,
                            ...)
{
        GList *l,*l2;
        va_list args;
        GHashTable *hash_table;
        NautilusFile *file;
        GList *createDate_list, *createDate_list_sorted;

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
                va_start (args, mode);

                hash_table = va_arg(args, GHashTable*);

                createDate_list = NULL;

                for (l = selection; l != NULL; l = l->next) {
                        CreateDateElem *elem;
                        elem = g_malloc (sizeof (CreateDateElem*));

                        file = NAUTILUS_FILE (l->data);

                        elem->file = file;
                        elem->position = (gint*) g_hash_table_lookup (hash_table, nautilus_file_get_name (file));

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

                va_end (args);
                g_list_free (createDate_list);
        }

        return selection;
}

GHashTable*
check_creation_date_for_selection (GList *selection)
{
        GError *error = NULL;
        TrackerSparqlConnection *connection;
        TrackerSparqlCursor *cursor;
        gchar *filter1, *filter2, *sparql, *tmp;
        GHashTable *hash_table;
        GList *l;
        gint i, *value;
        NautilusFile *file;
        gchar *query = "SELECT nfo:fileName(?file) nie:contentCreated(?file) WHERE { ?file a nfo:FileDataObject. ";

        filter1 = g_malloc (MAX_FILTER_LEN);
        g_snprintf (filter1, MAX_FILTER_LEN, "FILTER(tracker:uri-is-parent('%s', nie:url(?file)))",
                 nautilus_file_get_parent_uri (NAUTILUS_FILE (selection->data)));

        sparql = g_strconcat (query, filter1, NULL);

        for (l = selection; l != NULL; l = l->next) {
                filter2 = g_malloc (MAX_FILTER_LEN);

                file = NAUTILUS_FILE (l->data);

                if (l == selection)
                        g_snprintf (filter2, MAX_FILTER_LEN, "FILTER (nfo:fileName(?file) = '%s' ", nautilus_file_get_name (file));
                else
                        g_snprintf (filter2, MAX_FILTER_LEN, "|| nfo:fileName(?file) = '%s'", nautilus_file_get_name (file));

                tmp = sparql;
                sparql = g_strconcat (sparql, filter2, NULL);

                g_free (tmp);
                g_free (filter2);
        }

        tmp = sparql;
        sparql = g_strconcat (sparql, ")} ORDER BY ASC(nie:contentCreated(?file))", NULL);

        connection = tracker_sparql_connection_get (NULL, &error);
        if (!connection)
            return NULL;

        /* Make a synchronous query to the store */
        cursor = tracker_sparql_connection_query (connection,
                                                  sparql,
                                                  NULL,
                                                  &error);

        if (error)
                return NULL;

        /* Check results */
        if (!cursor) {
                return NULL;
        } else {
                hash_table = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    (GDestroyNotify) g_free,
                                                    (GDestroyNotify) g_free);
                i = 0;

                /* Iterate, synchronously, the results */
                while (tracker_sparql_cursor_next (cursor, NULL, &error)) {
                        value = g_malloc (sizeof(int));
                        *value = i++;

                        g_hash_table_insert (hash_table,
                                             strdup(tracker_sparql_cursor_get_string (cursor, 0, NULL)),
                                             value);

                        if (tracker_sparql_cursor_get_string (cursor, 1, NULL) == NULL) {
                                g_object_unref (connection);
                                g_hash_table_destroy (hash_table);
                                g_free (filter1);

                                return NULL;
                        }
                    }

                g_object_unref (cursor);
        }

        g_object_unref (connection);
        g_free (filter1);
        g_free (sparql);

        return hash_table;
}

gboolean
nautilus_file_can_rename_files (GList *selection)
{
    GList *l;
    NautilusFile *file;

    for (l = selection; l != NULL; l = l->next) {
        file = NAUTILUS_FILE (l->data);

        if (!nautilus_file_can_rename (file))
            return FALSE;
    }

    return TRUE;
}
