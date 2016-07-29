
#ifndef NAUTILUS_BATCH_RENAME_H
#define NAUTILUS_BATCH_RENAME_H

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include "nautilus-files-view.h"

G_BEGIN_DECLS

typedef enum {
        NAUTILUS_BATCH_RENAME_APPEND = 0,
        NAUTILUS_BATCH_RENAME_PREPEND = 1,
        NAUTILUS_BATCH_RENAME_REPLACE = 2,
        NAUTILUS_BATCH_RENAME_FORMAT = 3,
} NautilusBatchRenameMode;

typedef enum {
        ORIGINAL_ASCENDING = 0,
        ORIGINAL_DESCENDING = 1,
        FIRST_MODIFIED = 2,
        LAST_MODIFIED = 3,
        FIRST_CREATED = 4,
        LAST_CREATED = 5,
} SortingMode;

typedef struct {
    GString *file_name;

    /* Photo */
    GString *creation_date;
    GString *equipment;

    /* Video */
    GString *season;
    GString *episode_nr;

    /* Music */
    GString *track_nr;
    GString *artist_name;
} FileMetadata;

#define NAUTILUS_TYPE_BATCH_RENAME (nautilus_batch_rename_get_type())

G_DECLARE_FINAL_TYPE (NautilusBatchRename, nautilus_batch_rename, NAUTILUS, BATCH_RENAME, GtkDialog);

GtkWidget*      nautilus_batch_rename_new       (GList                  *selection,
                                                 NautilusDirectory      *model,
                                                 NautilusWindow         *window);

void            query_finished                  (NautilusBatchRename    *dialog,
                                                 GHashTable             *hash_table,
                                                 GList                  *selection_metadata);

void            check_conflict_for_file         (NautilusBatchRename    *dialog,
                                                 NautilusDirectory      *directory,
                                                 GList                  *files);

gint            compare_int                     (gconstpointer a,
                                                 gconstpointer b);

G_END_DECLS

#endif