#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "screens.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;
char *temp_machine_ID = "";
uint8_t temp_stopCode = 0;
ProductionStage temp_currentStage;
bool setStage = false;
bool isSettingStage = false;
uint8_t temp_code = 0;
ProductionStage temp_stage = 0;
lv_obj_t *content_panel = NULL;
lv_obj_t *tick_value_change_obj;
static lv_obj_t *current_content_panel = NULL;
static uint32_t last_duration = UINT32_MAX;
static const char *stage_names[] = {
  "ENCANTRAGE",
  "ENCANTRAGE PARTIEL",
  "FIN ENCANTRAGE",
  "NOUAGE ET PASSAGE",
  "PIQUAGE",
  "OURDISSAGE",
  "ENSOUPLAGE",
  "FIN ENSOUPLAGE"
};
static const struct {
  int code;
  const char *text;
} halt_reasons[] = {
  { 101, "ARRET NORMAL" },
  { 102, "CASSE FIL" },

  { 121, "ATTENTE MECANIQUE" },
  { 146, "REPARATION MECANIQUE" },
  { 122, "FIN REPARATION MECANIQUE" },

  { 123, "ATTENTE ELECTRIQUE" },
  { 147, "REPARATION ELECTRIQUE" },
  { 124, "FIN REPARATION ELECTRIQUE" },

  { 113, "PAUSE" },
  { 114, "FIN PAUSE" },
  { 115, "REPARATION OPERATEUR" }
};

//
// Event handlers
//
void action_send_command(lv_event_t *e) {
  lv_obj_t *target = lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);

  // === KEYBOARD READY or BUTTON OK on Operator ID page ===
  if ((target == objects.keyboard && code == LV_EVENT_READY) || target == objects.button_ok) {
    if (displayIndex == OPERATORID_PAGE) {
      const char *operator_id = lv_textarea_get_text(objects.operator_id_text_area);

      if (operator_id == NULL || operator_id[0] == '\0') {
        return;
      }

      // Check if it's only spaces/tabs
      bool is_empty = true;
      for (int i = 0; operator_id[i] != '\0'; i++) {
        if (operator_id[i] != ' ' && operator_id[i] != '\t') {
          is_empty = false;
          break;
        }
      }
      if (is_empty) {
        return;
      }

      // === SAFE COPY OF OPERATOR ID ===
      static char operator_id_buffer[32];  // Persistent buffer
      strncpy(operator_id_buffer, operator_id, sizeof(operator_id_buffer) - 1);
      operator_id_buffer[sizeof(operator_id_buffer) - 1] = '\0';

      operatorID = operator_id_buffer;  // Now safe

      // Decide command type
      if (isSettingStage) {
        command = "Stage";
        stageCode = temp_code;
        currentStage = temp_stage;
        stageDuration = 0;
      } else {
        command = "Halt";
        stopCode = temp_code;
        stopDuration = 0;
      }

      goto finish_operator_id;
    }
  }

  // ==================== Other actions (Settings, etc.) ====================
  if (target == objects.set_new_machine_id) {
    command = "machID";
    Machine_ID = atoi(lv_textarea_get_text(objects.machine_id_text_area));
  } else if (target == objects.set_new_circemferance) {
    command = "Circ";
    Drum_circemferance = lv_textarea_get_text(objects.circemferance_text_area);

  } else if (target == objects.set_database_url) {
    command = "URL";
    static char urlstr[64];
    snprintf(urlstr, sizeof(urlstr), "http://%s:%s/%s/",
             lv_textarea_get_text(objects.ip_textarea),
             lv_textarea_get_text(objects.port_textarea),
             lv_textarea_get_text(objects.db_textarea));
    URL = urlstr;
  } else if (target == objects.open_portal) {
    command = "Portal";
  } else {
    return;
  }

  SendData();
  return;

finish_operator_id:
  lv_keyboard_set_textarea(objects.keyboard, NULL);
  lv_obj_add_flag(objects.keyboard, LV_OBJ_FLAG_HIDDEN);

  displayIndex = DEFAULT_PAGE;
  isSettingStage = false;
  temp_code = 0;
  temp_stage = 0;

  SendData();
}

void action_return_icon_button(lv_event_t *e) {
  if (displayIndex == SETTINGS_PAGE) {
    displayIndex = DEFAULT_PAGE;
  } else if (displayIndex == CATEGORIES_PAGE) {
    displayIndex = DEFAULT_PAGE;
  } else if (displayIndex == HALTCODES_PAGE) {
    if (!lv_obj_has_flag(objects.avencement_panel, LV_OBJ_FLAG_HIDDEN)) {
      lv_textarea_set_text(objects.avencement_textarea, "");
      lv_obj_add_flag(objects.avencement_panel, LV_OBJ_FLAG_HIDDEN);
    }
    if (!lv_obj_has_flag(objects.encantrage_panel, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(objects.encantrage_panel, LV_OBJ_FLAG_HIDDEN);

    displayIndex = isSettingStage ? DEFAULT_PAGE : CATEGORIES_PAGE;
  } else if (displayIndex == OPERATORID_PAGE) {
    displayIndex = HALTCODES_PAGE;
  }
  lv_keyboard_set_textarea(objects.keyboard, NULL);
  lv_obj_add_flag(objects.keyboard, LV_OBJ_FLAG_HIDDEN);
}
void action_set_category(lv_event_t *e) {
  lv_obj_t *target = lv_event_get_target(e);
  if (target == objects.mecanical_category) {
    currentCategory = MECHANICAL;
  } else if (target == objects.electric_category) {
    currentCategory = ELECTRICAL;
  } else if (target == objects.operator_category) {
    currentCategory = OPERATOR;
  }

  displayIndex = HALTCODES_PAGE;
}
void action_set_operator_id(lv_event_t *e) {
  lv_obj_t *target = lv_event_get_target(e);
  if (target == objects.button_h) {
    lv_textarea_set_text(objects.operator_id_text_area, "H");
  } else if (target == objects.button_ar) {
    lv_textarea_set_text(objects.operator_id_text_area, "AR");
  } else if (target == objects.button_s) {
    lv_textarea_set_text(objects.operator_id_text_area, "S");
  }
}
void action_show_keyboard(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  lv_obj_t *kb = objects.keyboard;

  if (code == LV_EVENT_FOCUSED) {
    // Attach textarea
    lv_keyboard_set_textarea(kb, ta);

    // Set keyboard mode
    if (ta == objects.machine_id_text_area || ta == objects.ip_textarea || ta == objects.port_textarea || objects.avencement_textarea || objects.circemferance_text_area) {
      lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    } else if (ta == objects.db_textarea) {
      lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
    } else if (ta == objects.operator_id_text_area) {
      lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);  // Force numerical
    } else {
      lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    }

    // Show keyboard
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(kb);

    // Scroll ONLY when focusing db_textarea or port_textarea
    if (content_panel != NULL && ta != NULL) {
      lv_obj_update_layout(content_panel);

      if (ta == objects.db_textarea || ta == objects.port_textarea || objects.ip_textarea) {
        lv_obj_set_scrollbar_mode(content_panel, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_scroll_to_view(ta, LV_ANIM_ON);
      } else if (ta == objects.operator_id_text_area) {
        // On Operator ID page we usually don't need much scroll
        lv_obj_set_scrollbar_mode(content_panel, LV_SCROLLBAR_MODE_OFF);
      }
    }
  } else if (code == LV_EVENT_DEFOCUSED) {
    // Do NOT hide keyboard immediately on defocus for Operator ID page
    if (displayIndex != OPERATORID_PAGE) {
      lv_keyboard_set_textarea(kb, NULL);
      lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }

    // Reset scroll state when leaving focus (important to prevent leak)
    if (displayIndex != SETTINGS_PAGE) {
      lv_obj_set_scrollbar_mode(content_panel, LV_SCROLLBAR_MODE_OFF);
    }
  }
}
void action_close_haltabel(lv_event_t *e) {
  lv_obj_t *target = lv_event_get_target(e);

  if (target == objects.warning_stop) {
    hideReport = false;  // Show report when warning is clicked
  } else if (target == objects.close_stop_report) {
    hideReport = true;  // Hide report when close icon is clicked
  }
}
void action_display(lv_event_t *e) {
  lv_obj_t *target = lv_event_get_target(e);
  isSettingStage = false;
  if (target == objects.justefy_halt_cause) {
    lv_obj_add_flag(objects.halt_report_panel, LV_OBJ_FLAG_HIDDEN);
    if (objects.halt_report_overlay != NULL) {
      lv_obj_add_flag(objects.halt_report_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    hideReport = true;
    displayIndex = CATEGORIES_PAGE;
  } else if (target == objects.setting) {
    displayIndex = SETTINGS_PAGE;
    lv_keyboard_set_textarea(objects.keyboard, NULL);
    lv_obj_add_flag(objects.keyboard, LV_OBJ_FLAG_HIDDEN);
  } else if (target == objects.stage_label) {
    isSettingStage = true;  // ← Clear and set properly
    currentCategory = STAGES;
    temp_code = 0;  // reset temp
    displayIndex = HALTCODES_PAGE;
  } else if (target == objects.resume_production) {
    command = "Halt";
    stopCode = 100;
    hideReport = true;

    // === CRITICAL: Immediately hide the overlay BEFORE changing page ===
    if (!lv_obj_has_flag(objects.halt_report_panel, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_add_flag(objects.halt_report_panel, LV_OBJ_FLAG_HIDDEN);
    }
    if (objects.halt_report_overlay != NULL && !lv_obj_has_flag(objects.halt_report_overlay, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_add_flag(objects.halt_report_overlay, LV_OBJ_FLAG_HIDDEN);
    }

    displayIndex = DEFAULT_PAGE;
    SendData();
  }
}
void action_set_temp_code(lv_event_t *e) {
  void *user_data = lv_event_get_user_data(e);
  if (!user_data) return;

  int code = (int)(intptr_t)user_data;

  temp_code = code;  // Always store here

  if (currentCategory == STAGES) {
    isSettingStage = true;
    if (!lv_obj_has_flag(objects.encantrage_panel, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(objects.encantrage_panel, LV_OBJ_FLAG_HIDDEN);
    switch (code) {
      case 103: temp_stage = ENCANTRAGE; break;
      case 106: temp_stage = ENCANTRAGE_PARTIEL; break;
      case 104: temp_stage = FIN_ENCANTRAGE; break;
      case 105: temp_stage = NOUAGE; break;
      case 107: temp_stage = PIQUAGE; break;
      case 109: temp_stage = OURDISSAGE; break;
      case 111: temp_stage = ENSOUPLAGE; break;
      case 112: temp_stage = FIN_ENSOUPLAGE; break;
      default: temp_stage = 0; return;
    }
    if (code == 109) {
      if (objects.avencement_panel) {
        lv_obj_clear_flag(objects.avencement_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(objects.avencement_panel);  // bring to front

        // Focus textarea + show numerical keyboard
        if (objects.avencement_textarea) {
          lv_obj_add_state(objects.avencement_textarea, LV_STATE_FOCUSED);
          lv_keyboard_set_textarea(objects.keyboard, objects.avencement_textarea);
          lv_keyboard_set_mode(objects.keyboard, LV_KEYBOARD_MODE_NUMBER);
          lv_obj_clear_flag(objects.keyboard, LV_OBJ_FLAG_HIDDEN);
          lv_obj_move_foreground(objects.keyboard);
        }
      }
      return;
    }
  } else {
    isSettingStage = false;
    temp_stage = 0;  // not needed for halts
  }

  displayIndex = OPERATORID_PAGE;
}
void action_submit_avencement(lv_event_t *e) {
  if (objects.avencement_textarea == NULL) return;

  const char *value = lv_textarea_get_text(objects.avencement_textarea);
  if (value && value[0] != '\0') {
    static char avencement_buf[16];
    snprintf(avencement_buf, sizeof(avencement_buf), "%s", value);
    Avencement = avencement_buf;
  } else {
    Avencement = "2.000";
    return;
  }
  lv_textarea_set_text(objects.avencement_textarea, "");
  // Hide panel + keyboard immediately (very important)
  if (objects.avencement_panel) {
    lv_obj_add_flag(objects.avencement_panel, LV_OBJ_FLAG_HIDDEN);
  }
  lv_keyboard_set_textarea(objects.keyboard, NULL);
  lv_obj_add_flag(objects.keyboard, LV_OBJ_FLAG_HIDDEN);

  // NOW safely change the page (this will trigger clean only on next tick)
  displayIndex = OPERATORID_PAGE;
  isSettingStage = true;
}
void action_cancel_avencement(lv_event_t *e) {
  // Just hide the panel and go back to halt codes page
  lv_textarea_set_text(objects.avencement_textarea, "");
  if (objects.avencement_panel) lv_obj_add_flag(objects.avencement_panel, LV_OBJ_FLAG_HIDDEN);
  if (objects.keyboard) {
    lv_keyboard_set_textarea(objects.keyboard, NULL);
    lv_obj_add_flag(objects.keyboard, LV_OBJ_FLAG_HIDDEN);
  }
  displayIndex = HALTCODES_PAGE;
}
void action_open_encantrage_popup(lv_event_t *e) {
  if (objects.encantrage_panel) {
    lv_obj_clear_flag(objects.encantrage_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(objects.encantrage_panel);
  }
}
void action_cancel_encantrage(lv_event_t *e) {
  if (objects.encantrage_panel) {
    lv_obj_add_flag(objects.encantrage_panel, LV_OBJ_FLAG_HIDDEN);
  }
  displayIndex = HALTCODES_PAGE;  // go back to halt codes
}
//
// helper functions
//

// Helper: Create a label with common settings (position, text, font)
static lv_obj_t *create_label(lv_obj_t *parent, int32_t x, int32_t y, const char *text, const lv_font_t *font) {
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  if (text) lv_label_set_text(label, text);
  if (font) lv_obj_set_style_text_font(label, font, LV_PART_MAIN | LV_STATE_DEFAULT);
  return label;
}
static lv_obj_t *create_label_centered(lv_obj_t *parent, int32_t x, int32_t y, const char *text, const lv_font_t *font) {
  lv_obj_t *label = create_label(parent, x, y, text, font);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
  return label;
}
// Helper: Create a small status LED
static lv_obj_t *create_led(lv_obj_t *parent, int32_t x, int32_t y, uint32_t color_hex) {
  lv_obj_t *led = lv_led_create(parent);
  lv_obj_set_pos(led, x, y);
  lv_obj_set_size(led, 15, 15);
  lv_led_set_color(led, lv_color_hex(color_hex));
  lv_led_set_brightness(led, 255);
  lv_obj_remove_flag(led, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
  return led;
}
// Helper: Create a content panel with common settings
static lv_obj_t *create_content_panel(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h) {
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_set_pos(panel, x, y);
  lv_obj_set_size(panel, w, h);

  lv_obj_remove_flag(panel, LV_OBJ_FLAG_CLICKABLE);
  // Do NOT remove LV_OBJ_FLAG_SCROLLABLE here

  lv_obj_set_style_pad_top(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_right(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  return panel;
}
// Helper: Create a simple button with label (most common case)
static lv_obj_t *create_button(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, const char *text, lv_event_cb_t event_cb, void *user_data) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_pos(btn, x, y);
  lv_obj_set_size(btn, w, h);
  if (event_cb) {
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_RELEASED, user_data);
  }
  if (text) {
    lv_obj_t *label = lv_label_create(btn);
    lv_obj_set_size(label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_align(label, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, text);
  }
  return btn;
}
// Helper: Disable button and make it red if it matches current stopCode
static void style_halt_button(lv_obj_t *btn, int button_code) {
  if (!btn) return;

  if (stopCode == button_code || stageCode == button_code) {
    lv_obj_add_state(btn, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);    // red
    lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);  // white text
  } else {
    lv_obj_clear_state(btn, LV_STATE_DISABLED);
    // Reset to normal style (you can adjust these colors to match your theme)
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2196F3), LV_PART_MAIN | LV_STATE_DEFAULT);  // default blue-ish
    lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}
// Helper: Button with custom font
static lv_obj_t *create_button_with_font(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, const char *text, const lv_font_t *font, lv_event_cb_t event_cb, void *user_data) {
  lv_obj_t *btn = create_button(parent, x, y, w, h, text, event_cb, user_data);
  if (font) {
    lv_obj_set_style_text_font(btn, font, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
  return btn;
}
// Helper: Create a styled textarea
static lv_obj_t *create_textarea(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h, const char *placeholder, const char *accepted_chars, uint32_t max_length, lv_event_cb_t event_cb) {
  lv_obj_t *ta = lv_textarea_create(parent);
  lv_obj_set_pos(ta, x, y);
  lv_obj_set_size(ta, w, h);

  if (placeholder) lv_textarea_set_placeholder_text(ta, placeholder);
  if (accepted_chars) lv_textarea_set_accepted_chars(ta, accepted_chars);
  if (max_length > 0) lv_textarea_set_max_length(ta, max_length);

  lv_textarea_set_one_line(ta, true);
  lv_textarea_set_password_mode(ta, false);

  if (event_cb) {
    lv_obj_add_event_cb(ta, event_cb, LV_EVENT_ALL, NULL);
  }

  lv_obj_remove_flag(ta, LV_OBJ_FLAG_SCROLLABLE);
  add_style_text_aria(ta);  // keep your custom style

  lv_obj_set_style_outline_width(ta, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(ta, lv_color_hex(0xff99b6e2), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(ta, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(ta, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

  return ta;
}
// Helper: Create Avencement Panel as overlay (on main screen, not content_panel)
static lv_obj_t *create_avencement_panel(void) {
  objects.avencement_panel = lv_obj_create(objects.main);

  lv_obj_set_pos(objects.avencement_panel, 5, 60);  // moved down a bit
  lv_obj_set_size(objects.avencement_panel, 310, 190);
  lv_obj_add_flag(objects.avencement_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(objects.avencement_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_top(objects.avencement_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(objects.avencement_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Style like your halt report panel
  lv_obj_set_style_bg_color(objects.avencement_panel, lv_color_hex(0x1E2A44), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(objects.avencement_panel, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(objects.avencement_panel, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(objects.avencement_panel, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_width(objects.avencement_panel, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

  create_label_centered(objects.avencement_panel, 90, 8, "Avencement", &lv_font_montserrat_20);

  objects.avencement_textarea = create_textarea(objects.avencement_panel, 5, 45, 295, 55, "Inserer avencement", "0123456789.", 8, action_show_keyboard);
  lv_obj_add_event_cb(objects.avencement_panel, action_show_keyboard, LV_EVENT_FOCUSED, NULL);
  lv_obj_set_style_text_font(objects.avencement_textarea, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

  objects.submet_avencement = create_button_with_font(objects.avencement_panel, 5, 115, 145, 48, "Remettre", &lv_font_montserrat_20, action_submit_avencement, NULL);
  lv_obj_set_style_bg_color(objects.submet_avencement, lv_color_hex(0x00CC00), LV_PART_MAIN | LV_STATE_DEFAULT);

  objects.cancel_avencement = create_button_with_font(objects.avencement_panel, 153, 115, 145, 48, "Annuler", &lv_font_montserrat_20, action_cancel_avencement, NULL);
  lv_obj_set_style_bg_color(objects.cancel_avencement, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);

  return objects.avencement_panel;
}
// Helper: Create Encantrage selection popup using your existing helpers
static lv_obj_t *create_encantrage_panel(void) {
  // Create the main panel (similar to avencement_panel)
  objects.encantrage_panel = lv_obj_create(content_panel);

  lv_obj_set_pos(objects.encantrage_panel, 0, 50);
  lv_obj_set_size(objects.encantrage_panel, 308, 326);
  lv_obj_add_flag(objects.encantrage_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(objects.encantrage_panel, LV_OBJ_FLAG_SCROLLABLE);

  // Apply styling similar to avencement_panel
  lv_obj_set_style_bg_color(objects.encantrage_panel, lv_color_hex(0x1E2A44), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(objects.encantrage_panel, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(objects.encantrage_panel, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(objects.encantrage_panel, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_width(objects.encantrage_panel, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(objects.encantrage_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(objects.encantrage_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *parent = objects.encantrage_panel;

  // Title
  create_label_centered(parent, 77, 8, "ENCANTRAGE", &lv_font_montserrat_20);
  // ENCANTRAGE (103)
  objects.encantrage_button = create_button_with_font(parent, 3, 50, 290, 50, "ENCANTRAGE (103)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)103);
  style_halt_button(objects.encantrage_button, 103);
  // ENCANTRAGE PARTIEL (106)
  objects.encantrage_partiel_button = create_button_with_font(parent, 3, 105, 290, 50, "ENCANTRAGE PARTIEL (106)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)106);
  style_halt_button(objects.encantrage_partiel_button, 106);
  // FIN ENCANTRAGE (104)
  objects.fin_encantrage_button = create_button_with_font(parent, 3, 160, 290, 50, "FIN ENCANTRAGE (104)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)104);
  style_halt_button(objects.fin_encantrage_button, 104);
  // Cancel button (red like in your example)
  objects.cancel_encantrage_setting = create_button_with_font(parent, 76, 265, 145, 48, "Annuler", &lv_font_montserrat_18, action_cancel_encantrage, NULL);
  lv_obj_set_style_bg_color(objects.cancel_encantrage_setting, lv_color_hex(0xffcc0000), LV_PART_MAIN | LV_STATE_DEFAULT);

  return objects.encantrage_panel;
}

//
// Screens
//
void populate_DefaultPanel(lv_obj_t *parent) {
  create_label(parent, 81, 0, "Etape de Prodction", &lv_font_montserrat_18);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

  objects.encontrage_led = create_led(parent, 17, 30, 0xffdcdcdc);
  objects.noage_led = create_led(parent, 81, 30, 0xffdcdcdc);
  objects.piquage_led = create_led(parent, 146, 30, 0xffdcdcdc);
  objects.ourdissage_led = create_led(parent, 210, 30, 0xffdcdcdc);
  objects.ensoplage_led = create_led(parent, 274, 30, 0xffdcdcdc);

  objects.stage_label = create_label(parent, 8, 60, "", &lv_font_montserrat_24);
  lv_obj_set_size(objects.stage_label, 290, 56);
  lv_obj_set_style_outline_width(objects.stage_label, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(objects.stage_label, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(objects.stage_label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(objects.stage_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(objects.stage_label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(objects.stage_label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(objects.stage_label, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(objects.stage_label, action_display, LV_EVENT_RELEASED, NULL);
  // Background
  lv_obj_set_style_bg_color(objects.stage_label, lv_color_hex(0x408579), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(objects.stage_label, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  // Pressed effect
  lv_obj_set_style_bg_color(objects.stage_label, lv_color_hex(0x1976D2), LV_PART_MAIN | LV_STATE_PRESSED);

  create_label(parent, 5, 138, "Actual", &lv_font_montserrat_16);

  objects.performance_bar = lv_bar_create(parent);
  lv_obj_set_pos(objects.performance_bar, 68, 128);
  lv_obj_set_size(objects.performance_bar, 226, 42);
  lv_bar_set_mode(objects.performance_bar, LV_BAR_MODE_SYMMETRICAL);
  lv_obj_remove_flag(objects.performance_bar, LV_OBJ_FLAG_CLICKABLE);

  objects.performance_value = create_label_centered(objects.performance_bar, 0, 0, "", &lv_font_montserrat_20);
  lv_obj_set_style_text_color(objects.performance_value, lv_color_hex(0xffaaaaaa), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_align(objects.performance_value, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

  objects.stage_timer = create_label(parent, 8, 176, "00:00:00", &lv_font_montserrat_38);
  lv_obj_set_size(objects.stage_timer, 290, 60);
  lv_obj_set_style_outline_width(objects.stage_timer, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(objects.stage_timer, lv_color_hex(0xff7f8fe9), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(objects.stage_timer, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(objects.stage_timer, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(objects.stage_timer, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(objects.stage_timer, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Ourdissage sub-panel
  objects.Ourdissage_Panel = create_content_panel(parent, 8, 244, 290, 164);
  lv_obj_set_style_outline_width(objects.Ourdissage_Panel, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(objects.Ourdissage_Panel, lv_color_hex(0xff7f8fe9), LV_PART_MAIN | LV_STATE_DEFAULT);
  {
    // Beam Length Box
    objects.beam_length = create_content_panel(objects.Ourdissage_Panel, 3, 3, 138, 75);
    lv_obj_set_style_outline_color(objects.beam_length, lv_color_hex(0xffebdd1c), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(objects.beam_length, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    create_label(objects.beam_length, 0, 0, "Length", &lv_font_montserrat_16);
    create_label(objects.beam_length, 114, 0, "m", &lv_font_montserrat_16);

    objects.length_value = create_label_centered(objects.beam_length, 0, 0, "", &lv_font_montserrat_24);
    lv_obj_set_style_text_color(objects.length_value, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_align(objects.length_value, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);  // Force center

    create_label(objects.beam_length, 3, 55, "section", &lv_font_montserrat_16);
    objects.winding_section = create_label_centered(objects.beam_length, 112, 55, "", &lv_font_montserrat_16);

    // Drum Revolutions Box
    objects.drum_section_revolutions = create_content_panel(objects.Ourdissage_Panel, 147, 3, 136, 75);
    lv_obj_set_style_outline_color(objects.drum_section_revolutions, lv_color_hex(0xffebdd1c), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(objects.drum_section_revolutions, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    create_label(objects.drum_section_revolutions, 0, 0, "Revs", &lv_font_montserrat_16);

    objects.revolutions_value = create_label_centered(objects.drum_section_revolutions, 0, 0, "", &lv_font_montserrat_24);
    lv_obj_set_style_text_color(objects.revolutions_value, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_align(objects.revolutions_value, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Linear Speed Box
    objects.linear_speed = create_content_panel(objects.Ourdissage_Panel, 3, 84, 138, 72);
    lv_obj_set_style_outline_color(objects.linear_speed, lv_color_hex(0xffebdd1c), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(objects.linear_speed, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    create_label(objects.linear_speed, 0, 0, "Speed", &lv_font_montserrat_16);
    create_label(objects.linear_speed, 103, 1, "m/s", &lv_font_montserrat_16);

    objects.speed_value = create_label_centered(objects.linear_speed, 0, 0, "", &lv_font_montserrat_24);
    lv_obj_set_style_text_color(objects.speed_value, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_align(objects.speed_value, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Angular Speed Box
    objects.angulare_speed = create_content_panel(objects.Ourdissage_Panel, 147, 84, 136, 72);
    lv_obj_set_style_outline_color(objects.angulare_speed, lv_color_hex(0xffebdd1c), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(objects.angulare_speed, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    create_label(objects.angulare_speed, 0, 0, "RPM", &lv_font_montserrat_16);

    objects.angulare_speed_value = create_label_centered(objects.angulare_speed, 0, 0, "", &lv_font_montserrat_24);
    lv_obj_set_style_text_color(objects.angulare_speed_value, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_align(objects.angulare_speed_value, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
  }
}
void populate_CategoriesPanel(lv_obj_t *parent) {
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *lbl = create_label(parent, 0, 0, "Categories D'arrets ", &lv_font_montserrat_20);
  lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 5);
  objects.mecanical_category = create_button_with_font(parent, 8, 50, 290, 50, "Arrets Mecaniqe", &lv_font_montserrat_22, action_set_category, (void *)1);
  objects.electric_category = create_button_with_font(parent, 8, 105, 290, 50, "Arrets Electrique", &lv_font_montserrat_22, action_set_category, (void *)2);
  objects.operator_category = create_button_with_font(parent, 8, 160, 290, 50, "Interaction d\'Operateur", &lv_font_montserrat_22, action_set_category, (void *)3);
}
void populate_HaltCodesPanel(lv_obj_t *parent) {
  objects.category_label_title = create_label(parent, 0, 0, "", &lv_font_montserrat_20);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(objects.category_label_title, LV_ALIGN_TOP_MID, 0, 5);

  switch (currentCategory) {
    case STAGES: lv_label_set_text(objects.category_label_title, "Etapes de Production"); break;
    case MECHANICAL: lv_label_set_text(objects.category_label_title, "Arrets Mecanique"); break;
    case ELECTRICAL: lv_label_set_text(objects.category_label_title, "Arrets Electrique"); break;
    case OPERATOR: lv_label_set_text(objects.category_label_title, "Actions d'Operateur"); break;
    default: lv_label_set_text(objects.category_label_title, ""); break;
  }

  int y = 50;
  if (currentCategory == STAGES) {
    lv_obj_t *btn1 = create_button_with_font(parent, 5, y, 300, 50, "Encantrage", &lv_font_montserrat_18, action_open_encantrage_popup, NULL);
    if (stageCode == 103 || stageCode == 104 || stageCode == 106) {
      lv_obj_set_style_bg_color(btn1, lv_color_hex(0xCC11CC), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_color(btn1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    y += 55;
    lv_obj_t *btn4 = create_button_with_font(parent, 5, y, 300, 50, "Nouage et Passage (105)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)105);
    style_halt_button(btn4, 105);
    y += 55;
    lv_obj_t *btn5 = create_button_with_font(parent, 5, y, 300, 50, "Piquage (107)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)107);
    style_halt_button(btn5, 107);
    y += 55;
    lv_obj_t *btn6 = create_button_with_font(parent, 5, y, 300, 50, "Ourdissage (109)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)109);
    style_halt_button(btn6, 109);
    y += 55;
    lv_obj_t *btn7 = create_button_with_font(parent, 5, y, 300, 50, "Ensouplage (111)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)111);
    style_halt_button(btn7, 111);
    y += 55;
    lv_obj_t *btn8 = create_button_with_font(parent, 5, y, 300, 50, "Fin Ensouplage (112)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)112);
    style_halt_button(btn8, 112);

    create_encantrage_panel();
  } else if (currentCategory == MECHANICAL) {
    lv_obj_t *btn1 = create_button_with_font(parent, 5, y, 300, 50, "Attente Mecanique (121)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)121);
    style_halt_button(btn1, 121);
    y += 55;
    lv_obj_t *btn2 = create_button_with_font(parent, 5, y, 300, 50, "Reparation Mecanique (146)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)146);
    style_halt_button(btn2, 146);
    y += 55;
    lv_obj_t *btn3 = create_button_with_font(parent, 5, y, 300, 50, "Fin Reparation Mecanique (122)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)122);
    style_halt_button(btn3, 122);
  } else if (currentCategory == ELECTRICAL) {
    lv_obj_t *btn1 = create_button_with_font(parent, 5, y, 300, 50, "Attente Electrique (123)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)123);
    style_halt_button(btn1, 123);
    y += 55;
    lv_obj_t *btn2 = create_button_with_font(parent, 5, y, 300, 50, "Reparation Electrique (147)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)147);
    style_halt_button(btn2, 147);
    y += 55;
    lv_obj_t *btn3 = create_button_with_font(parent, 5, y, 300, 50, "Fin Reparation Electrique (124)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)124);
    style_halt_button(btn3, 124);
  } else if (currentCategory == OPERATOR) {
    lv_obj_t *btn1 = create_button_with_font(parent, 5, y, 300, 50, "Debut de Pause (113)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)113);
    style_halt_button(btn1, 113);
    y += 55;
    lv_obj_t *btn2 = create_button_with_font(parent, 5, y, 300, 50, "Fin de Pause (114)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)114);
    style_halt_button(btn2, 114);
    y += 55;
    lv_obj_t *btn3 = create_button_with_font(parent, 5, y, 300, 50, "Reparation Operateur (115)", &lv_font_montserrat_18, action_set_temp_code, (void *)(intptr_t)115);
    style_halt_button(btn3, 115);
  }
}
void populate_OperatorIDPanel(lv_obj_t *parent) {
  create_label(parent, 96, 5, "Identifiant ", &lv_font_montserrat_20);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
  objects.operator_id_text_area = create_textarea(parent, 3, 50, 300, 50, "insirer ID d\'operateur", "1,2,3,4,5,6,7,8,9,0,H,A,R,S", 16, action_show_keyboard);
  lv_obj_add_state(objects.operator_id_text_area, LV_STATE_FOCUSED);
  lv_obj_set_style_text_font(objects.operator_id_text_area, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_keyboard_set_textarea(objects.keyboard, objects.operator_id_text_area);
  lv_keyboard_set_mode(objects.keyboard, LV_KEYBOARD_MODE_NUMBER);
  lv_obj_clear_flag(objects.keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(objects.keyboard);


  // Quick buttons
  objects.button_h = create_button_with_font(parent, 4, 130, 56, 44, "H", &lv_font_montserrat_24, action_set_operator_id, (void *)0);
  lv_obj_set_style_bg_opa(objects.button_h, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_width(objects.button_h, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(objects.button_h, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);

  objects.button_ar = create_button_with_font(parent, 68, 130, 56, 44, "AR", &lv_font_montserrat_24, action_set_operator_id, NULL);
  lv_obj_set_style_bg_opa(objects.button_ar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_width(objects.button_ar, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(objects.button_ar, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);

  objects.button_s = create_button_with_font(parent, 131, 130, 56, 44, "S", &lv_font_montserrat_24, action_set_operator_id, (void *)0);
  lv_obj_set_style_bg_opa(objects.button_s, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_width(objects.button_s, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(objects.button_s, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);

  // OK Button
  objects.button_ok = create_button_with_font(parent, 195, 130, 108, 44, "OK", &lv_font_montserrat_24, action_send_command, (void *)1);
  lv_obj_set_style_bg_opa(objects.button_ok, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_width(objects.button_ok, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(objects.button_ok, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
}
void populate_SettingsPanel(lv_obj_t *parent) {
  lv_obj_set_scroll_dir(parent, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);                   // Hide scrollbar by default
  lv_obj_set_style_pad_bottom(parent, 100, LV_PART_MAIN | LV_STATE_DEFAULT);  // enough space for keyboard

  /// ==================== Machine ID Section ====================
  objects.set_machine_id = create_label(parent, 3, 10, "Machine ID", &lv_font_montserrat_18);
  lv_obj_set_size(objects.set_machine_id, 300, 48);
  lv_obj_remove_flag(objects.set_machine_id, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_outline_width(objects.set_machine_id, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(objects.set_machine_id, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(objects.set_machine_id, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(objects.set_machine_id, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(objects.set_machine_id, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(objects.set_machine_id, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_right(objects.set_machine_id, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

  objects.machine_id_text_area = create_textarea(objects.set_machine_id, 110, -9, 112, 38, "ID", "0,1,2,3,4,5,6,7,8,9", 4, action_show_keyboard);
  lv_obj_set_style_text_align(objects.machine_id_text_area, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_textarea_set_max_length(objects.machine_id_text_area, 4);

  objects.set_new_machine_id = create_button(objects.set_machine_id, 230, -6, 55, 38, "Set", action_send_command, NULL);
  lv_obj_set_style_bg_color(objects.set_new_machine_id, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);

  // ==================== Drum circumferance ====================
  objects.set_circemferance = create_label(parent, 3, 68, "Circemferance", &lv_font_montserrat_18);
  lv_obj_set_size(objects.set_circemferance, 300, 48);
  lv_obj_remove_flag(objects.set_circemferance, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_outline_width(objects.set_circemferance, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(objects.set_circemferance, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(objects.set_circemferance, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_top(objects.set_circemferance, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(objects.set_circemferance, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(objects.set_circemferance, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_right(objects.set_circemferance, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

  objects.circemferance_text_area = create_textarea(objects.set_circemferance, 110, -9, 112, 38, "3.125", "0,1,2,3,4,5,6,7,8,9,.", 8, action_show_keyboard);
  lv_obj_set_style_text_align(objects.circemferance_text_area, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_textarea_set_max_length(objects.circemferance_text_area, 8);

  objects.set_new_circemferance = create_button(objects.set_circemferance, 230, -6, 55, 38, "Set", action_send_command, NULL);
  lv_obj_set_style_bg_color(objects.set_new_circemferance, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);

  // ==================== Database Section ====================
  objects.set_database_link = create_content_panel(parent, 3, 126, 300, 210);
  lv_obj_set_style_outline_width(objects.set_database_link, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_color(objects.set_database_link, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(objects.set_database_link, 100, LV_PART_MAIN | LV_STATE_DEFAULT);

  create_label(objects.set_database_link, 5, 15, "Server IP", &lv_font_montserrat_18);
  objects.ip_textarea = create_textarea(objects.set_database_link, 116, 5, 175, 40, "192.168.1.xx", ".,0,1,2,3,4,5,6,7,8,9", 15, action_show_keyboard);

  create_label(objects.set_database_link, 5, 66, "API PORT", &lv_font_montserrat_18);
  objects.port_textarea = create_textarea(objects.set_database_link, 116, 55, 175, 40, "7272", "0,1,2,3,4,5,6,7,8,9", 8, action_show_keyboard);

  create_label(objects.set_database_link, 5, 116, "Database", &lv_font_montserrat_18);
  objects.db_textarea = create_textarea(objects.set_database_link, 116, 105, 175, 40, "DBLOG", NULL, 32, action_show_keyboard);

  objects.set_database_url = create_button_with_font(objects.set_database_link, 3, 157, 290, 40, "Set database URL", &lv_font_montserrat_20, action_send_command, NULL);

  // ==================== Open Portal Button ====================
  objects.open_portal = create_button_with_font(parent, 8, 345, 290, 40, "Open WIFI Portal", &lv_font_montserrat_20, action_send_command, NULL);
}

void create_screen_main(void) {
  objects.main = lv_obj_create(NULL);
  lv_obj_set_size(objects.main, 320, 480);
  lv_obj_remove_flag(objects.main, LV_OBJ_FLAG_SCROLLABLE);

  // Header Panel (stays forever)
  objects.header_panel = create_content_panel(objects.main, 5, 5, 310, 50);
  lv_obj_set_style_radius(objects.header_panel, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(objects.header_panel, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Return button (hidden by default)
  objects.return_button = create_label(objects.header_panel, 1, 4, LV_SYMBOL_BACKSPACE, &lv_font_montserrat_34);
  lv_obj_add_event_cb(objects.return_button, action_return_icon_button, LV_EVENT_RELEASED, NULL);
  lv_obj_add_flag(objects.return_button, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_CLICKABLE);

  // "R°:" label
  lv_obj_t *r_label = create_label(objects.header_panel, 49, 8, "R°:", &lv_font_montserrat_28);

  // Machine ID
  objects.machine_id = create_label(objects.header_panel, 90, 8, "", &lv_font_montserrat_28);

  // Warning stop icon
  objects.warning_stop = create_label(objects.header_panel, 145, 4, LV_SYMBOL_WARNING, &lv_font_montserrat_34);
  lv_obj_add_event_cb(objects.warning_stop, action_close_haltabel, LV_EVENT_RELEASED, NULL);
  lv_obj_add_flag(objects.warning_stop, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(objects.warning_stop, LV_OBJ_FLAG_HIDDEN);  // start visible (white)

  // WiFi symbol (will be updated in tick)
  objects.wifi = create_label(objects.header_panel, 215, 7, "", &lv_font_montserrat_28);

  // Setting icon
  objects.setting = create_label(objects.header_panel, 270, 4, LV_SYMBOL_SETTINGS, &lv_font_montserrat_34);
  lv_obj_add_event_cb(objects.setting, action_display, LV_EVENT_RELEASED, NULL);
  lv_obj_add_flag(objects.setting, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_text_color(objects.setting, lv_color_hex(0xff96e660), LV_PART_MAIN | LV_STATE_CHECKED);

  // Keyboard (stays forever)
  objects.keyboard = lv_keyboard_create(objects.main);
  lv_obj_set_pos(objects.keyboard, 0, -5);
  lv_obj_set_size(objects.keyboard, 320, 222);
  lv_obj_add_flag(objects.keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(objects.keyboard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(objects.keyboard);
  lv_obj_add_event_cb(objects.keyboard, action_send_command, LV_EVENT_READY, NULL);
  lv_obj_set_style_text_font(objects.keyboard, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

  // === SINGLE REUSABLE CONTENT CONTAINER (this is the key fix for DRAM) ===
  // All 5 pages will live here. When we switch pages we do lv_obj_clean() → only current page widgets exist.
  content_panel = create_content_panel(objects.main, 5, 60, 310, 415);
  lv_obj_set_scroll_dir(content_panel, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(content_panel, LV_SCROLLBAR_MODE_AUTO);

  // Halt report panel is an overlay (always present, never deleted)
  // Halt report panel - Make it a real popup/modal
  objects.halt_report_panel = lv_obj_create(objects.main);  // Create directly on main screen
  lv_obj_set_pos(objects.halt_report_panel, 5, 80);
  lv_obj_set_size(objects.halt_report_panel, 310, 315);

  lv_obj_add_flag(objects.halt_report_panel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(objects.halt_report_panel, LV_OBJ_FLAG_SCROLLABLE);

  // Make it look like a proper popup
  lv_obj_set_style_bg_color(objects.halt_report_panel, lv_color_hex(0x1E1E2E), LV_PART_MAIN | LV_STATE_DEFAULT);  // dark background
  lv_obj_set_style_bg_opa(objects.halt_report_panel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(objects.halt_report_panel, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(objects.halt_report_panel, lv_color_hex(0xff2196f3), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(objects.halt_report_panel, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_width(objects.halt_report_panel, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_color(objects.halt_report_panel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_shadow_opa(objects.halt_report_panel, LV_OPA_60, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_style_pad_top(objects.halt_report_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(objects.halt_report_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_left(objects.halt_report_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_right(objects.halt_report_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(objects.halt_report_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Optional: Add a semi-transparent overlay behind the panel to dim the background
  lv_obj_t *overlay = lv_obj_create(objects.main);
  lv_obj_set_pos(overlay, 0, 0);
  lv_obj_set_size(overlay, 320, 480);
  lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(overlay, 120, LV_PART_MAIN | LV_STATE_DEFAULT);  // semi-transparent black
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_user_data(overlay, objects.halt_report_panel);  // link them if needed
  objects.halt_report_overlay = overlay;                     // Add this to your objects_t if you want easy access
  {
    create_label(objects.halt_report_panel, 122, 10, "Cause", &lv_font_montserrat_20);

    objects.halt_cause_label = create_label(objects.halt_report_panel, 5, 50, "", &lv_font_montserrat_20);
    lv_obj_set_size(objects.halt_cause_label, 290, 62);
    lv_obj_set_style_outline_width(objects.halt_cause_label, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(objects.halt_cause_label, lv_color_hex(0xff7f8fe9), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(objects.halt_cause_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(objects.halt_cause_label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(objects.halt_cause_label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(objects.halt_cause_label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    create_label(objects.halt_report_panel, 107, 115, "Duration", &lv_font_montserrat_20);

    objects.halt_duration_timer_label = create_label(objects.halt_report_panel, 5, 142, "", &lv_font_montserrat_38);
    lv_obj_set_size(objects.halt_duration_timer_label, 290, LV_SIZE_CONTENT);
    lv_obj_set_style_outline_width(objects.halt_duration_timer_label, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(objects.halt_duration_timer_label, lv_color_hex(0xff7f8fe9), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(objects.halt_duration_timer_label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(objects.halt_duration_timer_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(objects.halt_duration_timer_label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(objects.halt_duration_timer_label, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    objects.close_stop_report = create_label(objects.halt_report_panel, 264, 5, LV_SYMBOL_CLOSE, &lv_font_montserrat_22);
    lv_obj_set_style_text_color(objects.close_stop_report, lv_color_hex(0xffff0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(objects.close_stop_report, action_close_haltabel, LV_EVENT_RELEASED, (void *)1);
    lv_obj_add_flag(objects.close_stop_report, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_outline_width(objects.close_stop_report, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_color(objects.close_stop_report, lv_color_hex(0xff5c86dd), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(objects.close_stop_report, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(objects.close_stop_report, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(objects.close_stop_report, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(objects.close_stop_report, 8, LV_PART_MAIN | LV_STATE_DEFAULT);



    objects.justefy_halt_cause = create_button_with_font(objects.halt_report_panel, 8, 220, 290, 38, "Justifier l'arret", &lv_font_montserrat_22, action_display, NULL);
    objects.resume_production = create_button_with_font(objects.halt_report_panel, 8, 265, 290, 38, "Poursuivre Production", &lv_font_montserrat_22, action_display, NULL);
    lv_obj_set_style_bg_color(objects.resume_production, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  // Start on default page
  populate_DefaultPanel(content_panel);
  create_avencement_panel();
}
void tick_screen_main() {
  static char buf[32];

  // === DRAM-OPTIMISED PAGE SWITCHING ===
  static int last_displayIndex = -1;
  if (displayIndex != last_displayIndex) {
    last_displayIndex = displayIndex;

    if (content_panel != NULL) {
      lv_obj_clean(content_panel);  // ← erases previous page + ALL its components
    }

    switch (displayIndex) {
      case DEFAULT_PAGE: populate_DefaultPanel(content_panel); break;
      case CATEGORIES_PAGE: populate_CategoriesPanel(content_panel); break;
      case HALTCODES_PAGE: populate_HaltCodesPanel(content_panel); break;
      case OPERATORID_PAGE: populate_OperatorIDPanel(content_panel); break;
      case SETTINGS_PAGE: populate_SettingsPanel(content_panel); break;
    }
  }

  // return button
  {
    if (displayIndex != DEFAULT_PAGE && lv_obj_has_flag(objects.return_button, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_remove_flag(objects.return_button, LV_OBJ_FLAG_HIDDEN);
    } else if (displayIndex == DEFAULT_PAGE && !lv_obj_has_flag(objects.return_button, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_add_flag(objects.return_button, LV_OBJ_FLAG_HIDDEN);
    }
  }
  // machine ID
  {
    snprintf(buf, sizeof(buf), "%ld", Machine_ID);
    const char *new_val = buf;
    const char *cur_val = lv_label_get_text(objects.machine_id);
    if (strcmp(new_val, cur_val) != 0) {
      tick_value_change_obj = objects.machine_id;
      lv_label_set_text(objects.machine_id, new_val);
      tick_value_change_obj = NULL;
    }
  }
  // wifi connectivity
  {
    const char *desired = isConnected ? LV_SYMBOL_WIFI : LV_SYMBOL_CLOSE;
    const char *current = lv_label_get_text(objects.wifi);

    if (strcmp(current, desired) != 0) {
      tick_value_change_obj = objects.wifi;
      lv_label_set_text(objects.wifi, desired);
      tick_value_change_obj = NULL;
    }
  }
  // default panel variables
  if (displayIndex == DEFAULT_PAGE) {
    // current stage label
    {
      if (currentStage < 8) {
        const char *new_val = stage_names[currentStage];
        if (strcmp(new_val, lv_label_get_text(objects.stage_label)) != 0) {
          lv_label_set_text(objects.stage_label, new_val);
        }
      }
      if (currentStage < NOUAGE) {
        lv_led_set_color(objects.encontrage_led, lv_color_hex(0xFFDCDCDC));
        lv_led_set_color(objects.noage_led, lv_color_hex(0xFFFF0000));
        lv_led_set_color(objects.piquage_led, lv_color_hex(0xFFFF0000));
        lv_led_set_color(objects.ourdissage_led, lv_color_hex(0xFFFF0000));
        lv_led_set_color(objects.ensoplage_led, lv_color_hex(0xFFFF0000));
      } else if (currentStage < PIQUAGE) {
        lv_led_set_color(objects.encontrage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.noage_led, lv_color_hex(0xFFDCDCDC));
        lv_led_set_color(objects.piquage_led, lv_color_hex(0xFFFF0000));
        lv_led_set_color(objects.ourdissage_led, lv_color_hex(0xFFFF0000));
        lv_led_set_color(objects.ensoplage_led, lv_color_hex(0xFFFF0000));
      } else if (currentStage < OURDISSAGE) {
        lv_led_set_color(objects.encontrage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.noage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.piquage_led, lv_color_hex(0xFFDCDCDC));
        lv_led_set_color(objects.ourdissage_led, lv_color_hex(0xFFFF0000));
        lv_led_set_color(objects.ensoplage_led, lv_color_hex(0xFFFF0000));
      } else if (currentStage < ENSOUPLAGE) {
        lv_led_set_color(objects.encontrage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.noage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.piquage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.ourdissage_led, lv_color_hex(0xFFDCDCDC));
        lv_led_set_color(objects.ensoplage_led, lv_color_hex(0xFFFF0000));
      } else if (currentStage < FIN_ENSOUPLAGE) {
        lv_led_set_color(objects.encontrage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.noage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.piquage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.ourdissage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.ensoplage_led, lv_color_hex(0xFFDCDCDC));
      } else {
        lv_led_set_color(objects.encontrage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.noage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.piquage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.ourdissage_led, lv_color_hex(0xFF00FF00));
        lv_led_set_color(objects.ensoplage_led, lv_color_hex(0xFF00FF00));
      }

      if (currentStage != OURDISSAGE && !lv_obj_has_flag(objects.Ourdissage_Panel, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(objects.Ourdissage_Panel, LV_OBJ_FLAG_HIDDEN);
      } else if (currentStage == OURDISSAGE && lv_obj_has_flag(objects.Ourdissage_Panel, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_remove_flag(objects.Ourdissage_Panel, LV_OBJ_FLAG_HIDDEN);
      }
    }
    // performance bar
    {
      int bar_val = (int)(performance);  // round
      int32_t cur_val = lv_bar_get_value(objects.performance_bar);
      if (bar_val != cur_val) {
        tick_value_change_obj = objects.performance_bar;
        lv_bar_set_value(objects.performance_bar, bar_val, LV_ANIM_ON);
        tick_value_change_obj = NULL;
        if (bar_val < 50) {
          lv_obj_set_style_bg_color(objects.performance_bar, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        } else if (bar_val < 60) {
          lv_obj_set_style_bg_color(objects.performance_bar, lv_color_hex(0xFF3300), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        } else if (bar_val < 70) {
          lv_obj_set_style_bg_color(objects.performance_bar, lv_color_hex(0xFF9900), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        } else if (bar_val < 80) {
          lv_obj_set_style_bg_color(objects.performance_bar, lv_color_hex(0xCCFF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        } else if (bar_val < 90) {
          lv_obj_set_style_bg_color(objects.performance_bar, lv_color_hex(0x66FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        } else {
          lv_obj_set_style_bg_color(objects.performance_bar, lv_color_hex(0x00FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
      }
    }
    // performance value
    {
      snprintf(buf, sizeof(buf), "%.1f %%", performance);  // 2 decimal places
      const char *new_val = buf;
      const char *cur_val = lv_label_get_text(objects.performance_value);

      if (strcmp(new_val, cur_val) != 0) {
        tick_value_change_obj = objects.performance_value;
        lv_label_set_text(objects.performance_value, new_val);
        tick_value_change_obj = NULL;
      }
    }
    // stage duration timer
    {
      if (stageDuration != last_duration) {
        last_duration = stageDuration;
        uint32_t totalSec = (uint32_t)stageDuration;
        uint16_t hh = totalSec / 3600;
        uint8_t mm = (totalSec % 3600) / 60;
        uint8_t ss = totalSec % 60;

        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss);

        const char *new_val = buf;
        const char *cur_val = lv_label_get_text(objects.stage_timer);
        if (strcmp(new_val, cur_val) != 0) {
          tick_value_change_obj = objects.stage_timer;
          lv_label_set_text(objects.stage_timer, new_val);
          tick_value_change_obj = NULL;
        }
      }
    }
    // beam length value
    {
      snprintf(buf, sizeof(buf), "%ld", beamLength);
      const char *new_val = buf;
      const char *cur_val = lv_label_get_text(objects.length_value);
      if (strcmp(new_val, cur_val) != 0) {
        tick_value_change_obj = objects.length_value;
        lv_label_set_text(objects.length_value, new_val);
        tick_value_change_obj = NULL;
      }
    }
    // beam section value
    {
      snprintf(buf, sizeof(buf), "%ld", section);
      const char *new_val = buf;
      const char *cur_val = lv_label_get_text(objects.winding_section);
      if (strcmp(new_val, cur_val) != 0) {
        tick_value_change_obj = objects.winding_section;
        lv_label_set_text(objects.winding_section, new_val);
        tick_value_change_obj = NULL;
      }
    }
    // druum revolutions value
    {
      snprintf(buf, sizeof(buf), "%ld", drumRevolutions);
      const char *new_val = buf;
      const char *cur_val = lv_label_get_text(objects.revolutions_value);
      if (strcmp(new_val, cur_val) != 0) {
        tick_value_change_obj = objects.revolutions_value;
        lv_label_set_text(objects.revolutions_value, new_val);
        tick_value_change_obj = NULL;
      }
    }
    //linear speed value
    {
      snprintf(buf, sizeof(buf), "%.2f", linearSpeed);
      const char *new_val = buf;
      const char *cur_val = lv_label_get_text(objects.speed_value);
      if (strcmp(new_val, cur_val) != 0) {
        tick_value_change_obj = objects.speed_value;
        lv_label_set_text(objects.speed_value, new_val);
        tick_value_change_obj = NULL;
      }
    }
    // angular speed value
    {
      snprintf(buf, sizeof(buf), "%.2f", angulareSpeed);
      const char *new_val = buf;
      const char *cur_val = lv_label_get_text(objects.angulare_speed_value);
      if (strcmp(new_val, cur_val) != 0) {
        tick_value_change_obj = objects.angulare_speed_value;
        lv_label_set_text(objects.angulare_speed_value, new_val);
        tick_value_change_obj = NULL;
      }
    }
  }
  // HALT REPORT + WARNING ICON LOGIC - Exact behavior requested
  {
    static int last_stopCode = 100;

    // Automatic show report when stopCode becomes non-zero (new halt or changed halt)
    if (stopCode != 100 && last_stopCode == 100) {
      hideReport = false;  // show report on new stop
    }
    // Also show report if stopCode changed while already active
    else if (stopCode != 100 && stopCode != last_stopCode) {
      hideReport = false;
    }
    // When stop ends completely
    else if (stopCode == 100 && last_stopCode != 100) {
      hideReport = true;
    }

    last_stopCode = stopCode;

    bool has_active_stop = (stopCode != 100);
    bool show_report = !hideReport;  // show if not manually hidden
    bool show_warning = hideReport;  // warning visible only when report is hidden

    // === Report Panel (Popup style) ===
    if (show_report) {
      if (lv_obj_has_flag(objects.halt_report_panel, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(objects.halt_report_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(objects.halt_report_panel);  // Bring to front

        // Show overlay if you created one
        if (objects.halt_report_overlay != NULL) {
          lv_obj_clear_flag(objects.halt_report_overlay, LV_OBJ_FLAG_HIDDEN);
          lv_obj_move_foreground(objects.halt_report_overlay);
          lv_obj_move_foreground(objects.halt_report_panel);  // panel on top of overlay
        }
      }
    } else {
      if (!lv_obj_has_flag(objects.halt_report_panel, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(objects.halt_report_panel, LV_OBJ_FLAG_HIDDEN);
        if (objects.halt_report_overlay != NULL) {
          lv_obj_add_flag(objects.halt_report_overlay, LV_OBJ_FLAG_HIDDEN);
        }
      }
    }

    // === Warning Icon ===
    if (show_warning) {
      if (lv_obj_has_flag(objects.warning_stop, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(objects.warning_stop, LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      if (!lv_obj_has_flag(objects.warning_stop, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(objects.warning_stop, LV_OBJ_FLAG_HIDDEN);
      }
    }

    // Color of warning icon
    if (has_active_stop) {
      lv_obj_set_style_text_color(objects.warning_stop, lv_color_hex(0xffff0000), LV_PART_MAIN | LV_STATE_DEFAULT);  // red
    } else {
      lv_obj_set_style_text_color(objects.warning_stop, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);  // white
    }

    // Update cause text when report is visible
    if (show_report && objects.halt_cause_label != NULL) {
      const char *cause_text = "INCONNU";
      for (size_t i = 0; i < sizeof(halt_reasons) / sizeof(halt_reasons[0]); i++) {
        if (halt_reasons[i].code == stopCode) {
          cause_text = halt_reasons[i].text;
          break;
        }
      }
      if (strcmp(cause_text, lv_label_get_text(objects.halt_cause_label)) != 0) {
        lv_label_set_text(objects.halt_cause_label, cause_text);
      }
    }

    // Update duration when report is visible
    if (show_report && objects.halt_duration_timer_label != NULL) {
      static uint32_t last_displayed = UINT32_MAX;
      if (stopDuration != last_displayed) {
        last_displayed = stopDuration;
        uint32_t totalSec = stopDuration;
        uint16_t hh = totalSec / 3600;
        uint8_t mm = (totalSec % 3600) / 60;
        uint8_t ss = totalSec % 60;

        static char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hh, mm, ss);
        lv_label_set_text(objects.halt_duration_timer_label, buf);
      }
    }

    // Reset duration when stop ends
    if (stopCode == 100 && stopDuration != 0) {
      stopDuration = 0;
      if (objects.halt_duration_timer_label != NULL) {
        lv_label_set_text(objects.halt_duration_timer_label, "00:00:00");
      }
    }
  }
}

typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
  tick_screen_main,
};
void tick_screen(int screen_index) {
  tick_screen_funcs[screen_index]();
}
void tick_screen_by_id(enum ScreensEnum screenId) {
  tick_screen_funcs[screenId - 1]();
}

//
// Fonts
//
ext_font_desc_t fonts[] = {
#if LV_FONT_MONTSERRAT_8
  { "MONTSERRAT_8", &lv_font_montserrat_8 },
#endif
#if LV_FONT_MONTSERRAT_10
  { "MONTSERRAT_10", &lv_font_montserrat_10 },
#endif
#if LV_FONT_MONTSERRAT_12
  { "MONTSERRAT_12", &lv_font_montserrat_12 },
#endif
#if LV_FONT_MONTSERRAT_14
  { "MONTSERRAT_14", &lv_font_montserrat_14 },
#endif
#if LV_FONT_MONTSERRAT_16
  { "MONTSERRAT_16", &lv_font_montserrat_16 },
#endif
#if LV_FONT_MONTSERRAT_18
  { "MONTSERRAT_18", &lv_font_montserrat_18 },
#endif
#if LV_FONT_MONTSERRAT_20
  { "MONTSERRAT_20", &lv_font_montserrat_20 },
#endif
#if LV_FONT_MONTSERRAT_22
  { "MONTSERRAT_22", &lv_font_montserrat_22 },
#endif
#if LV_FONT_MONTSERRAT_24
  { "MONTSERRAT_24", &lv_font_montserrat_24 },
#endif
#if LV_FONT_MONTSERRAT_26
  { "MONTSERRAT_26", &lv_font_montserrat_26 },
#endif
#if LV_FONT_MONTSERRAT_28
  { "MONTSERRAT_28", &lv_font_montserrat_28 },
#endif
#if LV_FONT_MONTSERRAT_30
  { "MONTSERRAT_30", &lv_font_montserrat_30 },
#endif
#if LV_FONT_MONTSERRAT_32
  { "MONTSERRAT_32", &lv_font_montserrat_32 },
#endif
#if LV_FONT_MONTSERRAT_34
  { "MONTSERRAT_34", &lv_font_montserrat_34 },
#endif
#if LV_FONT_MONTSERRAT_36
  { "MONTSERRAT_36", &lv_font_montserrat_36 },
#endif
#if LV_FONT_MONTSERRAT_38
  { "MONTSERRAT_38", &lv_font_montserrat_38 },
#endif
#if LV_FONT_MONTSERRAT_40
  { "MONTSERRAT_40", &lv_font_montserrat_40 },
#endif
#if LV_FONT_MONTSERRAT_42
  { "MONTSERRAT_42", &lv_font_montserrat_42 },
#endif
#if LV_FONT_MONTSERRAT_44
  { "MONTSERRAT_44", &lv_font_montserrat_44 },
#endif
#if LV_FONT_MONTSERRAT_46
  { "MONTSERRAT_46", &lv_font_montserrat_46 },
#endif
#if LV_FONT_MONTSERRAT_48
  { "MONTSERRAT_48", &lv_font_montserrat_48 },
#endif
};

//
// Color themes
//

uint32_t active_theme_index = 0;

//
//
//

void create_screens() {

  // Set default LVGL theme
  lv_display_t *dispp = lv_display_get_default();
  lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
  lv_display_set_theme(dispp, theme);

  // Initialize screens
  // Create screens
  create_screen_main();
}
