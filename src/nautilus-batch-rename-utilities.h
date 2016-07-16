#ifndef NAUTILUS_BATCH_RENAME_UTILITIES_H
#define NAUTILUS_BATCH_RENAME_UTILITIES_H

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <tracker-sparql.h>

GList* get_new_names_list                       (NautilusBatchRenameMode      mode,
                                                 GList                       *selection,
                                                 gchar                       *entry_text,
                                                 gchar                       *replace_text);

GList* list_has_duplicates                      (NautilusDirectory           *model,
                                                 GList                       *names,
                                                 GList                       *selection,
                                                 gboolean                     same_parent);

GList* nautilus_batch_rename_sort               (GList                       *selection,
                                                 SortingMode                  mode,
                                                 GHashTable                  *create_date);

gint compare_files_by_last_modified             (gconstpointer a,
                                                 gconstpointer b);

gint compare_files_by_first_modified            (gconstpointer a,
                                                 gconstpointer b);

gint compare_files_by_name_descending           (gconstpointer a,
                                                 gconstpointer b);

gint compare_files_by_name_ascending            (gconstpointer a,
                                                 gconstpointer b);

gint compare_files_by_first_created             (gconstpointer a,
                                                 gconstpointer b);

gint compare_files_by_last_created              (gconstpointer a,
                                                 gconstpointer b);

void check_creation_date_for_selection          (NautilusBatchRename *dialog,
                                                 GList               *selection);

gboolean selection_has_single_parent            (GList *selection);

void string_free                                (gpointer mem);

#endif /* NAUTILUS_BATCH_RENAME_UTILITIES_H */