#include <iostream>
//#include "xpp/core/logger.hpp"
#include "xpp/core/config_manager.hpp"
#include "xpp/core/ioc_container.hpp"
#include "xpp/core/event_bus.hpp"
#include "xpp/core/logger.hpp"
using namespace xpp;

int main() {
    std::cout << "=== XPP Framework Test ===" << std::endl;

    // Test 1: Logger (without file initialization)
    std::cout << "1. Testing Logger (console only)..." << std::endl;
    try {
        core::Logger::instance().initialize("logs", core::Logger::Level::Info, 1024*1024, 5);
        xpp::log_info("Logger initialized successfully!");
        std::cout << "   ✓ Logger works" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "   ✗ Logger failed: " << e.what() << std::endl;
    }

    // Test 2: Config Manager
    std::cout << "2. Testing Config Manager..." << std::endl;
    try {
        auto& config = core::ConfigManager::instance();
        config.set("test.value", 42);
        auto value = config.get<int>("test.value");
        if (value && *value == 42) {
            std::cout << "   ✓ Config Manager works" << std::endl;
        } else {
            std::cout << "   ✗ Config Manager failed" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "   ✗ Config Manager error: " << e.what() << std::endl;
    }

    // Test 3: IoC Container
    std::cout << "3. Testing IoC Container..." << std::endl;
    try {
        struct TestService {
            int getValue() { return 123; }
        };

        auto& container = core::IoCContainer::instance();
        container.register_service<TestService>();
        auto service = container.resolve<TestService>();

        if (service && service->getValue() == 123) {
            std::cout << "   ✓ IoC Container works" << std::endl;
        } else {
            std::cout << "   ✗ IoC Container failed" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "   ✗ IoC Container error: " << e.what() << std::endl;
    }

    // Test 4: Event Bus
    std::cout << "4. Testing Event Bus..." << std::endl;
    try {
        struct TestEvent {
            int value;
        };

        auto& bus = core::EventBus::instance();
        bool received = false;

        auto sub_id = bus.subscribe<TestEvent>([&received](const TestEvent& e) {
            received = (e.value == 999);
        });

        bus.publish(TestEvent{999});

        if (received) {
            std::cout << "   ✓ Event Bus works" << std::endl;
        } else {
            std::cout << "   ✗ Event Bus failed" << std::endl;
        }

        bus.unsubscribe(sub_id);
    } catch (const std::exception& e) {
        std::cout << "   ✗ Event Bus error: " << e.what() << std::endl;
    }

    std::cout << "\n=== All Core Components Tested ===" << std::endl;
    xpp::log_info("Framework test completed successfully!");

    return 0;
}
