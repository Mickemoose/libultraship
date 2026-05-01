#include "ship/controller/physicaldevice/ConnectedPhysicalDeviceManager.h"
#include <spdlog/spdlog.h>
#include <cstdio>

namespace Ship {
ConnectedPhysicalDeviceManager::ConnectedPhysicalDeviceManager() {
}

ConnectedPhysicalDeviceManager::~ConnectedPhysicalDeviceManager() {
}

std::unordered_map<int32_t, SDL_GameController*>
ConnectedPhysicalDeviceManager::GetConnectedSDLGamepadsForPort(uint8_t portIndex) {
    std::unordered_map<int32_t, SDL_GameController*> result;

    for (const auto& [instanceId, gamepad] : mConnectedSDLGamepads) {
        if (!PortIsIgnoringInstanceId(portIndex, instanceId)) {
            result[instanceId] = gamepad;
        }
    }

    return result;
}

std::unordered_map<int32_t, std::string> ConnectedPhysicalDeviceManager::GetConnectedSDLGamepadNames() {
    return mConnectedSDLGamepadNames;
}

std::unordered_set<int32_t> ConnectedPhysicalDeviceManager::GetIgnoredInstanceIdsForPort(uint8_t portIndex) {
    return mIgnoredInstanceIds[portIndex];
}

bool ConnectedPhysicalDeviceManager::PortIsIgnoringInstanceId(uint8_t portIndex, int32_t instanceId) {
    return GetIgnoredInstanceIdsForPort(portIndex).contains(instanceId);
}

void ConnectedPhysicalDeviceManager::IgnoreInstanceIdForPort(uint8_t portIndex, int32_t instanceId) {
    mIgnoredInstanceIds[portIndex].insert(instanceId);
}

void ConnectedPhysicalDeviceManager::UnignoreInstanceIdForPort(uint8_t portIndex, int32_t instanceId) {
    mIgnoredInstanceIds[portIndex].erase(instanceId);
}

void ConnectedPhysicalDeviceManager::HandlePhysicalDeviceConnect(int32_t sdlDeviceIndex) {
    RefreshConnectedSDLGamepads();
}

void ConnectedPhysicalDeviceManager::HandlePhysicalDeviceDisconnect(int32_t sdlJoystickInstanceId) {
    RefreshConnectedSDLGamepads();
}

void ConnectedPhysicalDeviceManager::RefreshConnectedSDLGamepads() {
    mConnectedSDLGamepads.clear();
    mConnectedSDLGamepadNames.clear();
    static SDL_JoystickGUID sZeroGuid;

    int32_t numJoysticks = SDL_NumJoysticks();
    std::fprintf(stderr,
                 "[RUMBLE-DIAG] RefreshConnectedSDLGamepads: SDL_NumJoysticks=%d\n",
                 numJoysticks);
    for (int32_t i = 0; i < numJoysticks; i++) {

        SDL_JoystickGUID deviceGUID = SDL_JoystickGetDeviceGUID(i);
        if (SDL_memcmp(&deviceGUID, &sZeroGuid, sizeof(deviceGUID)) == 0) {
            SPDLOG_WARN(
                "Calling SDL JoystickGetDeviceGUID with index ({:d}) returned zero GUID. This is likely due to an "
                "invalid index. Refer to https://wiki.libsdl.org/SDL2/SDL_JoystickGetDeviceGUID for more information.",
                i);
            continue;
        }

        char deviceGuidCStr[33] = "";
        SDL_JoystickGetGUIDString(deviceGUID, deviceGuidCStr, sizeof(deviceGuidCStr));

        const char* jsName = SDL_JoystickNameForIndex(i);
        bool isGC = SDL_IsGameController(i);
        std::fprintf(stderr,
                     "[RUMBLE-DIAG]   joystick[%d] guid=%s name=\"%s\" isGameController=%d\n",
                     i, deviceGuidCStr, jsName ? jsName : "(null)", (int)isGC);

        if (!SDL_IsGameController(i)) {
            SPDLOG_WARN("SDL Joystick (GUID: {}) not recognized as gamepad."
                        "This is likely due to a missing mapping string in gamecontrollerdb.txt."
                        "Refer to https://github.com/mdqinc/SDL_GameControllerDB for more information.",
                        deviceGuidCStr);
            continue;
        }

        auto gamepad = SDL_GameControllerOpen(i);
        if (gamepad == nullptr) {
            SPDLOG_ERROR("SDL GameControllerOpen error (GUID: {}): {}", deviceGuidCStr, SDL_GetError());
            continue;
        }

        auto instanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad));
        if (instanceId < 0) {
            SPDLOG_ERROR("SDL JoystickInstanceID error (GUID: {}): {}", deviceGuidCStr, SDL_GetError());
            continue;
        }

        std::string gamepadName;
        auto name = SDL_GameControllerName(gamepad);
        if (name == nullptr) {
            gamepadName = deviceGuidCStr;
            SPDLOG_WARN("SDL_GameControllerName returned null. Setting name to GUID \"{}\" instead.", gamepadName);
        } else {
            gamepadName = name;
        }

        mConnectedSDLGamepads[instanceId] = gamepad;
        mConnectedSDLGamepadNames[instanceId] = gamepadName;

        bool hasRumble = SDL_GameControllerHasRumble(gamepad);
        std::fprintf(stderr,
                     "[RUMBLE-DIAG]   opened: instanceId=%d name=\"%s\" hasRumble=%d (auto-assigned to port 0; ignored on ports 1-3)\n",
                     instanceId, gamepadName.c_str(), (int)hasRumble);

        for (uint8_t port = 1; port < 4; port++) {
            mIgnoredInstanceIds[port].insert(instanceId);
        }
    }
}
} // namespace Ship
