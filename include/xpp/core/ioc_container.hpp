#pragma once

#include <memory>
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <stdexcept>
#include <any>
#include <mutex>

namespace xpp::core {

/**
 * @brief Lightweight IoC (Inversion of Control) Container
 * Supports singleton and transient service lifetimes
 */
class IoCContainer {
public:
    enum class Lifetime {
        Singleton,  // Single instance shared across all resolves
        Transient   // New instance created for each resolve
    };

    static IoCContainer& instance() {
        static IoCContainer container;
        return container;
    }

    // Register a service with factory function
    template<typename Interface, typename Implementation = Interface>
    void register_service(
        std::function<std::shared_ptr<Interface>()> factory,
        Lifetime lifetime = Lifetime::Singleton
    ) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto type = std::type_index(typeid(Interface));
        services_[type] = ServiceDescriptor{
            .factory = [factory]() -> std::any {
                return std::static_pointer_cast<void>(factory());
            },
            .lifetime = lifetime,
            .instance = std::any{}
        };
    }

    // Register a service with default constructor
    template<typename Interface, typename Implementation = Interface>
    void register_service(Lifetime lifetime = Lifetime::Singleton) {
        register_service<Interface>(
            []() { return std::make_shared<Implementation>(); },
            lifetime
        );
    }

    // Register an existing instance (always singleton)
    template<typename Interface>
    void register_instance(std::shared_ptr<Interface> instance) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto type = std::type_index(typeid(Interface));
        services_[type] = ServiceDescriptor{
            .factory = nullptr,
            .lifetime = Lifetime::Singleton,
            .instance = std::static_pointer_cast<void>(instance)
        };
    }

    // Resolve a service
    template<typename T>
    std::shared_ptr<T> resolve() {
        std::lock_guard<std::mutex> lock(mutex_);

        auto type = std::type_index(typeid(T));
        auto it = services_.find(type);

        if (it == services_.end()) {
            throw std::runtime_error(
                std::string("Service not registered: ") + typeid(T).name()
            );
        }

        auto& descriptor = it->second;

        // Return existing singleton instance
        if (descriptor.lifetime == Lifetime::Singleton && descriptor.instance.has_value()) {
            return std::static_pointer_cast<T>(
                std::any_cast<std::shared_ptr<void>>(descriptor.instance)
            );
        }

        // Create new instance
        auto instance = descriptor.factory();
        auto typed_instance = std::static_pointer_cast<T>(
            std::any_cast<std::shared_ptr<void>>(instance)
        );

        // Store singleton instance
        if (descriptor.lifetime == Lifetime::Singleton) {
            descriptor.instance = instance;
        }

        return typed_instance;
    }

    // Check if service is registered
    template<typename T>
    bool is_registered() const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto type = std::type_index(typeid(T));
        return services_.find(type) != services_.end();
    }

    // Clear all services
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        services_.clear();
    }

private:
    struct ServiceDescriptor {
        std::function<std::any()> factory;
        Lifetime lifetime;
        mutable std::any instance;
    };

    IoCContainer() = default;
    ~IoCContainer() = default;
    IoCContainer(const IoCContainer&) = delete;
    IoCContainer& operator=(const IoCContainer&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::type_index, ServiceDescriptor> services_;
};

} // namespace xpp::core
