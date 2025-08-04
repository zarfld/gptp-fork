#include <gtest/gtest.h>
#include "core/timestamp_provider.hpp"
#include "utils/logger.hpp"

using namespace gptp;

class TimestampProviderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test environment
        Logger::instance().set_level(LogLevel::ERROR); // Reduce noise during testing
        provider_ = create_timestamp_provider();
    }

    void TearDown() override {
        if (provider_) {
            provider_->cleanup();
        }
    }

    std::unique_ptr<ITimestampProvider> provider_;
};

TEST_F(TimestampProviderTest, ProviderCreation) {
    ASSERT_NE(provider_, nullptr) << "Provider should be created successfully";
}

TEST_F(TimestampProviderTest, Initialization) {
    ASSERT_NE(provider_, nullptr);
    
    auto result = provider_->initialize();
    EXPECT_TRUE(result.is_success()) << "Provider initialization should succeed";
}

TEST_F(TimestampProviderTest, NetworkInterfaceDiscovery) {
    ASSERT_NE(provider_, nullptr);
    
    auto init_result = provider_->initialize();
    ASSERT_TRUE(init_result.is_success()) << "Provider must be initialized";
    
    auto interfaces_result = provider_->get_network_interfaces();
    EXPECT_TRUE(interfaces_result.is_success()) << "Should be able to get network interfaces";
    
    if (interfaces_result.is_success()) {
        const auto& interfaces = interfaces_result.value();
        EXPECT_GT(interfaces.size(), 0) << "Should find at least one network interface";
        
        for (const auto& interface : interfaces) {
            EXPECT_FALSE(interface.name.empty()) << "Interface should have a name";
            EXPECT_FALSE(interface.mac_address.empty()) << "Interface should have a MAC address";
        }
    }
}

TEST_F(TimestampProviderTest, HardwareTimestampingCheck) {
    ASSERT_NE(provider_, nullptr);
    
    // This test doesn't require initialization as it's a static capability check
    bool hw_available = provider_->is_hardware_timestamping_available();
    
    // We can't assert true or false since it depends on the hardware,
    // but we can verify the method doesn't crash
    SUCCEED() << "Hardware timestamping availability check completed: " 
              << (hw_available ? "Available" : "Not available");
}

// Test error handling
TEST_F(TimestampProviderTest, ErrorHandlingWithoutInitialization) {
    ASSERT_NE(provider_, nullptr);
    
    // Try to get capabilities without initialization
    auto caps_result = provider_->get_timestamp_capabilities("test_interface");
    EXPECT_TRUE(caps_result.has_error()) << "Should fail when not initialized";
    EXPECT_EQ(caps_result.error(), ErrorCode::INITIALIZATION_FAILED);
}

// Test Result template functionality
TEST(ResultTest, SuccessfulResult) {
    Result<int> result(42);
    
    EXPECT_TRUE(result.is_success());
    EXPECT_FALSE(result.has_error());
    EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ErrorResult) {
    Result<int> result(ErrorCode::NETWORK_ERROR);
    
    EXPECT_FALSE(result.is_success());
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error(), ErrorCode::NETWORK_ERROR);
    
    // Accessing value of failed result should throw
    EXPECT_THROW(result.value(), std::runtime_error);
}
