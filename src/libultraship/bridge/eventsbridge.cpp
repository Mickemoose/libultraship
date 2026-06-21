#include "libultraship/bridge/eventsbridge.h"
#include "ship/events/EventSystem.h"
#include "ship/Context.h"

extern "C" {

EventID EventSystemRegisterEvent(const char* name) {
    return Ship::Context::GetInstance()->GetEventSystem()->RegisterEvent(name);
}

ListenerID EventSystemRegisterListener(EventID id, EventCallback callback, EventPriority priority, const char* file,
                                       int line) {
    return Ship::Context::GetInstance()->GetEventSystem()->RegisterListener(id, callback, priority, file, line);
}

void EventSystemUnregisterListener(EventID ev, ListenerID id) {
    // Reachable from a mod's ModExit during ~Context -> ScriptLoader::UnloadAll,
    // at which point the Context shared_ptr is mid-destruction and GetInstance()
    // (mContext.lock()) returns null -> the chained deref would crash on exit.
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr) {
        return;
    }
    auto eventSystem = ctx->GetEventSystem();
    if (eventSystem == nullptr) {
        return;
    }
    eventSystem->UnregisterListener(ev, id);
}

void EventSystemCallEvent(EventID id, void* event, const char* file, int line, const char* key) {
    Ship::Context::GetInstance()->GetEventSystem()->CallEvent(id, static_cast<IEvent*>(event), file, line, key);
}
}