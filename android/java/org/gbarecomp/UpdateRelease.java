package org.gbarecomp;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/** GitHub's fixed Descarga release can receive new assets without a new release tag. */
final class UpdateRelease {
    private UpdateRelease() {}

    static UpdatePolicy.Asset parse(String json, UpdatePolicy.Version installed) throws JSONException {
        if (json == null || json.length() > UpdatePolicy.MAX_METADATA_BYTES) throw new JSONException("metadata");
        JSONObject release = new JSONObject(json);
        if (!"Descarga".equals(release.getString("tag_name")) || release.getBoolean("draft"))
            throw new JSONException("release");
        JSONArray array = release.getJSONArray("assets");
        if (array.length() > 1000) throw new JSONException("assets");
        List<UpdatePolicy.Asset> candidates = new ArrayList<>();
        for (int i = 0; i < array.length(); ++i) {
            JSONObject item = array.optJSONObject(i);
            if (item == null) continue;
            UpdatePolicy.Asset asset = readAsset(item);
            if (asset != null) candidates.add(asset);
        }
        return UpdatePolicy.newest(candidates, installed);
    }

    static UpdatePolicy.Asset readAsset(JSONObject item) {
        return UpdatePolicy.Asset.checked(item.optLong("id", -1), item.optLong("size", -1),
            item.optString("name", ""), item.optString("browser_download_url", ""),
            item.isNull("digest") ? "" : item.optString("digest", ""), item.optString("state", ""));
    }

    static String encode(UpdatePolicy.Asset asset) {
        if (asset == null) return "";
        try {
            return new JSONObject().put("id", asset.id).put("size", asset.size)
                .put("name", asset.name).put("browser_download_url", asset.url)
                .put("digest", asset.digest.isEmpty() ? "" : "sha256:" + asset.digest)
                .put("state", "uploaded").toString();
        } catch (JSONException impossible) { throw new IllegalStateException(impossible); }
    }

    static UpdatePolicy.Asset decode(String encoded) {
        if (encoded == null || encoded.length() > 4096 || encoded.isEmpty()) return null;
        try { return readAsset(new JSONObject(encoded)); }
        catch (JSONException error) { return null; }
    }
}
