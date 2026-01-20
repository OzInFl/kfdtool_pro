/**
 * @file ui.cpp
 * @brief KFDtool Professional UI Implementation - Full Featured
 */

#include "ui.h"
#include <Arduino.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include "device_info.h"
#include "container.h"
#include "kfd_protocol.h"
#include "crypto.h"
#include "p25_defs.h"

extern TWI_HAL& getTwiHal();
extern KFDProtocol& getKfdProtocol();

// Theme Colors
#define COLOR_BG_DARK       lv_color_hex(0x0A0E14)
#define COLOR_BG_PANEL      lv_color_hex(0x141A22)
#define COLOR_BG_INPUT      lv_color_hex(0x1C2430)
#define COLOR_BORDER        lv_color_hex(0x2A3442)
#define COLOR_ACCENT        lv_color_hex(0x00B4D8)
#define COLOR_ACCENT_DIM    lv_color_hex(0x0077B6)
#define COLOR_TEXT          lv_color_hex(0xE0E6ED)
#define COLOR_TEXT_DIM      lv_color_hex(0x8892A0)
#define COLOR_SUCCESS       lv_color_hex(0x00E676)
#define COLOR_WARNING       lv_color_hex(0xFFB74D)
#define COLOR_ERROR         lv_color_hex(0xFF5252)
#define COLOR_BUTTON        lv_color_hex(0x1A2332)
#define COLOR_BUTTON_PRESS  lv_color_hex(0x2A3850)

#define SCREEN_W 320
#define SCREEN_H 480
#define HEADER_H 44
#define FOOTER_H 32
#define PAD 8
#define BTN_H 44
#define RADIUS 6

// Screen pointers
static lv_obj_t* scr_login = nullptr;
static lv_obj_t* scr_main_menu = nullptr;
static lv_obj_t* scr_containers = nullptr;
static lv_obj_t* scr_container_edit = nullptr;
static lv_obj_t* scr_keys = nullptr;
static lv_obj_t* scr_key_edit = nullptr;
static lv_obj_t* scr_keyload = nullptr;
static lv_obj_t* scr_diagnostics = nullptr;
static lv_obj_t* scr_keyboard = nullptr;

// Current state
static int current_container_idx = -1;
static int current_group_idx = 0;
static int current_key_idx = -1;
static char pin_buffer[8] = {0};
static int pin_len = 0;
static UserRole pending_role = ROLE_NONE;

// UI elements that need updating
static lv_obj_t* status_label = nullptr;
static lv_obj_t* pin_label = nullptr;
static lv_obj_t* login_status = nullptr;
static lv_obj_t* keyload_status_label = nullptr;
static lv_obj_t* keyload_progress = nullptr;

// Keyboard callback
static lv_obj_t* kb_target = nullptr;
static void (*kb_callback)(const char*) = nullptr;

// Global keyboard object
static lv_obj_t* g_keyboard = nullptr;
static lv_obj_t* g_keyboard_target = nullptr;

// =============================================================================
// Keyboard Helpers
// =============================================================================
static void keyboard_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* kb = lv_event_get_target(e);
    
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        // Hide keyboard
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        g_keyboard_target = nullptr;
    }
}

static void textarea_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* ta = lv_event_get_target(e);
    
    if (code == LV_EVENT_FOCUSED) {
        // Show keyboard and attach to this textarea
        if (g_keyboard) {
            lv_keyboard_set_textarea(g_keyboard, ta);
            lv_obj_clear_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
            g_keyboard_target = ta;
        }
    } else if (code == LV_EVENT_DEFOCUSED) {
        // Hide keyboard
        if (g_keyboard) {
            lv_obj_add_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);
            g_keyboard_target = nullptr;
        }
    }
}

static void create_global_keyboard(lv_obj_t* parent) {
    if (g_keyboard) return;  // Already created
    
    g_keyboard = lv_keyboard_create(parent);
    lv_obj_set_size(g_keyboard, SCREEN_W, SCREEN_H / 2);
    lv_obj_align(g_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(g_keyboard, LV_OBJ_FLAG_HIDDEN);  // Start hidden
    lv_obj_add_event_cb(g_keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);
    
    // Style the keyboard
    lv_obj_set_style_bg_color(g_keyboard, COLOR_BG_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_keyboard, COLOR_BUTTON, LV_PART_ITEMS);
    lv_obj_set_style_text_color(g_keyboard, COLOR_TEXT, LV_PART_ITEMS);
}

// =============================================================================
// Style Helpers
// =============================================================================
static void style_screen(lv_obj_t* scr) {
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, COLOR_BG_DARK, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

static void style_panel(lv_obj_t* panel) {
    lv_obj_set_style_bg_color(panel, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, RADIUS, 0);
    lv_obj_set_style_pad_all(panel, PAD, 0);
}

static void style_btn(lv_obj_t* btn) {
    lv_obj_set_style_bg_color(btn, COLOR_BUTTON, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_radius(btn, RADIUS, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, COLOR_BUTTON_PRESS, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btn, COLOR_TEXT, 0);
}

static void style_btn_accent(lv_obj_t* btn) {
    style_btn(btn);
    lv_obj_set_style_bg_color(btn, COLOR_ACCENT_DIM, 0);
    lv_obj_set_style_bg_color(btn, COLOR_ACCENT, LV_STATE_PRESSED);
}

static void style_btn_danger(lv_obj_t* btn) {
    style_btn(btn);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x4A1515), 0);
    lv_obj_set_style_border_color(btn, COLOR_ERROR, 0);
    lv_obj_set_style_bg_color(btn, COLOR_ERROR, LV_STATE_PRESSED);
}

static void style_textarea(lv_obj_t* ta) {
    lv_obj_set_style_bg_color(ta, COLOR_BG_INPUT, 0);
    lv_obj_set_style_border_color(ta, COLOR_BORDER, 0);
    lv_obj_set_style_border_color(ta, COLOR_ACCENT, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(ta, 2, 0);
    lv_obj_set_style_text_color(ta, COLOR_TEXT, 0);
    lv_obj_set_style_radius(ta, RADIUS, 0);
}

// =============================================================================
// Common UI Elements
// =============================================================================
static lv_obj_t* create_header(lv_obj_t* parent, const char* title, lv_event_cb_t back_cb) {
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(header, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(header, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(header, 2, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    
    if (back_cb) {
        lv_obj_t* btn = lv_btn_create(header);
        lv_obj_set_size(btn, 60, 36);
        lv_obj_align(btn, LV_ALIGN_LEFT_MID, 4, 0);
        style_btn(btn);
        lv_obj_add_event_cb(btn, back_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, LV_SYMBOL_LEFT);
        lv_obj_center(lbl);
    }
    
    lv_obj_t* lbl = lv_label_create(header);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, back_cb ? 20 : 0, 0);
    
    return header;
}

static lv_obj_t* create_footer(lv_obj_t* parent) {
    lv_obj_t* footer = lv_obj_create(parent);
    lv_obj_set_size(footer, SCREEN_W, FOOTER_H);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(footer, COLOR_BG_PANEL, 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_set_style_pad_all(footer, 4, 0);
    
    status_label = lv_label_create(footer);
    lv_label_set_text(status_label, "Ready");
    lv_obj_set_style_text_color(status_label, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
    lv_obj_align(status_label, LV_ALIGN_LEFT_MID, 4, 0);
    
    return footer;
}

static lv_obj_t* create_labeled_input(lv_obj_t* parent, const char* label, int y, int width = SCREEN_W - 20) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, y);
    
    lv_obj_t* ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, width, 36);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, y + 16);
    lv_textarea_set_one_line(ta, true);
    style_textarea(ta);
    
    // Add keyboard event handler for this textarea
    lv_obj_add_event_cb(ta, textarea_event_cb, LV_EVENT_ALL, NULL);
    
    return ta;
}

// =============================================================================
// Navigation Callbacks
// =============================================================================
static void goto_main_menu(lv_event_t* e) { (void)e; ui_show_main_menu(); }
static void goto_containers(lv_event_t* e) { (void)e; ui_show_containers(); }
static void goto_keyload(lv_event_t* e) { (void)e; ui_show_keyload(); }
static void goto_diagnostics(lv_event_t* e) { (void)e; ui_show_diagnostics(); }

// =============================================================================
// Login Screen
// =============================================================================
static void build_login_screen() {
    if (scr_login) lv_obj_del(scr_login);
    scr_login = lv_obj_create(NULL);
    style_screen(scr_login);
    
    lv_obj_t* title = lv_label_create(scr_login);
    lv_label_set_text(title, "KFDtool PRO");
    lv_obj_set_style_text_color(title, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // Role buttons
    lv_obj_t* btn_op = lv_btn_create(scr_login);
    lv_obj_set_size(btn_op, 130, 40);
    lv_obj_align(btn_op, LV_ALIGN_TOP_LEFT, 20, 70);
    style_btn(btn_op);
    lv_obj_add_event_cb(btn_op, [](lv_event_t* e) {
        (void)e; pending_role = ROLE_OPERATOR;
        lv_label_set_text(login_status, "Enter OPERATOR PIN");
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l1 = lv_label_create(btn_op);
    lv_label_set_text(l1, "OPERATOR");
    lv_obj_center(l1);
    
    lv_obj_t* btn_adm = lv_btn_create(scr_login);
    lv_obj_set_size(btn_adm, 130, 40);
    lv_obj_align(btn_adm, LV_ALIGN_TOP_RIGHT, -20, 70);
    style_btn(btn_adm);
    lv_obj_add_event_cb(btn_adm, [](lv_event_t* e) {
        (void)e; pending_role = ROLE_ADMIN;
        lv_label_set_text(login_status, "Enter ADMIN PIN");
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l2 = lv_label_create(btn_adm);
    lv_label_set_text(l2, "ADMIN");
    lv_obj_center(l2);
    
    // PIN display
    lv_obj_t* pin_panel = lv_obj_create(scr_login);
    lv_obj_set_size(pin_panel, SCREEN_W - 40, 50);
    lv_obj_align(pin_panel, LV_ALIGN_TOP_MID, 0, 120);
    style_panel(pin_panel);
    lv_obj_set_style_bg_color(pin_panel, COLOR_BG_INPUT, 0);
    
    pin_label = lv_label_create(pin_panel);
    lv_label_set_text(pin_label, "----");
    lv_obj_set_style_text_color(pin_label, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(pin_label, &lv_font_montserrat_28, 0);
    lv_obj_center(pin_label);
    
    // Keypad
    static const char* kp[] = {"1","2","3","\n","4","5","6","\n","7","8","9","\n",LV_SYMBOL_BACKSPACE,"0",LV_SYMBOL_OK,""};
    lv_obj_t* keypad = lv_btnmatrix_create(scr_login);
    lv_btnmatrix_set_map(keypad, kp);
    lv_obj_set_size(keypad, SCREEN_W - 40, 220);
    lv_obj_align(keypad, LV_ALIGN_TOP_MID, 0, 180);
    lv_obj_set_style_bg_color(keypad, COLOR_BG_PANEL, LV_PART_MAIN);
    lv_obj_set_style_border_width(keypad, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(keypad, COLOR_BUTTON, LV_PART_ITEMS);
    lv_obj_set_style_border_color(keypad, COLOR_BORDER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(keypad, COLOR_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(keypad, &lv_font_montserrat_20, LV_PART_ITEMS);
    
    lv_obj_add_event_cb(keypad, [](lv_event_t* e) {
        uint32_t id = lv_btnmatrix_get_selected_btn(lv_event_get_target(e));
        const char* txt = lv_btnmatrix_get_btn_text(lv_event_get_target(e), id);
        if (!txt) return;
        
        if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
            if (pin_len > 0) pin_buffer[--pin_len] = '\0';
        } else if (strcmp(txt, LV_SYMBOL_OK) == 0) {
            if (pending_role != ROLE_NONE && DeviceManager::instance().login(pending_role, pin_buffer)) {
                ui_show_main_menu();
            } else {
                lv_label_set_text(login_status, "Invalid PIN");
                lv_obj_set_style_text_color(login_status, COLOR_ERROR, 0);
            }
            pin_len = 0; pin_buffer[0] = '\0';
        } else if (pin_len < 6) {
            pin_buffer[pin_len++] = txt[0]; pin_buffer[pin_len] = '\0';
        }
        char disp[8]; for (int i = 0; i < 4; i++) disp[i] = (i < pin_len) ? '*' : '-'; disp[4] = '\0';
        lv_label_set_text(pin_label, disp);
    }, LV_EVENT_VALUE_CHANGED, NULL);
    
    login_status = lv_label_create(scr_login);
    lv_label_set_text(login_status, "Select role and enter PIN");
    lv_obj_set_style_text_color(login_status, COLOR_TEXT_DIM, 0);
    lv_obj_align(login_status, LV_ALIGN_BOTTOM_MID, 0, -35);
    
    lv_obj_t* serial = lv_label_create(scr_login);
    char buf[32]; snprintf(buf, sizeof(buf), "S/N: %s", DeviceManager::instance().getSerialNumber());
    lv_label_set_text(serial, buf);
    lv_obj_set_style_text_color(serial, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(serial, &lv_font_montserrat_10, 0);
    lv_obj_align(serial, LV_ALIGN_BOTTOM_MID, 0, -10);
}

// =============================================================================
// Main Menu
// =============================================================================
static void build_main_menu() {
    if (scr_main_menu) lv_obj_del(scr_main_menu);
    scr_main_menu = lv_obj_create(NULL);
    style_screen(scr_main_menu);
    
    lv_obj_t* header = lv_obj_create(scr_main_menu);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(header, COLOR_BG_PANEL, 0);
    lv_obj_set_style_border_color(header, COLOR_ACCENT, 0);
    lv_obj_set_style_border_width(header, 2, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    
    lv_obj_t* t = lv_label_create(header);
    lv_label_set_text(t, "KFDtool PRO");
    lv_obj_set_style_text_color(t, COLOR_ACCENT, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_18, 0);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, 10, 0);
    
    lv_obj_t* u = lv_label_create(header);
    lv_label_set_text(u, DeviceManager::instance().isAdmin() ? "ADMIN" : "OPER");
    lv_obj_set_style_text_color(u, COLOR_SUCCESS, 0);
    lv_obj_align(u, LV_ALIGN_RIGHT_MID, -10, 0);
    
    int y = HEADER_H + 15;
    
    // Containers button
    lv_obj_t* b1 = lv_btn_create(scr_main_menu);
    lv_obj_set_size(b1, SCREEN_W - 20, BTN_H);
    lv_obj_align(b1, LV_ALIGN_TOP_MID, 0, y);
    style_btn(b1);
    lv_obj_add_event_cb(b1, goto_containers, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l1 = lv_label_create(b1);
    lv_label_set_text(l1, LV_SYMBOL_LIST "  KEY CONTAINERS");
    lv_obj_center(l1);
    y += BTN_H + 10;
    
    // Keyload button
    lv_obj_t* b2 = lv_btn_create(scr_main_menu);
    lv_obj_set_size(b2, SCREEN_W - 20, BTN_H);
    lv_obj_align(b2, LV_ALIGN_TOP_MID, 0, y);
    style_btn_accent(b2);
    lv_obj_add_event_cb(b2, goto_keyload, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l2 = lv_label_create(b2);
    lv_label_set_text(l2, LV_SYMBOL_UPLOAD "  KEYLOAD");
    lv_obj_center(l2);
    y += BTN_H + 10;
    
    // Diagnostics
    lv_obj_t* b3 = lv_btn_create(scr_main_menu);
    lv_obj_set_size(b3, SCREEN_W - 20, BTN_H);
    lv_obj_align(b3, LV_ALIGN_TOP_MID, 0, y);
    style_btn(b3);
    lv_obj_add_event_cb(b3, goto_diagnostics, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l3 = lv_label_create(b3);
    lv_label_set_text(l3, LV_SYMBOL_SETTINGS "  DIAGNOSTICS");
    lv_obj_center(l3);
    y += BTN_H + 10;
    
    // Logout
    lv_obj_t* b4 = lv_btn_create(scr_main_menu);
    lv_obj_set_size(b4, SCREEN_W - 20, 38);
    lv_obj_align(b4, LV_ALIGN_BOTTOM_MID, 0, -FOOTER_H - 10);
    style_btn_danger(b4);
    lv_obj_add_event_cb(b4, [](lv_event_t* e) { (void)e; DeviceManager::instance().logout(); ui_show_login(); }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l4 = lv_label_create(b4);
    lv_label_set_text(l4, LV_SYMBOL_POWER " LOGOUT");
    lv_obj_center(l4);
    
    create_footer(scr_main_menu);
}

// =============================================================================
// Containers List Screen
// =============================================================================
static void refresh_containers_list();
static lv_obj_t* containers_list = nullptr;

static void on_container_click(lv_event_t* e) {
    current_container_idx = (int)(uintptr_t)lv_event_get_user_data(e);
    current_group_idx = 0;
    ContainerManager::instance().setActiveIndex(current_container_idx);
    ui_show_container_detail(current_container_idx);
}

static void on_add_container(lv_event_t* e) {
    (void)e;
    Container c;
    c.name = "New Container";
    c.description = "";
    KeyGroup g; g.name = "Keys"; g.keysetId = 1;
    c.groups.push_back(g);
    current_container_idx = ContainerManager::instance().addContainer(c);
    ContainerManager::instance().setActiveIndex(current_container_idx);
    ui_show_container_detail(current_container_idx);
}

static void build_containers_screen() {
    if (scr_containers) lv_obj_del(scr_containers);
    scr_containers = lv_obj_create(NULL);
    style_screen(scr_containers);
    
    create_header(scr_containers, "KEY CONTAINERS", goto_main_menu);
    
    containers_list = lv_obj_create(scr_containers);
    lv_obj_set_size(containers_list, SCREEN_W - 10, SCREEN_H - HEADER_H - FOOTER_H - BTN_H - 25);
    lv_obj_align(containers_list, LV_ALIGN_TOP_MID, 0, HEADER_H + 5);
    lv_obj_set_style_bg_color(containers_list, COLOR_BG_DARK, 0);
    lv_obj_set_style_border_width(containers_list, 0, 0);
    lv_obj_set_style_pad_all(containers_list, 2, 0);
    lv_obj_set_flex_flow(containers_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(containers_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    refresh_containers_list();
    
    lv_obj_t* btn_add = lv_btn_create(scr_containers);
    lv_obj_set_size(btn_add, SCREEN_W - 16, BTN_H);
    lv_obj_align(btn_add, LV_ALIGN_BOTTOM_MID, 0, -FOOTER_H - 5);
    style_btn_accent(btn_add);
    lv_obj_add_event_cb(btn_add, on_add_container, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lbl = lv_label_create(btn_add);
    lv_label_set_text(lbl, LV_SYMBOL_PLUS " NEW CONTAINER");
    lv_obj_center(lbl);
    
    create_footer(scr_containers);
}

static void refresh_containers_list() {
    lv_obj_clean(containers_list);
    
    ContainerManager& cm = ContainerManager::instance();
    for (size_t i = 0; i < cm.getContainerCount(); i++) {
        const Container& c = cm.getContainer(i);
        
        lv_obj_t* item = lv_obj_create(containers_list);
        lv_obj_set_size(item, SCREEN_W - 20, 50);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        style_panel(item);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(item, on_container_click, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        
        lv_obj_t* name = lv_label_create(item);
        lv_label_set_text(name, c.name.c_str());
        lv_obj_set_style_text_color(name, COLOR_TEXT, 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 5, -8);
        
        char buf[32]; snprintf(buf, sizeof(buf), "%d keys", (int)c.totalKeyCount());
        lv_obj_t* info = lv_label_create(item);
        lv_label_set_text(info, buf);
        lv_obj_set_style_text_color(info, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(info, &lv_font_montserrat_12, 0);
        lv_obj_align(info, LV_ALIGN_LEFT_MID, 5, 10);
        
        lv_obj_t* arrow = lv_label_create(item);
        lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(arrow, COLOR_ACCENT, 0);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -5, 0);
    }
    
    if (cm.getContainerCount() == 0) {
        lv_obj_t* lbl = lv_label_create(containers_list);
        lv_label_set_text(lbl, "No containers.\nTap + to create one.");
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    }
}

// =============================================================================
// Container Detail / Keys List Screen
// =============================================================================
static lv_obj_t* keys_list = nullptr;
static lv_obj_t* ta_container_name = nullptr;

static void refresh_keys_list();

static void on_key_click(lv_event_t* e) {
    current_key_idx = (int)(uintptr_t)lv_event_get_user_data(e);
    ui_show_key_edit(current_container_idx, current_group_idx, current_key_idx);
}

static void on_key_checkbox(lv_event_t* e) {
    int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t* cb = lv_event_get_target(e);
    bool checked = lv_obj_has_state(cb, LV_STATE_CHECKED);
    
    Container* c = ContainerManager::instance().getActiveContainerMutable();
    if (c && current_group_idx < (int)c->groups.size() && idx < (int)c->groups[current_group_idx].keys.size()) {
        c->groups[current_group_idx].keys[idx].selected = checked;
    }
}

static void on_add_key(lv_event_t* e) {
    (void)e;
    Container* c = ContainerManager::instance().getActiveContainerMutable();
    if (!c || c->groups.empty()) return;
    
    KeySlot k;
    k.name = "New Key";
    k.algorithmId = P25::ALGO_AES_256;
    k.keyId = 1;
    k.sln = c->groups[current_group_idx].keys.size() + 1;
    k.keyHex = KeyGen::generateAes256();
    k.selected = true;
    
    c->groups[current_group_idx].keys.push_back(k);
    c->touch();
    ContainerManager::instance().save();
    refresh_keys_list();
}

static void on_save_container_name(lv_event_t* e) {
    (void)e;
    Container* c = ContainerManager::instance().getActiveContainerMutable();
    if (c && ta_container_name) {
        c->name = lv_textarea_get_text(ta_container_name);
        c->touch();
        ContainerManager::instance().save();
        ui_set_status("Saved");
    }
}

static void on_delete_container(lv_event_t* e) {
    (void)e;
    if (current_container_idx >= 0) {
        ContainerManager::instance().deleteContainer(current_container_idx);
        current_container_idx = -1;
        ui_show_containers();
    }
}

static void build_container_detail_screen() {
    if (scr_keys) lv_obj_del(scr_keys);
    scr_keys = lv_obj_create(NULL);
    style_screen(scr_keys);
    
    create_header(scr_keys, "CONTAINER", goto_containers);
    
    const Container* c = ContainerManager::instance().getActiveContainer();
    int y = HEADER_H + 8;
    
    // Container name input
    lv_obj_t* lbl = lv_label_create(scr_keys);
    lv_label_set_text(lbl, "Name:");
    lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, y);
    
    ta_container_name = lv_textarea_create(scr_keys);
    lv_obj_set_size(ta_container_name, SCREEN_W - 80, 34);
    lv_obj_align(ta_container_name, LV_ALIGN_TOP_LEFT, 55, y - 2);
    lv_textarea_set_one_line(ta_container_name, true);
    style_textarea(ta_container_name);
    if (c) lv_textarea_set_text(ta_container_name, c->name.c_str());
    lv_obj_add_event_cb(ta_container_name, on_save_container_name, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ta_container_name, textarea_event_cb, LV_EVENT_ALL, NULL);
    
    // Save button for name
    lv_obj_t* btn_save = lv_btn_create(scr_keys);
    lv_obj_set_size(btn_save, 45, 34);
    lv_obj_align(btn_save, LV_ALIGN_TOP_RIGHT, -10, y - 2);
    style_btn(btn_save);
    lv_obj_add_event_cb(btn_save, on_save_container_name, LV_EVENT_CLICKED, NULL);
    lv_obj_t* sl = lv_label_create(btn_save);
    lv_label_set_text(sl, LV_SYMBOL_SAVE);
    lv_obj_center(sl);
    
    y += 40;
    
    // Keys header
    lv_obj_t* kh = lv_label_create(scr_keys);
    lv_label_set_text(kh, "KEYS (tap to edit, check to load):");
    lv_obj_set_style_text_color(kh, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(kh, &lv_font_montserrat_12, 0);
    lv_obj_align(kh, LV_ALIGN_TOP_LEFT, 10, y);
    y += 18;
    
    // Keys list
    keys_list = lv_obj_create(scr_keys);
    lv_obj_set_size(keys_list, SCREEN_W - 10, SCREEN_H - y - FOOTER_H - BTN_H * 2 - 25);
    lv_obj_align(keys_list, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(keys_list, COLOR_BG_DARK, 0);
    lv_obj_set_style_border_width(keys_list, 0, 0);
    lv_obj_set_style_pad_all(keys_list, 2, 0);
    lv_obj_set_flex_flow(keys_list, LV_FLEX_FLOW_COLUMN);
    
    refresh_keys_list();
    
    // Add key button
    lv_obj_t* btn_add = lv_btn_create(scr_keys);
    lv_obj_set_size(btn_add, SCREEN_W - 16, BTN_H);
    lv_obj_align(btn_add, LV_ALIGN_BOTTOM_MID, 0, -FOOTER_H - BTN_H - 12);
    style_btn_accent(btn_add);
    lv_obj_add_event_cb(btn_add, on_add_key, LV_EVENT_CLICKED, NULL);
    lv_obj_t* la = lv_label_create(btn_add);
    lv_label_set_text(la, LV_SYMBOL_PLUS " ADD KEY");
    lv_obj_center(la);
    
    // Delete container button
    lv_obj_t* btn_del = lv_btn_create(scr_keys);
    lv_obj_set_size(btn_del, SCREEN_W - 16, BTN_H);
    lv_obj_align(btn_del, LV_ALIGN_BOTTOM_MID, 0, -FOOTER_H - 5);
    style_btn_danger(btn_del);
    lv_obj_add_event_cb(btn_del, on_delete_container, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ld = lv_label_create(btn_del);
    lv_label_set_text(ld, LV_SYMBOL_TRASH " DELETE CONTAINER");
    lv_obj_center(ld);
    
    create_footer(scr_keys);
    
    // Create keyboard (must be last so it appears on top)
    g_keyboard = nullptr;  // Reset so it gets recreated
    create_global_keyboard(scr_keys);
}

static void refresh_keys_list() {
    lv_obj_clean(keys_list);
    
    const Container* c = ContainerManager::instance().getActiveContainer();
    if (!c || c->groups.empty()) {
        lv_obj_t* lbl = lv_label_create(keys_list);
        lv_label_set_text(lbl, "No keys. Tap + to add.");
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        return;
    }
    
    const KeyGroup& g = c->groups[current_group_idx];
    for (size_t i = 0; i < g.keys.size(); i++) {
        const KeySlot& k = g.keys[i];
        
        lv_obj_t* item = lv_obj_create(keys_list);
        lv_obj_set_size(item, SCREEN_W - 20, 54);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        style_panel(item);
        
        // Checkbox for selection
        lv_obj_t* cb = lv_checkbox_create(item);
        lv_checkbox_set_text(cb, "");
        lv_obj_align(cb, LV_ALIGN_LEFT_MID, 0, 0);
        if (k.selected) lv_obj_add_state(cb, LV_STATE_CHECKED);
        lv_obj_add_event_cb(cb, on_key_checkbox, LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)i);
        
        // Clickable area for editing
        lv_obj_t* click_area = lv_obj_create(item);
        lv_obj_set_size(click_area, SCREEN_W - 80, 50);
        lv_obj_align(click_area, LV_ALIGN_LEFT_MID, 30, 0);
        lv_obj_set_style_bg_opa(click_area, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(click_area, 0, 0);
        lv_obj_add_flag(click_area, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(click_area, on_key_click, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        
        lv_obj_t* name = lv_label_create(click_area);
        lv_label_set_text(name, k.name.c_str());
        lv_obj_set_style_text_color(name, COLOR_TEXT, 0);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 0, 0);
        
        char buf[64];
        const char* algoName = P25::getAlgorithmName(k.algorithmId);
        snprintf(buf, sizeof(buf), "SLN:%d KID:%d %s", k.sln, k.keyId, algoName);
        lv_obj_t* info = lv_label_create(click_area);
        lv_label_set_text(info, buf);
        lv_obj_set_style_text_color(info, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(info, &lv_font_montserrat_10, 0);
        lv_obj_align(info, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        
        lv_obj_t* arrow = lv_label_create(item);
        lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(arrow, COLOR_ACCENT, 0);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -5, 0);
    }
}

// =============================================================================
// Key Edit Screen
// =============================================================================
static lv_obj_t* ta_key_name = nullptr;
static lv_obj_t* ta_key_sln = nullptr;
static lv_obj_t* ta_key_id = nullptr;
static lv_obj_t* ta_key_value = nullptr;
static lv_obj_t* dd_algo = nullptr;

static void save_key() {
    Container* c = ContainerManager::instance().getActiveContainerMutable();
    if (!c || current_group_idx >= (int)c->groups.size() || current_key_idx < 0 ||
        current_key_idx >= (int)c->groups[current_group_idx].keys.size()) return;
    
    KeySlot& k = c->groups[current_group_idx].keys[current_key_idx];
    k.name = lv_textarea_get_text(ta_key_name);
    k.sln = atoi(lv_textarea_get_text(ta_key_sln));
    k.keyId = atoi(lv_textarea_get_text(ta_key_id));
    k.keyHex = lv_textarea_get_text(ta_key_value);
    
    uint16_t sel = lv_dropdown_get_selected(dd_algo);
    static const uint8_t algos[] = {P25::ALGO_AES_256, P25::ALGO_DES_OFB, P25::ALGO_2_KEY_3DES, P25::ALGO_3_KEY_3DES};
    if (sel < 4) k.algorithmId = algos[sel];
    
    c->touch();
    ContainerManager::instance().save();
    ui_set_status("Key saved");
}

static void on_generate_key(lv_event_t* e) {
    (void)e;
    uint16_t sel = lv_dropdown_get_selected(dd_algo);
    std::string hex;
    switch (sel) {
        case 0: hex = KeyGen::generateAes256(); break;
        case 1: hex = KeyGen::generateDes(); break;
        case 2: hex = KeyGen::generate3Des2Key(); break;
        case 3: hex = KeyGen::generate3Des3Key(); break;
        default: hex = KeyGen::generateAes256();
    }
    lv_textarea_set_text(ta_key_value, hex.c_str());
    ui_set_status("Key generated");
}

static void on_delete_key(lv_event_t* e) {
    (void)e;
    Container* c = ContainerManager::instance().getActiveContainerMutable();
    if (!c || current_group_idx >= (int)c->groups.size() || current_key_idx < 0 ||
        current_key_idx >= (int)c->groups[current_group_idx].keys.size()) return;
    
    c->groups[current_group_idx].keys.erase(c->groups[current_group_idx].keys.begin() + current_key_idx);
    c->touch();
    ContainerManager::instance().save();
    ui_show_container_detail(current_container_idx);
}

static void build_key_edit_screen() {
    if (scr_key_edit) lv_obj_del(scr_key_edit);
    scr_key_edit = lv_obj_create(NULL);
    style_screen(scr_key_edit);
    
    create_header(scr_key_edit, "EDIT KEY", [](lv_event_t* e) {
        (void)e; save_key(); ui_show_container_detail(current_container_idx);
    });
    
    const Container* c = ContainerManager::instance().getActiveContainer();
    if (!c || current_group_idx >= (int)c->groups.size() || current_key_idx < 0 ||
        current_key_idx >= (int)c->groups[current_group_idx].keys.size()) return;
    
    const KeySlot& k = c->groups[current_group_idx].keys[current_key_idx];
    
    int y = HEADER_H + 10;
    
    // Name
    ta_key_name = create_labeled_input(scr_key_edit, "Key Name:", y);
    lv_textarea_set_text(ta_key_name, k.name.c_str());
    y += 55;
    
    // SLN
    lv_obj_t* l1 = lv_label_create(scr_key_edit);
    lv_label_set_text(l1, "SLN/CKR:");
    lv_obj_set_style_text_color(l1, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_12, 0);
    lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 10, y);
    
    ta_key_sln = lv_textarea_create(scr_key_edit);
    lv_obj_set_size(ta_key_sln, 100, 34);
    lv_obj_align(ta_key_sln, LV_ALIGN_TOP_LEFT, 10, y + 16);
    lv_textarea_set_one_line(ta_key_sln, true);
    lv_textarea_set_accepted_chars(ta_key_sln, "0123456789");
    style_textarea(ta_key_sln);
    lv_obj_add_event_cb(ta_key_sln, textarea_event_cb, LV_EVENT_ALL, NULL);
    char buf[16]; snprintf(buf, sizeof(buf), "%d", k.sln);
    lv_textarea_set_text(ta_key_sln, buf);
    
    // Key ID
    lv_obj_t* l2 = lv_label_create(scr_key_edit);
    lv_label_set_text(l2, "Key ID:");
    lv_obj_set_style_text_color(l2, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_12, 0);
    lv_obj_align(l2, LV_ALIGN_TOP_LEFT, 120, y);
    
    ta_key_id = lv_textarea_create(scr_key_edit);
    lv_obj_set_size(ta_key_id, 100, 34);
    lv_obj_align(ta_key_id, LV_ALIGN_TOP_LEFT, 120, y + 16);
    lv_textarea_set_one_line(ta_key_id, true);
    lv_textarea_set_accepted_chars(ta_key_id, "0123456789");
    style_textarea(ta_key_id);
    lv_obj_add_event_cb(ta_key_id, textarea_event_cb, LV_EVENT_ALL, NULL);
    snprintf(buf, sizeof(buf), "%d", k.keyId);
    lv_textarea_set_text(ta_key_id, buf);
    
    y += 60;
    
    // Algorithm dropdown
    lv_obj_t* l3 = lv_label_create(scr_key_edit);
    lv_label_set_text(l3, "Algorithm:");
    lv_obj_set_style_text_color(l3, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(l3, &lv_font_montserrat_12, 0);
    lv_obj_align(l3, LV_ALIGN_TOP_LEFT, 10, y);
    
    dd_algo = lv_dropdown_create(scr_key_edit);
    lv_obj_set_size(dd_algo, SCREEN_W - 20, 36);
    lv_obj_align(dd_algo, LV_ALIGN_TOP_MID, 0, y + 16);
    lv_dropdown_set_options(dd_algo, "AES-256\nDES-OFB\n2-key 3DES\n3-key 3DES");
    lv_obj_set_style_bg_color(dd_algo, COLOR_BG_INPUT, 0);
    lv_obj_set_style_border_color(dd_algo, COLOR_BORDER, 0);
    lv_obj_set_style_text_color(dd_algo, COLOR_TEXT, 0);
    
    int algoIdx = 0;
    if (k.algorithmId == P25::ALGO_DES_OFB) algoIdx = 1;
    else if (k.algorithmId == P25::ALGO_2_KEY_3DES) algoIdx = 2;
    else if (k.algorithmId == P25::ALGO_3_KEY_3DES) algoIdx = 3;
    lv_dropdown_set_selected(dd_algo, algoIdx);
    
    y += 60;
    
    // Key value
    lv_obj_t* l4 = lv_label_create(scr_key_edit);
    lv_label_set_text(l4, "Key Value (hex):");
    lv_obj_set_style_text_color(l4, COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(l4, &lv_font_montserrat_12, 0);
    lv_obj_align(l4, LV_ALIGN_TOP_LEFT, 10, y);
    
    ta_key_value = lv_textarea_create(scr_key_edit);
    lv_obj_set_size(ta_key_value, SCREEN_W - 20, 70);
    lv_obj_align(ta_key_value, LV_ALIGN_TOP_MID, 0, y + 16);
    lv_textarea_set_one_line(ta_key_value, false);
    lv_textarea_set_accepted_chars(ta_key_value, "0123456789ABCDEFabcdef");
    style_textarea(ta_key_value);
    lv_obj_add_event_cb(ta_key_value, textarea_event_cb, LV_EVENT_ALL, NULL);
    lv_textarea_set_text(ta_key_value, k.keyHex.c_str());
    
    y += 95;
    
    // Generate button
    lv_obj_t* btn_gen = lv_btn_create(scr_key_edit);
    lv_obj_set_size(btn_gen, SCREEN_W - 20, BTN_H);
    lv_obj_align(btn_gen, LV_ALIGN_TOP_MID, 0, y);
    style_btn_accent(btn_gen);
    lv_obj_add_event_cb(btn_gen, on_generate_key, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lg = lv_label_create(btn_gen);
    lv_label_set_text(lg, LV_SYMBOL_REFRESH " GENERATE RANDOM KEY");
    lv_obj_center(lg);
    
    y += BTN_H + 10;
    
    // Save button
    lv_obj_t* btn_save = lv_btn_create(scr_key_edit);
    lv_obj_set_size(btn_save, SCREEN_W - 20, BTN_H);
    lv_obj_align(btn_save, LV_ALIGN_TOP_MID, 0, y);
    style_btn(btn_save);
    lv_obj_add_event_cb(btn_save, [](lv_event_t* e) { (void)e; save_key(); }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ls = lv_label_create(btn_save);
    lv_label_set_text(ls, LV_SYMBOL_SAVE " SAVE KEY");
    lv_obj_center(ls);
    
    // Delete button
    lv_obj_t* btn_del = lv_btn_create(scr_key_edit);
    lv_obj_set_size(btn_del, SCREEN_W - 20, BTN_H);
    lv_obj_align(btn_del, LV_ALIGN_BOTTOM_MID, 0, -FOOTER_H - 5);
    style_btn_danger(btn_del);
    lv_obj_add_event_cb(btn_del, on_delete_key, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ld = lv_label_create(btn_del);
    lv_label_set_text(ld, LV_SYMBOL_TRASH " DELETE KEY");
    lv_obj_center(ld);
    
    create_footer(scr_key_edit);
    
    // Create keyboard (must be last so it appears on top)
    g_keyboard = nullptr;  // Reset so it gets recreated
    create_global_keyboard(scr_key_edit);
}

// =============================================================================
// Keyload Screen
// =============================================================================
static lv_obj_t* keyload_list = nullptr;

static void refresh_keyload_list();
static void do_keyload_selected();
static void do_keyload_single(int idx);

static void build_keyload_screen() {
    if (scr_keyload) lv_obj_del(scr_keyload);
    scr_keyload = lv_obj_create(NULL);
    style_screen(scr_keyload);
    
    create_header(scr_keyload, "KEYLOAD", goto_main_menu);
    
    const Container* c = ContainerManager::instance().getActiveContainer();
    int y = HEADER_H + 5;
    
    // Container info
    lv_obj_t* lbl = lv_label_create(scr_keyload);
    if (c) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Container: %s", c->name.c_str());
        lv_label_set_text(lbl, buf);
    } else {
        lv_label_set_text(lbl, "No container selected");
    }
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, y);
    y += 22;
    
    // Status panel
    lv_obj_t* status_panel = lv_obj_create(scr_keyload);
    lv_obj_set_size(status_panel, SCREEN_W - 16, 70);
    lv_obj_align(status_panel, LV_ALIGN_TOP_MID, 0, y);
    style_panel(status_panel);
    
    keyload_status_label = lv_label_create(status_panel);
    lv_label_set_text(keyload_status_label, "Select keys and tap LOAD.\nOr tap a single key to load it.");
    lv_obj_set_style_text_color(keyload_status_label, COLOR_TEXT, 0);
    lv_label_set_long_mode(keyload_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(keyload_status_label, SCREEN_W - 40);
    lv_obj_align(keyload_status_label, LV_ALIGN_TOP_LEFT, 5, 0);
    
    keyload_progress = lv_bar_create(status_panel);
    lv_obj_set_size(keyload_progress, SCREEN_W - 40, 14);
    lv_obj_align(keyload_progress, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_bar_set_value(keyload_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(keyload_progress, COLOR_BG_INPUT, LV_PART_MAIN);
    lv_obj_set_style_bg_color(keyload_progress, COLOR_SUCCESS, LV_PART_INDICATOR);
    
    y += 80;
    
    // Keys list with load buttons
    keyload_list = lv_obj_create(scr_keyload);
    lv_obj_set_size(keyload_list, SCREEN_W - 10, SCREEN_H - y - FOOTER_H - BTN_H - 15);
    lv_obj_align(keyload_list, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(keyload_list, COLOR_BG_DARK, 0);
    lv_obj_set_style_border_width(keyload_list, 0, 0);
    lv_obj_set_style_pad_all(keyload_list, 2, 0);
    lv_obj_set_flex_flow(keyload_list, LV_FLEX_FLOW_COLUMN);
    
    refresh_keyload_list();
    
    // Test buttons (for debugging)
    lv_obj_t* btn_test = lv_btn_create(scr_keyload);
    lv_obj_set_size(btn_test, (SCREEN_W - 24) / 2, BTN_H);
    lv_obj_align(btn_test, LV_ALIGN_BOTTOM_LEFT, 8, -FOOTER_H - 5 - BTN_H - 5);
    lv_obj_set_style_bg_color(btn_test, lv_color_hex(0x555555), 0);
    lv_obj_add_event_cb(btn_test, [](lv_event_t* e) { 
        (void)e; 
        KFDProtocol& kfd = getKfdProtocol();
        lv_label_set_text(keyload_status_label, "Testing inventory...");
        lv_refr_now(NULL);
        auto result = kfd.testInventory();
        lv_label_set_text(keyload_status_label, result.message.c_str());
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lt = lv_label_create(btn_test);
    lv_label_set_text(lt, "TEST INV");
    lv_obj_center(lt);
    
    // TEST DES button
    lv_obj_t* btn_test_des = lv_btn_create(scr_keyload);
    lv_obj_set_size(btn_test_des, (SCREEN_W - 24) / 2, BTN_H);
    lv_obj_align(btn_test_des, LV_ALIGN_BOTTOM_RIGHT, -8, -FOOTER_H - 5 - BTN_H - 5);
    lv_obj_set_style_bg_color(btn_test_des, lv_color_hex(0x555555), 0);
    lv_obj_add_event_cb(btn_test_des, [](lv_event_t* e) { 
        (void)e; 
        KFDProtocol& kfd = getKfdProtocol();
        lv_label_set_text(keyload_status_label, "Testing SLN 202...");
        lv_refr_now(NULL);
        auto result = kfd.testDESKey();
        lv_label_set_text(keyload_status_label, result.message.c_str());
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ltd = lv_label_create(btn_test_des);
    lv_label_set_text(ltd, "TEST 202");
    lv_obj_center(ltd);
    
    // Load selected button
    lv_obj_t* btn_load = lv_btn_create(scr_keyload);
    lv_obj_set_size(btn_load, SCREEN_W - 16, BTN_H);
    lv_obj_align(btn_load, LV_ALIGN_BOTTOM_MID, 0, -FOOTER_H - 5);
    style_btn_accent(btn_load);
    lv_obj_add_event_cb(btn_load, [](lv_event_t* e) { (void)e; do_keyload_selected(); }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ll = lv_label_create(btn_load);
    lv_label_set_text(ll, LV_SYMBOL_UPLOAD " LOAD SELECTED KEYS");
    lv_obj_center(ll);
    
    create_footer(scr_keyload);
}

static void refresh_keyload_list() {
    lv_obj_clean(keyload_list);
    
    const Container* c = ContainerManager::instance().getActiveContainer();
    if (!c || c->groups.empty()) {
        lv_obj_t* lbl = lv_label_create(keyload_list);
        lv_label_set_text(lbl, "No keys available.\nCreate keys in Containers.");
        lv_obj_set_style_text_color(lbl, COLOR_TEXT_DIM, 0);
        return;
    }
    
    const KeyGroup& g = c->groups[0];
    for (size_t i = 0; i < g.keys.size(); i++) {
        const KeySlot& k = g.keys[i];
        
        lv_obj_t* item = lv_obj_create(keyload_list);
        lv_obj_set_size(item, SCREEN_W - 20, 48);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        style_panel(item);
        
        // Checkbox
        lv_obj_t* cb = lv_checkbox_create(item);
        lv_checkbox_set_text(cb, "");
        lv_obj_align(cb, LV_ALIGN_LEFT_MID, 0, 0);
        if (k.selected) lv_obj_add_state(cb, LV_STATE_CHECKED);
        lv_obj_add_event_cb(cb, [](lv_event_t* e) {
            int idx = (int)(uintptr_t)lv_event_get_user_data(e);
            Container* c = ContainerManager::instance().getActiveContainerMutable();
            if (c && !c->groups.empty() && idx < (int)c->groups[0].keys.size()) {
                c->groups[0].keys[idx].selected = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
            }
        }, LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)i);
        
        // Info
        lv_obj_t* name = lv_label_create(item);
        lv_label_set_text(name, k.name.c_str());
        lv_obj_set_style_text_color(name, COLOR_TEXT, 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 35, -8);
        
        char buf[32];
        snprintf(buf, sizeof(buf), "SLN:%d", k.sln);
        lv_obj_t* info = lv_label_create(item);
        lv_label_set_text(info, buf);
        lv_obj_set_style_text_color(info, COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(info, &lv_font_montserrat_10, 0);
        lv_obj_align(info, LV_ALIGN_LEFT_MID, 35, 8);
        
        // Single load button
        lv_obj_t* btn = lv_btn_create(item);
        lv_obj_set_size(btn, 50, 32);
        lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -2, 0);
        style_btn(btn);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            int idx = (int)(uintptr_t)lv_event_get_user_data(e);
            do_keyload_single(idx);
        }, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        lv_obj_t* bl = lv_label_create(btn);
        lv_label_set_text(bl, "LOAD");
        lv_obj_set_style_text_font(bl, &lv_font_montserrat_10, 0);
        lv_obj_center(bl);
    }
}

static void do_keyload_single(int idx) {
    const Container* c = ContainerManager::instance().getActiveContainer();
    if (!c || c->groups.empty() || idx >= (int)c->groups[0].keys.size()) return;
    
    const KeySlot& k = c->groups[0].keys[idx];
    
    lv_label_set_text(keyload_status_label, "Loading key...");
    lv_obj_set_style_text_color(keyload_status_label, COLOR_WARNING, 0);
    lv_bar_set_value(keyload_progress, 50, LV_ANIM_ON);
    lv_refr_now(NULL);
    
    P25::KeyItem item = k.toKeyItem(c->groups[0].keysetId);
    auto result = getKfdProtocol().keyload(item);
    
    if (result.success) {
        lv_label_set_text(keyload_status_label, "Key loaded successfully!");
        lv_obj_set_style_text_color(keyload_status_label, COLOR_SUCCESS, 0);
        lv_bar_set_value(keyload_progress, 100, LV_ANIM_ON);
        DeviceManager::instance().recordKeyload();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "Failed: %s", result.message.c_str());
        lv_label_set_text(keyload_status_label, buf);
        lv_obj_set_style_text_color(keyload_status_label, COLOR_ERROR, 0);
        lv_bar_set_value(keyload_progress, 0, LV_ANIM_ON);
    }
}

static void do_keyload_selected() {
    const Container* c = ContainerManager::instance().getActiveContainer();
    if (!c) return;
    
    auto selected = c->getSelectedKeys();
    if (selected.empty()) {
        lv_label_set_text(keyload_status_label, "No keys selected!");
        lv_obj_set_style_text_color(keyload_status_label, COLOR_ERROR, 0);
        return;
    }
    
    std::vector<P25::KeyItem> keys;
    uint8_t keysetId = c->groups.empty() ? 1 : c->groups[0].keysetId;
    for (const auto& ks : selected) {
        keys.push_back(ks.toKeyItem(keysetId));
    }
    
    lv_label_set_text(keyload_status_label, "Starting keyload...");
    lv_obj_set_style_text_color(keyload_status_label, COLOR_WARNING, 0);
    lv_bar_set_value(keyload_progress, 0, LV_ANIM_OFF);
    lv_refr_now(NULL);
    
    auto result = getKfdProtocol().keyloadMultiple(keys, [](int cur, int tot, const char* status) {
        int pct = (tot > 0) ? (cur * 100 / tot) : 0;
        lv_bar_set_value(keyload_progress, pct, LV_ANIM_ON);
        lv_label_set_text(keyload_status_label, status);
        lv_refr_now(NULL);
    });
    
    if (result.success) {
        lv_label_set_text(keyload_status_label, "All keys loaded!");
        lv_obj_set_style_text_color(keyload_status_label, COLOR_SUCCESS, 0);
        lv_bar_set_value(keyload_progress, 100, LV_ANIM_ON);
        DeviceManager::instance().recordKeyload();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "Failed: %s", result.message.c_str());
        lv_label_set_text(keyload_status_label, buf);
        lv_obj_set_style_text_color(keyload_status_label, COLOR_ERROR, 0);
    }
}

// =============================================================================
// Diagnostics Screen
// =============================================================================
static void build_diagnostics_screen() {
    if (scr_diagnostics) lv_obj_del(scr_diagnostics);
    scr_diagnostics = lv_obj_create(NULL);
    style_screen(scr_diagnostics);
    
    create_header(scr_diagnostics, "DIAGNOSTICS", goto_main_menu);
    
    int y = HEADER_H + 10;
    
    // Device info
    lv_obj_t* info_panel = lv_obj_create(scr_diagnostics);
    lv_obj_set_size(info_panel, SCREEN_W - 16, 130);
    lv_obj_align(info_panel, LV_ALIGN_TOP_MID, 0, y);
    style_panel(info_panel);
    
    const DeviceInfo& info = DeviceManager::instance().getInfo();
    char buf[256];
    snprintf(buf, sizeof(buf), "Serial: %s\nModel: %s\nFirmware: %s\nUID: %08X\nKeyloads: %u",
        info.serialNumber, info.modelNumber, info.firmwareVer, info.uniqueId, info.keyloadCount);
    lv_obj_t* lbl = lv_label_create(info_panel);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 5, 5);
    
    y += 145;
    
    // Self-test button
    lv_obj_t* btn_test = lv_btn_create(scr_diagnostics);
    lv_obj_set_size(btn_test, SCREEN_W - 16, BTN_H);
    lv_obj_align(btn_test, LV_ALIGN_TOP_MID, 0, y);
    style_btn(btn_test);
    lv_obj_add_event_cb(btn_test, [](lv_event_t* e) {
        (void)e;
        uint8_t result = getTwiHal().selfTest();
        if (result == 0x00) ui_show_message("Self-Test", "PASSED\nAdapter OK.", 0);
        else { char b[64]; snprintf(b, sizeof(b), "FAILED\nCode: 0x%02X", result); ui_show_message("Self-Test", b, 2); }
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lt = lv_label_create(btn_test);
    lv_label_set_text(lt, LV_SYMBOL_REFRESH " SELF-TEST");
    lv_obj_center(lt);
    
    y += BTN_H + 10;
    
    // Enable interface button
    lv_obj_t* btn_en = lv_btn_create(scr_diagnostics);
    lv_obj_set_size(btn_en, SCREEN_W - 16, BTN_H);
    lv_obj_align(btn_en, LV_ALIGN_TOP_MID, 0, y);
    style_btn_accent(btn_en);
    lv_obj_add_event_cb(btn_en, [](lv_event_t* e) {
        (void)e;
        getTwiHal().enableInterface();
        ui_show_message("Interface", "SENSE pulled LOW.\nRadio should wake up.", 0);
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* le = lv_label_create(btn_en);
    lv_label_set_text(le, LV_SYMBOL_POWER " ENABLE INTERFACE");
    lv_obj_center(le);
    
    y += BTN_H + 10;
    
    // Disable interface button
    lv_obj_t* btn_dis = lv_btn_create(scr_diagnostics);
    lv_obj_set_size(btn_dis, SCREEN_W - 16, BTN_H);
    lv_obj_align(btn_dis, LV_ALIGN_TOP_MID, 0, y);
    style_btn(btn_dis);
    lv_obj_add_event_cb(btn_dis, [](lv_event_t* e) {
        (void)e;
        getTwiHal().disableInterface();
        ui_set_status("Interface disabled");
    }, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ld = lv_label_create(btn_dis);
    lv_label_set_text(ld, "DISABLE INTERFACE");
    lv_obj_center(ld);
    
    create_footer(scr_diagnostics);
}

// =============================================================================
// Public API
// =============================================================================
void ui_init(void) {
    build_login_screen();
    if (!DeviceManager::instance().getSettings().requireLogin) {
        DeviceManager::instance().login(ROLE_OPERATOR, DEFAULT_OPERATOR_PIN);
        build_main_menu();
        lv_scr_load(scr_main_menu);
    } else {
        lv_scr_load(scr_login);
    }
}

void ui_show_login(void) { build_login_screen(); lv_scr_load(scr_login); }
void ui_show_main_menu(void) { build_main_menu(); lv_scr_load(scr_main_menu); }
void ui_show_containers(void) { build_containers_screen(); lv_scr_load(scr_containers); }
void ui_show_container_detail(int idx) { current_container_idx = idx; build_container_detail_screen(); lv_scr_load(scr_keys); }
void ui_show_key_edit(int c, int g, int k) { current_container_idx = c; current_group_idx = g; current_key_idx = k; build_key_edit_screen(); lv_scr_load(scr_key_edit); }
void ui_show_keyload(void) { build_keyload_screen(); lv_scr_load(scr_keyload); }
void ui_show_diagnostics(void) { build_diagnostics_screen(); lv_scr_load(scr_diagnostics); }

void ui_show_message(const char* title, const char* msg, int type) {
    lv_color_t c = (type == 0) ? COLOR_ACCENT : (type == 1) ? COLOR_WARNING : COLOR_ERROR;
    lv_obj_t* mbox = lv_msgbox_create(NULL, title, msg, NULL, true);
    lv_obj_set_style_bg_color(mbox, COLOR_BG_PANEL, 0);
    lv_obj_set_style_border_color(mbox, c, 0);
    lv_obj_set_style_border_width(mbox, 2, 0);
    lv_obj_center(mbox);
}

void ui_set_status(const char* text) { if (status_label) lv_label_set_text(status_label, text); }
void ui_update_user_display(void) {}
int ui_get_screen_width(void) { return SCREEN_W; }
int ui_get_screen_height(void) { return SCREEN_H; }
bool ui_can_navigate_away(void) { return !getKfdProtocol().isOperationInProgress(); }

// Stubs
void ui_show_multiple_keyload(void) {}
void ui_show_key_erase(void) { ui_show_message("Key Erase", "Coming soon", 0); }
void ui_show_erase_all_keys(void) { ui_show_message("Erase All", "Coming soon", 0); }
void ui_show_view_key_info(void) { ui_show_message("View Info", "Coming soon", 0); }
void ui_show_view_keyset_info(void) {}
void ui_show_rsi_config(void) {}
void ui_show_kmf_config(void) {}
void ui_show_mnp_config(void) {}
void ui_show_mr_emulator(void) {}
void ui_show_settings(void) { ui_show_message("Settings", "Coming soon", 0); }
void ui_show_about(void) {}
void ui_show_confirm(const char*, const char*, void (*)(void), void (*)(void)) {}
void ui_show_password_entry(const char*, void (*)(const char*)) {}
void ui_show_progress(const char*, const char*) {}
void ui_update_progress(int, const char*) {}
void ui_close_progress(void) {}
void ui_update_radio_status(void) {}
