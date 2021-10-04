#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_net.h>
#include <SDL_ttf.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
//#include <SDL_gpu.h>
//#include <SFML/Network.hpp>
//#include <SFML/Graphics.hpp>
#include <algorithm>
#include <atomic>
#include <codecvt>
#include <functional>
#include <locale>
#include <mutex>
#include <thread>
#ifdef __ANDROID__
#include "vendor/PUGIXML/src/pugixml.hpp"
#include <android/log.h> //__android_log_print(ANDROID_LOG_VERBOSE, "EndlessRunner", "Example number log: %d", number);
#include <jni.h>
#include "vendor/GLM/include/glm/glm.hpp"
#include "vendor/GLM/include/glm/gtc/matrix_transform.hpp"
#include "vendor/GLM/include/glm/gtc/type_ptr.hpp"
#else
//#include <pugixml.hpp>
//#include <glm/glm.hpp>
//#include <glm/gtc/matrix_transform.hpp>
//#include <glm/gtc/type_ptr.hpp>
#ifdef __EMSCRIPTEN__
namespace fs = std::__fs::filesystem;
#else
namespace fs = std::filesystem;
#endif
using namespace std::chrono_literals;
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

// NOTE: Remember to uncomment it on every release
//#define RELEASE

#if defined _MSC_VER && defined RELEASE
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

//240 x 240 (smart watch)
//240 x 320 (QVGA)
//360 x 640 (Galaxy S5)
//640 x 480 (480i - Smallest PC monitor)

#define PLAYER_SPEED 0.1
#define GAME_SPEED 0.4
#define PLAYER_FRAME_MS 50
#define SCORE_MULTIPLIER 5
#define OBSTACKLE_SPEED 0.2

#define MAX_ROTATION_PLAYER 45
#define MIN_ROTATION_PLAYER 0
#define ROTATION_INCREMENT 0.05
#define SCREEN_PADDING 20
#define LETTER_WIDTH 20
#define BUTTON_SELECTED {255,0,0}
#define BUTTON_UNSELECTED {255,255,255}
#define MAINMENU_NUM_OPTIONS 2
#define GAMEOVER_NUM_OPTIONS 3
#define PAUSE_NUM_OPTIONS 3
#define FADE_TIME 500
#define STABLE_MAX_MS 1500
#define STABLE_CHANCE 50
#define JUMP_DELAY_MS 3000
#define MUSIC_VOLUME 64 // 128 is max

int windowWidth = 240;
int windowHeight = 320;
SDL_Point mousePos;
SDL_Point realMousePos;
bool lastKeys[SDL_NUM_SCANCODES];
bool keys[SDL_NUM_SCANCODES];
bool buttons[SDL_BUTTON_X2 + 1];
SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* robotoF;

enum class State {
    Main,
    Game,
    Paused,
    GameOver
};

void logOutputCallback(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
    std::cout << message << std::endl;
}

int random(int min, int max)
{
    return min + rand() % ((max + 1) - min);
}

int SDL_QueryTextureF(SDL_Texture* texture, Uint32* format, int* access, float* w, float* h)
{
    int wi, hi;
    int result = SDL_QueryTexture(texture, format, access, &wi, &hi);
    if (w) {
        *w = wi;
    }
    if (h) {
        *h = hi;
    }
    return result;
}

SDL_bool SDL_PointInFRect(const SDL_Point* p, const SDL_FRect* r)
{
    return ((p->x >= r->x) && (p->x < (r->x + r->w)) && (p->y >= r->y) && (p->y < (r->y + r->h))) ? SDL_TRUE : SDL_FALSE;
}

std::ostream& operator<<(std::ostream& os, SDL_FRect r)
{
    os << r.x << " " << r.y << " " << r.w << " " << r.h;
    return os;
}

std::ostream& operator<<(std::ostream& os, SDL_Rect r)
{
    SDL_FRect fR;
    fR.w = r.w;
    fR.h = r.h;
    fR.x = r.x;
    fR.y = r.y;
    os << fR;
    return os;
}

SDL_Texture* renderText(SDL_Texture* previousTexture, TTF_Font* font, SDL_Renderer* renderer, const std::string& text, SDL_Color color)
{
    if (previousTexture) {
        SDL_DestroyTexture(previousTexture);
    }
    SDL_Surface* surface;
    if (text.empty()) {
        surface = TTF_RenderUTF8_Blended(font, " ", color);
    }
    else {
        surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    }
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        return texture;
    }
    else {
        return 0;
    }
}

struct Text {
    std::string text;
    SDL_Surface* surface = 0;
    SDL_Texture* t = 0;
    SDL_FRect dstR{};
    bool autoAdjustW = false;
    bool autoAdjustH = false;
    float wMultiplier = 1;
    float hMultiplier = 1;

    ~Text()
    {
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
    }

    Text()
    {
    }

    Text(const Text& rightText)
    {
        text = rightText.text;
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
        if (rightText.surface) {
            surface = SDL_ConvertSurface(rightText.surface, rightText.surface->format, SDL_SWSURFACE);
        }
        if (rightText.t) {
            t = SDL_CreateTextureFromSurface(renderer, surface);
        }
        dstR = rightText.dstR;
        autoAdjustW = rightText.autoAdjustW;
        autoAdjustH = rightText.autoAdjustH;
        wMultiplier = rightText.wMultiplier;
        hMultiplier = rightText.hMultiplier;
    }

    Text& operator=(const Text& rightText)
    {
        text = rightText.text;
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
        if (rightText.surface) {
            surface = SDL_ConvertSurface(rightText.surface, rightText.surface->format, SDL_SWSURFACE);
        }
        if (rightText.t) {
            t = SDL_CreateTextureFromSurface(renderer, surface);
        }
        dstR = rightText.dstR;
        autoAdjustW = rightText.autoAdjustW;
        autoAdjustH = rightText.autoAdjustH;
        wMultiplier = rightText.wMultiplier;
        hMultiplier = rightText.hMultiplier;
        return *this;
    }

    void setText(SDL_Renderer* renderer, TTF_Font* font, std::string text, SDL_Color c = { 255, 255, 255 })
    {
        this->text = text;
#if 1 // NOTE: renderText
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
        if (text.empty()) {
            surface = TTF_RenderUTF8_Blended(font, " ", c);
        }
        else {
            surface = TTF_RenderUTF8_Blended(font, text.c_str(), c);
        }
        if (surface) {
            t = SDL_CreateTextureFromSurface(renderer, surface);
        }
#endif
        if (autoAdjustW) {
            SDL_QueryTextureF(t, 0, 0, &dstR.w, 0);
        }
        if (autoAdjustH) {
            SDL_QueryTextureF(t, 0, 0, 0, &dstR.h);
        }
        dstR.w *= wMultiplier;
        dstR.h *= hMultiplier;
    }

    void setText(SDL_Renderer* renderer, TTF_Font* font, int value, SDL_Color c = { 255, 255, 255 })
    {
        setText(renderer, font, std::to_string(value), c);
    }

    void draw(SDL_Renderer* renderer)
    {
        if (t) {
            SDL_RenderCopyF(renderer, t, 0, &dstR);
        }
    }
};

int SDL_RenderDrawCircle(SDL_Renderer* renderer, int x, int y, int radius)
{
    int offsetx, offsety, d;
    int status;

    offsetx = 0;
    offsety = radius;
    d = radius - 1;
    status = 0;

    while (offsety >= offsetx) {
        status += SDL_RenderDrawPoint(renderer, x + offsetx, y + offsety);
        status += SDL_RenderDrawPoint(renderer, x + offsety, y + offsetx);
        status += SDL_RenderDrawPoint(renderer, x - offsetx, y + offsety);
        status += SDL_RenderDrawPoint(renderer, x - offsety, y + offsetx);
        status += SDL_RenderDrawPoint(renderer, x + offsetx, y - offsety);
        status += SDL_RenderDrawPoint(renderer, x + offsety, y - offsetx);
        status += SDL_RenderDrawPoint(renderer, x - offsetx, y - offsety);
        status += SDL_RenderDrawPoint(renderer, x - offsety, y - offsetx);

        if (status < 0) {
            status = -1;
            break;
        }

        if (d >= 2 * offsetx) {
            d -= 2 * offsetx + 1;
            offsetx += 1;
        }
        else if (d < 2 * (radius - offsety)) {
            d += 2 * offsety - 1;
            offsety -= 1;
        }
        else {
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }

    return status;
}

int SDL_RenderFillCircle(SDL_Renderer* renderer, int x, int y, int radius)
{
    int offsetx, offsety, d;
    int status;

    offsetx = 0;
    offsety = radius;
    d = radius - 1;
    status = 0;

    while (offsety >= offsetx) {

        status += SDL_RenderDrawLine(renderer, x - offsety, y + offsetx,
            x + offsety, y + offsetx);
        status += SDL_RenderDrawLine(renderer, x - offsetx, y + offsety,
            x + offsetx, y + offsety);
        status += SDL_RenderDrawLine(renderer, x - offsetx, y - offsety,
            x + offsetx, y - offsety);
        status += SDL_RenderDrawLine(renderer, x - offsety, y - offsetx,
            x + offsety, y - offsetx);

        if (status < 0) {
            status = -1;
            break;
        }

        if (d >= 2 * offsetx) {
            d -= 2 * offsetx + 1;
            offsetx += 1;
        }
        else if (d < 2 * (radius - offsety)) {
            d += 2 * offsety - 1;
            offsety -= 1;
        }
        else {
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }

    return status;
}

struct Clock {
    Uint64 start = SDL_GetPerformanceCounter();

    float getElapsedTime()
    {
        Uint64 stop = SDL_GetPerformanceCounter();
        float secondsElapsed = (stop - start) / (float)SDL_GetPerformanceFrequency();
        return secondsElapsed * 1000;
    }

    float restart()
    {
        Uint64 stop = SDL_GetPerformanceCounter();
        float secondsElapsed = (stop - start) / (float)SDL_GetPerformanceFrequency();
        start = SDL_GetPerformanceCounter();
        return secondsElapsed * 1000;
    }
};

SDL_bool SDL_FRectEmpty(const SDL_FRect* r)
{
    return ((!r) || (r->w <= 0) || (r->h <= 0)) ? SDL_TRUE : SDL_FALSE;
}

SDL_bool SDL_IntersectFRect(const SDL_FRect* A, const SDL_FRect* B, SDL_FRect* result)
{
    int Amin, Amax, Bmin, Bmax;

    if (!A) {
        SDL_InvalidParamError("A");
        return SDL_FALSE;
    }

    if (!B) {
        SDL_InvalidParamError("B");
        return SDL_FALSE;
    }

    if (!result) {
        SDL_InvalidParamError("result");
        return SDL_FALSE;
    }

    /* Special cases for empty rects */
    if (SDL_FRectEmpty(A) || SDL_FRectEmpty(B)) {
        result->w = 0;
        result->h = 0;
        return SDL_FALSE;
    }

    /* Horizontal intersection */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin > Amin)
        Amin = Bmin;
    result->x = Amin;
    if (Bmax < Amax)
        Amax = Bmax;
    result->w = Amax - Amin;

    /* Vertical intersection */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin > Amin)
        Amin = Bmin;
    result->y = Amin;
    if (Bmax < Amax)
        Amax = Bmax;
    result->h = Amax - Amin;

    return (SDL_FRectEmpty(result) == SDL_TRUE) ? SDL_FALSE : SDL_TRUE;
}

SDL_bool SDL_HasIntersectionF(const SDL_FRect* A, const SDL_FRect* B)
{
    float Amin, Amax, Bmin, Bmax;

    if (!A) {
        SDL_InvalidParamError("A");
        return SDL_FALSE;
    }

    if (!B) {
        SDL_InvalidParamError("B");
        return SDL_FALSE;
    }

    /* Special cases for empty rects */
    if (SDL_FRectEmpty(A) || SDL_FRectEmpty(B)) {
        return SDL_FALSE;
    }

    /* Horizontal intersection */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin > Amin)
        Amin = Bmin;
    if (Bmax < Amax)
        Amax = Bmax;
    if (Amax <= Amin)
        return SDL_FALSE;

    /* Vertical intersection */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin > Amin)
        Amin = Bmin;
    if (Bmax < Amax)
        Amax = Bmax;
    if (Amax <= Amin)
        return SDL_FALSE;

    return SDL_TRUE;
}

int eventWatch(void* userdata, SDL_Event* event)
{
    // WARNING: Be very careful of what you do in the function, as it may run in a different thread
    if (event->type == SDL_APP_TERMINATING || event->type == SDL_APP_WILLENTERBACKGROUND) {
    }
    return 0;
}

float clamp(float n, float lower, float upper)
{
    return std::max(lower, std::min(n, upper));
}

struct Entity {
    SDL_FRect r{};
    SDL_Rect srcR{};
    int dx = 0;
    int dy = 0;
};

struct StabilityLevel {
    float speed;
    float maxRotation;
};



void UpdatePlayerPosition(const SDL_FRect triangleR, SDL_FRect& playerR, SDL_FPoint& playerRotPoint, int stableLevel) { 
    playerR.x = triangleR.x + 5;
    playerR.y = triangleR.y + 30;

    if (stableLevel >= 3) {
        playerRotPoint.x = playerR.x;
        playerRotPoint.y = playerR.y;
    }
    else{
        playerRotPoint.x = playerR.w - 5;
        playerRotPoint.y = 15;
    }
}

void UpdateStability(float& rotation, bool& rotationDir, const StabilityLevel levels[], int& levelIndex) {
    StabilityLevel currentLevel = levels[levelIndex];

    rotation += rotationDir ? currentLevel.speed : -currentLevel.speed;
    if (levelIndex == 3) {
        rotationDir = true;
    }
    if (rotation > currentLevel.maxRotation) {
        if (levelIndex == 3) {
            levelIndex = 4;
        }
        else {
            rotationDir = false;
        }
    }
    else if (rotation < 0.0f) {
        rotationDir = true;
    }
}

void UpdateScore(Text& scoreText, SDL_Renderer* renderer, TTF_Font* font, float scoreCounter) {
    int score = scoreCounter * SCORE_MULTIPLIER;
    scoreText.setText(renderer, font, "Score: " + std::to_string(score));
    scoreText.dstR.w = 50 + 20.0f * std::to_string(score).length();
    scoreText.dstR.x = windowWidth / 2.0f - scoreText.dstR.w / 2.0f;
}

enum class MenuOption {
    Play = 1,
    Resume,
    Restart,
    Controls,
    Main,
    Credits,
    Quit
};

struct MenuButton {
    MenuOption menuType;
    Text buttonText;
    std::string label;
    bool selected;

    void calculateButtonPosition(
        const int index,
        const int numButtons,
        const float width,
        const float height,
        float paddingVertical)
    {

        buttonText.dstR.x = width / 2.0f - buttonText.dstR.w / 2.0f;
        buttonText.dstR.y = height / 2.0f - numButtons * (buttonText.dstR.h / 2.0f + paddingVertical) + (index + 0.5f) * (buttonText.dstR.h + paddingVertical);
    }
};

void HandleMenuOption(MenuOption option, State& gameState, bool& gameRunning)
{
    switch (option) {
    case MenuOption::Play:
        gameState = State::Game;
        break;
    case MenuOption::Credits:
        //gameState = State::Credits;
        break;
    case MenuOption::Resume:
        gameState = State::Game;
        break;
    case MenuOption::Controls:
        //gameState = State::Controls;
        break;
    case MenuOption::Main:
        gameState = State::Main;
        break;
    case MenuOption::Quit:
        gameRunning = false;
        break;
    }
}

void MenuInit(SDL_FRect& container,
    Text& titleText,
    std::string titleString,
    MenuButton options[],
    const int numOptions,
    const int buttonPadding,
    const std::string labels[],
    const MenuOption menuTypes[])
{
    // Setup background and title
    container.w = windowWidth;
    container.h = windowHeight;
    container.x = windowWidth / 2.0f - container.w / 2.0f;
    container.y = 0;

    titleText.dstR.w = container.w - SCREEN_PADDING * 2;
    titleText.dstR.h = 30;
    titleText.dstR.x = windowWidth / 2.0f - titleText.dstR.w / 2.0f;
    titleText.dstR.y = SCREEN_PADDING;
    titleText.setText(renderer, robotoF, titleString, { 255, 0, 0 });

    // Setup buttons
    for (int i = 0; i < numOptions; ++i) {
        options[i].label = labels[i];
        options[i].menuType = menuTypes[i];
        options[i].selected = false;
        options[i].buttonText.dstR.w = strlen(options[i].label.c_str()) * LETTER_WIDTH;
        options[i].buttonText.dstR.h = 25;
        options[i].calculateButtonPosition(i, numOptions, windowWidth, windowHeight, buttonPadding);
        options[i].buttonText.dstR.y += titleText.dstR.h;
        options[i].buttonText.setText(renderer, robotoF, options[i].label, BUTTON_UNSELECTED);
    }
}

void RenderMenu(SDL_FRect& container,
    Text& titleText,
    MenuButton options[],
    const int numOptions)
{
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 200);
    SDL_RenderFillRectF(renderer, &container);
    titleText.draw(renderer);

    for (int i = 0; i < numOptions; ++i) {
        if (options[i].selected) {
            options[i].buttonText.setText(renderer, robotoF, options[i].label, BUTTON_SELECTED);
        }
        else {
            options[i].buttonText.setText(renderer, robotoF, options[i].label, BUTTON_UNSELECTED);
        }

        options[i].buttonText.draw(renderer);
    }
}

void GameOverInit(SDL_FRect& container,
    Text& titleText,
    Text& scoreText,
    MenuButton options[],
    const int numOptions,
    const int buttonPadding,
    const std::string labels[],
    const MenuOption menuTypes[])
{
    // Setup background and title
    container.w = windowWidth;
    container.h = windowHeight;
    container.x = windowWidth / 2.0f - container.w / 2.0f;
    container.y = 0;

    titleText.dstR.w = container.w - SCREEN_PADDING * 2;
    titleText.dstR.h = 30;
    titleText.dstR.x = windowWidth / 2.0f - titleText.dstR.w / 2.0f;
    titleText.dstR.y = SCREEN_PADDING;
    titleText.setText(renderer, robotoF, "GAME OVER!", { 255, 0, 0 });

    scoreText.dstR.w = container.w * 0.75f;
    scoreText.dstR.h = 25;
    scoreText.dstR.x = windowWidth / 2.0f - titleText.dstR.w / 2.0f;
    scoreText.dstR.y = titleText.dstR.y + titleText.dstR.h + SCREEN_PADDING;

    // Setup buttons
    for (int i = 0; i < numOptions; ++i) {
        options[i].label = labels[i];
        options[i].menuType = menuTypes[i];
        options[i].selected = false;
        options[i].buttonText.dstR.w = strlen(options[i].label.c_str()) * LETTER_WIDTH;
        options[i].buttonText.dstR.h = 25;
        options[i].calculateButtonPosition(i, numOptions, windowWidth, windowHeight, buttonPadding);
        options[i].buttonText.dstR.y += scoreText.dstR.h;
        options[i].buttonText.setText(renderer, robotoF, options[i].label, BUTTON_UNSELECTED);
    }
}

void RenderGameOver(SDL_FRect& container,
    Text& titleText,
    Text& scoreText,
    MenuButton options[],
    const int numOptions)
{
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 50);
    SDL_RenderFillRectF(renderer, &container);
    titleText.draw(renderer);
    scoreText.draw(renderer);

    for (int i = 0; i < numOptions; ++i) {
        if (options[i].selected) {
            options[i].buttonText.setText(renderer, robotoF, options[i].label, BUTTON_SELECTED);
        }
        else {
            options[i].buttonText.setText(renderer, robotoF, options[i].label, BUTTON_UNSELECTED);
        }

        options[i].buttonText.draw(renderer);
    }
}

int main(int argc, char* argv[])
{
    std::srand(std::time(0));
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
    SDL_LogSetOutputFunction(logOutputCallback, 0);
    SDL_Init(SDL_INIT_EVERYTHING);
	Mix_OpenAudio(44100, AUDIO_S16, 2, 4096);
    TTF_Init();
    SDL_GetMouseState(&mousePos.x, &mousePos.y);
    window = SDL_CreateWindow("FarmRodeo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    robotoF = TTF_OpenFont("res/roboto.ttf", 72);
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_RenderSetScale(renderer, w / (float)windowWidth, h / (float)windowHeight);
    SDL_AddEventWatch(eventWatch, 0);
    bool running = true;
    SDL_Texture* horseT = IMG_LoadTexture(renderer, "res/horse.png");
    SDL_Texture* horse2T = IMG_LoadTexture(renderer, "res/horse2.png");
    SDL_Texture* horse3T = IMG_LoadTexture(renderer, "res/horse3.png");
    SDL_Texture* bgT = IMG_LoadTexture(renderer, "res/bg.png");
    SDL_Texture* triangleT = IMG_LoadTexture(renderer, "res/triangle.png");
    SDL_Texture* playerT = IMG_LoadTexture(renderer, "res/player.png");
    SDL_Texture* obstackleT = IMG_LoadTexture(renderer, "res/obstackle.png");
    Entity player;
    player.r.w = 64;
    player.r.h = 64;
    player.r.x = windowWidth / 2 - player.r.w / 2;
    player.r.y = windowHeight / 2 - player.r.h / 2;
    player.srcR.w = 410 / 5;
    player.srcR.h = 66;
    player.srcR.x = 0;
    player.srcR.y = 0;
    Entity secondHorse;
    secondHorse.r.w = 64;
    secondHorse.r.h = 64;
    secondHorse.r.x = player.r.x;
    secondHorse.r.y = player.r.y - secondHorse.r.h - 50;
    Entity thirdHorse;
    thirdHorse.r.w = 64;
    thirdHorse.r.h = 64;
    thirdHorse.r.x = player.r.x;
    thirdHorse.r.y = player.r.y + player.r.h + 50;
    SDL_FRect bgR;
    bgR.w = windowWidth;
    bgR.h = windowHeight;
    bgR.x = 0;
    bgR.y = 0;
    SDL_FRect bg2R;
    bg2R.w = windowWidth;
    bg2R.h = windowHeight;
    bg2R.x = windowWidth;
    bg2R.y = 0;
    SDL_FRect triangleR;
    triangleR.w = 32;
    triangleR.h = 32;
    triangleR.x = player.r.x + player.r.w / 2 - triangleR.w / 2;
    triangleR.y = player.r.y - triangleR.h;
    SDL_FRect playerSprite;
    playerSprite.w = 20;
    playerSprite.h = 20;
    SDL_FPoint playerRotPoint;
    bool rotationDir = true;
    float rotation = 0.0f;
    bool isPlaying = true;
    int selectedHorse = 0;
    int currentHorse = selectedHorse;
    Mix_Chunk* sndJump = Mix_LoadWAV("res/jump.wav");
    Mix_Chunk* sndClick = Mix_LoadWAV("res/click.wav");
    Mix_Chunk* sndDeath = Mix_LoadWAV("res/death.wav");
    Mix_Chunk* sndWobble = Mix_LoadWAV("res/wobble.wav");
    int wobbleChannel = -1;
    Mix_Chunk* sndPause = Mix_LoadWAV("res/pause.wav");
	Mix_Music* musicGame = Mix_LoadMUS("res/music.ogg");
    Mix_Music* musicMainIntro = Mix_LoadMUS("res/menu0.ogg");
    Mix_Music* musicMainLoop = Mix_LoadMUS("res/menu1.ogg");
	Mix_Music* musicGameover = Mix_LoadMUS("res/gameover.xm");
	Mix_VolumeMusic(MUSIC_VOLUME);
	Mix_PlayMusic(musicMainIntro, 0);
    bool introPlayed = true;
    Text scoreText;
    float scoreCounter = 0;
    scoreText.dstR.y = 5;
    scoreText.dstR.h = 20;
    UpdateScore(scoreText, renderer, robotoF, scoreCounter);
    const StabilityLevel stableLevels[] =
    {
        {0.001f, 1.0f},
        {0.03f, 20.0f},
        {0.06f, 60.0f},
        {0.075f, 100.0f}
    };
    int stableLevel = 0;
    Clock stableCheck;

    State state = State::Main;
    MenuButton mainOptions[MAINMENU_NUM_OPTIONS];
    const std::string mainLabels[MAINMENU_NUM_OPTIONS] = {
        "Play",
        "Exit"
    };
    const MenuOption mainMenuTypes[MAINMENU_NUM_OPTIONS] = {
        MenuOption::Play,
        MenuOption::Quit
    };
    Text mainTitleText;
    SDL_FRect mainContainer;
    MenuInit(mainContainer, mainTitleText, "Farm Rodeo", mainOptions, MAINMENU_NUM_OPTIONS, 10, mainLabels, mainMenuTypes);

    MenuButton pauseOptions[PAUSE_NUM_OPTIONS];
    const std::string pauseLabels[PAUSE_NUM_OPTIONS] = {
        "Resume",
        "Main Menu",
        "Quit"
    };
    const MenuOption pauseMenuTypes[PAUSE_NUM_OPTIONS] = {
        MenuOption::Resume,
        MenuOption::Main,
        MenuOption::Quit
    };
    Text pauseTitleText;
    SDL_FRect pauseContainer;
    bool pauseKeyHeld = false;
    MenuInit(pauseContainer, pauseTitleText, "Paused", pauseOptions, PAUSE_NUM_OPTIONS, 10, pauseLabels, pauseMenuTypes);

    MenuButton gOverOptions[GAMEOVER_NUM_OPTIONS];
    const std::string gOverLabels[GAMEOVER_NUM_OPTIONS] = {
        "Play Again",
        "Main Menu",
        "Exit"
    };
    const MenuOption gOverTypes[GAMEOVER_NUM_OPTIONS] = {
        MenuOption::Play,
        MenuOption::Main,
        MenuOption::Quit
    };
    Text gOverTitleText;
    Text gOverScoreText;
    SDL_FRect gOverContainer;
    std::vector<SDL_FRect> obstackles;
    bool jumping = false;
    GameOverInit(gOverContainer, gOverTitleText, gOverScoreText, gOverOptions, GAMEOVER_NUM_OPTIONS, 10, gOverLabels, gOverTypes);
    Text jumpText;
    jumpText.setText(renderer, robotoF, "Can Jump: Yes");
    jumpText.dstR.w = jumpText.text.length() * LETTER_WIDTH * 0.5f;
    jumpText.dstR.h = scoreText.dstR.h;
    jumpText.dstR.x = 5;
    jumpText.dstR.y = windowHeight - jumpText.dstR.h - 5;
    bool canJump = true;

    Clock jumpClock;
    Clock globalClock;
    Clock playerAnimationClock;
    Clock obstacklesClock;
    while (running) {
        if (!Mix_PlayingMusic() && state == State::Main && introPlayed) {
            introPlayed = false;
            Mix_PlayMusic(musicMainLoop, -1);
        };
        float deltaTime = globalClock.restart();
        SDL_Event event;
        std::copy(std::begin(keys), std::end(keys), std::begin(lastKeys));
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_RenderSetScale(renderer, event.window.data1 / (float)windowWidth, event.window.data2 / (float)windowHeight);
            }
            if (event.type == SDL_KEYDOWN) {
                if (state == State::Game) {
                    if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                        if (!pauseKeyHeld) {
                            pauseKeyHeld = true;
                            state = State::Paused;
                            Mix_PauseMusic();
                        }
                    }
                    else {
                        keys[event.key.keysym.scancode] = true;
                        if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                            bool wasPlaying = isPlaying;
                            if (isPlaying && canJump) {
                                isPlaying = false;
                                Mix_PlayChannel(-1, sndPause, 0);
                            }
                            else if (!isPlaying) {
                                isPlaying = true;
                                Mix_PlayChannel(-1, sndPause, 0);
                            }
                            
                            if (isPlaying && !wasPlaying) {
                                if (currentHorse != selectedHorse) {
                                    canJump = false;
                                    jumpClock.restart();
                                    stableLevel = 0;
                                    stableCheck.restart();
                                    rotation = 0.0f;
                                }
                            }
                            else {
                                currentHorse = selectedHorse;
                            }
                        }
                        if (!isPlaying) {
                            if (event.key.keysym.scancode == SDL_SCANCODE_W) {
                                ++selectedHorse;
                                Mix_PlayChannel(-1, sndJump, 0);
                                if (selectedHorse > 2) {
                                    selectedHorse = 0;
                                }
                            }
                            if (event.key.keysym.scancode == SDL_SCANCODE_S) {
                                --selectedHorse;
                                Mix_PlayChannel(-1, sndJump, 0);
                                if (selectedHorse < 0) {
                                    selectedHorse = 2;
                                }
                            }
                        }
                    }
                }
                else if (state == State::Paused) {
                    if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                        if (!pauseKeyHeld) {
                            pauseKeyHeld = true;
                            state = State::Game;
                            Mix_ResumeMusic();
                        }
                    }
                    else {
                        if (event.key.keysym.scancode == SDL_SCANCODE_W) {
                            jumping = true;
                        }
                    }
                }
            }
            if (event.type == SDL_KEYUP) {
                if (state == State::Game || state == State::Paused) {
                    if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                        if (pauseKeyHeld) {
                            pauseKeyHeld = false;
                        }
                    }
                    else {
                        keys[event.key.keysym.scancode] = false;
                    }
                }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                buttons[event.button.button] = true;
                bool buttonUse = false;
                if (state == State::Main) {
                    for (int i = 0; i < MAINMENU_NUM_OPTIONS; ++i) {
                        if (SDL_PointInFRect(&mousePos, &mainOptions[i].buttonText.dstR)) {
                            HandleMenuOption(mainOptions[i].menuType, state, running);
                            buttonUse = true;
                        }
                    }
                }
                else if (state == State::GameOver) {
                    for (int i = 0; i < GAMEOVER_NUM_OPTIONS; ++i) {
                        if (SDL_PointInFRect(&mousePos, &gOverOptions[i].buttonText.dstR)) {
                            HandleMenuOption(gOverOptions[i].menuType, state, running);
                            buttonUse = true;
                        }
                    }
                }
                else if (state == State::Paused) {
                    for (int i = 0; i < PAUSE_NUM_OPTIONS; ++i) {
                        if (SDL_PointInFRect(&mousePos, &pauseOptions[i].buttonText.dstR)) {
                            HandleMenuOption(pauseOptions[i].menuType, state, running);
                            buttonUse = true;
                        }
                    }
                }
                if (buttonUse) {
                    Mix_PlayChannel(-1, sndClick, 0);
                    if (state == State::Game) {
                        if (Mix_PausedMusic()) {
                            Mix_ResumeMusic();
                        }
                        else {
                            Mix_FadeOutMusic(FADE_TIME);
                            Mix_FadeInMusic(musicGame, -1, FADE_TIME);
                        }
                    }
                    else if (state == State::Main) {
                        if (Mix_PausedMusic()) {
                            Mix_HaltMusic();
                        }
                        else {
                            Mix_FadeOutMusic(FADE_TIME);
                        }
                        Mix_FadeInMusic(musicMainIntro, 1, FADE_TIME);
                    }
                    else if (state == State::Paused) {
                        if (Mix_PlayingMusic()) {
                            Mix_PauseMusic();
                        }
                    }
                }

            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                buttons[event.button.button] = false;
            }
            if (event.type == SDL_MOUSEMOTION) {
                float scaleX, scaleY;
                SDL_RenderGetScale(renderer, &scaleX, &scaleY);
                mousePos.x = event.motion.x / scaleX;
                mousePos.y = event.motion.y / scaleY;
                realMousePos.x = event.motion.x;
                realMousePos.y = event.motion.y;

                if (state == State::Main) {
                    for (int i = 0; i < MAINMENU_NUM_OPTIONS; ++i) {
                        if (SDL_PointInFRect(&mousePos, &mainOptions[i].buttonText.dstR)) {
                            mainOptions[i].selected = true;
                        }
                        else {
                            mainOptions[i].selected = false;
                        }
                    }
                }
                else if (state == State::GameOver) {
                    for (int i = 0; i < GAMEOVER_NUM_OPTIONS; ++i) {
                        if (SDL_PointInFRect(&mousePos, &gOverOptions[i].buttonText.dstR)) {
                            gOverOptions[i].selected = true;
                        }
                        else {
                            gOverOptions[i].selected = false;
                        }
                    }
                }
                else if (state == State::Paused) {
                    for (int i = 0; i < PAUSE_NUM_OPTIONS; ++i) {
                        if (SDL_PointInFRect(&mousePos, &pauseOptions[i].buttonText.dstR)) {
                            pauseOptions[i].selected = true;
                        }
                        else {
                            pauseOptions[i].selected = false;
                        }
                    }
                }
            }
        }
        if (state == State::Game) {
            if (jumpClock.getElapsedTime() > JUMP_DELAY_MS && !canJump) {
                jumpText.setText(renderer, robotoF, "Can Jump: Yes");
                canJump = true;
            }
            jumpText.setText(renderer, robotoF, canJump ? "Can Jump: Yes" : "Can Jump: No");
            if (wobbleChannel == -1) {
                wobbleChannel = Mix_PlayChannel(-1, sndWobble, -1);
            }
            if (keys[SDL_SCANCODE_M] && !lastKeys[SDL_SCANCODE_M]) {
                if (Mix_PausedMusic()) {
                    Mix_ResumeMusic();
                }
                else {
                    Mix_PauseMusic();
                }
            }
            if (isPlaying) {
                player.dx = 0;
                player.dy = 0;
                if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) {
                    player.dx = -1;
                }
                else if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) {
                    player.dx = 1;
                }
                if (selectedHorse == 0) {
                    player.r.x += player.dx * deltaTime * PLAYER_SPEED;
                    player.r.y += player.dy * deltaTime * PLAYER_SPEED;
                }
                else if (selectedHorse == 1) {
                    secondHorse.r.x += player.dx * deltaTime * PLAYER_SPEED;
                    secondHorse.r.y += player.dy * deltaTime * PLAYER_SPEED;
                }
                else if (selectedHorse == 2) {
                    thirdHorse.r.x += player.dx * deltaTime * PLAYER_SPEED;
                    thirdHorse.r.y += player.dy * deltaTime * PLAYER_SPEED;
                }
                player.r.x = clamp(player.r.x, 0, windowWidth - player.r.w);
                secondHorse.r.x = clamp(secondHorse.r.x, 0, windowWidth - secondHorse.r.w);
                thirdHorse.r.x = clamp(thirdHorse.r.x, 0, windowWidth - thirdHorse.r.w);
                bgR.x += -deltaTime * GAME_SPEED;
                bg2R.x += -deltaTime * GAME_SPEED;
                if (bgR.x + bgR.w < 0) {
                    bgR.x = bg2R.x + bg2R.w;
                }
                if (bg2R.x + bg2R.w < 0) {
                    bg2R.x = bgR.x + bgR.w;
                }
                if (playerAnimationClock.getElapsedTime() > PLAYER_FRAME_MS) {
                    player.srcR.x += player.srcR.w;
                    if (player.srcR.x >= 410) {
                        player.srcR.x = 0;
                    }
                    playerAnimationClock.restart();
                }

                scoreCounter += deltaTime / 1000;
                UpdateScore(scoreText, renderer, robotoF, scoreCounter);

                if (stableCheck.getElapsedTime() > STABLE_MAX_MS && stableLevel < 3) {
                    if (random(0, 100) < STABLE_CHANCE) {
                        stableLevel++;
                    }
                    stableCheck.restart();
                }
                UpdateStability(rotation, rotationDir, stableLevels, stableLevel);
                switch (stableLevel) {
                case 0:
                    Mix_Volume(wobbleChannel, 0);
                    break;
                case 1:
                    Mix_Volume(wobbleChannel, 32);
                    break;
                case 2:
                    Mix_Volume(wobbleChannel, 64);
                    break;
                case 3:
                    Mix_Volume(wobbleChannel, 96);
                    break;
                case 4:
                    Mix_Volume(wobbleChannel, 0);
                    break;
                }
                if (stableLevel == 4) {
                    state = State::GameOver;
                    Mix_PlayChannel(-1, sndDeath, 0);
                    Mix_FadeOutMusic(FADE_TIME);
                    Mix_FadeInMusic(musicGameover, 1, FADE_TIME);
                }
            }
            else {  // !isPlaying
                Mix_Volume(wobbleChannel, 0);
            }
            if (selectedHorse == 0) {
                triangleR.x = player.r.x + player.r.w / 2 - triangleR.w / 2;
                triangleR.y = player.r.y - triangleR.h + 20;
            }
            else if (selectedHorse == 1) {
                triangleR.x = secondHorse.r.x + secondHorse.r.w / 2 - triangleR.w / 2;
                triangleR.y = secondHorse.r.y - triangleR.h + 20;
            }
            else if (selectedHorse == 2) {
                triangleR.x = thirdHorse.r.x + thirdHorse.r.w / 2 - triangleR.w / 2;
                triangleR.y = thirdHorse.r.y - triangleR.h + 20;
            }
        }
        else if (state == State::Paused) {
            if (wobbleChannel != -1) {
                Mix_Volume(wobbleChannel, 0);
            }
        }
        
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
        SDL_RenderClear(renderer);

        if (state == State::Main) {
            RenderMenu(mainContainer, mainTitleText, mainOptions, MAINMENU_NUM_OPTIONS);
        }
        else if (state == State::Paused) {
            RenderMenu(pauseContainer, pauseTitleText, pauseOptions, PAUSE_NUM_OPTIONS);
        }
        else if (state == State::GameOver) {
            gOverScoreText.setText(renderer, robotoF, scoreText.text, { 0, 191, 255 });
            RenderGameOver(gOverContainer, gOverTitleText, gOverScoreText, gOverOptions, GAMEOVER_NUM_OPTIONS);

            scoreCounter = 0.0f;
            stableCheck.restart();
            stableLevel = 0;
            rotation = 0.0f;
            selectedHorse = 0;
        }
        else if (state == State::Game) {
            UpdatePlayerPosition(triangleR, playerSprite, playerRotPoint, stableLevel);

            SDL_RenderCopyF(renderer, bgT, 0, &bgR);
            SDL_RenderCopyExF(renderer, bgT, 0, &bg2R, 0, 0, SDL_FLIP_HORIZONTAL);
            SDL_RenderCopyExF(renderer, horseT, &player.srcR, &player.r, 0, 0, SDL_FLIP_HORIZONTAL);
            SDL_RenderCopyExF(renderer, horse2T, &player.srcR, &secondHorse.r, 0, 0, SDL_FLIP_HORIZONTAL);
            SDL_RenderCopyExF(renderer, horse3T, &player.srcR, &thirdHorse.r, 0, 0, SDL_FLIP_HORIZONTAL);
            SDL_RenderCopyExF(renderer, playerT, 0, &playerSprite, rotation, &playerRotPoint, SDL_FLIP_HORIZONTAL);
            SDL_RenderCopyF(renderer, triangleT, 0, &triangleR);
            for (int i = 0; i < obstackles.size(); ++i) {
                SDL_RenderCopyF(renderer, obstackleT, 0, &obstackles[i]);
            }
            scoreText.draw(renderer);
            jumpText.draw(renderer);
        }

        SDL_RenderPresent(renderer);
    }
    Mix_FreeChunk(sndJump);
    Mix_FreeChunk(sndClick);
    Mix_FreeChunk(sndDeath);
    Mix_FreeChunk(sndWobble);
    Mix_FreeChunk(sndPause);
	Mix_FreeMusic(musicGame);
	Mix_FreeMusic(musicMainIntro);
	Mix_FreeMusic(musicMainLoop);
	Mix_FreeMusic(musicGameover);
	Mix_CloseAudio();
    // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
    return 0;
}
