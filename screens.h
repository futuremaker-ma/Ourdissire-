#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

  // Screens

  enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    _SCREEN_ID_LAST = 1
  };

  typedef struct _objects_t {
    // Screen & main containers
    lv_obj_t *main;
    lv_obj_t *header_panel;
    lv_obj_t *content_panel;      // ← the single reusable content panel
    lv_obj_t *halt_report_panel;  // overlay, never deleted

    // Header elements
    lv_obj_t *return_button;
    lv_obj_t *machine_id;
    lv_obj_t *warning_stop;
    lv_obj_t *wifi;
    lv_obj_t *setting;

    // Keyboard (always present)
    lv_obj_t *keyboard;

    // Default Panel objects
    lv_obj_t *encontrage_led;
    lv_obj_t *noage_led;
    lv_obj_t *piquage_led;
    lv_obj_t *ourdissage_led;
    lv_obj_t *ensoplage_led;
    lv_obj_t *stage_label;
    lv_obj_t *performance_bar;
    lv_obj_t *performance_value;
    lv_obj_t *stage_timer;

    // Ourdissage sub-panel
    lv_obj_t *Ourdissage_Panel;
    lv_obj_t *beam_length;
    lv_obj_t *length_value;
    lv_obj_t *winding_section;
    lv_obj_t *drum_section_revolutions;
    lv_obj_t *revolutions_value;
    lv_obj_t *linear_speed;
    lv_obj_t *speed_value;
    lv_obj_t *angulare_speed;
    lv_obj_t *angulare_speed_value;

    // Categories Panel
    lv_obj_t *mecanical_category;
    lv_obj_t *electric_category;
    lv_obj_t *operator_category;

    // Halt Codes Panel
    lv_obj_t *category_label_title;

    // Operator ID Panel
    lv_obj_t *operator_id_text_area;
    lv_obj_t *button_h;
    lv_obj_t *button_ar;
    lv_obj_t *button_s;
    lv_obj_t *button_ok;

    // Settings Panel
    lv_obj_t *set_machine_id;
    lv_obj_t *machine_id_text_area;
    lv_obj_t *set_new_machine_id;
    lv_obj_t *set_circemferance;
    lv_obj_t *circemferance_text_area;
    lv_obj_t *set_new_circemferance;
    lv_obj_t *set_database_link;
    lv_obj_t *ip_textarea;
    lv_obj_t *port_textarea;
    lv_obj_t *db_textarea;
    lv_obj_t *set_database_url;
    lv_obj_t *open_portal;

    // Halt Report Panel
    lv_obj_t *halt_cause_label;
    lv_obj_t *halt_duration_timer_label;
    lv_obj_t *close_stop_report;
    lv_obj_t *justefy_halt_cause;
    lv_obj_t *resume_production;
    lv_obj_t *halt_report_overlay;

    // avencement panel
    lv_obj_t *avencement_panel;
    lv_obj_t *avencement_textarea;
    lv_obj_t *submet_avencement;
    lv_obj_t *cancel_avencement;

    // Encantrage pop up
    lv_obj_t *encantrage_panel;
    lv_obj_t *encantrage_button;
    lv_obj_t *encantrage_partiel_button;
    lv_obj_t *fin_encantrage_button;
    lv_obj_t *cancel_encantrage_setting;

  } objects_t;

  extern objects_t objects;


  void SendData();

  void create_screen_main();
  void tick_screen_main();

  void tick_screen_by_id(enum ScreensEnum screenId);
  void tick_screen(int screen_index);

  void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/
