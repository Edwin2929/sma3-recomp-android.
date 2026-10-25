// Included inside namespace gbarecomp by the pinned engine's mod_runtime.cpp.
// Keep the established ZIP/TOML catalog implementation authoritative. This
// facade deliberately exposes only the one presentation plugin shipped by
// SMA3; arbitrary ROM patches and executable libraries are never loaded.
namespace {

constexpr std::uint64_t kProfileArchiveLimit = 2 * 1024 * 1024;
constexpr std::uint64_t kProfileExpandedLimit = 1024 * 1024;
constexpr std::uint64_t kProfileManifestLimit = 64 * 1024;
constexpr std::size_t kProfilePackageLimit = 64;
constexpr const char* kProfileGame = "sma3-usa";
constexpr const char* kProfileRom = "7352d2bd064d9ebaec579e264228aa21c7345b80";
constexpr const char* kProfilePlugin = "sma3.color-profile";
constexpr std::array<const char*, 9> kProfileOptions{
    "rr", "rg", "rb", "gr", "gg", "gb", "br", "bg", "bb"};

std::mutex profile_mutex;
std::string profile_warning;
std::map<std::string, std::string> rejected_profiles;
std::shared_ptr<const sma3::mods::ColorProfile> prepared_profile;

void profile_activate() {
    // This callback has no guest, PPU, hook, or audio side effects. The facade
    // invokes only this authorized plugin, avoiding generic activation resets.
    sma3::mods::publish(prepared_profile);
}

GBA_MOD_CONSTRUCTOR(register_sma3_color_profile) {
    gba_mod_register_activation_plugin(kProfilePlugin, profile_activate);
}

// Decode strictly, then escape all non-ASCII JSON. This also makes supplementary
// Unicode safe for JNI NewStringUTF, which accepts modified UTF-8 rather than
// standard UTF-8. Invalid diagnostic path bytes become the replacement glyph.
bool profile_codepoint(const std::string& input, std::size_t& at, std::uint32_t& cp) {
    const unsigned char first = input[at++];
    if (first < 0x80) { cp = first; return true; }
    int remaining = 0;
    std::uint32_t minimum = 0;
    if (first >= 0xc2 && first <= 0xdf) { remaining = 1; cp = first & 31; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { remaining = 2; cp = first & 15; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { remaining = 3; cp = first & 7; minimum = 0x10000; }
    else { cp = 0xfffd; return false; }
    if (input.size() - at < static_cast<std::size_t>(remaining)) { cp = 0xfffd; return false; }
    const std::size_t start = at;
    for (int i = 0; i < remaining; ++i) {
        const unsigned char byte = input[start + i];
        if ((byte & 0xc0) != 0x80) { cp = 0xfffd; return false; }
        cp = (cp << 6) | (byte & 63);
    }
    at += remaining;
    if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
        cp = 0xfffd;
        return false;
    }
    return true;
}

bool profile_text(const std::string& value, std::size_t maximum) {
    if (value.size() > maximum) return false;
    for (std::size_t at = 0; at < value.size();) {
        std::uint32_t cp = 0;
        if (!profile_codepoint(value, at, cp) ||
            (cp < 32 && cp != '\n' && cp != '\r' && cp != '\t'))
            return false;
    }
    return true;
}

std::string profile_json_string(const std::string& value) {
    std::string result = "\"";
    auto unicode = [&](std::uint32_t code) {
        constexpr const char* hex = "0123456789abcdef";
        result += "\\u";
        for (int shift = 12; shift >= 0; shift -= 4)
            result += hex[(code >> shift) & 15];
    };
    for (std::size_t at = 0; at < value.size();) {
        std::uint32_t cp = 0;
        profile_codepoint(value, at, cp);
        if (cp == '"' || cp == '\\') {
            result += '\\'; result += static_cast<char>(cp);
        } else if (cp < 32 || cp >= 127) {
            if (cp <= 0xffff) unicode(cp);
            else { cp -= 0x10000; unicode(0xd800 + (cp >> 10)); unicode(0xdc00 + (cp & 1023)); }
        } else result += static_cast<char>(cp);
    }
    return result + '"';
}

bool profile_schema(const Package& package, sma3::mods::ColorProfile* profile,
                    std::string* error) {
    if (package.targets.size() != 1 || package.targets[0].game_id != kProfileGame ||
        package.targets[0].rom_sha1 != kProfileRom) {
        set_error(error, "El perfil no corresponde a la version USA de SMA3 compatible.");
        return false;
    }
    if (package.features.size() != 1 || package.features[0].id != "colors" ||
        package.features[0].plugins != std::vector<std::string>{kProfilePlugin} ||
        !package.required_assets.empty() || package.options.size() > 9 ||
        package.save_compatibility != "shared") {
        set_error(error, "Solo se admite el plugin sma3.color-profile con la funcion colors, sin ROMs ni recursos externos.");
        return false;
    }
    if (!profile_text(package.name, 256) || !profile_text(package.author, 256) ||
        !profile_text(package.description, 4096) || !profile_text(package.license, 256)) {
        set_error(error, "Los metadatos del perfil deben ser texto UTF-8 de longitud limitada.");
        return false;
    }
    sma3::mods::ColorProfile result;
    for (const Option& option : package.options) {
        const auto position = std::find(kProfileOptions.begin(), kProfileOptions.end(), option.id);
        std::int64_t value = 0;
        if (position == kProfileOptions.end() || option.feature_id != "colors" ||
            option.type != OptionType::Integer || option.min_value != -200 ||
            option.max_value != 200 || option.step != 1 || !option.choices.empty() ||
            !parse_int(option.default_value, value) || value < -200 || value > 200) {
            set_error(error, "La matriz requiere rr,rg,rb,gr,gg,gb,br,bg,bb enteros entre -200 y 200; min=-200, max=200 y step=1.");
            return false;
        }
        result.matrix[static_cast<std::size_t>(position - kProfileOptions.begin())] = static_cast<int>(value);
    }
    if (profile) *profile = result;
    return true;
}

void profile_add_warning(const std::string& message) {
    if (profile_warning.size() < 8192) {
        if (!profile_warning.empty()) profile_warning += '\n';
        profile_warning += message;
    }
}

// Bounded, tolerant scan: a damaged installed profile can be removed from the
// manager and cannot prevent the unmodified game from starting.
bool scan_color_profiles(Runtime& runtime, std::string* error) {
    runtime.packages.clear();
    rejected_profiles.clear();
    profile_warning.clear();
    std::error_code ec;
    const fs::path packages = runtime.root / "packages";
    fs::create_directories(packages, ec);
    if (ec) { set_error(error, "No se puede abrir la carpeta de mods: " + ec.message()); return false; }
    std::size_t count = 0;
    for (fs::directory_iterator dirs(packages, ec), end; !ec && dirs != end; dirs.increment(ec)) {
        const fs::directory_entry& directory = *dirs;
        const std::string id = directory.path().filename().string();
        const auto status = directory.symlink_status(ec);
        if (ec) break;
        if (!fs::is_directory(status) || fs::is_symlink(status) || !valid_id(id)) continue;
        if (++count > kProfilePackageLimit) {
            profile_add_warning("Se alcanzo el limite de 64 perfiles instalados.");
            break;
        }
        std::size_t versions = 0;
        for (fs::directory_iterator items(directory.path(), ec), end2;
             !ec && items != end2; items.increment(ec)) {
            const auto item_status = items->symlink_status(ec);
            if (ec) break;
            if (!fs::is_directory(item_status) || fs::is_symlink(item_status)) continue;
            if (++versions > 8) { rejected_profiles[id] = "Demasiadas versiones instaladas."; break; }
            const fs::path manifest = items->path() / "manifest.toml";
            const auto manifest_status = fs::symlink_status(manifest, ec);
            if (ec) { ec.clear(); rejected_profiles[id] = "No se puede leer manifest.toml."; continue; }
            Package package;
            std::string problem;
            if (!fs::is_regular_file(manifest_status) || fs::is_symlink(manifest_status) ||
                !read_manifest(manifest, package, &problem) ||
                package.id != id || package.version != items->path().filename().string() ||
                !profile_schema(package, nullptr, &problem)) {
                rejected_profiles[id] = problem.empty() ? "Paquete danado o incompatible." : problem;
                continue;
            }
            package.root = items->path();
            // A package's default must never opt its author into activation.
            package.features[0].default_enabled = false;
            runtime.packages[id][package.version] = std::move(package);
        }
        if (versions == 0) rejected_profiles[id] = "El paquete no contiene una version valida.";
    }
    if (ec) { set_error(error, "No se puede leer el catalogo de mods: " + ec.message()); return false; }
    for (const auto& [id, problem] : rejected_profiles)
        profile_add_warning(id + ": " + problem);
    return true;
}

bool load_color_profile_state(Runtime& runtime, std::string* error) {
    runtime.selections.clear();
    const fs::path path = runtime.root / "state.toml";
    std::error_code ec;
    if (!fs::exists(path, ec) && !ec) return true;
    std::vector<std::uint8_t> bytes;
    if (ec || !read_file(path, bytes, error, 128 * 1024)) return false;
    std::istringstream input(std::string(bytes.begin(), bytes.end()));
    enum class Section { Root, Package, Feature } section = Section::Root;
    std::map<std::string, std::string> fields;
    std::map<std::string, PackageSelection> selections;
    std::set<std::string> feature_packages;
    auto finish = [&]() {
        if (section == Section::Root) {
            return fields.size() == 1 && fields.count("format_version") &&
                trim(fields.at("format_version")) == "1";
        }
        std::string id, version, package_id;
        bool enabled = false;
        if (section == Section::Package) {
            if (fields.size() != 2 || !fields.count("id") || !fields.count("version") ||
                !parse_string(fields.at("id"), id) || !valid_id(id) ||
                !parse_string(fields.at("version"), version) || !valid_version(version) ||
                selections.count(id) || selections.size() >= kProfilePackageLimit)
                return false;
            selections[id].version = version;
            return true;
        }
        if (fields.size() != 3 || !fields.count("package_id") || !fields.count("id") ||
            !fields.count("enabled") || !parse_string(fields.at("package_id"), package_id) ||
            !selections.count(package_id) || !parse_string(fields.at("id"), id) ||
            id != "colors" || !parse_bool(fields.at("enabled"), enabled) ||
            !feature_packages.insert(package_id).second)
            return false;
        auto& feature = selections[package_id].features["colors"];
        feature.enabled = enabled;
        feature.has_enabled = true;
        return true;
    };
    auto invalid = [&]() {
        set_error(error, "El archivo de seleccion de mods esta danado o usa un formato no compatible.");
        return false;
    };
    std::string raw;
    while (std::getline(input, raw)) {
        if (raw.find('\0') != std::string::npos) return invalid();
        const std::string line = trim(strip_comment(raw));
        if (line.empty()) continue;
        if (line == "[[package]]" || line == "[[feature]]") {
            if (!finish()) return invalid();
            fields.clear();
            section = line == "[[package]]" ? Section::Package : Section::Feature;
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos || !fields.emplace(trim(line.substr(0, equals)),
                                                         trim(line.substr(equals + 1))).second)
            return invalid();
    }
    if (!finish() || feature_packages.size() != selections.size()) return invalid();
    runtime.selections = std::move(selections);
    return true;
}

bool profile_is_enabled(Runtime& runtime, const Package& package) {
    return feature_enabled(runtime, package, package.features[0]);
}

std::string profile_response(bool ok, const std::string& error = {}) {
    Runtime& runtime = state();
    std::string output = std::string("{\"ok\":") + (ok ? "true" : "false") +
        ",\"error\":" + profile_json_string(error) +
        ",\"warning\":" + profile_json_string(profile_warning) + ",\"mods\":[";
    bool first = true;
    auto comma = [&] { if (!first) output += ','; first = false; };
    for (const auto& [id, versions] : runtime.packages) {
        (void)versions;
        const Package* package = selected_package(runtime, id);
        if (!package) continue;
        comma();
        output += "{\"id\":" + profile_json_string(id) +
            ",\"name\":" + profile_json_string(package->name) +
            ",\"version\":" + profile_json_string(package->version) +
            ",\"author\":" + profile_json_string(package->author) +
            ",\"description\":" + profile_json_string(package->description) +
            ",\"enabled\":" + (profile_is_enabled(runtime, *package) ? "true" : "false") +
            ",\"compatible\":true}";
    }
    for (const auto& [id, problem] : rejected_profiles) {
        if (runtime.packages.count(id)) continue;
        comma();
        output += "{\"id\":" + profile_json_string(id) + ",\"name\":" + profile_json_string(id) +
            ",\"version\":\"\",\"author\":\"\",\"description\":" + profile_json_string(problem) +
            ",\"enabled\":false,\"compatible\":false}";
    }
    return output + "]}";
}

void profile_disable_all(Runtime& runtime) {
    for (const auto& [id, versions] : runtime.packages) {
        (void)versions;
        const Package* package = selected_package(runtime, id);
        if (!package) continue;
        auto& feature = package_selection(runtime, *package).features["colors"];
        feature.enabled = false;
        feature.has_enabled = true;
    }
}

bool profile_commit(Runtime&& candidate, std::string* error) {
    std::shared_ptr<const sma3::mods::ColorProfile> next;
    std::size_t enabled = 0;
    candidate.committed_options.clear();
    candidate.committed_asset_paths.clear();
    for (const auto& [id, versions] : candidate.packages) {
        (void)versions;
        const Package* package = selected_package(candidate, id);
        if (!package) continue;
        sma3::mods::ColorProfile matrix;
        if (!profile_schema(*package, &matrix, error)) return false;
        if (!profile_is_enabled(candidate, *package)) continue;
        if (++enabled > 1) { set_error(error, "Solo puede haber un perfil de color activo."); return false; }
        if (!matrix.identity()) next = std::make_shared<const sma3::mods::ColorProfile>(matrix);
        for (const Option& option : package->options)
            candidate.committed_options[id]["colors"][option.id] = option.default_value;
    }
    candidate.validation = validate(candidate);
    if (!candidate.validation.ok) { set_error(error, "El perfil contiene un conflicto o una funcion no compatible."); return false; }
    candidate.committed = candidate.validation;
    // All allocations and validation precede the atomic state-file rename.
    // Never publish a frame matrix when persistence fails.
    if (!save_state(candidate, error)) return false;
    state() = std::move(candidate);
    prepared_profile = std::move(next);
    profile_activate();
    return true;
}

bool profile_ready(std::string* error) {
    if (state().initialized) return true;
    set_error(error, "El catalogo de mods aun no esta disponible. Vuelve a abrir este apartado cuando inicie el juego.");
    return false;
}

bool profile_entry_allowed(const ZipEntry& entry) {
    if (entry.directory) return false;
    if (entry.name == "manifest.toml") return entry.size <= kProfileManifestLimit;
    std::string name = entry.name;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name == "readme" || name == "readme.md" || name == "readme.txt" ||
        name == "license" || name == "license.md" || name == "license.txt";
}

fs::path profile_temporary(const char* prefix, std::string* error) {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned attempt = 0; attempt < 16; ++attempt) {
        const fs::path path = state().root / (std::string(prefix) + std::to_string(nonce) + "-" + std::to_string(attempt));
        std::error_code ec;
        if (fs::create_directory(path, ec)) return path;
        if (ec && ec != std::errc::file_exists) break;
    }
    set_error(error, "No se puede crear la carpeta temporal del mod.");
    return {};
}

struct ProfileTemporary {
    fs::path path;
    ~ProfileTemporary() { if (!path.empty()) { std::error_code ignored; fs::remove_all(path, ignored); } }
};

bool profile_restore_catalog(const fs::path& root, std::string* error) {
    prepared_profile.reset();
    sma3::mods::publish(nullptr);
    std::string problem;
    if (!mod_runtime_initialize(root, kProfileGame, kProfileRom, &problem)) {
        // A broken selection file must not block the base game or the manager.
        Runtime& runtime = state();
        runtime.selections.clear();
        if (!scan_color_profiles(runtime, &problem)) { set_error(error, problem); return false; }
        runtime.initialized = true;
        profile_add_warning("Se restablecio una seleccion de mods danada; todos los perfiles quedaron desactivados.");
    }
    Runtime candidate = state();
    std::map<std::string, PackageSelection> sanitized;
    unsigned active = 0;
    for (const auto& [id, versions] : candidate.packages) {
        (void)versions;
        const Package* package = selected_package(candidate, id);
        if (!package) continue;
        auto& selection = sanitized[id];
        selection.version = package->version;
        auto& feature = selection.features["colors"];
        feature.enabled = profile_is_enabled(candidate, *package);
        feature.has_enabled = true;
        if (feature.enabled) ++active;
    }
    candidate.selections = std::move(sanitized);
    if (active > 1) {
        profile_disable_all(candidate);
        profile_add_warning("Se desactivaron perfiles en conflicto; elige solo uno para continuar.");
    }
    if (!profile_commit(std::move(candidate), &problem)) {
        // In-memory fallback is deliberate on startup; it never rewrites or
        // deletes the prior persistent state after an I/O failure.
        profile_disable_all(state());
        profile_add_warning("Los mods quedaron desactivados: " + problem);
        set_error(error, problem);
        return false;
    }
    return true;
}

}  // namespace

bool sma3_mod_initialize(const fs::path& root, const fs::path& rom_path, std::string* error) {
    std::lock_guard<std::mutex> lock(profile_mutex);
    sma3::mods::publish(nullptr);
    prepared_profile.reset();
    state() = {};
    rejected_profiles.clear();
    profile_warning.clear();
    std::string digest;
    std::string problem;
    if (rom_path.empty() || !sha1_file(rom_path, digest, &problem) || digest != kProfileRom) {
        set_error(error, "No se pudo verificar la ROM de SMA3 para el catalogo de mods.");
        return false;
    }
    return profile_restore_catalog(root, error);
}

std::string sma3_mod_list() {
    std::lock_guard<std::mutex> lock(profile_mutex);
    std::string error;
    return profile_ready(&error) ? profile_response(true) : profile_response(false, error);
}

std::string sma3_mod_import(const fs::path& archive) {
    std::lock_guard<std::mutex> lock(profile_mutex);
    std::string error;
    if (!profile_ready(&error)) return profile_response(false, error);
    std::vector<std::uint8_t> bytes;
    std::vector<ZipEntry> entries;
    if (archive.extension() != ".gbamod")
        return profile_response(false, "Selecciona un archivo .gbamod compatible; no se admiten ROMs, IPS ni BPS.");
    if (!read_file(archive, bytes, &error, kProfileArchiveLimit) || !parse_zip(bytes, entries, &error))
        return profile_response(false, error);
    if (entries.empty() || entries.size() > 32)
        return profile_response(false, "El mod debe contener entre 1 y 32 entradas ZIP.");
    std::uint64_t expanded = 0;
    std::set<std::string> names;
    for (const ZipEntry& entry : entries) {
        expanded += entry.size;
        if (!profile_entry_allowed(entry) || !names.insert(entry.name).second || expanded > kProfileExpandedLimit)
            return profile_response(false, "El paquete solo puede contener manifest.toml (hasta 64 KiB) y README/LICENSE; limite total 1 MiB sin comprimir.");
    }
    if (!names.count("manifest.toml")) return profile_response(false, "El paquete no contiene manifest.toml en la raiz.");
    ProfileTemporary temporary{profile_temporary(".staging-profile-", &error)};
    if (temporary.path.empty() || !extract_zip(bytes, entries, temporary.path, &error))
        return profile_response(false, error);
    Package package;
    if (!read_manifest(temporary.path / "manifest.toml", package, &error) || !profile_schema(package, nullptr, &error))
        return profile_response(false, error);
    std::error_code ec;
    const fs::path package_directory = state().root / "packages" / package.id;
    if (fs::exists(package_directory, ec))
        return profile_response(false, "Ya existe un mod con ese identificador. Eliminalo antes de instalar otra version.");
    if (ec) return profile_response(false, "No se puede comprobar el catalogo: " + ec.message());
    if (state().packages.size() + rejected_profiles.size() >= kProfilePackageLimit)
        return profile_response(false, "Se alcanzo el limite de 64 perfiles. Elimina uno antes de importar otro.");
    const fs::path destination = package_directory / package.version;
    package.root = destination;
    package.features[0].default_enabled = false;
    Runtime candidate = state();
    candidate.packages[package.id][package.version] = package;
    auto& selection = candidate.selections[package.id];
    selection = {};
    selection.version = package.version;
    selection.features["colors"].enabled = false;
    selection.features["colors"].has_enabled = true;
    fs::create_directory(package_directory, ec);
    if (!ec) fs::rename(temporary.path, destination, ec);
    if (ec) {
        std::error_code ignored;
        fs::remove(package_directory, ignored);
        return profile_response(false, "No se pudo instalar el perfil: " + ec.message());
    }
    temporary.path = package_directory;  // Roll back all newly installed data on any failure.
    if (!profile_commit(std::move(candidate), &error)) {
        std::error_code ignored;
        fs::remove_all(destination, ignored);
        fs::remove(package_directory, ignored);
        return profile_response(false, error);
    }
    temporary.path.clear();
    return profile_response(true);
}

std::string sma3_mod_set_enabled(const std::string& id, bool enabled) {
    std::lock_guard<std::mutex> lock(profile_mutex);
    std::string error;
    if (!profile_ready(&error)) return profile_response(false, error);
    if (!valid_id(id)) return profile_response(false, "Identificador de mod no valido.");
    const Package* package = selected_package(state(), id);
    if (!package) return profile_response(false, "El perfil no esta instalado o no es compatible. Puedes eliminarlo.");
    // Re-read the installed manifest before enable: files replaced outside the
    // normal importer must not retain a stale, formerly accepted plan.
    Package verified;
    std::error_code ec;
    const fs::path manifest = package->root / "manifest.toml";
    if (enabled && (fs::is_symlink(fs::symlink_status(manifest, ec)) || ec ||
        !read_manifest(manifest, verified, &error) || verified.id != id ||
        verified.version != package->version || !profile_schema(verified, nullptr, &error)))
        return profile_response(false, error.empty() ? "El perfil instalado cambio o ya no es compatible." : error);
    Runtime candidate = state();
    if (enabled) {
        verified.root = package->root;
        verified.features[0].default_enabled = false;
        candidate.packages[id][verified.version] = std::move(verified);
        profile_disable_all(candidate);
    }
    auto& feature = candidate.selections[id].features["colors"];
    feature.enabled = enabled;
    feature.has_enabled = true;
    feature.values.clear();
    if (!profile_commit(std::move(candidate), &error)) return profile_response(false, error);
    return profile_response(true);
}

std::string sma3_mod_remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(profile_mutex);
    std::string error;
    if (!profile_ready(&error)) return profile_response(false, error);
    if (!valid_id(id)) return profile_response(false, "Identificador de mod no valido.");
    if (!state().packages.count(id) && !rejected_profiles.count(id))
        return profile_response(false, "El mod no esta instalado.");
    const fs::path directory = state().root / "packages" / id;
    std::error_code ec;
    const auto status = fs::symlink_status(directory, ec);
    if (ec || !fs::is_directory(status) || fs::is_symlink(status))
        return profile_response(false, "La carpeta del mod no es valida.");
    ProfileTemporary temporary{profile_temporary(".removed-profile-", &error)};
    if (temporary.path.empty()) return profile_response(false, error);
    Runtime candidate = state();
    candidate.packages.erase(id);
    candidate.selections.erase(id);
    // The move is reversible. Actual deletion happens only after successful
    // persistence and publication of the remaining profile (or identity).
    const fs::path parked = temporary.path / id;
    fs::rename(directory, parked, ec);
    if (ec) return profile_response(false, "No se pudo retirar el perfil: " + ec.message());
    bool committed = false;
    try {
        committed = profile_commit(std::move(candidate), &error);
    } catch (...) {
        fs::rename(parked, directory, ec);
        if (ec) temporary.path.clear();  // Preserve the backup even on allocation failure.
        throw;
    }
    if (!committed) {
        fs::rename(parked, directory, ec);
        if (ec) {
            // Keep the complete backup if restoration itself fails.
            temporary.path.clear();
            error += "; no se pudo restaurar la carpeta del perfil: " + ec.message();
        }
        return profile_response(false, error);
    }
    rejected_profiles.erase(id);
    profile_warning.clear();
    for (const auto& [bad_id, problem] : rejected_profiles) profile_add_warning(bad_id + ": " + problem);
    return profile_response(true);
}
