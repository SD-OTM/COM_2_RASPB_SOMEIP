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

class client_sample {
public:
    client_sample(bool _use_tcp, bool _be_quiet, uint32_t _cycle)
        : app_(vsomeip::runtime::get()->create_application()),
          request_(vsomeip::runtime::get()->create_request(_use_tcp)),
          use_tcp_(_use_tcp),
          be_quiet_(_be_quiet),
          cycle_(_cycle),
          running_(true),
          blocked_(false),
          is_available_(false),
          sender_(std::bind(&client_sample::run, this)) {
    }

    bool init() {
        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }

        std::cout << "Client settings [protocol="
                  << (use_tcp_ ? "TCP" : "UDP")
                  << ":quiet="
                  << (be_quiet_ ? "true" : "false")
                  << ":cycle="
                  << cycle_
                  << "]"
                  << std::endl;

        app_->register_state_handler(
                std::bind(
                    &client_sample::on_state,
                    this,
                    std::placeholders::_1));

        app_->register_message_handler(
                vsomeip::ANY_SERVICE, SAMPLE_INSTANCE_ID, vsomeip::ANY_METHOD,
                std::bind(&client_sample::on_message,
                          this,
                          std::placeholders::_1));

        request_->set_service(SAMPLE_SERVICE_ID);
        request_->set_instance(SAMPLE_INSTANCE_ID);
        request_->set_method(SAMPLE_METHOD_ID);

        app_->register_availability_handler(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID,
                std::bind(&client_sample::on_availability,
                          this,
                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

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
        app_->release_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        condition_.notify_one();
        if (std::this_thread::get_id() != sender_.get_id()) {
            if (sender_.joinable()) {
                sender_.join();
            }
        } else {
            sender_.detach();
        }
        app_->stop();
    }
#endif

    void on_state(vsomeip::state_type_e _state) {
        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            app_->request_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        }
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        std::cout << "Service ["
                << std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
                << "] is "
                << (_is_available ? "available." : "NOT available.")
                << std::endl;

        if (SAMPLE_SERVICE_ID == _service && SAMPLE_INSTANCE_ID == _instance) {
            if (is_available_  && !_is_available) {
                is_available_ = false;
            } else if (_is_available && !is_available_) {
                is_available_ = true;
                send_request();
            }
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message> &_response) {
        std::shared_ptr<vsomeip::payload> its_payload = _response->get_payload();
        std::vector<vsomeip::byte_t> its_payload_data(its_payload->get_length());

        // Copy data using memcpy
        std::memcpy(its_payload_data.data(), its_payload->get_data(), its_payload->get_length());

        if (its_payload_data.size() == 4) {
            // Interpret the 4-byte result
            uint32_t result = (static_cast<uint32_t>(its_payload_data[0]) << 24) |
                              (static_cast<uint32_t>(its_payload_data[1]) << 16) |
                              (static_cast<uint32_t>(its_payload_data[2]) << 8) |
                              (static_cast<uint32_t>(its_payload_data[3]));

            // Display the result and operands
            std::cout << "Received numbers: " << last_operand1_ << " + " << last_operand2_ << " = " << result << std::endl;
        } else {
            std::cerr << "Error: Response payload size is incorrect." << std::endl;
        }

        if (is_available_)
            send_request();
    }

    void send_request() {
    std::string input_line;
    char operation;

    // Prompt user to enter the operation
    std::cout << "Enter an expression (e.g., 12 + 34): ";
    std::getline(std::cin, input_line);

    // Check if the input line contains a valid operator
    size_t op_pos = input_line.find_first_of("+-");
    if (op_pos == std::string::npos) {
        std::cerr << "Error: Invalid input. Please enter an expression with + or -." << std::endl;
        return;
    }

    operation = input_line[op_pos];
    std::string operand1_str = input_line.substr(0, op_pos);
    std::string operand2_str = input_line.substr(op_pos + 1);

    // Convert string to uint32_t safely
    uint32_t operand1 = static_cast<uint32_t>(std::stoul(operand1_str));
    uint32_t operand2 = static_cast<uint32_t>(std::stoul(operand2_str));

    // Store the operands for later display
    last_operand1_ = operand1;
    last_operand2_ = operand2;

    // Prepare the payload: 1 byte for the operation, followed by 4 bytes for each operand
    std::vector<vsomeip::byte_t> payload_data(9);

    // Set the operation (1 byte)
    payload_data[0] = static_cast<vsomeip::byte_t>(operation);

    // Set the first operand (4 bytes)
    payload_data[1] = static_cast<vsomeip::byte_t>((operand1 >> 24) & 0xFF);
    payload_data[2] = static_cast<vsomeip::byte_t>((operand1 >> 16) & 0xFF);
    payload_data[3] = static_cast<vsomeip::byte_t>((operand1 >> 8) & 0xFF);
    payload_data[4] = static_cast<vsomeip::byte_t>(operand1 & 0xFF);

    // Set the second operand (4 bytes)
    payload_data[5] = static_cast<vsomeip::byte_t>((operand2 >> 24) & 0xFF);
    payload_data[6] = static_cast<vsomeip::byte_t>((operand2 >> 16) & 0xFF);
    payload_data[7] = static_cast<vsomeip::byte_t>((operand2 >> 8) & 0xFF);
    payload_data[8] = static_cast<vsomeip::byte_t>(operand2 & 0xFF);

    // Create a payload from the binary data
    std::shared_ptr<vsomeip::payload> its_payload = vsomeip::runtime::get()->create_payload();
    its_payload->set_data(payload_data);

    // Set the payload on the request message
    request_->set_payload(its_payload);

    if (!be_quiet_) {
        std::lock_guard<std::mutex> its_lock(mutex_);
        blocked_ = true;
        condition_.notify_one();
    }
}


    void run() {
        while (running_) {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (!blocked_) condition_.wait(its_lock);
            if (is_available_) {
                app_->send(request_);
                blocked_ = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
        }
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::message> request_;
    bool use_tcp_;
    bool be_quiet_;
    uint32_t cycle_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool running_;
    bool blocked_;
    bool is_available_;
    std::thread sender_;
    
    uint32_t last_operand1_; // To store the last operand1
    uint32_t last_operand2_; // To store the last operand2
};

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    client_sample *its_sample_ptr(nullptr);
    void handle_signal(int _signal) {
        if (its_sample_ptr != nullptr &&
                (_signal == SIGINT || _signal == SIGTERM))
            its_sample_ptr->stop();
    }
#endif

int main(int argc, char **argv) {
    bool use_tcp = false;
    bool be_quiet = false;
    uint32_t cycle = 1000;

    std::string tcp_enable("--tcp");
    std::string quiet_enable("--quiet");
    std::string cycle_arg("--cycle");

    int i = 1;
    while (i < argc) {
        if (tcp_enable == argv[i]) {
            use_tcp = true;
        } else if (quiet_enable == argv[i]) {
            be_quiet = true;
        } else if (cycle_arg == argv[i] && i+1 < argc) {
            i++;
            std::stringstream converter;
            converter << argv[i];
            converter >> cycle;
        }
        i++;
    }

    client_sample its_sample(use_tcp, be_quiet, cycle);
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