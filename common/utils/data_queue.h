#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace NATBuster::Common::Utils {
    template<typename T>
    class DataQueue {
        std::queue<T> data;
        std::mutex data_lock;

        std::condition_variable sleeper;

    public:
        DataQueue() {

        }

        void add(T elem) {
            //Add to queue
            {
                const std::lock_guard<std::mutex> lock(data_lock);
                data.push(elem);
            }

            //Wake a consumer
            sleeper.notify_one();
        }

        T get() {
            std::unique_lock(data_lock);

            sleeper.wait(data_lock, []() {return data.size() != 0});

            return data.pop();
        }

        size_t count() {
            std::unique_lock(data_lock);

            return data.size();
        }
    };
};