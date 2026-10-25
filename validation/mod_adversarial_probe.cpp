// Small stdin/stdout bridge to the actual native catalog. Python owns the
// hostile ZIP fixtures and filesystem fault injection; no ROM is embedded.
#include "mod_profiles.h"
#include "mod_color_profile.h"
#include "mod_runtime.h"
#include <array>
#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::vector<std::string> fields;
        std::istringstream input(line);
        std::string field;
        while (std::getline(input, field, '\t')) fields.push_back(field);
        if (!line.empty() && line.back() == '\t') fields.emplace_back();
        if (fields.empty()) continue;
        const auto& command = fields[0];
        if (command == "quit") break;
        if (command == "init" && fields.size() == 3) {
            std::string error;
            bool initialized = gbarecomp::sma3_mod_initialize(fields[1], fields[2], &error);
            std::cout << "{\"initialized\":" << (initialized ? "true" : "false")
                      << ",\"catalog\":" << gbarecomp::sma3_mod_list() << "}";
        } else if (command == "raw-init" && fields.size() == 2) {
            std::string error;
            bool initialized = gbarecomp::mod_runtime_initialize(fields[1], "sma3-usa",
                "7352d2bd064d9ebaec579e264228aa21c7345b80", &error);
            std::cout << "{\"initialized\":" << (initialized ? "true" : "false")
                      << ",\"catalog\":" << gbarecomp::sma3_mod_list() << "}";
        } else if (command == "list") std::cout << gbarecomp::sma3_mod_list();
        else if (command == "import" && fields.size() == 2)
            std::cout << gbarecomp::sma3_mod_import(fields[1]);
        else if ((command == "enable" || command == "disable") && fields.size() == 2)
            std::cout << gbarecomp::sma3_mod_set_enabled(fields[1], command == "enable");
        else if (command == "remove" && fields.size() == 2)
            std::cout << gbarecomp::sma3_mod_remove(fields[1]);
        else if (command == "concurrent" && fields.size() == 3) {
            const auto first = gbarecomp::sma3_mod_set_enabled(fields[1], true);
            const auto profile_a = sma3::mods::snapshot();
            const auto second = gbarecomp::sma3_mod_set_enabled(fields[2], true);
            const auto profile_b = sma3::mods::snapshot();
            std::array<std::uint8_t, 3> a{}, b{};
            const std::array<std::uint8_t, 3> pixel{120,150,200};
            if (!profile_a || !profile_b || first.find("\"ok\":true") == std::string::npos ||
                second.find("\"ok\":true") == std::string::npos) return 2;
            profile_a->map(pixel.data(), a.data(), 1);
            profile_b->map(pixel.data(), b.data(), 1);
            std::vector<std::uint8_t> source(240*160*3), scratch;
            for (std::size_t i=0;i<source.size();++i) source[i]=pixel[i%3];
            std::atomic<bool> started{false}, done{false}, good{true};
            std::thread worker([&] {
                started=true;
                for (unsigned i=0;i<100;++i) {
                    const auto response=gbarecomp::sma3_mod_set_enabled(fields[1+i%2],true);
                    if (response.find("\"ok\":true")==std::string::npos) good=false;
                    std::this_thread::yield();
                }
                done=true;
            });
            while (!started) std::this_thread::yield();
            unsigned frames=0;
            do {
                const auto* result=sma3::mods::apply(source.data(),240,160,scratch);
                const auto& expected = std::equal(a.begin(),a.end(),result) ? a : b;
                for (std::size_t i=0;i<source.size();++i)
                    if(result[i]!=expected[i%3]) good=false;
                ++frames;
            } while (!done || frames<100);
            worker.join();
            std::cout << "{\"coherent\":" << (good ? "true" : "false")
                      << ",\"frames\":" << frames << ",\"catalog_changes\":100}";
        } else if (command == "pixel") {
            const std::array<std::uint8_t, 9> source{120, 150, 200, 255, 1, 2, 0, 255, 99};
            std::vector<std::uint8_t> scratch;
            const auto* output = sma3::mods::apply(source.data(), 3, 1, scratch);
            std::cout << "{\"pixel\":[";
            for (std::size_t i = 0; i < source.size(); ++i) {
                if (i) std::cout << ',';
                std::cout << static_cast<unsigned>(output[i]);
            }
            std::cout << "],\"identity_pointer\":"
                      << (output == source.data() ? "true" : "false") << "}";
        } else std::cout << "{\"probe_error\":\"unknown command\"}";
        std::cout << '\n' << std::flush;
    }
}
