#pragma once

#include <string_view>
#include <cstdint>

namespace bayou::client
{
enum class UiSound { Hover, Tap, Select, Open, Close, Drag, Drop, Slider,
    Paper, Confirm, Reject, Coins, Reveal, Alert, Power, Water, Fire, Creak };
// Visible controls nominate the hovered target each frame. Staying on a target
// is silent; leaving/re-entering, switching targets, or opening a new surface
// permits a new cue. A click in the same frame takes precedence over hovering.
class UiHoverTracker
{
public:
    void begin(std::uint64_t scope, bool enabled = true)
    {
        if (scope != scope_) previous_ = 0;
        scope_ = scope; candidate_ = 0; activated_ = false; enabled_ = enabled;
    }
    void offer(std::uint64_t target, bool hovered)
    {
        if (hovered && target) candidate_ = target;
    }
    void activated() { activated_ = true; }
    void clear() { candidate_ = 0; }
    bool finish()
    {
        const bool entered = enabled_ && candidate_ && candidate_ != previous_ && !activated_;
        previous_ = enabled_ ? candidate_ : 0;
        return entered;
    }
private:
    std::uint64_t scope_ = 0, previous_ = 0, candidate_ = 0;
    bool activated_ = false, enabled_ = true;
};
using UiSoundHandler = void (*)(UiSound);
void beginUiSoundFrame(std::uint64_t surface, bool enabled = true);
void observeUiHover(std::uint64_t target, bool hovered);
void observeUiHoverRect(float x, float y, float width, float height, bool hovered);
void clearUiHoverTargets();
void finishUiSoundFrame();
// Drawing may nominate an interactive target; only the frame boundary emits
// hover feedback. Redraws and movement within the same control stay silent.
void setUiSoundHandler(UiSoundHandler handler);
void emitUiSound(UiSound sound);
void emitUiStatus(std::string_view message, bool success);
}
