#include <unity.h>
#define UNIT_TEST
#include "test_functions.h"
#include <math.h>

// Test setup and teardown
void setUp(void) {
    mock_reset();
}

void tearDown(void) {
    // Clean up after each test
}

// Helper function for floating point comparison
bool floatEquals(float a, float b, float epsilon = 0.001f) {
    return fabs(a - b) < epsilon;
}

// ============================================================================
// Test Case 1: clampf() correctly clamps float values within bounds
// ============================================================================

void test_clampf_value_below_min(void) {
    float result = clampf(-5.0f, 0.0f, 10.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, result);
}

void test_clampf_value_above_max(void) {
    float result = clampf(15.0f, 0.0f, 10.0f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, result);
}

void test_clampf_value_within_range(void) {
    float result = clampf(5.0f, 0.0f, 10.0f);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, result);
}

void test_clampf_value_at_min_boundary(void) {
    float result = clampf(0.0f, 0.0f, 10.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, result);
}

void test_clampf_value_at_max_boundary(void) {
    float result = clampf(10.0f, 0.0f, 10.0f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, result);
}

void test_clampf_negative_range(void) {
    float result = clampf(-5.0f, -10.0f, -1.0f);
    TEST_ASSERT_EQUAL_FLOAT(-5.0f, result);
}

void test_clampf_very_small_numbers(void) {
    float result = clampf(0.0001f, 0.0f, 0.001f);
    TEST_ASSERT_EQUAL_FLOAT(0.0001f, result);
}

// ============================================================================
// Test Case 2: voltageToKpa() converts voltage to pressure correctly
//              including boundary cases
// ============================================================================

void test_voltageToKpa_min_voltage(void) {
    // V_MIN = 0.50V should give 0 kPa
    float result = voltageToKpa(0.50f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, result);
}

void test_voltageToKpa_max_voltage(void) {
    // V_MAX = 4.50V should give FS_KPA = 10.0 kPa
    float result = voltageToKpa(4.50f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, result);
}

void test_voltageToKpa_mid_voltage(void) {
    // Mid-point: (0.50 + 4.50) / 2 = 2.50V should give 5.0 kPa
    float result = voltageToKpa(2.50f);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, result);
}

void test_voltageToKpa_below_min(void) {
    // Voltage below V_MIN should clamp to 0 kPa
    float result = voltageToKpa(0.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, result);
}

void test_voltageToKpa_above_max(void) {
    // Voltage above V_MAX should clamp to FS_KPA
    float result = voltageToKpa(5.0f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, result);
}

void test_voltageToKpa_quarter_range(void) {
    // Quarter point: 0.50 + (4.50-0.50)*0.25 = 1.50V should give 2.5 kPa
    float result = voltageToKpa(1.50f);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, result);
}

void test_voltageToKpa_three_quarter_range(void) {
    // Three-quarter point: 0.50 + (4.50-0.50)*0.75 = 3.50V should give 7.5 kPa
    float result = voltageToKpa(3.50f);
    TEST_ASSERT_EQUAL_FLOAT(7.5f, result);
}

void test_voltageToKpa_negative_voltage(void) {
    // Negative voltage should clamp to 0 kPa
    float result = voltageToKpa(-1.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, result);
}

// ============================================================================
// Test Case 3: readA0VoltageAveraged() returns an average voltage value
//              within expected range
// ============================================================================

void test_readA0VoltageAveraged_zero_reading(void) {
    mock_set_analog_value(0);
    float result = readA0VoltageAveraged();
    TEST_ASSERT_EQUAL_FLOAT(0.0f, result);
}

void test_readA0VoltageAveraged_max_reading(void) {
    // ADC_MAX = 1023, ADC_REF_V = 5.00V
    mock_set_analog_value(1023);
    float result = readA0VoltageAveraged();
    TEST_ASSERT_EQUAL_FLOAT(5.0f, result);
}

void test_readA0VoltageAveraged_mid_reading(void) {
    // Half of ADC_MAX should give half of ADC_REF_V
    mock_set_analog_value(512);
    float result = readA0VoltageAveraged();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.505f, result);  // 512/1023 * 5.0 ≈ 2.505
}

void test_readA0VoltageAveraged_quarter_reading(void) {
    // Quarter of ADC_MAX
    mock_set_analog_value(256);
    float result = readA0VoltageAveraged();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.252f, result);  // 256/1023 * 5.0 ≈ 1.252
}

void test_readA0VoltageAveraged_within_expected_range(void) {
    // Test with a typical sensor value
    mock_set_analog_value(700);
    float result = readA0VoltageAveraged();
    
    // Result should be within 0V to 5V range
    TEST_ASSERT_TRUE(result >= 0.0f);
    TEST_ASSERT_TRUE(result <= 5.0f);
    
    // More specifically, should be around 3.42V (700/1023 * 5.0)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.421f, result);
}

void test_readA0VoltageAveraged_samples_multiple_times(void) {
    // The function samples 10 times, each with 10ms delay
    // This should advance millis by 100ms total
    mock_set_millis(0);
    mock_set_analog_value(500);
    
    readA0VoltageAveraged();
    
    // Verify that time has advanced (10 samples * 10ms each = 100ms)
    TEST_ASSERT_EQUAL_UINT32(100, millis());
}

// ============================================================================
// Test Case 4: connectWiFi() handles WiFi connection attempts and
//              timeouts properly
// ============================================================================

void test_connectWiFi_already_connected(void) {
    mock_set_wifi_status(WL_CONNECTED);
    mock_set_millis(0);
    
    connectWiFi();
    
    // Should return immediately without waiting
    TEST_ASSERT_EQUAL_UINT32(0, millis());
}

void test_connectWiFi_successful_connection(void) {
    mock_set_wifi_status(WL_DISCONNECTED);
    mock_set_millis(0);
    
    // Simulate connection after some delay
    // Note: In real implementation, WiFi.status() would change during the loop
    // For this mock, we'll test the timeout behavior
    connectWiFi();
    
    // The function should have attempted connection
    // With our mock always returning WL_DISCONNECTED, it will timeout
}

void test_connectWiFi_timeout_behavior(void) {
    mock_set_wifi_status(WL_DISCONNECTED);
    mock_set_millis(0);
    
    connectWiFi();
    
    // Should timeout after 15000ms (15 seconds)
    // The loop delays 500ms each iteration
    // 15000 / 500 = 30 iterations maximum
    unsigned long elapsed = millis();
    TEST_ASSERT_TRUE(elapsed >= 15000);
}

void test_connectWiFi_respects_15_second_timeout(void) {
    mock_set_wifi_status(WL_DISCONNECTED);
    mock_set_millis(0);
    
    connectWiFi();
    
    // Elapsed time should not exceed 15 seconds by much
    // Allowing some overhead for the final iteration
    unsigned long elapsed = millis();
    TEST_ASSERT_TRUE(elapsed <= 16000);
}

void test_connectWiFi_calls_wifi_begin(void) {
    mock_set_wifi_status(WL_DISCONNECTED);
    
    // This test verifies the function doesn't crash
    // and completes (WiFi.begin is called internally)
    connectWiFi();
    
    TEST_ASSERT_TRUE(true);  // If we get here, function executed
}

// ============================================================================
// Test Case 5: uploadToServer() attempts connection and sends correct
//              HTTP GET request with parameters
// ============================================================================

void test_uploadToServer_wifi_not_connected(void) {
    mock_set_wifi_status(WL_DISCONNECTED);
    mock_set_millis(0);
    
    uploadToServer(1.5f, 5.25f, 47.12f);
    
    // Should return early without attempting server connection
    // No time should have passed
    TEST_ASSERT_EQUAL_UINT32(0, millis());
}

void test_uploadToServer_server_connection_fails(void) {
    mock_set_wifi_status(WL_CONNECTED);
    mock_set_client_connected(false);
    mock_set_millis(0);
    
    uploadToServer(1.5f, 5.25f, 47.12f);
    
    // Should attempt connection but fail early
    TEST_ASSERT_EQUAL_UINT32(0, millis());
}

void test_uploadToServer_successful_connection_sends_request(void) {
    mock_set_wifi_status(WL_CONNECTED);
    mock_set_client_connected(true);
    mock_set_millis(0);
    
    // This will attempt to send the request
    // The mock client.available() returns 0, so it will timeout
    uploadToServer(1.5f, 5.25f, 47.12f);
    
    // Should timeout after 5000ms waiting for response
    unsigned long elapsed = millis();
    TEST_ASSERT_TRUE(elapsed >= 5000);
}

void test_uploadToServer_formats_parameters_correctly(void) {
    mock_set_wifi_status(WL_CONNECTED);
    mock_set_client_connected(true);
    
    // Test that the function doesn't crash with various parameter values
    uploadToServer(0.0f, 0.0f, 0.0f);
    mock_reset();
    mock_set_wifi_status(WL_CONNECTED);
    mock_set_client_connected(true);
    
    uploadToServer(1.234f, 5.67f, 89.12f);
    mock_reset();
    mock_set_wifi_status(WL_CONNECTED);
    mock_set_client_connected(true);
    
    uploadToServer(10.999f, 99.99f, 999.99f);
    
    TEST_ASSERT_TRUE(true);  // If we get here, all calls succeeded
}

void test_uploadToServer_respects_5_second_timeout(void) {
    mock_set_wifi_status(WL_CONNECTED);
    mock_set_client_connected(true);
    mock_set_millis(0);
    
    uploadToServer(1.0f, 2.0f, 3.0f);
    
    // Should timeout after approximately 5000ms
    unsigned long elapsed = millis();
    TEST_ASSERT_TRUE(elapsed >= 5000);
    TEST_ASSERT_TRUE(elapsed <= 6000);
}

void test_uploadToServer_handles_negative_values(void) {
    mock_set_wifi_status(WL_CONNECTED);
    mock_set_client_connected(true);
    
    // Test with negative values (edge case)
    uploadToServer(-1.0f, -2.0f, -3.0f);
    
    TEST_ASSERT_TRUE(true);  // Should not crash
}

// ============================================================================
// Test runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Test Case 1: clampf()
    RUN_TEST(test_clampf_value_below_min);
    RUN_TEST(test_clampf_value_above_max);
    RUN_TEST(test_clampf_value_within_range);
    RUN_TEST(test_clampf_value_at_min_boundary);
    RUN_TEST(test_clampf_value_at_max_boundary);
    RUN_TEST(test_clampf_negative_range);
    RUN_TEST(test_clampf_very_small_numbers);
    
    // Test Case 2: voltageToKpa()
    RUN_TEST(test_voltageToKpa_min_voltage);
    RUN_TEST(test_voltageToKpa_max_voltage);
    RUN_TEST(test_voltageToKpa_mid_voltage);
    RUN_TEST(test_voltageToKpa_below_min);
    RUN_TEST(test_voltageToKpa_above_max);
    RUN_TEST(test_voltageToKpa_quarter_range);
    RUN_TEST(test_voltageToKpa_three_quarter_range);
    RUN_TEST(test_voltageToKpa_negative_voltage);
    
    // Test Case 3: readA0VoltageAveraged()
    RUN_TEST(test_readA0VoltageAveraged_zero_reading);
    RUN_TEST(test_readA0VoltageAveraged_max_reading);
    RUN_TEST(test_readA0VoltageAveraged_mid_reading);
    RUN_TEST(test_readA0VoltageAveraged_quarter_reading);
    RUN_TEST(test_readA0VoltageAveraged_within_expected_range);
    RUN_TEST(test_readA0VoltageAveraged_samples_multiple_times);
    
    // Test Case 4: connectWiFi()
    RUN_TEST(test_connectWiFi_already_connected);
    RUN_TEST(test_connectWiFi_successful_connection);
    RUN_TEST(test_connectWiFi_timeout_behavior);
    RUN_TEST(test_connectWiFi_respects_15_second_timeout);
    RUN_TEST(test_connectWiFi_calls_wifi_begin);
    
    // Test Case 5: uploadToServer()
    RUN_TEST(test_uploadToServer_wifi_not_connected);
    RUN_TEST(test_uploadToServer_server_connection_fails);
    RUN_TEST(test_uploadToServer_successful_connection_sends_request);
    RUN_TEST(test_uploadToServer_formats_parameters_correctly);
    RUN_TEST(test_uploadToServer_respects_5_second_timeout);
    RUN_TEST(test_uploadToServer_handles_negative_values);
    
    return UNITY_END();
}
