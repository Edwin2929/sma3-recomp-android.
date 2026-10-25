package org.gbarecomp;

/** Pure selection rules shared by the Android requester and host-side tests. */
final class RefreshRatePolicy {
    static final float GBA_HZ = (float) (16777216.0 / 280896.0);
    static final float MIN_DOUBLE_HZ = 119f;

    static final class Mode {
        final int id, width, height;
        final float hz;
        Mode(int id, int width, int height, float hz) {
            this.id = id; this.width = width; this.height = height; this.hz = hz;
        }
    }

    private RefreshRatePolicy() {}

    static boolean validRate(float hz) { return Float.isFinite(hz) && hz > 0f; }

    static boolean supportsDouble(int width, int height, Mode[] modes) {
        if (modes == null) return false;
        for (Mode mode : modes) {
            if (mode != null && mode.width == width && mode.height == height &&
                    validRate(mode.hz) && mode.hz >= MIN_DOUBLE_HZ) return true;
        }
        return false;
    }

    static int selectMode(int requestedFps, int width, int height, Mode[] modes) {
        if (modes == null || width <= 0 || height <= 0) return 0;
        final int target = requestedFps == 120 ? 120 : 60;
        int selected = 0;
        double score = Double.POSITIVE_INFINITY;
        for (Mode mode : modes) {
            if (mode == null || mode.id <= 0 || mode.width != width || mode.height != height ||
                    !validRate(mode.hz)) continue;
            int multiple = Math.round(mode.hz / target);
            if (multiple < 1 || multiple > 8) continue;
            double difference = Math.abs(mode.hz - multiple * target);
            // Prefer the exact requested refresh, then a clean integer multiple.
            // Do not force 60 Hz onto a 90 Hz-only panel or switch resolution.
            if (difference > 1.0) continue;
            double candidate = (multiple - 1) * 1000.0 + difference;
            if (candidate < score || (candidate == score && mode.id < selected)) {
                score = candidate; selected = mode.id;
            }
        }
        return selected;
    }

    static boolean doubleRateActive(boolean resumed, boolean validSurface,
                                    boolean requested120, float physicalHz, float appHz) {
        return resumed && validSurface && requested120 && validRate(physicalHz) &&
            validRate(appHz) && physicalHz >= MIN_DOUBLE_HZ && appHz >= MIN_DOUBLE_HZ;
    }
}
