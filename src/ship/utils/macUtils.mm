// macUtils.mm
#ifdef __APPLE__
#import "ship/utils/macUtils.h"
#import <SDL_syswm.h>
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <atomic>

// Native fullscreen on macOS is animated and asynchronous: [NSWindow
// toggleFullScreen:] returns immediately and AppKit reparents the
// contentView between an in-window and a fullscreen NSWindow over ~0.5s.
// During that window the SDL CAMetalLayer (and any MTLTextures /
// CAImageQueue frames in flight) can be deallocated mid-composite. The
// CA::Context dispatch queue then UAFs in objc_msgSend / objc_retain on
// stale layer delegates and ImGui's MTLTexture handles (issue #83).
//
// Apple's recommendation (and Mozilla / Electron's mitigation) is to
// (a) serialize transitions and (b) avoid driving new GPU work into the
// layer while the transition is in progress. We do both:
//   - Defer the toggle to the next main-runloop tick via dispatch_async,
//     so the in-flight ImGui frame finishes presenting first.
//   - Track transition state via NSWindow's will/did enter/exit-fullscreen
//     notifications and expose it to the renderer, which gates frame
//     submission until the transition is complete.

namespace {
    std::atomic<bool> gInTransition{ false };
    bool gObserversInstalled = false;

    void installFullscreenObserversOnce(NSWindow* nswindow) {
        if (gObserversInstalled) {
            return;
        }
        gObserversInstalled = true;
        NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
        NSOperationQueue* mainQueue = [NSOperationQueue mainQueue];
        // Will-enter / will-exit fire synchronously inside [toggleFullScreen:],
        // so they reliably mark the start of the transition. Did-enter / did-exit
        // fire after the AppKit animation completes (~0.5s), marking the end.
        [nc addObserverForName:NSWindowWillEnterFullScreenNotification
                        object:nswindow
                         queue:mainQueue
                    usingBlock:^(NSNotification*) { gInTransition.store(true); }];
        [nc addObserverForName:NSWindowWillExitFullScreenNotification
                        object:nswindow
                         queue:mainQueue
                    usingBlock:^(NSNotification*) { gInTransition.store(true); }];
        [nc addObserverForName:NSWindowDidEnterFullScreenNotification
                        object:nswindow
                         queue:mainQueue
                    usingBlock:^(NSNotification*) { gInTransition.store(false); }];
        [nc addObserverForName:NSWindowDidExitFullScreenNotification
                        object:nswindow
                         queue:mainQueue
                    usingBlock:^(NSNotification*) { gInTransition.store(false); }];
    }
}

void toggleNativeMacOSFullscreen(SDL_Window* window) {
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        return;
    }
    NSWindow* nswindow = wmInfo.info.cocoa.window;
    installFullscreenObserversOnce(nswindow);

    // Mark transitioning right away so the renderer skips any frame that
    // would otherwise race the dispatched toggleFullScreen: call. The
    // Will-* observer will keep it asserted; the Did-* observer clears it.
    gInTransition.store(true);

    dispatch_async(dispatch_get_main_queue(), ^{
        [nswindow toggleFullScreen:nil];
    });
}

bool isNativeMacOSFullscreenActive(SDL_Window* window) {
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(window, &wmInfo)) {
        NSWindow* nswindow = wmInfo.info.cocoa.window;
        return (([nswindow styleMask] & NSWindowStyleMaskFullScreen) == NSWindowStyleMaskFullScreen);
    }
    return false;
}

bool isNativeMacOSFullscreenTransitioning(void) {
    return gInTransition.load();
}

void macSetMetalLayerPresentsWithTransaction(void* metalLayer, bool enable) {
    if (metalLayer == nullptr) {
        return;
    }
    CAMetalLayer* layer = (__bridge CAMetalLayer*)metalLayer;
    layer.presentsWithTransaction = enable ? YES : NO;
}

void macPresentMetalDrawable(void* metalDrawable) {
    if (metalDrawable == nullptr) {
        return;
    }
    id<CAMetalDrawable> drawable = (__bridge id<CAMetalDrawable>)metalDrawable;
    [drawable present];
}
#endif
