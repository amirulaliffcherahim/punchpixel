#ifndef MODEL_HPP
#define MODEL_HPP

#include <cstdint>

#include "SDK/Interfaces/IGuiLifeCycleCallback.hpp"
#include "SDK/Kernel/Kernel.hpp"

class ModelListener;

class Model : public SDK::Interface::IGuiLifeCycleCallback
{
public:
    Model();

    void bind(ModelListener* listener) { modelListener = listener; }

    void     exitApp();
    uint32_t nowMs() const { return mKernel.sys.getTimeMs(); }

    /// Ask the Service to play a sound + vibration cue (kind = Pp::Sfx).
    void sfx(uint32_t kind);

protected:
    ModelListener*     modelListener;
    const SDK::Kernel& mKernel;

    void onStart() override;
    void onResume() override;
    void onSuspend() override;
    void onStop() override;
};

#endif
