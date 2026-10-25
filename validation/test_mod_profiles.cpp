// A ROM-free driver for the real catalog/parser implementation. Including the
// engine translation unit exposes its internal restore seam only to this test;
// the production initializer always verifies the user's ROM before calling it.
#include "mod_runtime.cpp"

#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path root(argv[1]);
    std::string error;
    gbarecomp::profile_restore_catalog(root, &error);
    std::cout << gbarecomp::sma3_mod_list() << std::endl;
    std::string line;
    while (std::getline(std::cin, line)) {
        const auto split = line.find('\t');
        const std::string command = line.substr(0, split);
        const std::string argument = split == std::string::npos ? "" : line.substr(split + 1);
        if (command == "list") std::cout << gbarecomp::sma3_mod_list();
        else if (command == "import") std::cout << gbarecomp::sma3_mod_import(argument);
        else if (command == "enable") std::cout << gbarecomp::sma3_mod_set_enabled(argument, true);
        else if (command == "disable") std::cout << gbarecomp::sma3_mod_set_enabled(argument, false);
        else if (command == "remove") std::cout << gbarecomp::sma3_mod_remove(argument);
        else if (command == "reload") {
            gbarecomp::profile_restore_catalog(root, &error);
            std::cout << gbarecomp::sma3_mod_list();
        } else if (command == "bad-rom") {
            gbarecomp::sma3_mod_initialize(root, root / "nonexistent.gba", &error);
            std::cout << gbarecomp::sma3_mod_list();
        } else if (command == "pixels") {
            const std::vector<std::uint8_t> source{17, 83, 129, 255, 255, 255, 0, 0, 0};
            const auto original = source;
            std::vector<std::uint8_t> scratch;
            const auto* result = sma3::mods::apply(source.data(), 3, 1, scratch);
            std::cout << "{\"rgb\":[";
            for (std::size_t i = 0; i < source.size(); ++i) {
                if (i) std::cout << ',';
                std::cout << static_cast<unsigned>(result[i]);
            }
            std::cout << "],\"source_unchanged\":" << (source == original ? "true" : "false") << '}';
        } else if (command == "snapshot-race") {
            const auto prior = sma3::mods::snapshot();
            auto first = std::make_shared<sma3::mods::ColorProfile>();
            first->matrix = {0, 100, 0, 0, 0, 100, 100, 0, 0};
            auto second = std::make_shared<sma3::mods::ColorProfile>();
            second->matrix = {0, 0, 100, 100, 0, 0, 0, 100, 0};
            std::atomic<bool> done{false};
            sma3::mods::publish(first);
            std::thread worker([&] {
                for (unsigned i = 0; i < 10000; ++i) sma3::mods::publish(i & 1 ? first : second);
                done.store(true);
            });
            std::vector<std::uint8_t> source(4096 * 3), scratch;
            for (std::size_t i = 0; i < 4096; ++i) {
                source[3 * i] = 17; source[3 * i + 1] = 83; source[3 * i + 2] = 129;
            }
            bool consistent = true;
            unsigned frames = 0;
            do {
                const auto* pixels = sma3::mods::apply(source.data(), 256, 16, scratch);
                const bool is_first = pixels[0] == 83 && pixels[1] == 129 && pixels[2] == 17;
                const bool is_second = pixels[0] == 129 && pixels[1] == 17 && pixels[2] == 83;
                consistent &= is_first || is_second;
                for (std::size_t i = 1; i < 4096; ++i)
                    consistent &= pixels[3 * i] == pixels[0] && pixels[3 * i + 1] == pixels[1] && pixels[3 * i + 2] == pixels[2];
                ++frames;
            } while (!done.load() || frames < 100);
            worker.join();
            sma3::mods::publish(prior);
            std::cout << "{\"consistent\":" << (consistent ? "true" : "false") << ",\"frames\":" << frames << '}';
        } else return 3;
        std::cout << std::endl;
    }
}
