#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>
#include <mutex>
#include <algorithm>
#include <any>

namespace xpp::core {

/**
 * @brief Thread-safe Event Bus for decoupled communication
 * Supports publish-subscribe pattern with type-safe events
 */
class EventBus {
public:
    using SubscriptionId = uint64_t;

    static EventBus& instance() {
        static EventBus bus;
        return bus;
    }

    /**
     * @brief Subscribe to an event type
     * @tparam Event The event type to subscribe to
     * @param handler Function to be called when event is published
     * @return Subscription ID for later unsubscribe
     */
    template<typename Event>
    SubscriptionId subscribe(std::function<void(const Event&)> handler) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto type = std::type_index(typeid(Event));
        auto id = next_subscription_id_++;

        // Wrap handler in type-erased function
        auto wrapper = [handler](const std::any& event) {
            handler(std::any_cast<const Event&>(event));
        };

        subscriptions_[type].emplace_back(Subscription{id, wrapper});
        return id;
    }

    /**
     * @brief Subscribe with async execution (non-blocking)
     * @tparam Event The event type to subscribe to
     * @param handler Function to be called asynchronously
     * @return Subscription ID
     */
    template<typename Event>
    SubscriptionId subscribe_async(std::function<void(const Event&)> handler) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto type = std::type_index(typeid(Event));
        auto id = next_subscription_id_++;

        auto wrapper = [handler](const std::any& event) {
            // Execute in thread pool (simple async for now)
            std::thread([handler, event]() {
                handler(std::any_cast<const Event&>(event));
            }).detach();
        };

        subscriptions_[type].emplace_back(Subscription{id, wrapper});
        return id;
    }

    /**
     * @brief Unsubscribe from events
     * @param id Subscription ID returned from subscribe
     */
    void unsubscribe(SubscriptionId id) {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& [type, subs] : subscriptions_) {
            auto it = std::remove_if(subs.begin(), subs.end(),
                [id](const Subscription& sub) { return sub.id == id; });

            if (it != subs.end()) {
                subs.erase(it, subs.end());
                return;
            }
        }
    }

    /**
     * @brief Publish an event (synchronous)
     * @tparam Event The event type
     * @param event The event object to publish
     */
    template<typename Event>
    void publish(const Event& event) {
        std::vector<std::function<void(const std::any&)>> handlers;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto type = std::type_index(typeid(Event));
            auto it = subscriptions_.find(type);

            if (it == subscriptions_.end()) {
                return;
            }

            // Copy handlers to avoid deadlock
            for (const auto& sub : it->second) {
                handlers.push_back(sub.handler);
            }
        }

        // Execute handlers outside lock
        std::any event_any = event;
        for (const auto& handler : handlers) {
            try {
                handler(event_any);
            } catch (const std::exception& e) {
                // Log error (requires logger integration)
                // For now, silently continue
            }
        }
    }

    /**
     * @brief Publish an event (move semantics)
     */
    template<typename Event>
    void publish(Event&& event) {
        publish(static_cast<const Event&>(event));
    }

    /**
     * @brief Clear all subscriptions for a specific event type
     */
    template<typename Event>
    void clear_subscriptions() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto type = std::type_index(typeid(Event));
        subscriptions_.erase(type);
    }

    /**
     * @brief Clear all subscriptions
     */
    void clear_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        subscriptions_.clear();
    }

    /**
     * @brief Get number of subscribers for an event type
     */
    template<typename Event>
    size_t subscriber_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto type = std::type_index(typeid(Event));
        auto it = subscriptions_.find(type);
        return it != subscriptions_.end() ? it->second.size() : 0;
    }

private:
    struct Subscription {
        SubscriptionId id;
        std::function<void(const std::any&)> handler;
    };

    EventBus() : next_subscription_id_(0) {}
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    mutable std::mutex mutex_;
    SubscriptionId next_subscription_id_;
    std::unordered_map<std::type_index, std::vector<Subscription>> subscriptions_;
};

// RAII subscription guard
class ScopedSubscription {
public:
    explicit ScopedSubscription(EventBus::SubscriptionId id)
        : id_(id), bus_(&EventBus::instance()) {}

    ~ScopedSubscription() {
        if (bus_ && id_ != 0) {
            bus_->unsubscribe(id_);
        }
    }

    ScopedSubscription(const ScopedSubscription&) = delete;
    ScopedSubscription& operator=(const ScopedSubscription&) = delete;

    ScopedSubscription(ScopedSubscription&& other) noexcept
        : id_(other.id_), bus_(other.bus_) {
        other.id_ = 0;
        other.bus_ = nullptr;
    }

private:
    EventBus::SubscriptionId id_;
    EventBus* bus_;
};

} // namespace xpp::core
