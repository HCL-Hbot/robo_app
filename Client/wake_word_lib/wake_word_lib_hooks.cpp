#include <stddef.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#include <wake_word_lib_hooks.h>
#include <dlfcn.h>


static void *open_dl(const char *dl_path)
{

    return dlopen(dl_path, RTLD_NOW);
}

static void *load_symbol(void *handle, const char *symbol)
{

    return dlsym(handle, symbol);
}

static void print_dl_error(const char *message)
{
    fprintf(stderr, "%s with '%s'.\n", message, dlerror());
}
void print_error_message(char **message_stack, int32_t message_stack_depth)
{
    for (int32_t i = 0; i < message_stack_depth; i++)
    {
        fprintf(stderr, "  [%d] %s\n", i, message_stack[i]);
    }
}

static void close_dl(void *handle) {

    dlclose(handle);
}


void wake_word_lib::init_functions_from_dynamic_library(const char *library_path) {
    porcupine_library = open_dl(library_path);
    if (!porcupine_library)
    {
        fprintf(stderr, "failed to open library.\n");
        exit(1);
    }

    pv_status_to_string_func = (const char *(*)(pv_status_t))load_symbol(porcupine_library, "pv_status_to_string");
    if (!pv_status_to_string_func)
    {
        print_dl_error("failed to load 'pv_status_to_string'");
        exit(1);
    }

    pv_sample_rate_func = (int32_t(*)())load_symbol(porcupine_library, "pv_sample_rate");
    if (!pv_sample_rate_func)
    {
        print_dl_error("failed to load 'pv_sample_rate'");
        exit(1);
    }

    pv_porcupine_init_func = (pv_status_t(*)(const char *, const char *, int32_t, const char *const *, const float *, pv_porcupine_t **))load_symbol(porcupine_library, "pv_porcupine_init");
    if (!pv_porcupine_init_func)
    {
        print_dl_error("failed to load 'pv_porcupine_init'");
        exit(1);
    }

    pv_porcupine_delete_func = (void(*)(pv_porcupine_t*))load_symbol(porcupine_library, "pv_porcupine_delete");
    if (!pv_porcupine_delete_func)
    {
        print_dl_error("failed to load 'pv_porcupine_delete'");
        exit(1);
    }

    pv_porcupine_process_func = (pv_status_t(*)(pv_porcupine_t*, const int16_t*, int32_t*))load_symbol(porcupine_library, "pv_porcupine_process");
    if (!pv_porcupine_process_func)
    {
        print_dl_error("failed to load 'pv_porcupine_process'");
        exit(1);
    }

    pv_porcupine_version_func = (const char*(*)())load_symbol(porcupine_library, "pv_porcupine_version");
    if (!pv_porcupine_version_func)
    {
        print_dl_error("failed to load 'pv_porcupine_version'");
        exit(1);
    }

    pv_get_error_stack_func = (pv_status_t(*)(char***, int32_t*))load_symbol(porcupine_library, "pv_get_error_stack");
    if (!pv_get_error_stack_func)
    {
        print_dl_error("failed to load 'pv_get_error_stack_func'");
        exit(1);
    }

    pv_free_error_stack_func = (void(*)(char**))load_symbol(porcupine_library, "pv_free_error_stack");
    if (!pv_free_error_stack_func)
    {
        print_dl_error("failed to load 'pv_free_error_stack_func'");
        exit(1);
    }
}

int wake_word_lib::init_wake_word_lib(const char *library_path, const char *api_key, const char *model_path, const char *keyword_path) {
    pv_status_t error_status = PV_STATUS_RUNTIME_ERROR;
    pv_status_t porcupine_status = pv_porcupine_init_func(api_key, model_path, 1, &keyword_path, &sensitivity, &porcupine);

    if (porcupine_status != PV_STATUS_SUCCESS)
    {
        fprintf(stderr, "'pv_porcupine_init' failed with '%s'", pv_status_to_string_func(porcupine_status));
        error_status = pv_get_error_stack_func(&message_stack, &message_stack_depth);
        if (error_status != PV_STATUS_SUCCESS)
        {
            fprintf(stderr, ".\nUnable to get Porcupine error state with '%s'.\n", pv_status_to_string_func(error_status));
            exit(1);
        }

        if (message_stack_depth > 0)
        {
            fprintf(stderr, ":\n");
            print_error_message(message_stack, message_stack_depth);
            pv_free_error_stack_func(message_stack);
        }
        else
        {
            fprintf(stderr, ".\n");
        }
        exit(1);
    }
    return error_status;
}
int wake_word_lib::init_pv_recorder(int audio_device_id) {
    int32_t (*pv_sample_rate)()  = (int32_t(*)())load_symbol(porcupine_library, "pv_sample_rate");
    if(!pv_sample_rate) {
        print_dl_error("failed to load 'pv_sample_rate'");
        exit(1);
    }
    printf("%d\n",pv_sample_rate());
    int32_t (*pv_porcupine_frame_length_func)() = (int32_t(*)())load_symbol(porcupine_library, "pv_porcupine_frame_length");
    if (!pv_porcupine_frame_length_func)
    {
        print_dl_error("failed to load 'pv_porcupine_frame_length'");
        exit(1);
    }
    frame_length = pv_porcupine_frame_length_func();
    pv_recorder_status_t recorder_status = pv_recorder_init(audio_device_id, frame_length, 100, true, true, &recorder);
    if (recorder_status != PV_RECORDER_STATUS_SUCCESS)
    {
        fprintf(stderr, "Failed to initialize device with %s.\n", pv_recorder_status_to_string(recorder_status));
        exit(1);
    }

    const char *selected_device = pv_recorder_get_selected_device(recorder);
    fprintf(stdout, "Selected device: %s.\n", selected_device);
    return 0;
}


wake_word_lib::wake_word_lib(int audio_device_id, const char *library_path, const char *api_key, const char *model_path, const char *keyword_path) {
    init_functions_from_dynamic_library(library_path);
    init_wake_word_lib(library_path, api_key, model_path, keyword_path);
    fprintf(stdout, "V%s\n\n", pv_porcupine_version_func());
    init_pv_recorder(audio_device_id);

}

int wake_word_lib::detect_wakeword(int16_t* pcm) {
    int32_t keyword_index = -1;
    pv_status_t porcupine_status = pv_porcupine_process_func(porcupine, pcm, &keyword_index);
    if (porcupine_status != PV_STATUS_SUCCESS)
    {
        fprintf(stderr, "'pv_porcupine_process' failed with '%s'", pv_status_to_string_func(porcupine_status));
        pv_status_t error_status = pv_get_error_stack_func(&message_stack, &message_stack_depth);
        if (error_status != PV_STATUS_SUCCESS)
        {
            fprintf(stderr, ".\nUnable to get Porcupine error state with '%s'.\n", pv_status_to_string_func(error_status));
            exit(1);
        }

        if (message_stack_depth > 0)
        {
            fprintf(stderr, ":\n");
            print_error_message(message_stack, message_stack_depth);
            pv_free_error_stack_func(message_stack);
        }
        else
        {
            fprintf(stderr, ".\n");
        }
        exit(1);
    }

    if (keyword_index != -1)
    {
        return 1;
    }
    return 0;
}

wake_word_lib::~wake_word_lib() {
    pv_porcupine_delete_func(porcupine);
    close_dl(porcupine_library);
}
