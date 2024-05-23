
#ifndef WAKE_WORD_LIB_HOOK_H
#define WAKE_WORD_LIB_HOOK_H

#ifdef __cplusplus

extern "C" {

#endif
#include <pv_porcupine.h>
#include <pv_recorder.h>
#include <picovoice.h>

#ifdef __cplusplus

}

#endif
#include <stddef.h>

// typedef struct {
// int frame_length;
// pv_recorder_t* pv_recorder;
// }pv_recorder_handle_t;

// int init_wake_word_lib(const char *library_path, const char *api_key, const char *model_path, const char *keyword_path);

// pv_recorder_handle_t init_pv_recorder();

// int detect_wakeword(int16_t *pcm, pv_recorder_t *recorder);

// void clean_up_wake_word_lib();

class wake_word_lib {
public:
wake_word_lib(int audio_device_id, const char *library_path, const char *api_key, const char *model_path, const char *keyword_path);

int detect_wakeword(int16_t* pcm);

pv_recorder_t *get_recorder_inst() {
    return recorder;
}

uint32_t get_frame_length() {
    return frame_length;
}

~wake_word_lib();

private:
const float sensitivity = 0.5f;

int init_wake_word_lib(const char *library_path, const char *api_key, const char *model_path, const char *keyword_path);

int init_pv_recorder(int audio_device_id);

void init_functions_from_dynamic_library(const char *library_path);

void *porcupine_library = NULL;
pv_porcupine_t *porcupine = NULL;
pv_recorder* recorder = NULL;
uint32_t frame_length = 0;

char **message_stack = NULL;
int32_t message_stack_depth = 0;

/* Functions that will be fetched during runtime from the linked library (.so file!) */
const char *(*pv_status_to_string_func)(pv_status_t);
int32_t (*pv_sample_rate_func)();
pv_status_t (*pv_porcupine_init_func)(const char *, const char *, int32_t, const char *const *, const float *, pv_porcupine_t **);
void (*pv_porcupine_delete_func)(pv_porcupine_t *);
pv_status_t (*pv_porcupine_process_func)(pv_porcupine_t *, const int16_t *, int32_t *);
const char *(*pv_porcupine_version_func)();
pv_status_t (*pv_get_error_stack_func)(char ***, int32_t *);
void (*pv_free_error_stack_func)(char **);
int32_t (*pv_porcupine_frame_length_func)();
};



#endif /* WAKE_WORD_LIB_HOOK_H*/