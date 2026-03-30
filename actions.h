#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_send_command(lv_event_t * e);
extern void action_return_icon_button(lv_event_t * e);
extern void action_set_category(lv_event_t * e);
extern void action_set_temp_current_stage(lv_event_t * e);
extern void action_set_temp_halt_code(lv_event_t * e);
extern void action_set_operator_id(lv_event_t * e);
extern void action_submet_operator_id(lv_event_t * e);
extern void action_show_keyboard(lv_event_t * e);
extern void action_set_machine_id(lv_event_t * e);
extern void action_close_haltabel(lv_event_t * e);
extern void action_display(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/