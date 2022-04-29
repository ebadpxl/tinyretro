#include "core.hpp"
#include "libretro.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <unordered_map>

#include <SFML/Graphics.hpp>

#define DIE(...) do { \
        printf(__VA_ARGS__); \
        exit(-1); \
    } while (0)

// SFML Window.
sf::RenderWindow gWindow(sf::VideoMode(800, 600), ".: TinyRetro :.");

// Retro.
Core gCore;

// SFML key to libretro joypad mapping.
#define USE_LIGHTGUN 1
#ifdef USE_LIGHTGUN
std::unordered_map<sf::Keyboard::Key, unsigned> gKeyBinding {
    { sf::Keyboard::Z, RETRO_DEVICE_ID_LIGHTGUN_START},
    { sf::Keyboard::X, RETRO_DEVICE_ID_LIGHTGUN_SELECT},
    { sf::Keyboard::A, RETRO_DEVICE_ID_LIGHTGUN_AUX_A},
    { sf::Keyboard::S, RETRO_DEVICE_ID_LIGHTGUN_AUX_B},
    { sf::Keyboard::Enter, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER},
    { sf::Keyboard::Space, RETRO_DEVICE_ID_LIGHTGUN_RELOAD},
};
#else
std::unordered_map<sf::Keyboard::Key, unsigned> gKeyBinding {
    { sf::Keyboard::Up, RETRO_DEVICE_ID_JOYPAD_UP },
    { sf::Keyboard::Down, RETRO_DEVICE_ID_JOYPAD_DOWN },
    { sf::Keyboard::Left, RETRO_DEVICE_ID_JOYPAD_LEFT },
    { sf::Keyboard::Right, RETRO_DEVICE_ID_JOYPAD_RIGHT },
    { sf::Keyboard::Enter, RETRO_DEVICE_ID_JOYPAD_START},
    { sf::Keyboard::Z, RETRO_DEVICE_ID_JOYPAD_A},
    { sf::Keyboard::X, RETRO_DEVICE_ID_JOYPAD_B},
};
#endif

// Stores which buttons are pressed on the first joypad. Note that only support a single
// joypad; each joypads should have their own mapping state.
unsigned gJoy[RETRO_DEVICE_ID_JOYPAD_R3+1] = {0};

// Video Format used by the core.
retro_pixel_format gCoreFormat = RETRO_PIXEL_FORMAT_UNKNOWN;

/// Represents a video buffer (retro<->SFML).
struct VideoBuffer {
    retro_pixel_format      pixelFormat;    // libretro pixel format
    sf::Texture             texture;        // SFML backing texture
    std::vector<uint8_t>    pixelData;      // pixel data to be uploaded to SFML
} gVideoBuffer;

void
CoreLog(retro_log_level level, char const* fmt, ...)
{
    char buffer[4096] = {0};
    static const char * levelstr[] = { "dbg", "inf", "wrn", "err" };
    va_list va;

    va_start(va, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);

    fprintf(stderr, "[%s] %s", levelstr[level], buffer);
    fflush(stderr);

    if (level == RETRO_LOG_ERROR)
        exit(EXIT_FAILURE);
}

bool
RetroEnvironment(unsigned cmd, void *data)
{
    bool *bval;

    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            ((struct retro_log_callback *) data)->log = CoreLog;
            break;
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            bval = (bool*)data;
            *bval = true;
            break;
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            gVideoBuffer.pixelFormat = *((retro_pixel_format *) data);
            return true;
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *(const char **)data = ".";
            return true;
        default:
            return false;
    }

    return true;
}

void
RetroVideoRefresh(void const *_data, unsigned width, unsigned height, size_t pitch)
{
    // Resize the video buffer if needed. This should only happen once when
    // the libretro core is initialized.
    bool resizeTexture = (width != gVideoBuffer.texture.getSize().x)
        || (height != gVideoBuffer.texture.getSize().y);
    if (resizeTexture) {
        gVideoBuffer.texture.create(width, height);
        gVideoBuffer.pixelData.resize(width * height * 4);
    }

    if (gVideoBuffer.pixelFormat == RETRO_PIXEL_FORMAT_0RGB1555) {
        // XXX(sgosselin): something odd is happening with PCSX. It seems to
        // initially select the RGB565 format. However, it calls the video refresh
        // callback with this pixel format. For now, let's do the RGB565 conversion
        // but it's worth digging into later.
        uint16_t const *data = (uint16_t const *)_data;
        for (size_t i = 0; i < width*height; ++i) {
            gVideoBuffer.pixelData[4 * i + 0] = (255.f / 31.f) * ((data[i] & 0xf800) >> 11);
            gVideoBuffer.pixelData[4 * i + 1] = (255.f / 63.f) * ((data[i] & 0x07e0) >> 5);
            gVideoBuffer.pixelData[4 * i + 2] = (255.f / 31.f) * ((data[i] & 0x001f));
            gVideoBuffer.pixelData[4 * i + 3] = 255;
        }
    } else if (gVideoBuffer.pixelFormat == RETRO_PIXEL_FORMAT_XRGB8888) {
        uint8_t const *data = (uint8_t const *)data;
        for (size_t i = 0; i < width * height; ++i) {
            gVideoBuffer.pixelData[4 * i + 0] = data[4 * i + 2];
            gVideoBuffer.pixelData[4 * i + 1] = data[4 * i + 1];
            gVideoBuffer.pixelData[4 * i + 2] = data[4 * i + 0];
            gVideoBuffer.pixelData[4 * i + 3] = 255;
        }
    } else if (gVideoBuffer.pixelFormat == RETRO_PIXEL_FORMAT_RGB565) {
        uint16_t const *data = (uint16_t const *)_data;
        for (size_t i = 0; i < width*height; ++i) {
            gVideoBuffer.pixelData[4 * i + 0] = (255.f / 31.f) * ((data[i] & 0xf800) >> 11);
            gVideoBuffer.pixelData[4 * i + 1] = (255.f / 63.f) * ((data[i] & 0x07e0) >> 5);
            gVideoBuffer.pixelData[4 * i + 2] = (255.f / 31.f) * ((data[i] & 0x001f));
            gVideoBuffer.pixelData[4 * i + 3] = 255;
        }
    } else {
        DIE("failed to convert libretro pixel format (fmt=%d)", (int)gVideoBuffer.pixelFormat);
    }

    gVideoBuffer.texture.update(&gVideoBuffer.pixelData[0]);
}

void
RetroInputPoll(void)
{
    for (auto const& key: gKeyBinding) {
        gJoy[key.second] = sf::Keyboard::isKeyPressed(key.first);
    }

#ifdef USE_LIGHTGUN
    float mouseX = sf::Mouse::getPosition(gWindow).x / ((float) gWindow.getSize().x);
    float mouseY = sf::Mouse::getPosition(gWindow).y / ((float) gWindow.getSize().y);
    // libretro expects mouse (x, y) in screen space so do the conversion here.
    int screenPosX = 0x8000 * (2 * mouseX - 1);
    int screenPosY = 0x8000 * (2 * mouseY - 1);
    gJoy[RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X] = screenPosX;
    gJoy[RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y] = screenPosY;
#endif
}

int16_t
RetroInputState(unsigned port, unsigned device, unsigned index, unsigned id)
{
    // Only a single joypad is supported so ignore other requests from the core.
    if ((port != 0) || (index != 0))
        return 0;
    return gJoy[id];
}

void
RetroAudioSample(int16_t left, int16_t right)
{
}

size_t
RetroAudioSampleBatch(int16_t const *data, size_t frame)
{
    return frame;
}

// Loads a ROM.
bool
LoadRomFromFile(Core *core, std::string const& romPath)
{
	retro_game_info info = { romPath.c_str(), 0 };

    assert(core);

    std::ifstream file(romPath, std::ifstream::binary);
    if (!file)
        DIE("couldn't open rom: %s", romPath.c_str());

    // Determine the size of the file.
    file.seekg(0, file.end);
    info.size = file.tellg();
    file.seekg(0, file.beg);

    retro_system_info system = {0};
    core->retro_get_system_info(&system);

    // Read entire file into memory when the core can.
    if (!system.need_fullpath) {
        info.data = malloc(info.size);
        assert(info.data);

        file.read((char *)info.data, info.size);
        if (!file)
            DIE("couldn't read rom content");
    }

    if (!core->retro_load_game(&info))
        DIE("loading game content into libretro failed");

    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 3)
        DIE("usage: %s [core] [rom]\n", argv[0]);

    const std::string corePath = argv[1];
    const std::string romPath = argv[2];
    printf("running with core:'%s' rom:'%s'\n", corePath.c_str(), romPath.c_str());

    // Initialize the libretro core.
    if (!CoreInitFromSoFile(&gCore, corePath))
        DIE("couldn't initialize libretro core");
    gCore.retro_set_environment(RetroEnvironment);
    gCore.retro_set_video_refresh(RetroVideoRefresh);
    gCore.retro_set_input_poll(RetroInputPoll);
    gCore.retro_set_input_state(RetroInputState);
    gCore.retro_set_audio_sample(RetroAudioSample);
    gCore.retro_set_audio_sample_batch(RetroAudioSampleBatch);
    gCore.retro_init();
    gCore.initialized = true;

    // Load the rom.
    if (!LoadRomFromFile(&gCore, romPath))
        DIE("couldn't load rom into libretro core");

    // Assign a joypad to the first slot.
#ifdef USE_LIGHTGUN
    gCore.retro_set_controller_port_device(0, RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_LIGHTGUN, 0));
#else
    gCore.retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
#endif

    // Open an SFML window that runs at 60 FPS since the NES roughly runs at 60Hz.
    gWindow.setFramerateLimit(60);
    while (gWindow.isOpen()) {
        sf::Event event;
        while (gWindow.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                gWindow.close();
            if (event.type == sf::Event::KeyPressed) {
                switch (event.key.code) {
                    case sf::Keyboard::Escape:
                    case sf::Keyboard::Q:
                        gWindow.close();
                        break;
                    default:
                        break;
                }
            }
        }

        gCore.retro_run();

        gWindow.clear(sf::Color::Black);
        sf::Sprite sprite(gVideoBuffer.texture);
        sprite.setScale(
            gWindow.getSize().x / sprite.getLocalBounds().width,
            gWindow.getSize().y / sprite.getLocalBounds().height);
        gWindow.draw(sprite);
        gWindow.display();
    }

    return 0;
}

