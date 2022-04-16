#include "core.hpp"

#include <cassert>

#include <dlfcn.h>

#define _core_load_sym(S)                                                   \
    do {                                                                    \
        *(void **) &core->S = dlsym(core->handle, #S);                      \
        if (!core->S) {                                                     \
            fprintf(stderr, "error loading core, reason: %s\n", dlerror()); \
            return false;                                                   \
        }                                                                   \
    } while(0)

bool
CoreInitFromSoFile(Core *core, std::string const& path)
{
    assert(core);

    core->handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!core->handle) {
        fprintf(stderr, "error loading core, reason: %s\n", dlerror());
        return false;
    }

    _core_load_sym(retro_init);
    _core_load_sym(retro_deinit);
    _core_load_sym(retro_api_version);
    _core_load_sym(retro_get_system_info);
    _core_load_sym(retro_get_system_av_info);
    _core_load_sym(retro_set_controller_port_device);
    _core_load_sym(retro_reset);
    _core_load_sym(retro_run);
    _core_load_sym(retro_load_game);
    _core_load_sym(retro_unload_game);
    _core_load_sym(retro_set_environment);
    _core_load_sym(retro_set_video_refresh);
    _core_load_sym(retro_set_input_poll);
    _core_load_sym(retro_set_input_state);
    _core_load_sym(retro_set_audio_sample);
    _core_load_sym(retro_set_audio_sample_batch);

    return true;
}

