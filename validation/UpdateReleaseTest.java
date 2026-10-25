package org.gbarecomp;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;

/** Real release JSON playback plus synthetic future assets. No release is published. */
public final class UpdateReleaseTest {
    private static int checks;
    private static void check(boolean condition, String description) {
        ++checks; if (!condition) throw new AssertionError(description);
    }
    private static JSONObject asset(long id, String version) {
        String name = "sma3-recomp-" + version + "-arm64.apk";
        return new JSONObject().put("id", id).put("name", name).put("size", 27817240)
            .put("state", "uploaded").put("browser_download_url", UpdatePolicy.DOWNLOAD_PREFIX + name)
            .put("digest", "sha256:" + "a".repeat(64));
    }
    private static void reject(String json, UpdatePolicy.Version installed) {
        try { UpdateRelease.parse(json, installed); throw new AssertionError("Expected malformed metadata rejection"); }
        catch (JSONException expected) { ++checks; }
    }
    public static void main(String[] args) throws Exception {
        if (args.length != 1) throw new IllegalArgumentException("Supply the recorded Descarga release JSON");
        String original = Files.readString(Path.of(args[0]), StandardCharsets.UTF_8);
        JSONObject live = new JSONObject(original);
        UpdatePolicy.Version current = UpdatePolicy.Version.parse("0.10.5-beta");
        check(live.getString("tag_name").equals("Descarga"), "real fixed release tag");
        check(UpdateRelease.parse(original, current) == null, "actual older release is not offered as an update");
        JSONObject simulated = new JSONObject(original);
        JSONArray assets = simulated.getJSONArray("assets");
        assets.put(asset(90001, "0.10.6-beta"));
        UpdatePolicy.Asset next = UpdateRelease.parse(simulated.toString(), current);
        check(next != null && next.id == 90001, "new asset detected without changing tag or release creation time");
        check(next.version.text.equals("0.10.6-beta"), "future version label");
        check(next.digest.equals("a".repeat(64)), "digest decoded");
        assets.put(asset(90002, "0.10.10-beta"));
        assets.put(asset(90003, "0.10.7-beta"));
        check(UpdateRelease.parse(simulated.toString(), current).id == 90002, "highest numeric APK version selected");
        JSONObject wrongAbi = asset(90004, "9.0.0");
        wrongAbi.put("name", wrongAbi.getString("name").replace("arm64", "x86"));
        assets.put(wrongAbi);
        JSONObject wrongSource = asset(90005, "9.0.1"); wrongSource.put("browser_download_url", "https://evil.invalid/update.apk"); assets.put(wrongSource);
        JSONObject incomplete = asset(90006, "9.0.2"); incomplete.put("state", "new"); assets.put(incomplete);
        JSONObject oversized = asset(90007, "9.0.3"); oversized.put("size", UpdatePolicy.MAX_APK_BYTES + 1); assets.put(oversized);
        JSONObject badDigest = asset(90008, "9.0.4"); badDigest.put("digest", "sha256:wrong"); assets.put(badDigest);
        check(UpdateRelease.parse(simulated.toString(), current).id == 90002, "incompatible and unsafe assets ignored");
        UpdatePolicy.Asset cached = UpdateRelease.decode(UpdateRelease.encode(next));
        check(cached != null && cached.id == next.id && cached.size == next.size && cached.digest.equals(next.digest) && cached.url.equals(next.url), "validated metadata cache roundtrip");
        check(UpdateRelease.decode(wrongSource.toString()) == null, "tampered cached source rejected");
        check(UpdateRelease.decode("invalid") == null, "damaged metadata cache rejected");
        check(UpdateRelease.decode("x".repeat(4097)) == null, "cached metadata bounded");
        JSONObject withoutDigest = asset(90010, "0.10.6-beta"); withoutDigest.put("digest", JSONObject.NULL);
        check(UpdateRelease.readAsset(withoutDigest).digest.isEmpty(), "missing GitHub digest remains optional; APK identity still mandatory");
        JSONObject draft = new JSONObject(original).put("draft", true);
        reject(draft.toString(), current);
        reject(new JSONObject(original).put("tag_name", "unrelated").toString(), current);
        reject(new JSONObject(original).put("assets", "wrong").toString(), current);
        reject("{\"tag_name\":", current);
        reject("x".repeat(UpdatePolicy.MAX_METADATA_BYTES + 1), current);
        JSONArray tooMany = new JSONArray(); for (int i = 0; i < 1001; ++i) tooMany.put(new JSONObject());
        reject(new JSONObject(original).put("assets", tooMany).toString(), current);
        JSONObject noAssets = new JSONObject(original).put("assets", new JSONArray());
        check(UpdateRelease.parse(noAssets.toString(), current) == null, "empty release does not invent update");
        System.out.println("PASS real Descarga JSON and future fixed-tag asset checks=" + checks);
    }
}
