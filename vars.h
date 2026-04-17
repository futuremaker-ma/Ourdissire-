#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

  // enum declarations
  typedef enum {
    ENCANTRAGE,
    ENCANTRAGE_PARTIEL,
    FIN_ENCANTRAGE,
    NOUAGE,
    PIQUAGE,
    OURDISSAGE,
    ENSOUPLAGE,
    FIN_ENSOUPLAGE
  } ProductionStage;

  typedef enum {
    DEFAULT_PAGE,
    CATEGORIES_PAGE,
    HALTCODES_PAGE,
    OPERATORID_PAGE,
    SETTINGS_PAGE
  } Page;

  typedef enum {
    IDLE,
    MECHANICAL,
    ELECTRICAL,
    OPERATOR,
    STAGES
  } Category;

  // Flow global variables
  enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_NONE
  };

  // Native global variables
  extern int Machine_ID;
  extern char *Drum_circemferance;
  extern char *productionStageLabel;
  extern char *stopLabel;
  extern bool isConnected;
  extern bool isRunning;
  extern bool hideReport;
  extern bool setStage;
  extern ProductionStage currentStage;
  extern Page displayIndex;
  extern Category currentCategory;
  extern float performance;
  extern int beamLength;
  extern uint8_t section;
  extern int drumRevolutions;
  extern float linearSpeed;
  extern float angulareSpeed;
  extern double stageDuration;
  extern double stopDuration;
  extern char *URL;
  extern char *operatorID;
  extern char *command;
  extern uint8_t stopCode;
  extern uint8_t stageCode;
  extern char *Avencement;

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/
