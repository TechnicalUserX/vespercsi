#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h> 

// --- POSIX Includes for mmap ---
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_UI_ROWS 100 // Cap UI rows per group to prevent GTK freezes

// --- FeitCSI Constants & Bitmasks ---
#define RATE_MCS_MOD_TYPE_POS 8
#define RATE_MCS_MOD_TYPE_MSK (0x7 << RATE_MCS_MOD_TYPE_POS)
#define RATE_MCS_CCK_MSK (0 << RATE_MCS_MOD_TYPE_POS)
#define RATE_MCS_LEGACY_OFDM_MSK (1 << RATE_MCS_MOD_TYPE_POS)
#define RATE_MCS_HT_MSK (2 << RATE_MCS_MOD_TYPE_POS)
#define RATE_MCS_VHT_MSK (3 << RATE_MCS_MOD_TYPE_POS)
#define RATE_MCS_HE_MSK (4 << RATE_MCS_MOD_TYPE_POS)
#define RATE_MCS_EHT_MSK (5 << RATE_MCS_MOD_TYPE_POS)

#define RATE_MCS_CHAN_WIDTH_POS 11
#define RATE_MCS_CHAN_WIDTH_MSK (0x7 << RATE_MCS_CHAN_WIDTH_POS)
#define RATE_MCS_CHAN_WIDTH_20 (0 << RATE_MCS_CHAN_WIDTH_POS)
#define RATE_MCS_CHAN_WIDTH_40 (1 << RATE_MCS_CHAN_WIDTH_POS)
#define RATE_MCS_CHAN_WIDTH_80 (2 << RATE_MCS_CHAN_WIDTH_POS)
#define RATE_MCS_CHAN_WIDTH_160 (3 << RATE_MCS_CHAN_WIDTH_POS)
#define RATE_MCS_CHAN_WIDTH_320 (4 << RATE_MCS_CHAN_WIDTH_POS)

#define RATE_HT_MCS_CODE_MSK 7
#define RATE_MCS_ANT_A_MSK (1 << 14)
#define RATE_MCS_ANT_B_MSK (1 << 15)
#define RATE_MCS_LDPC_MSK (1 << 16)
#define RATE_MCS_SS_MSK   (1 << 16)
#define RATE_MCS_BEAMF_MSK (1 << 16)

// --- Data Structures ---
typedef struct {
    uint16_t action_index;
    uint16_t action_label;
} label_header_t;

typedef struct {
    uint32_t csi_size;
    uint32_t ftm_clock;
    uint64_t timestamp;
    uint8_t num_rx;
    uint8_t num_tx;
    uint32_t num_subcarriers;
    uint32_t rssi1;
    uint32_t rssi2;
    uint8_t source_mac[6];
    char source_mac_string[18];
    uint32_t rate_flags;
    char rate_format[16];
    char channel_width[8];
    uint8_t mcs;
    bool antenna_a;
    bool antenna_b;
    bool ldpc;
    uint8_t ss;
    bool beamforming;
} feit_header_t;

// --- Application State Model ---
typedef struct csi_record {
    int global_index;        
    bool has_label;          
    label_header_t label;    
    feit_header_t header;    
    uint8_t raw_header[272]; 
    uint8_t *payload;        
    struct csi_record *next; 
    GtkWidget *row_action_lbl; 
} csi_record_t;

typedef struct GroupRecord {
    csi_record_t *rec;
    GtkWidget *row_box;
} GroupRecord;

struct GroupContext; 
struct TransitionContext;

typedef struct {
    csi_record_t *head;
    csi_record_t *tail;
    bool use_labeled_header;
    int total_records;
    char target_file[1024];
    
    GtkWidget *main_list_container; 
    GtkWidget *status_label;
    GtkWidget *header_toggle_btn; 
    GtkWidget *btn_open;
    GtkWidget *btn_save;
    GtkWidget *spinner;

    struct GroupContext *group_contexts; 
    
    // Arrays for dynamic transitioning
    csi_record_t **labeled_records;
    int num_labeled;
    struct TransitionContext *transitions;
    int num_transitions;
    
    // Efficient memory management for massive files
    uint8_t *file_data;
    size_t file_size;
    csi_record_t *records_array;
    
} AppState;

AppState app_state = {NULL, NULL, false, 0, "", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, 0, NULL, 0, NULL};

typedef struct GroupContext {
    uint16_t action_index;
    uint16_t action_label;
    GroupRecord *records;
    int capacity;
    int count;
    GtkWidget *expander;        
    struct GroupContext *next;
} GroupContext;

typedef struct TransitionContext {
    int transition_idx;
    int initial_transition_idx; 
    GtkAdjustment *adj;
    GtkWidget *lbl_time;
    struct TransitionContext *prev;
    struct TransitionContext *next;
    GroupContext *left_gc;
    GroupContext *right_gc;
} TransitionContext;

typedef struct {
    char filepath[1024];
    bool success;
    char err_msg[2048];
} LoadTask;


// --- Forward Declarations ---
void populate_ui();


// --- UI Styling & Label Helpers ---

const char* get_action_name(uint16_t label) {
    switch(label) {
        case 0: return "NOLABEL";
        case 1: return "WALK";
        case 2: return "RUN";
        case 3: return "JUMP";
        case 4: return "STILL";
        case 5: return "STANDUP";
        case 6: return "SITDOWN";
        case 7: return "SITSTAND";
        case 8: return "FALL";
        default: return "INVALID_LABEL";
    }
}

const char* get_action_css_class(uint16_t label) {
    switch(label) {
        case 0: return "action-box-nolabel";
        case 1: return "action-box-walk";
        case 2: return "action-box-run"; 
        case 3: return "action-box-jump";
        case 4: return "action-box-still";
        case 5: return "action-box-standup";
        case 6: return "action-box-sitdown";
        case 8: return "action-box-fall";
        default: return "action-box-nolabel";
    }
}

static void load_css() {
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css =
        /* Deep Gray Segments Theme & Breathing Margins/Padding */
        "window.dark-window { background-color: #121212; }"
        ".dark-header { background-color: #1a1a1a; border-bottom: 1px solid #333333; padding: 8px; }"
        ".dark-panel { background-color: #1e1e1e; border: 1px solid #2d2d2d; border-radius: 12px; padding: 12px; margin: 8px 0px; }"
        ".dark-row { background-color: #242424; border: 1px solid #2a2a2a; border-radius: 10px; margin-bottom: 6px; padding: 8px; }"
        ".dark-row:hover { background-color: #2f2f2f; }"
        ".boundary-box { background-color: #1a1a1a; border: 1px solid #3a3a3a; border-radius: 12px; padding: 15px; margin: 8px 0px; }"
        
        /* Functional Components */
        ".group-header { border-radius: 12px; overflow: hidden; margin-top: 4px; margin-bottom: 4px; }"
        ".group-id-box { background-color: #333333; color: #FFFFFF; padding: 10px 18px; }"
        ".action-badge { border-radius: 8px; font-weight: bold; margin: 0 12px; padding: 6px 14px; min-width: 80px; text-align: center; }"
        
        ".action-box-nolabel { background-color: #FFFFFF; color: #000000; padding: 10px 18px; }"
        ".action-box-walk    { background-color: #FFA500; color: #000000; padding: 10px 18px; }"
        ".action-box-run     { background-color: #00FFFF; color: #000000; padding: 10px 18px; }"
        ".action-box-jump    { background-color: #FF4444; color: #FFFFFF; padding: 10px 18px; }"
        ".action-box-still   { background-color: #FFFF00; color: #000000; padding: 10px 18px; }"
        ".action-box-standup { background-color: #FFC0CB; color: #000000; padding: 10px 18px; }"
        ".action-box-sitdown { background-color: #FF00FF; color: #FFFFFF; padding: 10px 18px; }"
        ".action-box-fall    { background-color: #00C851; color: #FFFFFF; padding: 10px 18px; }"
        
        ".boundary-separator { min-height: 1px; background-image: none; background-color: #555555; border: none; margin-right: 15px; }";
        
    gtk_css_provider_load_from_string(provider, css);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), 
                                               GTK_STYLE_PROVIDER(provider), 
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

// --- Core Logic ---

void format_timestamp(uint64_t ts, char *buf, size_t max_len) {
    time_t seconds = (time_t)(ts / 1000000ULL);
    uint64_t micros = ts % 1000000ULL;
    
    struct tm *tm_info = localtime(&seconds);
    if (tm_info != NULL) {
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y/%m/%d %H:%M:%S", tm_info);
        snprintf(buf, max_len, "%s.%06lu", time_str, micros);
    } else {
        snprintf(buf, max_len, "Invalid Date");
    }
}

void parse_header(const uint8_t *data, feit_header_t *header) {
    memcpy(&header->csi_size, data + 0, 4);
    memcpy(&header->ftm_clock, data + 8, 4);
    memcpy(&header->timestamp, data + 12, 8);
    
    header->num_rx = data[46];
    header->num_tx = data[47];
    
    memcpy(&header->num_subcarriers, data + 52, 4);
    memcpy(&header->rssi1, data + 60, 4);
    memcpy(&header->rssi2, data + 64, 4);
    memcpy(header->source_mac, data + 68, 6);
    
    snprintf(header->source_mac_string, sizeof(header->source_mac_string),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             header->source_mac[0], header->source_mac[1], header->source_mac[2],
             header->source_mac[3], header->source_mac[4], header->source_mac[5]);
             
    memcpy(&header->rate_flags, data + 92, 4);

    uint32_t rate_format = header->rate_flags & RATE_MCS_MOD_TYPE_MSK;
    if (rate_format == RATE_MCS_CCK_MSK) strcpy(header->rate_format, "CCK");
    else if (rate_format == RATE_MCS_LEGACY_OFDM_MSK) strcpy(header->rate_format, "LEGACY_OFDM");
    else if (rate_format == RATE_MCS_HT_MSK) strcpy(header->rate_format, "HT");
    else if (rate_format == RATE_MCS_VHT_MSK) strcpy(header->rate_format, "VHT");
    else if (rate_format == RATE_MCS_HE_MSK) strcpy(header->rate_format, "HE");
    else if (rate_format == RATE_MCS_EHT_MSK) strcpy(header->rate_format, "EHT");
    else strcpy(header->rate_format, "unknown");

    uint32_t channel_width = header->rate_flags & RATE_MCS_CHAN_WIDTH_MSK;
    if (channel_width == RATE_MCS_CHAN_WIDTH_20) strcpy(header->channel_width, "20");
    else if (channel_width == RATE_MCS_CHAN_WIDTH_40) strcpy(header->channel_width, "40");
    else if (channel_width == RATE_MCS_CHAN_WIDTH_80) strcpy(header->channel_width, "80");
    else if (channel_width == RATE_MCS_CHAN_WIDTH_160) strcpy(header->channel_width, "160");
    else if (channel_width == RATE_MCS_CHAN_WIDTH_320) strcpy(header->channel_width, "320");
    else strcpy(header->channel_width, "unknown");

    header->mcs = header->rate_flags & RATE_HT_MCS_CODE_MSK;
    header->antenna_a = (header->rate_flags & RATE_MCS_ANT_A_MSK) != 0;
    header->antenna_b = (header->rate_flags & RATE_MCS_ANT_B_MSK) != 0;
    header->ldpc = (header->rate_flags & RATE_MCS_LDPC_MSK) != 0;
    header->ss = (header->rate_flags & RATE_MCS_SS_MSK) ? 2 : 1;
    header->beamforming = (header->rate_flags & RATE_MCS_BEAMF_MSK) != 0;
}

void clear_csi_data() {
    GtkWidget *child = gtk_widget_get_first_child(app_state.main_list_container);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(app_state.main_list_container), child);
        child = next;
    }

    GroupContext *gc = app_state.group_contexts;
    while(gc) {
        GroupContext *next_gc = gc->next;
        free(gc->records);
        free(gc);
        gc = next_gc;
    }
    app_state.group_contexts = NULL;

    if (app_state.labeled_records) {
        free(app_state.labeled_records);
        app_state.labeled_records = NULL;
    }
    
    if (app_state.transitions) {
        free(app_state.transitions);
        app_state.transitions = NULL;
    }

    // Clean up contiguous array and mmap
    if (app_state.records_array) {
        free(app_state.records_array);
        app_state.records_array = NULL;
    }

    if (app_state.file_data && app_state.file_data != MAP_FAILED) {
        munmap(app_state.file_data, app_state.file_size);
        app_state.file_data = NULL;
    }

    app_state.head = NULL;
    app_state.tail = NULL;
    app_state.total_records = 0;
    app_state.num_labeled = 0;
    app_state.num_transitions = 0;
    app_state.file_size = 0;
}

gboolean on_load_finished_idle(gpointer data) {
    LoadTask *task = (LoadTask*)data;
    
    gtk_spinner_stop(GTK_SPINNER(app_state.spinner));
    gtk_widget_set_visible(app_state.spinner, FALSE);
    
    gtk_widget_set_sensitive(app_state.btn_open, TRUE);
    gtk_widget_set_sensitive(app_state.btn_save, TRUE);
    
    if (!task->success) {
        gtk_label_set_text(GTK_LABEL(app_state.status_label), task->err_msg);
    } else {
        strncpy(app_state.target_file, task->filepath, sizeof(app_state.target_file) - 1);
        populate_ui(); 
    }
    
    free(task);
    return G_SOURCE_REMOVE;
}

gpointer load_csi_thread_func(gpointer data) {
    LoadTask *task = (LoadTask*)data;
    const char *filepath = task->filepath;

    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        snprintf(task->err_msg, sizeof(task->err_msg), "Failed to open file: %s", filepath);
        task->success = false;
        g_idle_add((GSourceFunc)on_load_finished_idle, task);
        return NULL;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        snprintf(task->err_msg, sizeof(task->err_msg), "Failed to read file stats.");
        task->success = false;
        g_idle_add((GSourceFunc)on_load_finished_idle, task);
        return NULL;
    }
    app_state.file_size = st.st_size;

    app_state.file_data = mmap(NULL, app_state.file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (app_state.file_data == MAP_FAILED) {
        snprintf(task->err_msg, sizeof(task->err_msg), "Memory mapping failed. File too large or out of memory.");
        task->success = false;
        g_idle_add((GSourceFunc)on_load_finished_idle, task);
        return NULL;
    }

    size_t capacity = 50000; 
    app_state.records_array = malloc(capacity * sizeof(csi_record_t));

    size_t offset = 0;
    int index = 1;
    app_state.total_records = 0;

    while (offset < app_state.file_size) {
        if (app_state.total_records >= capacity) {
            capacity *= 2;
            app_state.records_array = realloc(app_state.records_array, capacity * sizeof(csi_record_t));
        }

        csi_record_t *rec = &app_state.records_array[app_state.total_records];
        memset(rec, 0, sizeof(csi_record_t));
        
        rec->has_label = app_state.use_labeled_header;
        rec->global_index = index;

        if (rec->has_label) {
            if (offset + 4 > app_state.file_size) break;
            memcpy(&rec->label.action_index, app_state.file_data + offset, 2);
            memcpy(&rec->label.action_label, app_state.file_data + offset + 2, 2);
            offset += 4;
        }

        if (offset + 272 > app_state.file_size) break;
        memcpy(rec->raw_header, app_state.file_data + offset, 272);
        parse_header(rec->raw_header, &rec->header);
        offset += 272;

        if (offset + rec->header.csi_size > app_state.file_size) break;
        
        rec->payload = app_state.file_data + offset; 
        offset += rec->header.csi_size;

        app_state.total_records++;
        index++;
    }

    if (app_state.total_records > 0) {
        app_state.head = &app_state.records_array[0];
        for (int i = 0; i < app_state.total_records - 1; i++) {
            app_state.records_array[i].next = &app_state.records_array[i+1];
        }
        app_state.records_array[app_state.total_records - 1].next = NULL;
        app_state.tail = &app_state.records_array[app_state.total_records - 1];
    }
    
    task->success = true;
    g_idle_add((GSourceFunc)on_load_finished_idle, task);
    return NULL;
}

void start_async_load(const char *filepath) {
    clear_csi_data(); 
    
    gtk_spinner_start(GTK_SPINNER(app_state.spinner));
    gtk_widget_set_visible(app_state.spinner, TRUE);
    
    gtk_widget_set_sensitive(app_state.btn_open, FALSE);
    gtk_widget_set_sensitive(app_state.btn_save, FALSE);
    
    gtk_label_set_text(GTK_LABEL(app_state.status_label), "Loading and Parsing File... Please wait.");
    
    LoadTask *task = malloc(sizeof(LoadTask));
    strncpy(task->filepath, filepath, sizeof(task->filepath)-1);
    task->success = false;
    
    g_thread_new("load_csi", load_csi_thread_func, task);
}

void save_csi_file(const char *filepath) {
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        char err_msg[2048];
        snprintf(err_msg, sizeof(err_msg), "Failed to open %s for writing", filepath);
        gtk_label_set_text(GTK_LABEL(app_state.status_label), err_msg);
        return;
    }

    csi_record_t *curr = app_state.head;
    int saved_count = 0;
    
    while (curr) {
        if (curr->has_label) {
            fwrite(&curr->label, 1, 4, fp);
        }
        fwrite(curr->raw_header, 1, 272, fp); 
        fwrite(curr->payload, 1, curr->header.csi_size, fp);
        saved_count++;
        curr = curr->next;
    }
    fclose(fp);
    
    char status_msg[2048];
    snprintf(status_msg, sizeof(status_msg), "Saved %d records to %s", saved_count, filepath);
    gtk_label_set_text(GTK_LABEL(app_state.status_label), status_msg);
    g_print("%s\n", status_msg);
}

// --- UI Callbacks ---

static void on_header_toggle_changed(GtkToggleButton *button, gpointer user_data) {
    app_state.use_labeled_header = gtk_toggle_button_get_active(button);
}

static void on_inspect_clicked(GtkButton *button, gpointer user_data) {
    csi_record_t *rec = (csi_record_t *)user_data;
    GtkWidget *parent_window = GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(button)));

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent_window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_title(GTK_WINDOW(dialog), "CSI Record Inspector");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 380, 450);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);

    GString *info = g_string_new("");

    if (rec->has_label) {
        g_string_append_printf(info, "<b><span size='large'>Group Labeling</span></b>\n");
        g_string_append_printf(info, "Action Index: %u\n", rec->label.action_index);
        g_string_append_printf(info, "Action Label: %s (%u)\n\n", get_action_name(rec->label.action_label), rec->label.action_label);
    }

    char fmt_ts[128];
    format_timestamp(rec->header.timestamp, fmt_ts, sizeof(fmt_ts));

    g_string_append_printf(info, "<b><span size='large'>General Header</span></b>\n");
    g_string_append_printf(info, "Global Sequence Index: %d\n", rec->global_index);
    g_string_append_printf(info, "Timestamp: %lu\n<i>(%s)</i>\n", rec->header.timestamp, fmt_ts);
    g_string_append_printf(info, "FTM Clock: %u\n", rec->header.ftm_clock);
    g_string_append_printf(info, "CSI Payload Size: %u bytes\n", rec->header.csi_size);
    g_string_append_printf(info, "Source MAC: %s\n\n", rec->header.source_mac_string);

    g_string_append_printf(info, "<b><span size='large'>Hardware/Signal Properties</span></b>\n");
    g_string_append_printf(info, "Subcarriers: %u\n", rec->header.num_subcarriers);
    g_string_append_printf(info, "RX Antennas: %u  |  TX Antennas: %u\n", rec->header.num_rx, rec->header.num_tx);
    g_string_append_printf(info, "RSSI 1: %u  |  RSSI 2: %u\n\n", rec->header.rssi1, rec->header.rssi2);

    g_string_append_printf(info, "<b><span size='large'>Rate Information</span></b>\n");
    g_string_append_printf(info, "Rate Format: %s\n", rec->header.rate_format);
    g_string_append_printf(info, "Channel Width: %s MHz\n", rec->header.channel_width);
    g_string_append_printf(info, "MCS Index: %u\n", rec->header.mcs);
    g_string_append_printf(info, "Spatial Streams: %u\n", rec->header.ss);
    g_string_append_printf(info, "Antenna A: %s  |  Antenna B: %s\n", rec->header.antenna_a ? "Yes" : "No", rec->header.antenna_b ? "Yes" : "No");
    g_string_append_printf(info, "LDPC: %s  |  Beamforming: %s\n", rec->header.ldpc ? "Yes" : "No", rec->header.beamforming ? "Yes" : "No");

    GtkWidget *details_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(details_label), info->str);
    gtk_widget_set_halign(details_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), details_label);

    g_string_free(info, TRUE);

    GtkWidget *close_btn = gtk_button_new_with_label("Close Inspector");
    gtk_widget_set_margin_top(close_btn, 10);
    g_signal_connect_swapped(close_btn, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    gtk_box_append(GTK_BOX(vbox), close_btn);

    gtk_window_present(GTK_WINDOW(dialog));
}

// --- Boundary Transition Callbacks ---

static void update_row_visuals(csi_record_t *r) {
    if (!r || !r->row_action_lbl) return;
    
    gtk_label_set_text(GTK_LABEL(r->row_action_lbl), get_action_name(r->label.action_label));
    
    const char* classes[] = {
        "action-box-nolabel", "action-box-walk", "action-box-run", 
        "action-box-jump", "action-box-still", "action-box-standup", 
        "action-box-sitdown", "action-box-fall"
    };
    for (int i = 0; i < 8; i++) {
        gtk_widget_remove_css_class(r->row_action_lbl, classes[i]);
    }
    gtk_widget_add_css_class(r->row_action_lbl, get_action_css_class(r->label.action_label));
}

static void on_transition_moved(GtkAdjustment *adjustment, gpointer user_data) {
    TransitionContext *tc = (TransitionContext *)user_data;
    int val = (int)(gtk_adjustment_get_value(adjustment) + 0.5);
    
    int min_idx = tc->prev ? tc->prev->transition_idx : 0;
    int max_idx = tc->next ? tc->next->transition_idx : app_state.num_labeled;
    
    if (val < min_idx) {
        val = min_idx;
        gtk_adjustment_set_value(adjustment, val);
    } else if (val > max_idx) {
        val = max_idx;
        gtk_adjustment_set_value(adjustment, max_idx);
    }
    
    tc->transition_idx = val;
    
    uint64_t ts = 0;
    if (val >= 0 && val < app_state.num_labeled) {
        ts = app_state.labeled_records[val]->header.timestamp;
    } else if (val == app_state.num_labeled && val > 0) {
        ts = app_state.labeled_records[val-1]->header.timestamp;
    }
    
    char fmt_ts[128];
    char time_str[256];
    format_timestamp(ts, fmt_ts, sizeof(fmt_ts));
    snprintf(time_str, sizeof(time_str), "Boundary Time: %lu\n<b>%s</b>", ts, fmt_ts);
    gtk_label_set_markup(GTK_LABEL(tc->lbl_time), time_str);
    
    // Propagate assignments dynamically based on the current boundaries
    int start_idx = tc->prev ? tc->prev->transition_idx : 0;
    int end_idx = tc->next ? tc->next->transition_idx : app_state.num_labeled;
    
    for (int i = start_idx; i < val; i++) {
        app_state.labeled_records[i]->label.action_index = tc->left_gc->action_index;
        app_state.labeled_records[i]->label.action_label = tc->left_gc->action_label;
        update_row_visuals(app_state.labeled_records[i]);
    }
    for (int i = val; i < end_idx; i++) {
        app_state.labeled_records[i]->label.action_index = tc->right_gc->action_index;
        app_state.labeled_records[i]->label.action_label = tc->right_gc->action_label;
        update_row_visuals(app_state.labeled_records[i]);
    }
}

static void on_transition_reset_clicked(GtkButton *button, gpointer user_data) {
    TransitionContext *tc = (TransitionContext *)user_data;
    gtk_adjustment_set_value(tc->adj, tc->initial_transition_idx);
}

int compare_groups_chronologically(const void *a, const void *b) {
    GroupContext *g1 = *(GroupContext **)a;
    GroupContext *g2 = *(GroupContext **)b;
    
    if (g1->count == 0 || g2->count == 0) return 0;

    uint64_t t1 = g1->records[0].rec->header.timestamp;
    uint64_t t2 = g2->records[0].rec->header.timestamp;
    
    if (t1 < t2) return -1;
    if (t1 > t2) return 1;
    return 0;
}

void populate_ui() {
    csi_record_t *curr = app_state.head;
    
    // Phase 1: Contiguous Grouping Data (Extracting Base Groups)
    while (curr) {
        if (curr->has_label) {
            GroupContext *tail_gc = app_state.group_contexts;
            if (tail_gc != NULL) {
                while (tail_gc->next != NULL) {
                    tail_gc = tail_gc->next;
                }
            }

            GroupContext *gc = NULL;
            if (tail_gc != NULL && 
                tail_gc->action_index == curr->label.action_index && 
                tail_gc->action_label == curr->label.action_label) {
                gc = tail_gc;
            }

            if (gc == NULL) {
                gc = calloc(1, sizeof(GroupContext));
                gc->action_index = curr->label.action_index;
                gc->action_label = curr->label.action_label;
                gc->capacity = 16;
                gc->records = malloc(gc->capacity * sizeof(GroupRecord));
                gc->count = 0;
                gc->next = NULL;
                
                if (app_state.group_contexts == NULL) {
                    app_state.group_contexts = gc;
                } else {
                    tail_gc->next = gc; 
                }
            }

            if (gc->count >= gc->capacity) {
                gc->capacity *= 2;
                gc->records = realloc(gc->records, gc->capacity * sizeof(GroupRecord));
            }

            gc->records[gc->count].rec = curr;
            gc->count++;
        }
        curr = curr->next;
    }

    // Phase 1.5: Ensure Strict Chronological Ordering
    int num_groups = 0;
    GroupContext *temp_gc = app_state.group_contexts;
    while(temp_gc) {
        num_groups++;
        temp_gc = temp_gc->next;
    }

    if (num_groups > 1) {
        GroupContext **gc_array = malloc(num_groups * sizeof(GroupContext *));
        temp_gc = app_state.group_contexts;
        for (int i = 0; i < num_groups; i++) {
            gc_array[i] = temp_gc;
            temp_gc = temp_gc->next;
        }

        qsort(gc_array, num_groups, sizeof(GroupContext *), compare_groups_chronologically);

        app_state.group_contexts = gc_array[0];
        for (int i = 0; i < num_groups - 1; i++) {
            gc_array[i]->next = gc_array[i + 1];
        }
        gc_array[num_groups - 1]->next = NULL;

        free(gc_array);
    }
    
    // Phase 1.8: Setup Boundary/Transition Structs
    app_state.num_labeled = 0;
    temp_gc = app_state.group_contexts;
    while(temp_gc) { 
        app_state.num_labeled += temp_gc->count; 
        temp_gc = temp_gc->next; 
    }
    
    if (app_state.num_labeled > 0) {
        app_state.labeled_records = malloc(app_state.num_labeled * sizeof(csi_record_t*));
        int idx = 0;
        temp_gc = app_state.group_contexts;
        while(temp_gc) {
            for(int i = 0; i < temp_gc->count; i++) {
                app_state.labeled_records[idx++] = temp_gc->records[i].rec;
            }
            temp_gc = temp_gc->next;
        }
    }
    
    if (num_groups > 1) {
        app_state.num_transitions = num_groups - 1;
        app_state.transitions = calloc(app_state.num_transitions, sizeof(TransitionContext));
        
        GroupContext *left = app_state.group_contexts;
        int accum_idx = 0;
        for (int i = 0; i < app_state.num_transitions; i++) {
            GroupContext *right = left->next;
            accum_idx += left->count;
            
            app_state.transitions[i].transition_idx = accum_idx;
            app_state.transitions[i].initial_transition_idx = accum_idx;
            app_state.transitions[i].left_gc = left;
            app_state.transitions[i].right_gc = right;
            app_state.transitions[i].prev = (i > 0) ? &app_state.transitions[i-1] : NULL;
            app_state.transitions[i].next = (i < app_state.num_transitions - 1) ? &app_state.transitions[i+1] : NULL;
            
            left = right;
        }
    }

    // Phase 2: Building UI
    GroupContext *gc = app_state.group_contexts;
    int g_idx = 0;
    while (gc != NULL) {
        gc->expander = gtk_expander_new(NULL);

        GtkWidget *inner_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_widget_add_css_class(inner_vbox, "dark-panel"); 
        gtk_expander_set_child(GTK_EXPANDER(gc->expander), inner_vbox);

        GtkWidget *list_box = gtk_list_box_new();
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(list_box), GTK_SELECTION_NONE);
        
        // Virtualized / Capped UI Rendering
        for (int i = 0; i < gc->count; i++) {
            bool show_row = (i < MAX_UI_ROWS / 2) || (i >= gc->count - (MAX_UI_ROWS / 2));
            if (gc->count <= MAX_UI_ROWS) show_row = true;
            
            if (!show_row) {
                if (i == MAX_UI_ROWS / 2) {
                    GtkWidget *ph_lbl = gtk_label_new(NULL);
                    char ph[128];
                    snprintf(ph, sizeof(ph), "<i>... %d records omitted for UI performance ...</i>", gc->count - MAX_UI_ROWS);
                    gtk_label_set_markup(GTK_LABEL(ph_lbl), ph);
                    gtk_widget_set_margin_top(ph_lbl, 10);
                    gtk_widget_set_margin_bottom(ph_lbl, 10);
                    gtk_list_box_append(GTK_LIST_BOX(list_box), ph_lbl);
                }
                continue; 
            }

            csi_record_t *r = gc->records[i].rec;

            GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
            gtk_widget_add_css_class(row_box, "dark-row");

            char label_text[512];
            snprintf(label_text, sizeof(label_text), 
                     "[Index: %04d] MAC: %s | %s | %sMHz | Subs: %d", 
                     i, r->header.source_mac_string, r->header.rate_format, 
                     r->header.channel_width, r->header.num_subcarriers);

            GtkWidget *info_label = gtk_label_new(label_text);
            gtk_widget_set_hexpand(info_label, TRUE);
            gtk_widget_set_halign(info_label, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(row_box), info_label);
            
            GtkWidget *action_badge = gtk_label_new(get_action_name(r->label.action_label));
            gtk_widget_add_css_class(action_badge, "action-badge");
            gtk_widget_add_css_class(action_badge, get_action_css_class(r->label.action_label));
            r->row_action_lbl = action_badge;
            gtk_box_append(GTK_BOX(row_box), action_badge);

            GtkWidget *inspect_btn = gtk_button_new_with_label("Inspect");
            g_signal_connect(inspect_btn, "clicked", G_CALLBACK(on_inspect_clicked), r);
            gtk_box_append(GTK_BOX(row_box), inspect_btn);

            gc->records[i].row_box = row_box;
            gtk_list_box_append(GTK_LIST_BOX(list_box), row_box);
        }
        
        gtk_box_append(GTK_BOX(inner_vbox), list_box);
        
        char left_text[128];
        snprintf(left_text, sizeof(left_text), "<b>Group %d</b>", gc->action_index);

        char right_text[256];
        snprintf(right_text, sizeof(right_text), "<b>Initial Category: %s (%d)</b>  <i>(%d records)</i>", 
                 get_action_name(gc->action_label), gc->action_label, gc->count);

        GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_hexpand(header_box, TRUE);
        gtk_widget_add_css_class(header_box, "group-header");

        GtkWidget *lbl_left = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(lbl_left), left_text);
        gtk_widget_add_css_class(lbl_left, "group-id-box");
        gtk_box_append(GTK_BOX(header_box), lbl_left);

        GtkWidget *lbl_right = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(lbl_right), right_text);
        gtk_widget_add_css_class(lbl_right, get_action_css_class(gc->action_label));
        gtk_label_set_xalign(GTK_LABEL(lbl_right), 0.0);  
        gtk_widget_set_hexpand(lbl_right, TRUE);          
        gtk_box_append(GTK_BOX(header_box), lbl_right);

        gtk_expander_set_label_widget(GTK_EXPANDER(gc->expander), header_box);
        gtk_box_append(GTK_BOX(app_state.main_list_container), gc->expander);

        if (g_idx < app_state.num_transitions) {
            TransitionContext *tc = &app_state.transitions[g_idx];
            
            GtkWidget *b_expander = gtk_expander_new(NULL);
            
            GtkWidget *b_lbl_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_widget_set_hexpand(b_lbl_box, TRUE);
            
            GtkWidget *b_lbl = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(b_lbl), "<b>Boundaries</b>");
            gtk_box_append(GTK_BOX(b_lbl_box), b_lbl);
            
            GtkWidget *b_sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
            gtk_widget_set_hexpand(b_sep, TRUE);
            gtk_widget_set_valign(b_sep, GTK_ALIGN_CENTER);
            gtk_widget_add_css_class(b_sep, "boundary-separator");
            gtk_box_append(GTK_BOX(b_lbl_box), b_sep);
            
            gtk_expander_set_label_widget(GTK_EXPANDER(b_expander), b_lbl_box);
            
            GtkWidget *b_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
            gtk_widget_add_css_class(b_box, "boundary-box");
            gtk_expander_set_child(GTK_EXPANDER(b_expander), b_box);
            
            char b_title[256];
            snprintf(b_title, sizeof(b_title), "<b>Boundary Split: %s </b><span alpha='50%%'>&#8596;</span><b> %s</b>", 
                     get_action_name(tc->left_gc->action_label),
                     get_action_name(tc->right_gc->action_label));
                     
            GtkWidget *lbl_title = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(lbl_title), b_title);
            gtk_widget_set_halign(lbl_title, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(b_box), lbl_title);
            
            tc->adj = gtk_adjustment_new(tc->transition_idx, 0, app_state.num_labeled, 1, 10, 0);
            g_signal_connect(tc->adj, "value-changed", G_CALLBACK(on_transition_moved), tc);
            
            GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            
            GtkWidget *scale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, tc->adj);
            gtk_scale_set_digits(GTK_SCALE(scale), 0);
            gtk_range_set_round_digits(GTK_RANGE(scale), 0);
            gtk_widget_set_hexpand(scale, TRUE);
            gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
            gtk_box_append(GTK_BOX(hbox), scale);
            
            GtkWidget *spin = gtk_spin_button_new(tc->adj, 1, 0);
            gtk_box_append(GTK_BOX(hbox), spin);
            
            GtkWidget *reset_btn = gtk_button_new_with_label("Reset");
            g_signal_connect(reset_btn, "clicked", G_CALLBACK(on_transition_reset_clicked), tc);
            gtk_box_append(GTK_BOX(hbox), reset_btn);

            tc->lbl_time = gtk_label_new("Boundary Time: N/A");
            gtk_widget_set_size_request(tc->lbl_time, 240, -1);
            gtk_box_append(GTK_BOX(hbox), tc->lbl_time);
            
            gtk_box_append(GTK_BOX(b_box), hbox);
            
            gtk_box_append(GTK_BOX(app_state.main_list_container), b_expander);
            
            on_transition_moved(tc->adj, tc);
        }

        g_idx++;
        gc = gc->next;
    }

    // Unlabeled list capping
    GtkWidget *unlabeled_list = NULL;
    curr = app_state.head;
    int unlabeled_rendered = 0;
    while(curr) {
        if (!curr->has_label) {
            if (unlabeled_list == NULL) {
                unlabeled_list = gtk_list_box_new();
                gtk_list_box_set_selection_mode(GTK_LIST_BOX(unlabeled_list), GTK_SELECTION_NONE);
                gtk_box_append(GTK_BOX(app_state.main_list_container), unlabeled_list);
            }
            
            if (unlabeled_rendered >= MAX_UI_ROWS) {
                GtkWidget *ph_lbl = gtk_label_new("<i>... remaining unlabeled records omitted for UI performance ...</i>");
                gtk_label_set_use_markup(GTK_LABEL(ph_lbl), TRUE);
                gtk_widget_set_margin_top(ph_lbl, 10);
                gtk_widget_set_margin_bottom(ph_lbl, 10);
                gtk_list_box_append(GTK_LIST_BOX(unlabeled_list), ph_lbl);
                break; 
            }

            GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
            gtk_widget_add_css_class(row_box, "dark-row");
            
            char label_text[512];
            snprintf(label_text, sizeof(label_text), "[Global: %04d] MAC: %s | %s | %sMHz", 
                     curr->global_index, curr->header.source_mac_string, curr->header.rate_format, curr->header.channel_width);
            
            GtkWidget *info_label = gtk_label_new(label_text);
            gtk_widget_set_hexpand(info_label, TRUE); gtk_widget_set_halign(info_label, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(row_box), info_label);

            GtkWidget *inspect_btn = gtk_button_new_with_label("Inspect");
            g_signal_connect(inspect_btn, "clicked", G_CALLBACK(on_inspect_clicked), curr);
            gtk_box_append(GTK_BOX(row_box), inspect_btn);

            gtk_list_box_append(GTK_LIST_BOX(unlabeled_list), row_box);
            unlabeled_rendered++;
        }
        curr = curr->next;
    }

    char status_msg[2048];
    snprintf(status_msg, sizeof(status_msg), "Loaded %d records from %s", app_state.total_records, app_state.target_file);
    gtk_label_set_text(GTK_LABEL(app_state.status_label), status_msg);
}

// --- Menu Bar / Dialog Callbacks ---

static void on_open_ready(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    
    GFile *file = gtk_file_dialog_open_finish(dialog, res, &error);
    
    if (file != NULL) {
        char *filepath = g_file_get_path(file);
        start_async_load(filepath);
        g_free(filepath);
        g_object_unref(file);
    } else {
        if (error) g_error_free(error);
    }
}

static void on_action_open(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GtkWindow *window = GTK_WINDOW(user_data);
    GtkFileDialog *dialog = gtk_file_dialog_new();
    
    gtk_file_dialog_set_title(dialog, "Open CSI File");
    gtk_file_dialog_open(dialog, window, NULL, on_open_ready, NULL);
    g_object_unref(dialog);
}

static void on_save_ready(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GError *error = NULL;
    
    GFile *file = gtk_file_dialog_save_finish(dialog, res, &error);
    
    if (file != NULL) {
        char *filepath = g_file_get_path(file);
        save_csi_file(filepath);
        g_free(filepath);
        g_object_unref(file);
    } else {
        if (error) g_error_free(error);
    }
}

static void on_action_save_as(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    if (app_state.total_records == 0) {
        gtk_label_set_text(GTK_LABEL(app_state.status_label), "No data to save!");
        return;
    }

    GtkWindow *window = GTK_WINDOW(user_data);
    GtkFileDialog *dialog = gtk_file_dialog_new();
    
    gtk_file_dialog_set_title(dialog, "Save CSI File");
    gtk_file_dialog_set_initial_name(dialog, "vesper_modified.csi");
    
    gtk_file_dialog_save(dialog, window, NULL, on_save_ready, NULL);
    g_object_unref(dialog);
}

static void on_action_exit(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GtkWindow *window = GTK_WINDOW(user_data);
    gtk_window_destroy(window);
}

// --- GTK4 Application Activation ---
static void on_activate(GtkApplication *app, gpointer user_data) {
    load_css();

    GtkWidget *window = gtk_application_window_new(app);
    gtk_widget_add_css_class(window, "dark-window");
    gtk_window_set_title(GTK_WINDOW(window), "VesperCSI Viewer");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700); 

    GtkSettings *settings = gtk_settings_get_default();
    g_object_set(settings, "gtk-application-prefer-dark-theme", TRUE, NULL);

    GSimpleAction *act_open = g_simple_action_new("open", NULL);
    g_signal_connect(act_open, "activate", G_CALLBACK(on_action_open), window);
    g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(act_open));

    GSimpleAction *act_save_as = g_simple_action_new("save-as", NULL);
    g_signal_connect(act_save_as, "activate", G_CALLBACK(on_action_save_as), window);
    g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(act_save_as));

    GSimpleAction *act_exit = g_simple_action_new("exit", NULL);
    g_signal_connect(act_exit, "activate", G_CALLBACK(on_action_exit), window);
    g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(act_exit));

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    GMenu *menubar = g_menu_new();
    GMenu *file_menu = g_menu_new();
    g_menu_append(file_menu, "Open...", "win.open");
    g_menu_append(file_menu, "Save As...", "win.save-as");
    g_menu_append(file_menu, "Exit", "win.exit");
    g_menu_append_submenu(menubar, "File", G_MENU_MODEL(file_menu));

    GtkWidget *menu_widget = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(menubar));
    gtk_box_append(GTK_BOX(vbox), menu_widget);

    GtkWidget *header_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(header_bar, "dark-header");
    
    app_state.btn_open = gtk_button_new_with_label("Open CSI File");
    g_signal_connect(app_state.btn_open, "clicked", G_CALLBACK(on_action_open), window);
    gtk_box_append(GTK_BOX(header_bar), app_state.btn_open);
    
    app_state.btn_save = gtk_button_new_with_label("Commit Changes");
    // Removed "suggested-action" so it inherits the normal button color
    g_signal_connect(app_state.btn_save, "clicked", G_CALLBACK(on_action_save_as), window);
    gtk_box_append(GTK_BOX(header_bar), app_state.btn_save);

    app_state.header_toggle_btn = gtk_toggle_button_new_with_label("Expect Labeled Header");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app_state.header_toggle_btn), app_state.use_labeled_header);
    g_signal_connect(app_state.header_toggle_btn, "toggled", G_CALLBACK(on_header_toggle_changed), NULL);
    gtk_box_append(GTK_BOX(header_bar), app_state.header_toggle_btn);
    
    app_state.status_label = gtk_label_new("Ready");
    gtk_widget_set_hexpand(app_state.status_label, TRUE);
    gtk_widget_set_halign(app_state.status_label, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(header_bar), app_state.status_label);

    gtk_box_append(GTK_BOX(vbox), header_bar);

    // --- Overlay & Scrolled Window ---
    GtkWidget *overlay = gtk_overlay_new();
    gtk_widget_set_vexpand(overlay, TRUE);
    gtk_box_append(GTK_BOX(vbox), overlay);

    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_overlay_set_child(GTK_OVERLAY(overlay), scrolled_window);

    app_state.main_list_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    // Added breathing room margins to the main layout
    gtk_widget_set_margin_start(app_state.main_list_container, 15);
    gtk_widget_set_margin_end(app_state.main_list_container, 15);
    gtk_widget_set_margin_top(app_state.main_list_container, 10);
    gtk_widget_set_margin_bottom(app_state.main_list_container, 10);
    
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), app_state.main_list_container);

    // --- Center Spinner overlaying the data area ---
    app_state.spinner = gtk_spinner_new();
    gtk_widget_set_halign(app_state.spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(app_state.spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(app_state.spinner, 80, 80); 
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), app_state.spinner);
    gtk_widget_set_visible(app_state.spinner, FALSE);

    if (strlen(app_state.target_file) > 0) {
        start_async_load(app_state.target_file); 
    } else {
        gtk_label_set_text(GTK_LABEL(app_state.status_label), "No input file provided.");
    }

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--labeled-header") == 0) {
            app_state.use_labeled_header = true;
        } else if(argv[i][0] != '-') {
            strncpy(app_state.target_file, argv[i], sizeof(app_state.target_file) - 1);
        }
    }

    GtkApplication *app = gtk_application_new("com.vesper.csiviewer", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);

    clear_csi_data();

    return status;
}
