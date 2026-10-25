package org.gbarecomp;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.util.Arrays;

/** Host JDK test of the actual Android mod-document transfer helper. */
public final class ModArchiveCopyTest {
    private static int checks;
    private static void require(boolean condition, String message) {
        ++checks;
        if (!condition) throw new AssertionError(message);
    }
    public static void main(String[] args) throws Exception {
        byte[] data = new byte[140_001];
        for (int i = 0; i < data.length; ++i) data[i] = (byte) (i * 31);
        for (int size : new int[]{0, 1, 65_535, 65_536, 65_537, data.length}) {
            byte[] source = Arrays.copyOf(data, size);
            ByteArrayOutputStream destination = new ByteArrayOutputStream();
            require(ModArchiveCopy.copy(new ByteArrayInputStream(source), destination, size) == size,
                "exact limit count");
            require(Arrays.equals(source, destination.toByteArray()), "byte-exact transfer");
        }
        for (int limit : new int[]{0, 1, 65_535, 65_536, 65_537}) {
            ByteArrayOutputStream destination = new ByteArrayOutputStream();
            boolean rejected = false;
            try { ModArchiveCopy.copy(new ByteArrayInputStream(data), destination, limit); }
            catch (ModArchiveCopy.TooLargeException expected) { rejected = true; }
            require(rejected, "actual oversized stream rejected without relying on provider metadata");
            require(destination.size() <= limit, "destination cannot exceed limit");
        }
        InputStream zeroThenBytes = new ByteArrayInputStream(data) {
            private boolean zero = true;
            @Override public synchronized int read(byte[] b, int offset, int length) {
                if (zero) { zero = false; return 0; }
                return super.read(b, offset, length);
            }
        };
        ByteArrayOutputStream destination = new ByteArrayOutputStream();
        require(ModArchiveCopy.copy(zeroThenBytes, destination, data.length) == data.length,
            "zero-length read makes progress");
        require(Arrays.equals(data, destination.toByteArray()), "provider zero-read preserves bytes");
        boolean interrupted = false;
        Thread.currentThread().interrupt();
        try { ModArchiveCopy.copy(new ByteArrayInputStream(data), new ByteArrayOutputStream(), data.length); }
        catch (InterruptedIOException expected) { interrupted = true; }
        finally { Thread.interrupted(); }
        require(interrupted, "activity shutdown cancellation honored");
        boolean inputFailure = false;
        try {
            ModArchiveCopy.copy(new InputStream() {
                @Override public int read() throws IOException { throw new IOException("provider failed"); }
            }, new ByteArrayOutputStream(), 100);
        } catch (IOException expected) { inputFailure = true; }
        require(inputFailure, "provider failures propagate for cleanup");
        System.out.println("PASS mod archive stream transfer checks=" + checks);
    }
}
