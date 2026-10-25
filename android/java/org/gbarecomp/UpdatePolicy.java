package org.gbarecomp;

import java.net.URI;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Pure policy shared by Android and host tests. No trust is placed in a tag name. */
final class UpdatePolicy {
    static final String REPOSITORY = "Edwin2929/sma3-recomp-android.";
    static final String RELEASE_API = "https://api.github.com/repos/" + REPOSITORY + "/releases/tags/Descarga";
    static final String RELEASE_PAGE = "https://github.com/" + REPOSITORY + "/releases/tag/Descarga";
    static final String DOWNLOAD_PREFIX = "https://github.com/" + REPOSITORY + "/releases/download/Descarga/";
    static final long MAX_APK_BYTES = 128L * 1024 * 1024;
    static final int MAX_METADATA_BYTES = 2 * 1024 * 1024;
    static final long CHECK_INTERVAL_MS = 6L * 60 * 60 * 1000;
    static final long FAILURE_BACKOFF_MS = 15L * 60 * 1000;
    private static final Pattern VERSION = Pattern.compile(
        "(0|[1-9][0-9]{0,5})\\.(0|[1-9][0-9]{0,5})(?:\\.(0|[1-9][0-9]{0,5}))?(?:-([a-z0-9]+(?:[.-][a-z0-9]+)*))?");
    private static final Pattern ASSET = Pattern.compile("sma3-recomp-(.+)-arm64\\.apk");

    private UpdatePolicy() {}

    static final class Version implements Comparable<Version> {
        final int major, minor, patch;
        final String suffix, text;
        private Version(int major, int minor, int patch, String suffix, String text) {
            this.major = major; this.minor = minor; this.patch = patch;
            this.suffix = suffix; this.text = text;
        }
        static Version parse(String text) {
            if (text == null || text.length() > 80) return null;
            Matcher m = VERSION.matcher(text);
            if (!m.matches()) return null;
            return new Version(Integer.parseInt(m.group(1)), Integer.parseInt(m.group(2)),
                m.group(3) == null ? 0 : Integer.parseInt(m.group(3)), m.group(4), text);
        }
        @Override public int compareTo(Version other) {
            int value = Integer.compare(major, other.major);
            if (value == 0) value = Integer.compare(minor, other.minor);
            if (value == 0) value = Integer.compare(patch, other.patch);
            if (value != 0) return value;
            if (suffix == null) return other.suffix == null ? 0 : 1;
            if (other.suffix == null) return -1;
            String[] a = suffix.split("[.-]"), b = other.suffix.split("[.-]");
            for (int i = 0; i < Math.min(a.length, b.length); ++i) {
                boolean na = a[i].matches("[0-9]+"), nb = b[i].matches("[0-9]+");
                if (na && nb) {
                    String x = a[i].replaceFirst("^0+(?!$)", "");
                    String y = b[i].replaceFirst("^0+(?!$)", "");
                    value = Integer.compare(x.length(), y.length());
                    if (value == 0) value = x.compareTo(y);
                } else if (na != nb) value = na ? -1 : 1;
                else value = a[i].compareTo(b[i]);
                if (value != 0) return value;
            }
            return Integer.compare(a.length, b.length);
        }
    }

    static final class Asset {
        final long id, size;
        final String name, url, digest;
        final Version version;
        private Asset(long id, long size, String name, String url, String digest, Version version) {
            this.id = id; this.size = size; this.name = name; this.url = url;
            this.digest = digest; this.version = version;
        }
        static Asset checked(long id, long size, String name, String url, String digest, String state) {
            if (id <= 0 || size <= 0 || size > MAX_APK_BYTES || name == null || name.length() > 120 ||
                    !"uploaded".equals(state)) return null;
            Matcher match = ASSET.matcher(name);
            if (!match.matches()) return null;
            Version version = Version.parse(match.group(1));
            if (version == null || !safeDownloadUrl(url, name)) return null;
            String sha = "";
            if (digest != null && !digest.isEmpty()) {
                if (!digest.matches("sha256:[0-9a-fA-F]{64}")) return null;
                sha = digest.substring(7).toLowerCase(Locale.ROOT);
            }
            return new Asset(id, size, name, url, sha, version);
        }
    }

    static Asset newest(List<Asset> assets, Version installed) {
        if (installed == null) return null;
        Asset best = null;
        for (Asset asset : assets) {
            if (asset == null || asset.version.compareTo(installed) <= 0) continue;
            if (best == null || asset.version.compareTo(best.version) > 0 ||
                    (asset.version.compareTo(best.version) == 0 && asset.id > best.id)) best = asset;
        }
        return best;
    }

    private static URI secure(String value) {
        try {
            if (value == null || value.length() > 8192) return null;
            URI uri = new URI(value);
            if (!"https".equalsIgnoreCase(uri.getScheme()) || uri.getHost() == null ||
                    uri.getRawUserInfo() != null || uri.getRawFragment() != null ||
                    (uri.getPort() != -1 && uri.getPort() != 443)) return null;
            return uri;
        } catch (Exception error) { return null; }
    }

    static boolean safeDownloadUrl(String url, String name) {
        URI uri = secure(url);
        return uri != null && url.equals(DOWNLOAD_PREFIX + name) && uri.getRawQuery() == null;
    }

    static boolean safeRedirect(String url, boolean metadata, String originalAssetName) {
        URI uri = secure(url);
        if (uri == null) return false;
        if (metadata) return RELEASE_API.equals(url);
        String host = uri.getHost().toLowerCase(Locale.ROOT);
        if (host.equals("github.com")) return safeDownloadUrl(url, originalAssetName);
        return (host.equals("release-assets.githubusercontent.com") ||
                host.equals("objects.githubusercontent.com") ||
                host.equals("github-releases.githubusercontent.com")) &&
            uri.getRawPath() != null && uri.getRawPath().startsWith("/") &&
            uri.getRawPath().length() > 1;
    }

    static boolean shouldCheck(long now, long nextCheck, boolean auto) {
        // A moved-back wall clock must not disable checks indefinitely.
        return auto && (nextCheck <= now || nextCheck - now > CHECK_INTERVAL_MS);
    }

    static boolean isArm64LibraryHeader(byte[] elf) {
        return elf != null && elf.length >= 20 &&
            elf[0] == 0x7f && elf[1] == 'E' && elf[2] == 'L' && elf[3] == 'F' &&
            elf[4] == 2 && elf[5] == 1 && (elf[18] & 255) == 183 && elf[19] == 0;
    }

    static String identityError(String installedPackage, long installedCode, List<String> installedSigners,
            String archivePackage, long archiveCode, List<String> archiveSigners,
            String archiveVersion, Version advertisedVersion, int minimumSdk, int deviceSdk,
            boolean hasArm64Game) {
        if (!installedPackage.equals(archivePackage)) return "package";
        if (archiveCode <= installedCode) return "older";
        Version actual = Version.parse(archiveVersion);
        if (actual == null || advertisedVersion == null || actual.compareTo(advertisedVersion) != 0)
            return "version";
        if (minimumSdk > deviceSdk || !hasArm64Game) return "platform";
        if (installedSigners == null || archiveSigners == null || installedSigners.isEmpty() ||
                archiveSigners.isEmpty()) return "signature";
        List<String> a = new ArrayList<>(installedSigners), b = new ArrayList<>(archiveSigners);
        Collections.sort(a); Collections.sort(b);
        return a.equals(b) ? null : "signature";
    }
}
