#ifndef NAUTILUS_BATCH_RENAME_UTILITIES_H
#define NAUTILUS_BATCH_RENAME_UTILITIES_H

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <tracker-sparql.h>

gchar* get_new_name             (NautilusBatchRenameMode   mode,
                                 gchar                     *file_name,
                                 gchar                     *entry_text,
                                 ...);

GList* get_new_names_list       (NautilusBatchRenameMode     mode,
                                 GList                       *selection,
                                 gchar                       *entry_text,
                                 gchar                       *replace_text);

gchar* get_new_display_name     (NautilusBatchRenameMode     mode,
                                 gchar                       *file_name,
                                 gchar                       *entry_text,
                                 gchar                       *replace_text);

GList* list_has_duplicates      (NautilusFilesView           *view,
                                 GList                       *names,
                                 GList                       *old_names);

GList* nautilus_batch_rename_sort (GList       *selection,
                                   SortingMode mode,
                                   ...);

gint compare_files_by_last_modified     (gconstpointer a,
                                         gconstpointer b);

gint compare_files_by_first_modified    (gconstpointer a,
                                         gconstpointer b);

gint compare_files_by_name_descending   (gconstpointer a,
                                         gconstpointer b);

gint compare_files_by_name_ascending    (gconstpointer a,
                                         gconstpointer b);

gint compare_files_by_first_created     (gconstpointer a,
                                         gconstpointer b);

gint compare_files_by_last_created      (gconstpointer a,
                                         gconstpointer b);

GHashTable* check_creation_date_for_selection  (GList *selection);
gboolean    nautilus_file_can_rename_files     (GList *selection);

#endif /* NAUTILUS_BATCH_RENAME_UTILITIES_H */