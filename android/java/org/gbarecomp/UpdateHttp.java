package org.gbarecomp;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Locale;

/** Bounded HTTPS only. Redirects never forward credentials or leave GitHub's download hosts. */
final class UpdateHttp {
    interface ConnectionFactory { HttpURLConnection open(URL url) throws IOException; }
    interface Progress { void bytes(long complete, long total); }
    static final class Failure extends IOException {
        final String kind;
        final long retryAt;
        Failure(String kind) { this(kind, 0); }
        Failure(String kind, long retryAt) { super(kind); this.kind = kind; this.retryAt = retryAt; }
    }
    static final class Cancel {
        private volatile boolean cancelled;
        private volatile HttpURLConnection connection;
        void cancel() { cancelled = true; HttpURLConnection c = connection; if (c != null) c.disconnect(); }
        void check() throws Failure {
            if (cancelled || Thread.currentThread().isInterrupted()) throw new Failure("cancelled");
        }
    }
    static final class Metadata {
        final byte[] bytes;
        final String etag;
        final boolean unchanged;
        Metadata(byte[] bytes, String etag, boolean unchanged) {
            this.bytes = bytes; this.etag = etag; this.unchanged = unchanged;
        }
    }
    private final ConnectionFactory factory;
    UpdateHttp() { this(url -> (HttpURLConnection) url.openConnection()); }
    UpdateHttp(ConnectionFactory factory) { this.factory = factory; }

    private HttpURLConnection open(String address, boolean metadata, String name, String etag,
            Cancel cancel) throws IOException {
        URL url = new URL(address);
        for (int count = 0; count <= 5; ++count) {
            cancel.check();
            if (!UpdatePolicy.safeRedirect(url.toExternalForm(), metadata, name)) throw new Failure("source");
            HttpURLConnection connection = factory.open(url);
            cancel.connection = connection;
            connection.setInstanceFollowRedirects(false);
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(15000);
            connection.setUseCaches(false);
            connection.setRequestProperty("User-Agent", "SMA3-Android-Updater");
            connection.setRequestProperty("Accept", metadata ? "application/vnd.github+json" : "application/octet-stream");
            connection.setRequestProperty("Accept-Encoding", "identity");
            if (metadata) {
                connection.setRequestProperty("X-GitHub-Api-Version", "2022-11-28");
                connection.setRequestProperty("Cache-Control", "no-cache");
                if (etag != null && etag.length() <= 256 && etag.indexOf('\n') < 0 && etag.indexOf('\r') < 0)
                    connection.setRequestProperty("If-None-Match", etag);
            }
            int status;
            try { status = connection.getResponseCode(); }
            catch (IOException error) { connection.disconnect(); cancel.check(); throw error; }
            if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
                String location = connection.getHeaderField("Location");
                connection.disconnect();
                if (location == null || count == 5) throw new Failure("source");
                url = new URL(url, location);
                continue;
            }
            if (status != 200 && !(metadata && status == 304)) {
                long retryAt = 0;
                if (status == 403 || status == 429) {
                    long now = System.currentTimeMillis();
                    retryAt = now + 30 * 60 * 1000;
                    try {
                        String retry = connection.getHeaderField("Retry-After");
                        if (retry != null) retryAt = now + Math.min(6 * 60 * 60, Math.max(60, Long.parseLong(retry))) * 1000;
                        else {
                            long reset = Long.parseLong(connection.getHeaderField("X-RateLimit-Reset")) * 1000;
                            retryAt = Math.min(now + 6 * 60 * 60 * 1000, Math.max(now + 60000, reset));
                        }
                    } catch (NumberFormatException ignored) {}
                }
                connection.disconnect();
                throw new Failure(status == 404 ? "missing" : retryAt > 0 ? "rate" : "network", retryAt);
            }
            cancel.check();
            return connection;
        }
        throw new Failure("source");
    }

    Metadata metadata(String etag, Cancel cancel) throws IOException {
        HttpURLConnection connection = open(UpdatePolicy.RELEASE_API, true, "", etag, cancel);
        try {
            String receivedTag = connection.getHeaderField("ETag");
            if (receivedTag == null || receivedTag.length() > 256 || receivedTag.indexOf('\n') >= 0 ||
                    receivedTag.indexOf('\r') >= 0) receivedTag = "";
            if (connection.getResponseCode() == 304) return new Metadata(null, receivedTag, true);
            if (connection.getContentLengthLong() > UpdatePolicy.MAX_METADATA_BYTES) throw new Failure("metadata");
            ByteArrayOutputStream output = new ByteArrayOutputStream();
            long deadline = System.nanoTime() + 60L * 1000 * 1000 * 1000;
            try (InputStream input = connection.getInputStream()) {
                byte[] buffer = new byte[16384]; int count;
                while ((count = input.read(buffer)) != -1) {
                    cancel.check();
                    if (System.nanoTime() > deadline || count == 0) throw new Failure("network");
                    if (output.size() > UpdatePolicy.MAX_METADATA_BYTES - count) throw new Failure("metadata");
                    output.write(buffer, 0, count);
                }
            }
            return new Metadata(output.toByteArray(), receivedTag, false);
        } finally { connection.disconnect(); cancel.connection = null; }
    }

    String download(UpdatePolicy.Asset asset, File partial, Cancel cancel, Progress progress) throws IOException {
        HttpURLConnection connection = open(asset.url, false, asset.name, null, cancel);
        boolean completed = false;
        try {
            long declared = connection.getContentLengthLong();
            if (declared >= 0 && declared != asset.size) throw new Failure("size");
            MessageDigest digest = sha256();
            long size = 0, deadline = System.nanoTime() + 10L * 60 * 1000 * 1000 * 1000;
            try (InputStream input = connection.getInputStream(); FileOutputStream output = new FileOutputStream(partial)) {
                byte[] buffer = new byte[65536]; int count;
                while ((count = input.read(buffer)) != -1) {
                    cancel.check();
                    if (System.nanoTime() > deadline || count == 0) throw new Failure("network");
                    if (size > asset.size - count || size > UpdatePolicy.MAX_APK_BYTES - count) throw new Failure("size");
                    output.write(buffer, 0, count); digest.update(buffer, 0, count); size += count;
                    progress.bytes(size, asset.size);
                }
                cancel.check();
                if (size != asset.size) throw new Failure("size");
                output.getFD().sync();
            }
            String actual = hex(digest.digest());
            if (!asset.digest.isEmpty() && !asset.digest.equals(actual)) throw new Failure("digest");
            completed = true;
            return actual;
        } finally {
            connection.disconnect(); cancel.connection = null;
            if (!completed && partial.exists()) partial.delete();
        }
    }

    static MessageDigest sha256() {
        try { return MessageDigest.getInstance("SHA-256"); }
        catch (NoSuchAlgorithmException impossible) { throw new IllegalStateException(impossible); }
    }
    static String hex(byte[] bytes) {
        StringBuilder result = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) result.append(String.format(Locale.ROOT, "%02x", value & 255));
        return result.toString();
    }
}
