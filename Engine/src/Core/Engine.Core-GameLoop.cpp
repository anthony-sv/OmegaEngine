module Engine.Core:GameLoop;

import :GameLoop;
import std;

namespace Engine::Core {

    void GameLoop::addFrameTime(float rawDt) {
        m_accumulator += std::min(rawDt, m_cfg.maxFrameDt);
    }

    bool GameLoop::consumeTick() {
        if(m_accumulator < m_cfg.fixedDt) return false;
        m_accumulator -= m_cfg.fixedDt;
        return true;
    }

    void GameLoop::reset() {
        m_accumulator = 0.0f;
    }

} // namespace Core