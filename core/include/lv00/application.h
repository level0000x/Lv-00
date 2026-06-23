#ifndef LV00_APPLICATION_H
#define LV00_APPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the application. */
int lv00_application_init(void);
/** Run the application main loop. */
int lv00_application_run(void);
/** Shutdown the application. */
void lv00_application_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
