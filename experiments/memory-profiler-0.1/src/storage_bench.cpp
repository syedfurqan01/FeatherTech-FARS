#include "fars/storage_bench.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace fars {

StorageBenchResult run_storage_bench(const std::string& directory, std::size_t size_mib) {
    StorageBenchResult result;
    result.bytes_tested = size_mib * 1024ULL * 1024ULL;

    try {
        fs::path dir(directory);
        if (!fs::exists(dir) || !fs::is_directory(dir)) {
            result.error = "Directory does not exist or is not a directory";
            return result;
        }

        auto tmp = dir / ("fars_storage_bench_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp");
        result.path_used = tmp.string();

        const std::size_t chunk = 1 * 1024 * 1024; // 1 MiB
        std::vector<char> buffer(chunk);
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist(0, 255);
        for (auto& b : buffer) b = static_cast<char>(dist(rng));

        // Sequential write
        {
            auto t0 = std::chrono::steady_clock::now();
            std::ofstream out(tmp, std::ios::binary);
            if (!out) {
                result.error = "Failed to open temporary file for writing";
                return result;
            }
            std::size_t remaining = result.bytes_tested;
            while (remaining > 0) {
                std::size_t n = std::min(chunk, remaining);
                out.write(buffer.data(), static_cast<std::streamsize>(n));
                remaining -= n;
            }
            out.close();
            auto t1 = std::chrono::steady_clock::now();
            double sec = std::chrono::duration<double>(t1 - t0).count();
            if (sec > 0.0) {
                result.sequential_write_mib_s = (static_cast<double>(size_mib) / sec);
            }
        }

        // Sequential read
        {
            auto t0 = std::chrono::steady_clock::now();
            std::ifstream in(tmp, std::ios::binary);
            if (!in) {
                result.error = "Failed to open temporary file for reading";
                fs::remove(tmp);
                return result;
            }
            std::size_t remaining = result.bytes_tested;
            while (remaining > 0) {
                std::size_t n = std::min(chunk, remaining);
                in.read(buffer.data(), static_cast<std::streamsize>(n));
                remaining -= n;
            }
            in.close();
            auto t1 = std::chrono::steady_clock::now();
            double sec = std::chrono::duration<double>(t1 - t0).count();
            if (sec > 0.0) {
                result.sequential_read_mib_s = (static_cast<double>(size_mib) / sec);
            }
        }

        fs::remove(tmp);
    } catch (const std::exception& e) {
        result.error = e.what();
    }

    return result;
}

} // namespace fars