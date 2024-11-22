#include <iostream>
#include <list>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <algorithm> 
// #include <boost/thread/shared_mutex.hpp>  // Include Boost's shared_mutex
// #include <boost/thread/locks.hpp>
#include "datatypes.hpp"
#include <functional>


class HashTable {

    private:

        struct Bucket {
            std::list<std::string> items;
            std::shared_mutex lock;
            // boost::shared_mutex lock;
        };
        int tableSize;
        std::vector<Bucket> table;

        // uint32_t hashFunction(std::string input) {
        //     uint32_t index = 0;
        //     for (char c: input) { index += c; }
        //     return index % tableSize;
        // }

        uint32_t hashFunction(std::string input) {
            std::hash<std::string> hasher;
            uint64_t hashed_value = hasher(input);
            return (uint32_t)(hashed_value % tableSize);
        }


    public:

        HashTable(int size): tableSize(size), table(size) {}
        ~HashTable(){};

        void insert(const std::string& input_string) {
            uint32_t index = hashFunction(input_string);
            std::unique_lock<std::shared_mutex> lock(table[index].lock);
            // boost::unique_lock<boost::shared_mutex> lock(table[index].lock);  
            table[index].items.emplace_back(input_string);
        }

        bool read(const std::string& input_string) {
            uint32_t index = hashFunction(input_string);
            std::shared_lock<std::shared_mutex> lock(table[index].lock);
            // boost::shared_lock<boost::shared_mutex> lock(table[index].lock); 
            for (const auto& item : table[index].items) {
                if (item == input_string) {
                    return true;
                }
            }
            return false;
        }

        void remove(const std::string& input_string) {
            uint32_t index = hashFunction(input_string);
            std::unique_lock<std::shared_mutex> lock(table[index].lock);
            // boost::unique_lock<boost::shared_mutex> lock(table[index].lock); 
            auto iteration = std::find(table[index].items.begin(), table[index].items.end(), input_string);
            if (iteration != table[index].items.end()) {table[index].items.erase(iteration);};
        }

};