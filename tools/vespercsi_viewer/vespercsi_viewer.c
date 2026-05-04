#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <time.h> 

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
    bool is_deleted;         
    bool has_label;          
    label_header_t label;    
    feit_header_t header;    
    uint8_t raw_header[272]; 
    uint8_t *payload;        
    struct csi_record *next; 
} csi_record_t;

typedef struct GroupRecord {
    csi_record_t *rec;
    GtkWidget *row_box;
    GtkWidget *del_btn;
} GroupRecord;

struct GroupContext; 

typedef struct {
    csi_record_t *head;
    csi_record_t *tail;
    bool use_labeled_header;
    int total_records;
    char target_file[1024];
    GtkWidget *main_list_container; 
    GtkWidget *status_label;
    struct GroupContext *group_contexts; 
} AppState;

AppState app_state = {NULL, NULL, false, 0, "", NULL, NULL, NULL};

typedef struct GroupContext {
    uint16_t action_index;
    uint16_t action_label;
    GroupRecord *records;
    int capacity;
    int count;
    GtkAdjustment *adj_start;
    GtkAdjustment *adj_end;
    GtkWidget *lbl_time_start;
    GtkWidget *lbl_time_end;
    GtkWidget *expander;        
    struct GroupContext *next;
} GroupContext;


// --- Core Logic ---

const char* get_action_name(uint16_t label) {
    switch(label) {
        case 1: return "WALK";
        case 2: return "RUN";
        case 3: return "JUMP";
        case 4: return "STILL";
        case 5: return "STANDUP";
        case 6: return "SITDOWN";
        case 7: return "SITSTAND";
        case 8: return "FALL";
        default: return "UNKNOWN";
    }
}

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

    csi_record_t *curr = app_state.head;
    while (curr) {
        csi_record_t *next = curr->next;
        free(curr->payload);
        free(curr);
        curr = next;
    }

    app_state.head = NULL;
    app_state.tail = NULL;
    app_state.total_records = 0;
}

void load_csi_file(const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        char err_msg[2048];
        snprintf(err_msg, sizeof(err_msg), "Failed to open file: %s", filepath);
        gtk_label_set_text(GTK_LABEL(app_state.status_label), err_msg);
        return;
    }

    uint8_t label_buf[4];
    int index = 1;

    while (true) {
        csi_record_t *rec = calloc(1, sizeof(csi_record_t));
        rec->has_label = app_state.use_labeled_header;
        rec->global_index = index;

        if (rec->has_label) {
            if (fread(label_buf, 1, 4, fp) != 4) { free(rec); break; }
            memcpy(&rec->label.action_index, label_buf, 2);
            memcpy(&rec->label.action_label, label_buf + 2, 2);
        }

        if (fread(rec->raw_header, 1, 272, fp) != 272) { free(rec); break; }
        parse_header(rec->raw_header, &rec->header);

        rec->payload = malloc(rec->header.csi_size);
        if (fread(rec->payload, 1, rec->header.csi_size, fp) != rec->header.csi_size) {
            free(rec->payload); free(rec); break;
        }

        if (!app_state.head) app_state.head = rec;
        if (app_state.tail) app_state.tail->next = rec;
        app_state.tail = rec;
        app_state.total_records++;
        index++;
    }
    fclose(fp);
    
    strncpy(app_state.target_file, filepath, sizeof(app_state.target_file) - 1);
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
        if (!curr->is_deleted) {
            if (curr->has_label) {
                fwrite(&curr->label, 1, 4, fp);
            }
            fwrite(curr->raw_header, 1, 272, fp); 
            fwrite(curr->payload, 1, curr->header.csi_size, fp);
            saved_count++;
        }
        curr = curr->next;
    }
    fclose(fp);
    
    char status_msg[2048];
    snprintf(status_msg, sizeof(status_msg), "Saved %d records to %s", saved_count, filepath);
    gtk_label_set_text(GTK_LABEL(app_state.status_label), status_msg);
    g_print("%s\n", status_msg);
}

// --- UI Callbacks ---

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

static void set_row_delete_state(GroupRecord *grec, bool is_deleted) {
    grec->rec->is_deleted = is_deleted;
    if (is_deleted) {
        gtk_widget_set_opacity(grec->row_box, 0.4);
        gtk_button_set_label(GTK_BUTTON(grec->del_btn), "Undo Delete");
    } else {
        gtk_widget_set_opacity(grec->row_box, 1.0);
        gtk_button_set_label(GTK_BUTTON(grec->del_btn), "Stage Delete");
    }
}

static void on_single_delete_clicked(GtkButton *button, gpointer user_data) {
    GroupRecord *grec = (GroupRecord *)user_data;
    set_row_delete_state(grec, !grec->rec->is_deleted);
}

// --- Trimming Callbacks ---

static void on_trim_value_changed(GtkAdjustment *adjustment, gpointer user_data) {
    GroupContext *gc = (GroupContext *)user_data;
    
    // Add 0.5 before casting to ensure flawless rounding from doubles to integers
    int start_idx = (int)(gtk_adjustment_get_value(gc->adj_start) + 0.5);
    int end_idx = (int)(gtk_adjustment_get_value(gc->adj_end) + 0.5);

    if (start_idx > end_idx) {
        if (adjustment == gc->adj_start) {
            gtk_adjustment_set_value(gc->adj_start, end_idx);
            start_idx = end_idx;
        } else {
            gtk_adjustment_set_value(gc->adj_end, start_idx);
            end_idx = start_idx;
        }
    }

    char fmt_ts[128];
    char time_str[256];

    if (start_idx >= 0 && start_idx < gc->count) {
        uint64_t ts = gc->records[start_idx].rec->header.timestamp;
        format_timestamp(ts, fmt_ts, sizeof(fmt_ts));
        snprintf(time_str, sizeof(time_str), "Val: %lu\n<b>%s</b>", ts, fmt_ts);
        gtk_label_set_markup(GTK_LABEL(gc->lbl_time_start), time_str);
    }
    
    if (end_idx >= 0 && end_idx < gc->count) {
        uint64_t ts = gc->records[end_idx].rec->header.timestamp;
        format_timestamp(ts, fmt_ts, sizeof(fmt_ts));
        snprintf(time_str, sizeof(time_str), "Val: %lu\n<b>%s</b>", ts, fmt_ts);
        gtk_label_set_markup(GTK_LABEL(gc->lbl_time_end), time_str);
    }

    // Live auto-apply deletion based on bounds. 
    // Data outside bounds is deleted. Data inside is restored.
    for (int i = 0; i < gc->count; i++) {
        bool is_outside_bounds = (i < start_idx || i > end_idx);
        
        // Only update UI if state actually flipped (prevents UI flicker when scrubbing)
        if (gc->records[i].rec->is_deleted != is_outside_bounds) {
            set_row_delete_state(&gc->records[i], is_outside_bounds);
        }
    }
}


void populate_ui() {
    csi_record_t *curr = app_state.head;
    
    // Phase 1: Grouping Data
    while (curr) {
        if (curr->has_label) {
            GroupContext *gc = app_state.group_contexts;
            while (gc != NULL) {
                if (gc->action_index == curr->label.action_index && gc->action_label == curr->label.action_label) {
                    break;
                }
                gc = gc->next;
            }

            if (gc == NULL) {
                gc = calloc(1, sizeof(GroupContext));
                gc->action_index = curr->label.action_index;
                gc->action_label = curr->label.action_label;
                gc->capacity = 16;
                gc->records = malloc(gc->capacity * sizeof(GroupRecord));
                gc->count = 0;
                
                gc->next = app_state.group_contexts;
                app_state.group_contexts = gc;
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

    // Phase 2: Building UI
    GroupContext *gc = app_state.group_contexts;
    while (gc != NULL) {
        gc->expander = gtk_expander_new(NULL);
        gtk_widget_set_margin_start(gc->expander, 10);
        gtk_widget_set_margin_end(gc->expander, 10);
        gtk_widget_set_margin_top(gc->expander, 5);
        gtk_widget_set_margin_bottom(gc->expander, 5);

        GtkWidget *inner_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_expander_set_child(GTK_EXPANDER(gc->expander), inner_vbox);

        if (gc->count > 0) {
            GtkWidget *trim_frame = gtk_frame_new("Trim Group Data Interval (Auto-Applies)");
            gtk_widget_set_margin_start(trim_frame, 5);
            gtk_widget_set_margin_end(trim_frame, 5);
            gtk_widget_set_margin_top(trim_frame, 5);
            gtk_widget_set_margin_bottom(trim_frame, 10);

            GtkWidget *trim_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
            gtk_widget_set_margin_start(trim_vbox, 10);
            gtk_widget_set_margin_end(trim_vbox, 10);
            gtk_widget_set_margin_top(trim_vbox, 10);
            gtk_widget_set_margin_bottom(trim_vbox, 10);
            gtk_frame_set_child(GTK_FRAME(trim_frame), trim_vbox);

            gc->adj_start = gtk_adjustment_new(0, 0, gc->count - 1, 1, 10, 0);
            gc->adj_end = gtk_adjustment_new(gc->count - 1, 0, gc->count - 1, 1, 10, 0);

            g_signal_connect(gc->adj_start, "value-changed", G_CALLBACK(on_trim_value_changed), gc);
            g_signal_connect(gc->adj_end, "value-changed", G_CALLBACK(on_trim_value_changed), gc);

            GtkWidget *hbox_start = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_box_append(GTK_BOX(hbox_start), gtk_label_new("Start Index:"));
            GtkWidget *spin_start = gtk_spin_button_new(gc->adj_start, 1, 0);
            gtk_box_append(GTK_BOX(hbox_start), spin_start);
            GtkWidget *scale_start = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, gc->adj_start);
            
            // Force GTK scale to snap precisely to integer values
            gtk_scale_set_digits(GTK_SCALE(scale_start), 0);
            gtk_range_set_round_digits(GTK_RANGE(scale_start), 0);
            
            gtk_widget_set_hexpand(scale_start, TRUE);
            gtk_scale_set_draw_value(GTK_SCALE(scale_start), FALSE);
            gtk_box_append(GTK_BOX(hbox_start), scale_start);
            gc->lbl_time_start = gtk_label_new("Time: N/A");
            gtk_widget_set_size_request(gc->lbl_time_start, 240, -1);
            gtk_label_set_xalign(GTK_LABEL(gc->lbl_time_start), 0.0);
            gtk_box_append(GTK_BOX(hbox_start), gc->lbl_time_start);
            gtk_box_append(GTK_BOX(trim_vbox), hbox_start);

            GtkWidget *hbox_end = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_box_append(GTK_BOX(hbox_end), gtk_label_new("End Index: "));
            GtkWidget *spin_end = gtk_spin_button_new(gc->adj_end, 1, 0);
            gtk_box_append(GTK_BOX(hbox_end), spin_end);
            GtkWidget *scale_end = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, gc->adj_end);
            
            // Force GTK scale to snap precisely to integer values
            gtk_scale_set_digits(GTK_SCALE(scale_end), 0);
            gtk_range_set_round_digits(GTK_RANGE(scale_end), 0);
            
            gtk_widget_set_hexpand(scale_end, TRUE);
            gtk_scale_set_draw_value(GTK_SCALE(scale_end), FALSE);
            gtk_box_append(GTK_BOX(hbox_end), scale_end);
            gc->lbl_time_end = gtk_label_new("Time: N/A");
            gtk_widget_set_size_request(gc->lbl_time_end, 240, -1);
            gtk_label_set_xalign(GTK_LABEL(gc->lbl_time_end), 0.0);
            gtk_box_append(GTK_BOX(hbox_end), gc->lbl_time_end);
            gtk_box_append(GTK_BOX(trim_vbox), hbox_end);

            gtk_box_append(GTK_BOX(inner_vbox), trim_frame);

            on_trim_value_changed(gc->adj_start, gc);
        }

        GtkWidget *list_box = gtk_list_box_new();
        for (int i = 0; i < gc->count; i++) {
            csi_record_t *r = gc->records[i].rec;

            GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
            gtk_widget_set_margin_start(row_box, 10);
            gtk_widget_set_margin_end(row_box, 10);
            gtk_widget_set_margin_top(row_box, 5);
            gtk_widget_set_margin_bottom(row_box, 5);

            char label_text[512];
            snprintf(label_text, sizeof(label_text), 
                     "[Index: %04d] MAC: %s | %s | %sMHz | Subs: %d", 
                     i, r->header.source_mac_string, r->header.rate_format, 
                     r->header.channel_width, r->header.num_subcarriers);

            GtkWidget *info_label = gtk_label_new(label_text);
            gtk_widget_set_hexpand(info_label, TRUE);
            gtk_widget_set_halign(info_label, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(row_box), info_label);

            GtkWidget *inspect_btn = gtk_button_new_with_label("Inspect");
            g_signal_connect(inspect_btn, "clicked", G_CALLBACK(on_inspect_clicked), r);
            gtk_box_append(GTK_BOX(row_box), inspect_btn);

            GtkWidget *del_btn = gtk_button_new_with_label("Stage Delete");
            
            gc->records[i].row_box = row_box;
            gc->records[i].del_btn = del_btn;
            
            g_signal_connect(del_btn, "clicked", G_CALLBACK(on_single_delete_clicked), &gc->records[i]);
            gtk_box_append(GTK_BOX(row_box), del_btn);

            gtk_list_box_append(GTK_LIST_BOX(list_box), row_box);
        }
        
        gtk_box_append(GTK_BOX(inner_vbox), list_box);
        gtk_box_append(GTK_BOX(app_state.main_list_container), gc->expander);

        char title[256];
        snprintf(title, sizeof(title), "<b>Group: %d | Action: %s (%d)</b>  <i>(%d records)</i>", 
                 gc->action_index, get_action_name(gc->action_label), gc->action_label, gc->count);
        
        GtkWidget *exp_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(exp_label), title);
        gtk_expander_set_label_widget(GTK_EXPANDER(gc->expander), exp_label);

        gc = gc->next;
    }

    GtkWidget *unlabeled_list = NULL;
    curr = app_state.head;
    while(curr) {
        if (!curr->has_label) {
            if (unlabeled_list == NULL) {
                unlabeled_list = gtk_list_box_new();
                gtk_box_append(GTK_BOX(app_state.main_list_container), unlabeled_list);
            }
            GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
            gtk_widget_set_margin_start(row_box, 10); gtk_widget_set_margin_end(row_box, 10);
            
            char label_text[512];
            snprintf(label_text, sizeof(label_text), "[Global: %04d] MAC: %s | %s | %sMHz", 
                     curr->global_index, curr->header.source_mac_string, curr->header.rate_format, curr->header.channel_width);
            
            GtkWidget *info_label = gtk_label_new(label_text);
            gtk_widget_set_hexpand(info_label, TRUE); gtk_widget_set_halign(info_label, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(row_box), info_label);

            GtkWidget *inspect_btn = gtk_button_new_with_label("Inspect");
            g_signal_connect(inspect_btn, "clicked", G_CALLBACK(on_inspect_clicked), curr);
            gtk_box_append(GTK_BOX(row_box), inspect_btn);

            GroupRecord *grec = malloc(sizeof(GroupRecord)); 
            grec->rec = curr; grec->row_box = row_box; grec->del_btn = gtk_button_new_with_label("Stage Delete");
            g_signal_connect(grec->del_btn, "clicked", G_CALLBACK(on_single_delete_clicked), grec);
            gtk_box_append(GTK_BOX(row_box), grec->del_btn);

            gtk_list_box_append(GTK_LIST_BOX(unlabeled_list), row_box);
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
        clear_csi_data();
        load_csi_file(filepath);
        populate_ui();
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

// --- GTK4 Application Activation ---
static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
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

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    GMenu *menubar = g_menu_new();
    GMenu *file_menu = g_menu_new();
    g_menu_append(file_menu, "Open...", "win.open");
    g_menu_append(file_menu, "Save As...", "win.save-as");
    g_menu_append_submenu(menubar, "File", G_MENU_MODEL(file_menu));

    GtkWidget *menu_widget = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(menubar));
    gtk_box_append(GTK_BOX(vbox), menu_widget);

    GtkWidget *header_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(header_bar, 10);
    gtk_widget_set_margin_end(header_bar, 10);
    gtk_widget_set_margin_top(header_bar, 10);
    gtk_widget_set_margin_bottom(header_bar, 10);    
    
    GtkWidget *btn_save = gtk_button_new_with_label("Commit Changes (Save As...)");
    gtk_widget_add_css_class(btn_save, "suggested-action"); 
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_action_save_as), window);
    gtk_box_append(GTK_BOX(header_bar), btn_save);
    
    app_state.status_label = gtk_label_new("Ready");
    gtk_widget_set_hexpand(app_state.status_label, TRUE);
    gtk_widget_set_halign(app_state.status_label, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(header_bar), app_state.status_label);

    gtk_box_append(GTK_BOX(vbox), header_bar);

    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_box_append(GTK_BOX(vbox), scrolled_window);

    app_state.main_list_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), app_state.main_list_container);

    if (strlen(app_state.target_file) > 0) {
        load_csi_file(app_state.target_file); 
        populate_ui();
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
