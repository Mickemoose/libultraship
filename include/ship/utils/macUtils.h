#pragma once

#include <SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
void toggleNativeMacOSFullscreen(SDL_Window* window);
bool isNativeMacOSFullscreenActive(SDL_Window* window);
bool isNativeMacOSFullscreenTransitioning(void);

// Apple's recommended pattern for glitchless rendering across resize / native
// fullscreen transitions: opt the layer into transactional presentation, then
// at frame-end commit the command buffer, waitUntilScheduled, and call
// [drawable present] manually instead of -presentDrawable:. macUtils owns
// these wrappers so gfx_metal.cpp doesn't have to depend on Cocoa headers.
// Args are CAMetalLayer* / id<CAMetalDrawable> as opaque void*.
void macSetMetalLayerPresentsWithTransaction(void* metalLayer, bool enable);
void macPresentMetalDrawable(void* metalDrawable);
#ifdef __cplusplus
}
#endif
