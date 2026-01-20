#pragma once

/**
 * @file ui.h
 * @brief KFDtool Professional UI - Screen Declarations
 * 
 * Provides all UI screens matching KFDtool desktop functionality:
 * - Splash screen with serial number
 * - Login screen (Operator/Admin)
 * - Main menu with navigation
 * - Key container management
 * - Keyload operations
 * - Key erase operations
 * - View key info
 * - RSI configuration
 * - KMF configuration
 * - MNP configuration
 * - Keyset management
 * - MR Emulator
 * - Diagnostics & Self-test
 * - Settings
 */

#include <lvgl.h>

// =============================================================================
// UI Initialization
// =============================================================================

/**
 * @brief Initialize the UI system
 * Creates all screens and shows the appropriate startup screen
 */
void ui_init(void);

// =============================================================================
// Screen Navigation
// =============================================================================

/**
 * @brief Show the login screen
 */
void ui_show_login(void);

/**
 * @brief Show the main menu
 */
void ui_show_main_menu(void);

/**
 * @brief Show container management screen
 */
void ui_show_containers(void);

/**
 * @brief Show container detail/edit screen
 * @param containerIndex Index of container to show (-1 for new)
 */
void ui_show_container_detail(int containerIndex);

/**
 * @brief Show key edit screen
 * @param containerIndex Container index
 * @param groupIndex Group index within container
 * @param keyIndex Key index (-1 for new)
 */
void ui_show_key_edit(int containerIndex, int groupIndex, int keyIndex);

/**
 * @brief Show keyload screen
 */
void ui_show_keyload(void);

/**
 * @brief Show multiple keyload screen (container-based)
 */
void ui_show_multiple_keyload(void);

/**
 * @brief Show key erase screen
 */
void ui_show_key_erase(void);

/**
 * @brief Show erase all keys screen
 */
void ui_show_erase_all_keys(void);

/**
 * @brief Show view key info screen
 */
void ui_show_view_key_info(void);

/**
 * @brief Show view keyset info screen
 */
void ui_show_view_keyset_info(void);

/**
 * @brief Show RSI configuration screen
 */
void ui_show_rsi_config(void);

/**
 * @brief Show KMF configuration screen
 */
void ui_show_kmf_config(void);

/**
 * @brief Show MNP configuration screen
 */
void ui_show_mnp_config(void);

/**
 * @brief Show MR emulator screen
 */
void ui_show_mr_emulator(void);

/**
 * @brief Show diagnostics screen (self-test, pin config)
 */
void ui_show_diagnostics(void);

/**
 * @brief Show settings screen
 */
void ui_show_settings(void);

/**
 * @brief Show about screen
 */
void ui_show_about(void);

// =============================================================================
// Dialog Functions
// =============================================================================

/**
 * @brief Show message box
 * @param title Title text
 * @param message Message text
 * @param type 0=info, 1=warning, 2=error
 */
void ui_show_message(const char* title, const char* message, int type);

/**
 * @brief Show confirmation dialog
 * @param title Title text
 * @param message Message text
 * @param onConfirm Callback when confirmed
 * @param onCancel Callback when cancelled (can be NULL)
 */
void ui_show_confirm(const char* title, const char* message,
                     void (*onConfirm)(void), void (*onCancel)(void));

/**
 * @brief Show password entry dialog
 * @param title Title text
 * @param onComplete Callback with entered password
 */
void ui_show_password_entry(const char* title, void (*onComplete)(const char* password));

/**
 * @brief Show progress dialog
 * @param title Title text
 * @param message Initial message
 */
void ui_show_progress(const char* title, const char* message);

/**
 * @brief Update progress dialog
 * @param progress Progress 0-100
 * @param message Updated message
 */
void ui_update_progress(int progress, const char* message);

/**
 * @brief Close progress dialog
 */
void ui_close_progress(void);

// =============================================================================
// Status Updates
// =============================================================================

/**
 * @brief Update status bar text
 * @param text Status text
 */
void ui_set_status(const char* text);

/**
 * @brief Update user display in header
 */
void ui_update_user_display(void);

/**
 * @brief Update radio connection indicator
 */
void ui_update_radio_status(void);

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Get screen width
 */
int ui_get_screen_width(void);

/**
 * @brief Get screen height
 */
int ui_get_screen_height(void);

/**
 * @brief Check if current operation can be interrupted
 */
bool ui_can_navigate_away(void);
