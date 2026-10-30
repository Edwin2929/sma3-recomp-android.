package org.gbarecomp;

import java.util.Arrays;

/** Pure, monotonic evidence model. IDs and metric positions are persistent contracts. */
final class AchievementsModel {
    static final int SCHEMA = 1;
    static final int METRICS = 7;
    static final long LEVEL_MASK = (1L << 60) - 1;
    static final long TRANSFORMATION_MASK = 0x1f;
    static final String[] METRIC_KEYS = {"completed_levels", "perfect_levels", "transformations",
        "red_coins", "flowers", "stars", "events"};

    static final class Definition {
        final String id;
        final String[] title, description;
        final int metric, target;
        final boolean bitCount;
        Definition(String id, int metric, int target, boolean bitCount,
                   String es, String en, String pt, String des, String den, String dpt) {
            this.id = id; this.metric = metric; this.target = target; this.bitCount = bitCount;
            title = new String[]{es, en, pt}; description = new String[]{des, den, dpt};
        }
        int progress(State state) {
            long value = bitCount ? Long.bitCount(state.metrics[metric]) : state.metrics[metric];
            return (int)Math.min(target, value);
        }
    }

    static final Definition[] DEFINITIONS = {
        new Definition("first_flower", 4, 1, false,
            "Primera flor", "First flower", "Primeira flor",
            "Recoge una flor durante el juego.", "Collect a flower during play.", "Pegue uma flor durante o jogo."),
        new Definition("five_flowers", 4, 5, false,
            "Ramo completo", "Full bouquet", "Buquê completo",
            "Reúne las 5 flores de un mismo nivel.", "Gather all 5 flowers in one level.", "Reúna as 5 flores de uma mesma fase."),
        new Definition("first_red_coin", 3, 1, false,
            "Rojo brillante", "Shining red", "Vermelho brilhante",
            "Recoge una moneda roja durante el juego.", "Collect a red coin during play.", "Pegue uma moeda vermelha durante o jogo."),
        new Definition("twenty_red_coins", 3, 20, false,
            "Veinte tesoros", "Twenty treasures", "Vinte tesouros",
            "Reúne las 20 monedas rojas de un mismo nivel.", "Gather all 20 red coins in one level.", "Reúna as 20 moedas vermelhas de uma mesma fase."),
        new Definition("thirty_stars", 5, 30, false,
            "Treinta estrellas", "Thirty stars", "Trinta estrelas",
            "Alcanza 30 estrellas durante una partida.", "Reach 30 stars during play.", "Alcance 30 estrelas durante uma partida."),
        new Definition("first_transformation", 2, 1, true,
            "Cambio de forma", "A new shape", "Mudança de forma",
            "Transforma a Yoshi usando una burbuja.", "Transform Yoshi using a morph bubble.", "Transforme Yoshi usando uma bolha."),
        new Definition("fuzzy_recovery", 6, 1, false,
            "Cabeza despejada", "Clear head", "Cabeça leve",
            "Supera de forma natural el efecto de un Fuzzy.", "Naturally recover from a Fuzzy's effect.", "Recupere-se naturalmente do efeito de um Fuzzy."),
        new Definition("first_clear", 0, 1, true,
            "Primera meta", "First finish", "Primeira chegada",
            "Cruza una meta o completa la victoria de un jefe.", "Cross a goal or complete a boss victory.", "Atravesse uma meta ou conclua a vitória contra um chefe."),
        new Definition("five_clears", 0, 5, true,
            "Cinco aventuras", "Five adventures", "Cinco aventuras",
            "Completa 5 niveles distintos.", "Complete 5 different levels.", "Conclua 5 fases diferentes."),
        new Definition("perfect_clear", 1, 1, true,
            "Puntuación perfecta", "Perfect score", "Pontuação perfeita",
            "Completa un nivel con 100 puntos y confirma el resultado.", "Finish a level with 100 points and confirm the result.", "Conclua uma fase com 100 pontos e confirme o resultado.")
    };

    static final class State {
        final long[] metrics;
        final long[] unlockedAt;
        final boolean notifications;
        State(long[] metrics, long[] unlockedAt, boolean notifications) {
            validateMetrics(metrics);
            if (unlockedAt == null || unlockedAt.length != DEFINITIONS.length)
                throw new IllegalArgumentException("Achievement date count");
            this.metrics = metrics.clone(); this.unlockedAt = unlockedAt.clone();
            this.notifications = notifications;
            for (int i = 0; i < unlockedAt.length; ++i) {
                if (unlockedAt[i] < 0 || (unlockedAt[i] > 0) != (DEFINITIONS[i].progress(this) == DEFINITIONS[i].target))
                    throw new IllegalArgumentException("Achievement date and evidence mismatch");
            }
        }
        int unlocked() {
            int count = 0;
            for (long date : unlockedAt) if (date > 0) ++count;
            return count;
        }
        State notifications(boolean enabled) { return new State(metrics, unlockedAt, enabled); }
        boolean same(State other) {
            return other != null && notifications == other.notifications
                && Arrays.equals(metrics, other.metrics) && Arrays.equals(unlockedAt, other.unlockedAt);
        }
    }

    static State empty() { return new State(new long[METRICS], new long[DEFINITIONS.length], true); }

    static void validateMetrics(long[] values) {
        if (values == null || values.length != METRICS)
            throw new IllegalArgumentException("Achievement metric count");
        if ((values[0] & ~LEVEL_MASK) != 0 || (values[1] & ~LEVEL_MASK) != 0
                || (values[1] & ~values[0]) != 0 || (values[2] & ~TRANSFORMATION_MASK) != 0
                || values[3] < 0 || values[3] > 20 || values[4] < 0 || values[4] > 5
                || values[5] < 0 || values[5] > 30 || (values[6] & ~1L) != 0)
            throw new IllegalArgumentException("Invalid achievement evidence");
    }

    static State merge(State previous, long[] snapshot, long now) {
        if (snapshot == null || snapshot.length != METRICS + 1 || snapshot[0] != SCHEMA)
            throw new IllegalArgumentException("Unsupported achievement snapshot");
        long[] incoming = Arrays.copyOfRange(snapshot, 1, snapshot.length);
        validateMetrics(incoming);
        long[] values = previous.metrics.clone();
        for (int i = 0; i < values.length; ++i)
            values[i] = i >= 3 && i <= 5 ? Math.max(values[i], incoming[i]) : values[i] | incoming[i];
        long[] dates = previous.unlockedAt.clone();
        for (int i = 0; i < DEFINITIONS.length; ++i) {
            Definition def = DEFINITIONS[i];
            long progress = def.bitCount ? Long.bitCount(values[def.metric]) : values[def.metric];
            if (dates[i] == 0 && progress >= def.target) dates[i] = Math.max(1, now);
        }
        return new State(values, dates, previous.notifications);
    }

    static int newUnlocks(State saved, State next) {
        int mask = 0;
        for (int i = 0; i < DEFINITIONS.length; ++i)
            if (saved.unlockedAt[i] == 0 && next.unlockedAt[i] > 0) mask |= 1 << i;
        return mask;
    }

    private AchievementsModel() {}
}
