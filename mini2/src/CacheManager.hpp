#ifndef CACHE_MANAGER_HPP
#define CACHE_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <chrono>
#include <iostream>
#include "dataserver.pb.h"

using mini2::DataChunk;

struct CacheEntry {
    std::deque<DataChunk> chunks;
    std::chrono::steady_clock::time_point timestamp;
    int hit_count;
    
    CacheEntry() : hit_count(0) {
        timestamp = std::chrono::steady_clock::now();
    }
};

class CacheManager {
public:
    CacheManager(int max_size = 100, int ttl_seconds = 300) 
        : max_size_(max_size), ttl_seconds_(ttl_seconds) {
        std::cout << "CacheManager: Initialized with max_size=" << max_size 
                  << ", TTL=" << ttl_seconds << "s" << std::endl;
    }

    bool isCached(const std::string& query) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(query);
        if (it == cache_.end()) {
            return false;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.timestamp).count();
        
        if (age > ttl_seconds_) {
            std::cout << "CacheManager: Cache EXPIRED for query: " << query 
                      << " (age: " << age << "s)" << std::endl;
            cache_.erase(it);
            return false;
        }
        
        return true;
    }

    bool getCachedChunks(const std::string& query, std::deque<DataChunk>& chunks) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(query);
        if (it == cache_.end()) {
            return false;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.timestamp).count();
        
        if (age > ttl_seconds_) {
            cache_.erase(it);
            return false;
        }
        
        chunks = it->second.chunks;
        it->second.hit_count++;
        
        std::cout << "CacheManager: ✅ CACHE HIT for query: " << query 
                  << " (hit_count: " << it->second.hit_count 
                  << ", age: " << age << "s)" << std::endl;
        
        return true;
    }

    void cacheChunks(const std::string& query, const std::deque<DataChunk>& chunks) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (cache_.size() >= max_size_) {
            evictOldest();
        }
        
        CacheEntry entry;
        entry.chunks = chunks;
        entry.timestamp = std::chrono::steady_clock::now();
        entry.hit_count = 0;
        
        cache_[query] = entry;
        
        std::cout << "CacheManager: 💾 CACHED query: " << query 
                  << " (" << chunks.size() << " chunks)" << std::endl;
    }

    void printStats() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "📊 CACHE STATISTICS" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Cache size: " << cache_.size() << "/" << max_size_ << std::endl;
        std::cout << "TTL: " << ttl_seconds_ << "s" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        if (cache_.empty()) {
            std::cout << "Cache is empty" << std::endl;
        } else {
            std::cout << "Cached queries:" << std::endl;
            for (const auto& pair : cache_) {
                auto age = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - pair.second.timestamp).count();
                
                std::cout << "  - " << pair.first 
                          << " (chunks: " << pair.second.chunks.size()
                          << ", hits: " << pair.second.hit_count
                          << ", age: " << age << "s)" << std::endl;
            }
        }
        std::cout << "========================================\n" << std::endl;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        std::cout << "CacheManager: Cache cleared" << std::endl;
    }

    void invalidate(const std::string& query) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(query);
        if (it != cache_.end()) {
            cache_.erase(it);
            std::cout << "CacheManager: Invalidated cache for: " << query << std::endl;
        }
    }

private:
    void evictOldest() {
        if (cache_.empty()) return;
        
        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->second.timestamp < oldest->second.timestamp) {
                oldest = it;
            }
        }
        
        std::cout << "CacheManager: Evicting oldest entry: " << oldest->first << std::endl;
        cache_.erase(oldest);
    }

    std::unordered_map<std::string, CacheEntry> cache_;
    std::mutex mutex_;
    size_t max_size_;
    int ttl_seconds_;
};

#endif // CACHE_MANAGER_HPP