package org.gbarecomp;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Host tests exercise the production selection, identity and bounded HTTP implementation. */
public final class UpdatePolicyTest {
    private static int checks;
    private static void check(boolean condition, String description) {
        ++checks; if (!condition) throw new AssertionError(description);
    }
    private static UpdatePolicy.Asset asset(long id, String version, byte[] bytes, String digest) {
        String name = "sma3-recomp-" + version + "-arm64.apk";
        return UpdatePolicy.Asset.checked(id, bytes.length, name, UpdatePolicy.DOWNLOAD_PREFIX + name, digest, "uploaded");
    }
    private static final class Response extends HttpURLConnection {
        final int status; final byte[] payload; final Map<String,String> headers = new HashMap<>();
        boolean disconnected; long declared;
        Response(String url, int status, byte[] payload) throws Exception {
            super(new URL(url)); this.status = status; this.payload = payload; declared = payload.length;
        }
        Response header(String key, String value) { headers.put(key, value); return this; }
        @Override public void disconnect() { disconnected = true; }
        @Override public boolean usingProxy() { return false; }
        @Override public void connect() {}
        @Override public int getResponseCode() { return status; }
        @Override public String getHeaderField(String key) { return headers.get(key); }
        @Override public long getContentLengthLong() { return declared; }
        @Override public InputStream getInputStream() { return new ByteArrayInputStream(payload); }
    }
    private static UpdateHttp http(Response... responses) {
        int[] index = {0};
        return new UpdateHttp(url -> {
            if (index[0] >= responses.length) throw new AssertionError("Unexpected extra HTTP call");
            Response response = responses[index[0]++];
            check(response.getURL().equals(url), "expected request URL");
            return response;
        });
    }
    private interface Action { void run() throws Exception; }
    private static void fails(String expected, Action action) throws Exception {
        try { action.run(); throw new AssertionError("Expected failure " + expected); }
        catch (UpdateHttp.Failure error) { check(expected.equals(error.kind), "failure category " + expected); }
    }

    private static void policy() throws Exception {
        String[] invalid = {"", "v0.10.5", "0.010.5", "-1.2.3", "1.2.3/evil", "1.2.3-beta+path", "9999999.0.0", "1.2.3-BETA", "1.2.3-.."};
        for (String text : invalid) check(UpdatePolicy.Version.parse(text) == null, "reject version " + text);
        check(UpdatePolicy.Version.parse("0.10").compareTo(UpdatePolicy.Version.parse("0.10.0")) == 0, "old two-part version");
        check(UpdatePolicy.Version.parse("0.10.10-beta").compareTo(UpdatePolicy.Version.parse("0.10.9-beta")) > 0, "numeric patch ordering");
        check(UpdatePolicy.Version.parse("1.0.0").compareTo(UpdatePolicy.Version.parse("1.0.0-beta")) > 0, "stable follows beta");
        check(UpdatePolicy.Version.parse("1.0.0-beta.10").compareTo(UpdatePolicy.Version.parse("1.0.0-beta.2")) > 0, "numeric prerelease ordering");
        byte[] bytes = "signed APK fixture".getBytes(StandardCharsets.UTF_8);
        UpdatePolicy.Asset a = asset(1, "0.10.2-beta", bytes, "");
        UpdatePolicy.Asset b = asset(2, "0.10.6-beta", bytes, "");
        UpdatePolicy.Asset c = asset(3, "0.11.0-beta", bytes, "");
        check(UpdatePolicy.newest(Arrays.asList(c, a, b), UpdatePolicy.Version.parse("0.10.5-beta")) == c, "highest asset independent of order and fixed tag");
        check(UpdatePolicy.newest(Arrays.asList(a), UpdatePolicy.Version.parse("0.10.5-beta")) == null, "older public Descarga is not update");
        check(UpdatePolicy.newest(Arrays.asList(b), b.version) == null, "no self-update loop");
        check(UpdatePolicy.newest(Arrays.asList(a, b), null) == null, "unparseable installed version fails closed");
        UpdatePolicy.Asset newerId = asset(9, "0.10.6-beta", bytes, "");
        check(UpdatePolicy.newest(Arrays.asList(b, newerId), a.version) == newerId, "deterministic replacement asset tie");
        String name = b.name, url = b.url;
        for (String unsafe : Arrays.asList(url.replace("https:", "http:"), url.replace("github.com", "github.com.evil.invalid"),
                url.replace("github.com/", "github.com@evil.invalid/"), url + "?redirect=evil", url + "#fragment",
                url.replace("Descarga", "other"), url.replace("Edwin2929", "attacker"), url.replace("://", "://user:pass@")))
            check(!UpdatePolicy.safeDownloadUrl(unsafe, name), "reject altered asset URL");
        check(!UpdatePolicy.safeRedirect("https://release-assets.githubusercontent.com.evil.invalid/x", false, name), "redirect host suffix trap");
        check(!UpdatePolicy.safeRedirect("http://release-assets.githubusercontent.com/x", false, name), "HTTPS downgrade rejected");
        check(!UpdatePolicy.safeRedirect("https://github.com/login", false, name), "login redirect rejected");
        check(!UpdatePolicy.safeRedirect("https://api.github.com/repos/other/repo/releases", true, ""), "metadata repository redirect rejected");
        check(UpdatePolicy.safeRedirect("https://release-assets.githubusercontent.com/github-production-release-asset/file?token=abc", false, name), "signed GitHub asset redirect");
        check(UpdatePolicy.Asset.checked(2, 100, "sma3-recomp-0.10.6-beta-x86.apk", url, "", "uploaded") == null, "wrong ABI filename rejected");
        check(UpdatePolicy.Asset.checked(2, UpdatePolicy.MAX_APK_BYTES + 1, name, url, "", "uploaded") == null, "size bound");
        check(UpdatePolicy.Asset.checked(2, 0, name, url, "", "uploaded") == null, "empty size");
        check(UpdatePolicy.Asset.checked(2, 100, name, url, "md5:abcd", "uploaded") == null, "malformed provided digest rejected");
        check(UpdatePolicy.Asset.checked(2, 100, name, url, "", "new") == null, "incomplete upload excluded");
        check(UpdatePolicy.Asset.checked(0, 100, name, url, "", "uploaded") == null, "missing asset ID excluded");
        check(UpdatePolicy.shouldCheck(1000, 0, true), "first launch checks");
        check(!UpdatePolicy.shouldCheck(1000, 2000, true), "foreground throttle");
        check(!UpdatePolicy.shouldCheck(1000, 0, false), "automatic off");
        check(UpdatePolicy.shouldCheck(1000, 1000 + UpdatePolicy.CHECK_INTERVAL_MS + 1, true), "clock rollback recovery");

        List<String> signer = Arrays.asList("original-certificate");
        check(UpdatePolicy.identityError("org.sma3", 16, signer, "org.sma3", 17, signer, "0.10.6-beta", b.version, 28, 35, true) == null, "matching identity accepted");
        check("package".equals(UpdatePolicy.identityError("org.sma3", 16, signer, "org.other", 17, signer, "0.10.6-beta", b.version, 28, 35, true)), "wrong package");
        for (long code : new long[]{0, 15, 16}) check("older".equals(UpdatePolicy.identityError("org.sma3", 16, signer, "org.sma3", code, signer, "0.10.6-beta", b.version, 28, 35, true)), "non-increasing actual versionCode");
        check("signature".equals(UpdatePolicy.identityError("org.sma3", 16, signer, "org.sma3", 17, Arrays.asList("other"), "0.10.6-beta", b.version, 28, 35, true)), "wrong signer");
        check("signature".equals(UpdatePolicy.identityError("org.sma3", 16, signer, "org.sma3", 17, new ArrayList<>(), "0.10.6-beta", b.version, 28, 35, true)), "unsigned APK");
        check("version".equals(UpdatePolicy.identityError("org.sma3", 16, signer, "org.sma3", 17, signer, "0.10.4-beta", b.version, 28, 35, true)), "misleading filename rejected");
        check("platform".equals(UpdatePolicy.identityError("org.sma3", 16, signer, "org.sma3", 17, signer, "0.10.6-beta", b.version, 36, 35, true)), "Android minimum mismatch");
        check("platform".equals(UpdatePolicy.identityError("org.sma3", 16, signer, "org.sma3", 17, signer, "0.10.6-beta", b.version, 28, 35, false)), "no ARM64 runtime");

        byte[] elf = new byte[20];
        elf[0] = 0x7f; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
        elf[4] = 2; elf[5] = 1; elf[18] = (byte) 183;
        check(UpdatePolicy.isArm64LibraryHeader(elf), "ELF64 little-endian AArch64 library");
        check(!UpdatePolicy.isArm64LibraryHeader(null), "missing ELF header");
        check(!UpdatePolicy.isArm64LibraryHeader(Arrays.copyOf(elf, 19)), "truncated ELF header");
        byte[] wrongElf = elf.clone(); wrongElf[0] = 0;
        check(!UpdatePolicy.isArm64LibraryHeader(wrongElf), "non-ELF renamed native library");
        wrongElf = elf.clone(); wrongElf[18] = 62;
        check(!UpdatePolicy.isArm64LibraryHeader(wrongElf), "x86-64 renamed as ARM64 rejected");
        wrongElf = elf.clone(); wrongElf[4] = 1; wrongElf[18] = 40;
        check(!UpdatePolicy.isArm64LibraryHeader(wrongElf), "32-bit ARM rejected");
        wrongElf = elf.clone(); wrongElf[5] = 2;
        check(!UpdatePolicy.isArm64LibraryHeader(wrongElf), "big-endian ELF rejected");
    }

    private static void transport() throws Exception {
        byte[] payload = new byte[150123];
        for (int i = 0; i < payload.length; ++i) payload[i] = (byte) (i * 31);
        String digest = UpdateHttp.hex(UpdateHttp.sha256().digest(payload));
        UpdatePolicy.Asset asset = asset(1, "0.10.6-beta", payload, "sha256:" + digest);
        File folder = Files.createTempDirectory("sma3-update-test-").toFile();
        File output = new File(folder, "update.part");
        try {
            String cdn = "https://release-assets.githubusercontent.com/github-production-release-asset/test?signature=fixture";
            Response redirect = new Response(asset.url, 302, new byte[0]).header("Location", cdn);
            Response downloaded = new Response(cdn, 200, payload);
            long[] last = {0};
            check(digest.equals(http(redirect, downloaded).download(asset, output, new UpdateHttp.Cancel(), (done,total) -> {
                check(done >= last[0] && total == payload.length, "monotonic exact progress"); last[0] = done;
            })), "complete download digest");
            check(Arrays.equals(Files.readAllBytes(output.toPath()), payload), "exact downloaded bytes");
            check(redirect.disconnected && downloaded.disconnected, "connections closed");
            check(!downloaded.getInstanceFollowRedirects(), "redirects explicitly validated");
            check(downloaded.getRequestProperty("Authorization") == null, "no credentials on CDN");
            output.delete();
            Response shortBody = new Response(asset.url, 200, Arrays.copyOf(payload, payload.length - 1)); shortBody.declared = -1;
            fails("size", () -> http(shortBody).download(asset, output, new UpdateHttp.Cancel(), (a,b) -> {}));
            check(!output.exists(), "truncated partial removed");
            Response longBody = new Response(asset.url, 200, Arrays.copyOf(payload, payload.length + 1)); longBody.declared = -1;
            fails("size", () -> http(longBody).download(asset, output, new UpdateHttp.Cancel(), (a,b) -> {}));
            check(!output.exists(), "oversize partial removed");
            byte[] wrong = payload.clone(); wrong[123] ^= 1;
            fails("digest", () -> http(new Response(asset.url, 200, wrong)).download(asset, output, new UpdateHttp.Cancel(), (a,b) -> {}));
            check(!output.exists(), "digest mismatch removed");
            Response wrongLength = new Response(asset.url, 200, payload); wrongLength.declared = 1;
            fails("size", () -> http(wrongLength).download(asset, output, new UpdateHttp.Cancel(), (a,b) -> {}));
            check(!output.exists(), "declared length mismatch never written");
            UpdateHttp.Cancel cancel = new UpdateHttp.Cancel();
            fails("cancelled", () -> http(new Response(asset.url, 200, payload)).download(asset, output, cancel, (a,b) -> cancel.cancel()));
            check(!output.exists(), "cancelled partial removed");
            fails("source", () -> http(new Response(asset.url, 302, new byte[0]).header("Location", "https://evil.invalid/apk")).download(asset, output, new UpdateHttp.Cancel(), (a,b) -> {}));
            check(!output.exists(), "untrusted redirect never downloaded");
            fails("missing", () -> http(new Response(asset.url, 404, new byte[0])).download(asset, output, new UpdateHttp.Cancel(), (a,b) -> {}));

            byte[] json = "{\"tag_name\":\"Descarga\"}".getBytes(StandardCharsets.UTF_8);
            Response meta = new Response(UpdatePolicy.RELEASE_API, 200, json).header("ETag", "\"release-v2\"");
            UpdateHttp.Metadata result = http(meta).metadata("\"release-v1\"", new UpdateHttp.Cancel());
            check(Arrays.equals(result.bytes, json) && !result.unchanged, "bounded JSON response");
            check(result.etag.equals("\"release-v2\""), "ETag retained");
            check(meta.getRequestProperty("If-None-Match").equals("\"release-v1\""), "conditional fixed-tag request");
            check(http(new Response(UpdatePolicy.RELEASE_API, 304, new byte[0])).metadata("tag", new UpdateHttp.Cancel()).unchanged, "304 reuses validated metadata");
            Response largeMeta = new Response(UpdatePolicy.RELEASE_API, 200, new byte[UpdatePolicy.MAX_METADATA_BYTES + 1]); largeMeta.declared = -1;
            fails("metadata", () -> http(largeMeta).metadata(null, new UpdateHttp.Cancel()));
            Response limit = new Response(UpdatePolicy.RELEASE_API, 429, new byte[0]).header("Retry-After", "120");
            try { http(limit).metadata(null, new UpdateHttp.Cancel()); throw new AssertionError("expected rate limit"); }
            catch (UpdateHttp.Failure error) {
                check(error.kind.equals("rate") && error.retryAt > System.currentTimeMillis() + 60000, "rate limit distinguished and backoff retained");
            }
            Response[] loops = new Response[6];
            for (int i = 0; i < loops.length; ++i) loops[i] = new Response(asset.url, 302, new byte[0]).header("Location", asset.url);
            fails("source", () -> http(loops).download(asset, output, new UpdateHttp.Cancel(), (a,b) -> {}));
        } finally { output.delete(); folder.delete(); }
    }

    public static void main(String[] args) throws Exception {
        policy(); transport();
        System.out.println("PASS updater policy, identity and HTTPS transfer checks=" + checks);
    }
}
