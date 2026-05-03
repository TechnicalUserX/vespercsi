#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

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
    bool is_deleted;         
    bool has_label;          
    label_header_t label;    
    feit_header_t header;    
    uint8_t raw_header[272]; 
    uint8_t *payload;        
    struct csi_record *next; 
} csi_record_t;

typedef struct {
    csi_record_t *head;
    csi_record_t *tail;
    bool use_labeled_header;
    int total_records;
    char target_file[1024];
    GtkWidget *list_box;
    GtkWidget *status_label;
} AppState;

AppState app_state = {NULL, NULL, false, 0, "", NULL, NULL};

// --- Core Logic ---

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
    // 1. Remove all items from the GTK list box
    GtkWidget *child = gtk_widget_get_first_child(app_state.list_box);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(app_state.list_box), child);
        child = next;
    }

    // 2. Free linked list memory
    csi_record_t *curr = app_state.head;
    while (curr) {
        csi_record_t *next = curr->next;
        free(curr->payload);
        free(curr);
        curr = next;
    }

    // 3. Reset application state
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

    while (true) {
        csi_record_t *rec = calloc(1, sizeof(csi_record_t));
        rec->has_label = app_state.use_labeled_header;

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
    }
    fclose(fp);
    
    // Save current file path for context
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

static void on_delete_clicked(GtkButton *button, gpointer user_data) {
    csi_record_t *rec = (csi_record_t *)user_data;
    rec->is_deleted = !rec->is_deleted; 
    
    GtkWidget *row = gtk_widget_get_parent(GTK_WIDGET(button));
    if (rec->is_deleted) {
        gtk_widget_set_opacity(row, 0.4);
        gtk_button_set_label(button, "Undo Delete");
    } else {
        gtk_widget_set_opacity(row, 1.0);
        gtk_button_set_label(button, "Stage Delete");
    }
}

void populate_ui() {
    csi_record_t *curr = app_state.head;
    int index = 1;
    
    while (curr) {
        GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
        gtk_widget_set_margin_start(row_box, 10);
        gtk_widget_set_margin_end(row_box, 10);
        gtk_widget_set_margin_top(row_box, 5);
        gtk_widget_set_margin_bottom(row_box, 5);

        char label_text[512];
        if (curr->has_label) {
            snprintf(label_text, sizeof(label_text), 
                     "[%04d] Group: %d | Action: %d | MAC: %s | %s | %sMHz | Subs: %d", 
                     index, curr->label.action_index, curr->label.action_label, 
                     curr->header.source_mac_string, curr->header.rate_format, 
                     curr->header.channel_width, curr->header.num_subcarriers);
        } else {
            snprintf(label_text, sizeof(label_text), 
                     "[%04d] MAC: %s | %s | %sMHz | Subs: %d | CSI Size: %d", 
                     index, curr->header.source_mac_string, curr->header.rate_format, 
                     curr->header.channel_width, curr->header.num_subcarriers, curr->header.csi_size);
        }

        GtkWidget *info_label = gtk_label_new(label_text);
        gtk_widget_set_hexpand(info_label, TRUE);
        gtk_widget_set_halign(info_label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(row_box), info_label);

        GtkWidget *del_btn = gtk_button_new_with_label("Stage Delete");
        g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_clicked), curr);
        gtk_box_append(GTK_BOX(row_box), del_btn);

        gtk_list_box_append(GTK_LIST_BOX(app_state.list_box), row_box);
        
        curr = curr->next;
        index++;
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
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);

    GSimpleAction *act_open = g_simple_action_new("open", NULL);
    g_signal_connect(act_open, "activate", G_CALLBACK(on_action_open), window);
    g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(act_open));

    GSimpleAction *act_save_as = g_simple_action_new("save-as", NULL);
    g_signal_connect(act_save_as, "activate", G_CALLBACK(on_action_save_as), window);
    g_action_map_add_action(G_ACTION_MAP(window), G_ACTION(act_save_as));

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    // --- Create Menu Bar ---
    GMenu *menubar = g_menu_new();
    GMenu *file_menu = g_menu_new();
    g_menu_append(file_menu, "Open...", "win.open");
    g_menu_append(file_menu, "Save As...", "win.save-as");
    g_menu_append_submenu(menubar, "File", G_MENU_MODEL(file_menu));

    GtkWidget *menu_widget = gtk_popover_menu_bar_new_from_model(G_MENU_MODEL(menubar));
    gtk_box_append(GTK_BOX(vbox), menu_widget);

    // --- Toolbar ---
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

    // --- Scrollable List Area ---
    GtkWidget *scrolled_window = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_box_append(GTK_BOX(vbox), scrolled_window);

    app_state.list_box = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), app_state.list_box);

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
