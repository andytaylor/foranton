/*
  Copyright (c) 2017 IG Group

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef connection_h
#define connection_h

#include <proton/messaging_handler.hpp>
#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/work_queue.hpp>

#include <chrono>
#include <mutex>
#include <iostream>
#include <queue>
#include <proton/message.hpp>
#include <proton/delivery.hpp>
#include <map>
#include <unordered_map>
#include <thread>

#include "metrics/metrics.hpp"
#include "consumer.hpp"
#include "producer.hpp"


// Lock output from threads to avoid scramblin
std::mutex out_lock;


#define OUT(x) do { std::lock_guard<std::mutex> l(out_lock); x; } while (false)



namespace ig {

    class connection : public proton::messaging_handler {

        std::mutex lock_;

        std::unordered_map<std::string, ig::consumer *> consumers;

        std::unordered_map<std::string, ig::producer *> producers;

        ig::Metrics *metrics_;

        // Invariant
        const std::string url_;
        const proton::connection_options connection_options_;

        proton::connection connection_;


    public:
        connection(const std::string &url, const proton::connection_options connection_options, ig::Metrics *metrics)
                : url_(url), connection_options_(connection_options), metrics_(metrics)
	 	{
            //OUT(std::cout << "just inside ig:connection::connection"  << std::endl);
		}

        // == messaging_handler overrides, only called in proton hander thread

        // Note: this example creates a connection when the container starts.
        // To create connections after the container has started, use
        // container::connect().
        // See @ref multithreaded_client_flow_control.cpp for an example.
        void on_container_start(proton::container &container) override {
            //OUT(std::cout << "just inside ig:connection::on_container_start "  << std::endl);
            container.connect(url_, connection_options_);
        }


        void on_connection_open(proton::connection &connection) override {
            //OUT(std::cout << "just inside ig:connection::on_connection_open "  << std::endl);
            connection_ = connection;
        }

        void on_message(proton::delivery &dlv, proton::message &msg) override {
            //OUT(std::cout << "just inside ig:connection::on_message "  << std::endl);
            std::lock_guard<std::mutex> l(lock_);
            ig::consumer *con = consumers[dlv.receiver().name()];
            if (con != NULL) {
                con->on_message(dlv, msg);
            }
        }


        void on_error(const proton::error_condition &e) override {
            //OUT(std::cerr << "unexpected error: " << e << std::endl);
            exit(1);
        }

        producer *create_producer(std::string address) {
            //OUT(std::cout << "just inside ig:connection::create_producer "  << std::endl);
            std::lock_guard<std::mutex> l(lock_);
            //OUT(std::cout << "ig:connection::create_producer guard locked"  << std::endl);
            while (!connection_) 
			{
            	//OUT(std::cout << "ig:connection::create_producer while !connection pre-sleep"  << std::endl);
				std::this_thread::sleep_for(std::chrono::seconds(1));
            	//OUT(std::cout << "ig:connection::create_producer while !connection post-sleep"  << std::endl);
			}
           	//OUT(std::cout << "ig:connection::create_producer done while about to new producer" << std::endl);
            producer *aProducer = new producer(connection_.open_sender(address));
           	//OUT(std::cout << "ig:connection::create_producer newed producer" << std::endl);
            producers[aProducer->sender().name()] = aProducer;
           	//OUT(std::cout << "ig:connection::create_producer added newed consumer to producers array" << std::endl);
            return aProducer;
        }


        consumer *create_consumer(std::string queue) {
            //OUT(std::cout << "just inside ig:connection::create_consumer "  << std::endl);
            //OUT(std::cout << "for: "  << queue << std::endl);
            std::lock_guard<std::mutex> l(lock_);
            //OUT(std::cout << "ig:connection::create_consumer guard locked"  << std::endl);
            while (!connection_) 
			{
            	//OUT(std::cout << "ig:connection::create_consumer while !connection pre-sleep"  << std::endl);
				std::this_thread::sleep_for(std::chrono::seconds(1));
            	//OUT(std::cout << "ig:connection::create_consumer while !connection post-sleep"  << std::endl);
			}
           	//OUT(std::cout << "ig:connection::create_consumer done while about to new consumer" << std::endl);
            consumer *aConsumer = new consumer(connection_.open_receiver(queue), metrics_);
           	//OUT(std::cout << "ig:connection::create_consumer newed consumer" << std::endl);
            consumers[aConsumer->receiver().name()] = aConsumer;
           	//OUT(std::cout << "ig:connection::create_consumer added newed consumer to consumers array" << std::endl);
            return aConsumer;
        }

        // Thread safe
        void close() {
            std::lock_guard<std::mutex> l(lock_);
            for (auto it : consumers) {
                it.second->close();
                delete (it.second);
            }
            consumers.clear();
            for (auto it : producers) {
                it.second->close();
                delete (it.second);
            }
            producers.clear();
            connection_.close();
        }
    };

} // namespace ig

#endif
