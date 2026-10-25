package org.gbarecomp;

import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.io.OutputStream;

/** Bounded stream transfer for a selected mod document; never extracts archives. */
final class ModArchiveCopy {
    private ModArchiveCopy() {}

    static long copy(InputStream source, OutputStream destination, long maximumBytes)
            throws IOException {
        if (source == null || destination == null || maximumBytes < 0)
            throw new IllegalArgumentException("Invalid mod copy request");
        byte[] buffer = new byte[64 * 1024];
        long total = 0;
        for (;;) {
            if (Thread.currentThread().isInterrupted())
                throw new InterruptedIOException("Mod import cancelled");
            int count = source.read(buffer);
            if (count < 0) return total;
            // Some document providers return zero transiently. A single-byte
            // read guarantees progress instead of spinning on that provider.
            if (count == 0) {
                int value = source.read();
                if (value < 0) return total;
                buffer[0] = (byte) value;
                count = 1;
            }
            if (count > maximumBytes - total)
                throw new TooLargeException();
            destination.write(buffer, 0, count);
            total += count;
        }
    }

    static final class TooLargeException extends IOException {
        TooLargeException() { super("Mod archive exceeds the import limit"); }
    }
}
