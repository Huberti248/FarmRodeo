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

#define MAX_ROTATION_PLAYER 45
#define MIN_ROTATION_PLAYER 0
#define ROTATION_INCREMENT 0.05

int windowWidth = 240;
int windowHeight = 320;
SDL_Point mousePos;
SDL_Point realMousePos;
bool lastKeys[SDL_NUM_SCANCODES];
bool keys[SDL_NUM_SCANCODES];
bool buttons[SDL_BUTTON_X2 + 1];
SDL_Window* window;
SDL_Renderer* renderer;

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

void UpdateStability(float& rotation, bool& rotationDir, const StabilityLevel levels[], int levelIndex) {
    int index = levelIndex > 3 ? 3 : levelIndex;
    StabilityLevel currentLevel = levels[index];

    rotation += rotationDir ? currentLevel.speed : -currentLevel.speed;
    if (rotation > currentLevel.maxRotation) {
        rotationDir = false;
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
    TTF_Font* robotoF = TTF_OpenFont("res/roboto.ttf", 72);
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
	Mix_Music *music = Mix_LoadMUS("res/music.ogg");
	Mix_PlayMusic(music, -1);
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

    Clock globalClock;
    Clock playerAnimationClock;
    while (running) {
        float deltaTime = globalClock.restart();
        SDL_Event event;
		std::copy(std::begin(keys), std::end(keys), std::begin(lastKeys));
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
				(event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_RenderSetScale(renderer, event.window.data1 / (float)windowWidth, event.window.data2 / (float)windowHeight);
            }
            if (event.type == SDL_KEYDOWN) {
                keys[event.key.keysym.scancode] = true;
				if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
					isPlaying = !isPlaying;
                    if (isPlaying) {
                        if (currentHorse != selectedHorse) {
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
                        if (selectedHorse > 2) {
                            selectedHorse = 0;
                        }
                    }
                    if (event.key.keysym.scancode == SDL_SCANCODE_S) {
                        --selectedHorse;
                        if (selectedHorse < 0) {
                            selectedHorse = 2;
                        }
                    }
                }
            }
            if (event.type == SDL_KEYUP) {
                keys[event.key.keysym.scancode] = false;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                buttons[event.button.button] = true;
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
            }
        }
		if (keys[SDL_SCANCODE_M] && !lastKeys[SDL_SCANCODE_M]) {
			if (Mix_PausedMusic()) {
				Mix_ResumeMusic();
			} else {
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
			if (selectedHorse==0) {
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

            if (stableCheck.getElapsedTime() > 2000 && stableLevel <= 3) {
                if (random(0, 100) < 40) {
                    stableLevel++;
                    if (stableLevel == 3) {
                        printf("GameOver\n");
                    }
                }
                printf("Level %d\n", stableLevel);
                stableCheck.restart();
            }
            UpdateStability(rotation, rotationDir, stableLevels, stableLevel);
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
        
        UpdatePlayerPosition(triangleR, playerSprite, playerRotPoint, stableLevel);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
        SDL_RenderClear(renderer);

        SDL_RenderCopyF(renderer, bgT, 0, &bgR);
        SDL_RenderCopyExF(renderer, bgT, 0, &bg2R, 0, 0, SDL_FLIP_HORIZONTAL);
        SDL_RenderCopyExF(renderer, horseT, &player.srcR, &player.r, 0, 0, SDL_FLIP_HORIZONTAL);
        SDL_RenderCopyExF(renderer, horse2T, &player.srcR, &secondHorse.r, 0, 0, SDL_FLIP_HORIZONTAL);
        SDL_RenderCopyExF(renderer, horse3T, &player.srcR, &thirdHorse.r, 0, 0, SDL_FLIP_HORIZONTAL);
        SDL_RenderCopyExF(renderer, playerT, 0, &playerSprite, rotation, &playerRotPoint, SDL_FLIP_HORIZONTAL);
        SDL_RenderCopyF(renderer, triangleT, 0, &triangleR);
        scoreText.draw(renderer);

        SDL_RenderPresent(renderer);
    }
	Mix_FreeMusic(music);
	Mix_CloseAudio();
    // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
    return 0;
}
