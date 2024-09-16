#include <cstring>  // for memcpy  
#include <csignal>
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vsomeip/vsomeip.hpp>
#include "sample-ids.hpp"

class service_sample {
public:
    service_sample(bool _use_static_routing)
        : app_(vsomeip::runtime::get()->create_application()),
          is_registered_(false),
          use_static_routing_(_use_static_routing),
          blocked_(false),
          running_(true),
          offer_thread_(std::bind(&service_sample::run, this)) {
    }

    bool init() {
        std::lock_guard<std::mutex> its_lock(mutex_);

        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }
        app_->register_state_handler(
                std::bind(&service_sample::on_state, this, std::placeholders::_1));
        app_->register_message_handler(
                SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_METHOD_ID,
                std::bind(&service_sample::on_message, this, std::placeholders::_1));

        return true;
    }

    void start() {
        app_->start();
    }

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    void stop() {
        running_ = false;
        blocked_ = true;
        app_->clear_all_handler();
        stop_offer();
        condition_.notify_one();
        if (std::this_thread::get_id() != offer_thread_.get_id()) {
            if (offer_thread_.joinable()) {
                offer_thread_.join();
            }
        } else {
            offer_thread_.detach();
        }
        app_->stop();
    }
#endif

    void offer() {
        app_->offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
    }

    void stop_offer() {
        app_->stop_offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
    }

    void run() {
        while (running_) {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (!blocked_)
                condition_.wait(its_lock);
            if (is_registered_) {
                app_->offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
                blocked_ = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    void on_state(vsomeip::state_type_e _state) {
        std::cout << "Service state "
                  << (_state == vsomeip::state_type_e::ST_REGISTERED
                          ? "REGISTERED" : "DEREGISTERED") << std::endl;

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            is_registered_ = true;
            offer();
        } else {
            is_registered_ = false;
            stop_offer();
        }
    }

    // response-sample.cpp

    // response-sample.cpp

    void on_message(const std::shared_ptr<vsomeip::message> &_request) {
    std::shared_ptr<vsomeip::payload> its_payload = _request->get_payload();
    std::vector<vsomeip::byte_t> its_payload_data(its_payload->get_length());

    // Copy the received payload
    std::memcpy(its_payload_data.data(), its_payload->get_data(), its_payload->get_length());

    if (its_payload_data.size() == 8) { // We expect two 4-byte integers
        // Convert the first 4 bytes to an integer (first operand)
        uint32_t operand1 = (static_cast<uint32_t>(its_payload_data[0]) << 24) |
                            (static_cast<uint32_t>(its_payload_data[1]) << 16) |
                            (static_cast<uint32_t>(its_payload_data[2]) << 8) |
                            (static_cast<uint32_t>(its_payload_data[3]));

        // Convert the second 4 bytes to an integer (second operand)
        uint32_t operand2 = (static_cast<uint32_t>(its_payload_data[4]) << 24) |
                            (static_cast<uint32_t>(its_payload_data[5]) << 16) |
                            (static_cast<uint32_t>(its_payload_data[6]) << 8) |
                            (static_cast<uint32_t>(its_payload_data[7]));

        // Perform addition
        uint32_t result = operand1 + operand2;

        // Prepare the result to be sent as a response (4 bytes)
        std::vector<vsomeip::byte_t> response_payload_data(4);
        response_payload_data[0] = static_cast<vsomeip::byte_t>((result >> 24) & 0xFF);
        response_payload_data[1] = static_cast<vsomeip::byte_t>((result >> 16) & 0xFF);
        response_payload_data[2] = static_cast<vsomeip::byte_t>((result >> 8) & 0xFF);
        response_payload_data[3] = static_cast<vsomeip::byte_t>(result & 0xFF);

        std::shared_ptr<vsomeip::payload> response_payload = vsomeip::runtime::get()->create_payload();
        response_payload->set_data(response_payload_data);

        // Create and send the response message
        std::shared_ptr<vsomeip::message> response = vsomeip::runtime::get()->create_response(_request);
        response->set_payload(response_payload);
        app_->send(response);

        std::cout << "Received numbers: " << operand1 << " + " << operand2 << " = " << result << std::endl;
    } else {
        std::cerr << "Error: Invalid payload size. Expected 8 bytes." << std::endl;
    }
}



private:
    std::shared_ptr<vsomeip::application> app_;
    bool is_registered_;
    bool use_static_routing_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool blocked_;
    bool running_;
    std::thread offer_thread_;
};

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    service_sample *its_sample_ptr(nullptr);
    void handle_signal(int _signal) {
        if (its_sample_ptr != nullptr &&
                (_signal == SIGINT || _signal == SIGTERM))
            its_sample_ptr->stop();
    }
#endif

int main(int argc, char **argv) {
    bool use_static_routing = false;

    std::string static_routing_enable("--static-routing");

    int i = 1;
    while (i < argc) {
        if (static_routing_enable == argv[i]) {
            use_static_routing = true;
        }
        i++;
    }

    service_sample its_sample(use_static_routing);
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    its_sample_ptr = &its_sample;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif
    if (its_sample.init()) {
        its_sample.start();
        return 0;
    } else {
        return 1;
    }
}
