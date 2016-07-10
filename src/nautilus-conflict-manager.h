#ifndef NAUTILUS_CONFLICT_MANAGER
#define NAUTILUS_CONFLICT_MANAGER


typedef struct {
    int id;
    char *new_name;
    gboolean apply_to_all;
} FileConflictResponse;


FileConflictResponse * copy_move_file_conflict_ask_user_action (GtkWindow *parent_window,
                                                                GFile     *src,
                                                                GFile     *dest,
                                                                GFile     *dest_dir);

FileConflictResponse * extract_file_conflict_ask_user_action (GtkWindow *parent_window,
                                                              GFile     *src_name,
                                                              GFile     *dest_name,
                                                              GFile     *dest_dir_name);
#endif
