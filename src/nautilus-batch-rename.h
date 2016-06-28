
#ifndef NAUTILUS_BATCH_RENAME_H
#define NAUTILUS_BATCH_RENAME_H

#include <glib.h>
#include <gtk/gtk.h>
#include "nautilus-files-view.h"

G_BEGIN_DECLS

typedef enum {
        NAUTILUS_BATCH_RENAME_APPEND = 0,
        NAUTILUS_BATCH_RENAME_PREPEND = 1,
        NAUTILUS_BATCH_RENAME_REPLACE = 2,
        NAUTILUS_BATCH_RENAME_FORMAT = 3,
} NautilusBatchRenameModes;

typedef enum {
        ORIGINAL_ASCENDING = 0,
        ORIGINAL_DESCENDING = 1,
        FIRST_CREATED = 2,
        LAST_CREATED = 3,
        FIRST_MODIFIED = 4,
        LAST_MODIFIED = 5,
} SortingMode;

#define NAUTILUS_TYPE_BATCH_RENAME (nautilus_batch_rename_get_type())

G_DECLARE_FINAL_TYPE (NautilusBatchRename, nautilus_batch_rename, NAUTILUS, BATCH_RENAME, GtkDialog);

GtkWidget*      nautilus_batch_rename_new       (NautilusFilesView *view);

G_END_DECLS

#endif