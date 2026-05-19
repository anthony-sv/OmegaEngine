export module Engine.Core:EventBus;

import std;

namespace Engine::Core {

    using EventHandleID = std::uint64_t;

    export class EventBus {
    public:

        template<typename EventT>
        EventHandleID subscribe(std::function<void(EventT const&)> handler) {
            auto id = m_nextID++;
            auto& vec = m_handlers[std::type_index { typeid(EventT) }];

            vec.push_back(
                {
                    .id = id,
                    .handler = [fn = std::move(handler)](void const* raw) {
                        fn(*static_cast<EventT const*>(raw));
                    }
                }
            );

            return id;
        }

        void unsubscribe(EventHandleID id) {
            for(auto& [type, vec] : m_handlers) {
                std::erase_if(vec, [id](Entry const& e) {
                    return e.id == id;
                              });
            }
        }

        template<typename EventT>
        void publish(EventT const& event) {
            auto it = m_handlers.find(std::type_index { typeid(EventT) });
            if(it == m_handlers.end()) return;

            for(auto& entry : it->second)
                entry.handler(&event);
        }

    private:
        struct Entry {
            EventHandleID                     id;
            std::function<void(void const*)>  handler;
        };

        std::unordered_map<std::type_index, std::vector<Entry>> m_handlers;

        EventHandleID m_nextID { 1 };
    }; // class EventBus

} // namespace Core