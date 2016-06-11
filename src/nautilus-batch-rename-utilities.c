#include "nautilus-batch-rename.h"
#include "nautilus-batch-rename-utilities.h"
#include "nautilus-files-view.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdarg.h>

#define MAX_DISPLAY_LEN 40

static gchar*
batch_rename_prepend (gchar *file_name,
                      gchar *entry_text)
{
        gchar *result;

        result = malloc (strlen (entry_text) + strlen (file_name) + 1);
        if (result == NULL) {
            return strdup (file_name);
        }
        sprintf (result, "%s%s", file_name, entry_text);

        return result;
}

static gchar*
batch_rename_append (gchar *file_name,
                     gchar *entry_text)
{
        gchar *result;

        result = malloc (strlen (entry_text) + strlen (file_name) + 1);
        if (result == NULL) {
            return strdup (file_name);
        }

        sprintf (result, "%s%s", entry_text, file_name);

        return result;
}

static gchar*
batch_rename_replace (gchar *string,
                      gchar *substr,
                      gchar *replacement)
{
        gchar *tok = NULL;
        gchar *newstr = NULL;
        gchar *oldstr = NULL;
        gint   skip_chars;

        if (substr == NULL || replacement == NULL) {
                return strdup (string);
        }

        if (strcmp (substr, "") == 0) {
                return strdup (string);
        }

        newstr = strdup (string);

        skip_chars = 0;

        while ((tok = strstr (newstr + skip_chars, substr))) {
                oldstr = newstr;
                newstr = malloc (strlen (oldstr) - strlen (substr) + strlen (replacement) + 1);

                if (newstr == NULL) {
                        g_free (oldstr);
                        return strdup (string);
                }

                memcpy (newstr, oldstr, tok - oldstr);
                memcpy (newstr + (tok - oldstr), replacement, strlen (replacement));
                memcpy (newstr + (tok - oldstr) + strlen( replacement ), tok + strlen ( substr ),
                        strlen (oldstr) - strlen (substr) - (tok - oldstr));
                memset (newstr + strlen (oldstr) - strlen (substr) + strlen (replacement) , '\0', 1 );

                skip_chars = strlen (oldstr) - strlen (tok) + strlen (replacement);
                g_free (oldstr);
        }

        return newstr;
}

gchar*
get_new_name (NautilusBatchRenameModes  mode,
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
get_new_names_list (NautilusBatchRenameModes    mode,
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
                                                 (gpointer) batch_rename_prepend (file_name, entry_text));

                if (mode == NAUTILUS_BATCH_RENAME_APPEND)
                        result = g_list_prepend (result,
                                                 (gpointer) batch_rename_append (file_name, entry_text));

                if (mode == NAUTILUS_BATCH_RENAME_REPLACE)
                        result = g_list_prepend (result,
                                                 (gpointer) batch_rename_replace (file_name, entry_text, replace_text));
                
                g_free (file_name);
        }

        return result;
}

gchar*
get_new_display_name (NautilusBatchRenameModes    mode,
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
                                                 (gpointer) (l1->data));
                }

                g_free (file_name);
        }
        return result;
}

gchar*
concat(gchar *s1, gchar *s2)
{
    gchar *result;

    result = malloc (strlen(s1) + strlen(s2) + 1);

    memcpy(result, s1, strlen(s1));
    memcpy(result + strlen(s1), s2, strlen(s2) + 1);

    return result;
}