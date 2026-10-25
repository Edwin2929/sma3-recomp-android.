package org.gbarecomp;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

/** A single read-only private APK; no directory roots, path parameters or write grants. */
public final class UpdateApkProvider extends ContentProvider {
    static Uri uri(Context context) {
        return Uri.parse("content://" + context.getPackageName() + ".updates/verified-update.apk");
    }
    @Override public boolean onCreate() { return true; }
    private File file(Uri uri) throws FileNotFoundException {
        Context context = getContext();
        if (context == null || !uri(context).equals(uri)) throw new FileNotFoundException("Unknown update URI");
        File folder = new File(context.getCacheDir(), "sma3-updates");
        File apk = new File(folder, "verified-update.apk");
        try {
            if (!apk.getCanonicalFile().getParentFile().equals(folder.getCanonicalFile()) ||
                    !apk.isFile() || apk.length() <= 0 || apk.length() > UpdatePolicy.MAX_APK_BYTES)
                throw new FileNotFoundException("Verified update unavailable");
        } catch (IOException error) { throw new FileNotFoundException("Verified update unavailable"); }
        return apk;
    }
    @Override public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (!"r".equals(mode)) throw new FileNotFoundException("Read-only update");
        return ParcelFileDescriptor.open(file(uri), ParcelFileDescriptor.MODE_READ_ONLY);
    }
    @Override public String getType(Uri uri) {
        if (getContext() == null || !uri(getContext()).equals(uri)) throw new IllegalArgumentException("Unknown update URI");
        return "application/vnd.android.package-archive";
    }
    @Override public Cursor query(Uri uri, String[] projection, String selection, String[] args, String sortOrder) {
        File apk;
        try { apk = file(uri); } catch (FileNotFoundException error) { return null; }
        String[] columns = projection == null ? new String[]{OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE} : projection;
        MatrixCursor cursor = new MatrixCursor(columns, 1);
        Object[] row = new Object[columns.length];
        for (int i = 0; i < columns.length; ++i) {
            if (OpenableColumns.DISPLAY_NAME.equals(columns[i])) row[i] = "sma3-update.apk";
            else if (OpenableColumns.SIZE.equals(columns[i])) row[i] = apk.length();
        }
        cursor.addRow(row); return cursor;
    }
    @Override public Uri insert(Uri uri, ContentValues values) { throw new UnsupportedOperationException("Read only"); }
    @Override public int update(Uri uri, ContentValues values, String selection, String[] args) { throw new UnsupportedOperationException("Read only"); }
    @Override public int delete(Uri uri, String selection, String[] args) { throw new UnsupportedOperationException("Read only"); }
}
