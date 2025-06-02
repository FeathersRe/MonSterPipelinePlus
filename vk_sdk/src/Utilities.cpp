#include <condition_variable>
#include <memory>
#include <mutex>

#include "ctrl_c/ctrl-c.h"
#include "vk_sdk/Utilities.hpp"

namespace vkc {
    struct CtrlCState {
        std::mutex mMutex;
        std::condition_variable mCondVar;
        bool pressed = false;
    };

    bool waitForCtrlCSignal() {
        std::shared_ptr<CtrlCState> state = std::make_shared<CtrlCState>();

        auto handlerId = CtrlCLibrary::SetCtrlCHandler([state](auto signal) {
            if (signal == CtrlCLibrary::kCtrlCSignal) {
                {
                    std::lock_guard lock(state->mMutex);
                    state->pressed = true;
                }
                state->mCondVar.notify_one();
                return true; // Ensure signal is handled
            }
            return false;
        });

        if (handlerId == CtrlCLibrary::kErrorID) {
            return false;
        }

        {
            std::unique_lock lock(state->mMutex);
            while (!state->pressed) {
                state->mCondVar.wait(lock);
            }
        }

        CtrlCLibrary::ResetCtrlCHandler(handlerId);
        return true;
    }

}