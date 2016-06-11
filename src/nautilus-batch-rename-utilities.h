#ifndef NAUTILUS_BATCH_RENAME_UTILITIES_H
#define NAUTILUS_BATCH_RENAME_UTILITIES_H

#include <gio/gio.h>
#include <gtk/gtk.h>

gchar* get_new_name             (NautilusBatchRenameModes  mode,
                                 gchar                     *file_name,
                                 gchar                     *entry_text,
                                 ...);

GList* get_new_names_list       (NautilusBatchRenameModes    mode,
                                 GList                       *selection,
                                 gchar                       *entry_text,
                                 gchar                       *replace_text);

gchar* get_new_display_name     (NautilusBatchRenameModes    mode,
                                 gchar                       *file_name,
                                 gchar                       *entry_text,
                                 gchar                       *replace_text);

GList* list_has_duplicates      (NautilusFilesView           *view,
                                 GList                       *names,
                                 GList                       *old_names);

gchar* concat                   (gchar                       *s1,
                                 gchar                       *s2);

#endif /* NAUTILUS_BATCH_RENAME_UTILITIES_H */