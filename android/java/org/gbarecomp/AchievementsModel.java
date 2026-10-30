package org.gbarecomp;

import java.util.Arrays;

/** Pure, monotonic evidence model. IDs and metric positions are persistent contracts. */
final class AchievementsModel {
    static final int SCHEMA = 1; // Native snapshot contract; independent of the file format.
    static final int LEGACY_COUNT = 10;
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
        final long requiredBit;
        Definition(String id, int metric, int target, boolean bitCount,
                   String es, String en, String pt, String des, String den, String dpt) {
            this(id, metric, target, bitCount, 0, es, en, pt, des, den, dpt);
        }
        Definition(String id, int metric, int target, boolean bitCount, long requiredBit,
                   String es, String en, String pt, String des, String den, String dpt) {
            this.id = id; this.metric = metric; this.target = target; this.bitCount = bitCount;
            this.requiredBit = requiredBit;
            title = new String[]{es, en, pt}; description = new String[]{des, den, dpt};
        }
        int progress(State state) { return progress(state.metrics); }
        int progress(long[] metrics) {
            long value = requiredBit != 0 ? ((metrics[metric] & requiredBit) != 0 ? 1 : 0)
                : bitCount ? Long.bitCount(metrics[metric]) : metrics[metric];
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
            "Completa un nivel con 100 puntos y confirma el resultado.", "Finish a level with 100 points and confirm the result.", "Conclua uma fase com 100 pontos e confirme o resultado."),
        new Definition("three_flowers", 4, 3, false,
            "Trío de flores", "Flower trio", "Trio de flores",
            "Reúne 3 flores en un mismo nivel.", "Gather 3 flowers in one level.", "Reúna 3 flores em uma mesma fase."),
        new Definition("five_red_coins", 3, 5, false,
            "Cinco monedas rojas", "Five red coins", "Cinco moedas vermelhas",
            "Reúne 5 monedas rojas en un mismo nivel.", "Gather 5 red coins in one level.", "Reúna 5 moedas vermelhas em uma mesma fase."),
        new Definition("ten_red_coins", 3, 10, false,
            "Diez monedas rojas", "Ten red coins", "Dez moedas vermelhas",
            "Reúne 10 monedas rojas en un mismo nivel.", "Gather 10 red coins in one level.", "Reúna 10 moedas vermelhas em uma mesma fase."),
        new Definition("ten_clears", 0, 10, true,
            "Diez aventuras", "Ten adventures", "Dez aventuras",
            "Completa 10 niveles distintos.", "Complete 10 different levels.", "Conclua 10 fases diferentes."),
        new Definition("twenty_clears", 0, 20, true,
            "Veinte aventuras", "Twenty adventures", "Vinte aventuras",
            "Completa 20 niveles distintos.", "Complete 20 different levels.", "Conclua 20 fases diferentes."),
        new Definition("thirty_clears", 0, 30, true,
            "Treinta aventuras", "Thirty adventures", "Trinta aventuras",
            "Completa 30 niveles distintos.", "Complete 30 different levels.", "Conclua 30 fases diferentes."),
        new Definition("forty_clears", 0, 40, true,
            "Cuarenta aventuras", "Forty adventures", "Quarenta aventuras",
            "Completa 40 niveles distintos.", "Complete 40 different levels.", "Conclua 40 fases diferentes."),
        new Definition("five_perfect_clears", 1, 5, true,
            "Cinco niveles perfectos", "Five perfect levels", "Cinco fases perfeitas",
            "Completa 5 niveles distintos con 100 puntos y confirma sus resultados.",
            "Finish 5 different levels with 100 points and confirm their results.",
            "Conclua 5 fases diferentes com 100 pontos e confirme seus resultados."),
        new Definition("ten_perfect_clears", 1, 10, true,
            "Diez niveles perfectos", "Ten perfect levels", "Dez fases perfeitas",
            "Completa 10 niveles distintos con 100 puntos y confirma sus resultados.",
            "Finish 10 different levels with 100 points and confirm their results.",
            "Conclua 10 fases diferentes com 100 pontos e confirme seus resultados."),
        new Definition("twenty_perfect_clears", 1, 20, true,
            "Veinte niveles perfectos", "Twenty perfect levels", "Vinte fases perfeitas",
            "Completa 20 niveles distintos con 100 puntos y confirma sus resultados.",
            "Finish 20 different levels with 100 points and confirm their results.",
            "Conclua 20 fases diferentes com 100 pontos e confirme seus resultados."),
        new Definition("car_transformation", 2, 1, false, 1,
            "Coche", "Car", "Carro",
            "Transforma a Yoshi en coche.", "Transform Yoshi into a car.", "Transforme Yoshi em carro."),
        new Definition("mole_transformation", 2, 1, false, 2,
            "Tanque topo", "Mole tank", "Tanque-toupeira",
            "Transforma a Yoshi en tanque topo.", "Transform Yoshi into a mole tank.", "Transforme Yoshi em tanque-toupeira."),
        new Definition("helicopter_transformation", 2, 1, false, 4,
            "Helicóptero", "Helicopter", "Helicóptero",
            "Transforma a Yoshi en helicóptero.", "Transform Yoshi into a helicopter.", "Transforme Yoshi em helicóptero."),
        new Definition("train_transformation", 2, 1, false, 8,
            "Tren", "Train", "Trem",
            "Transforma a Yoshi en tren.", "Transform Yoshi into a train.", "Transforme Yoshi em trem."),
        new Definition("submarine_transformation", 2, 1, false, 16,
            "Submarino", "Submarine", "Submarino",
            "Transforma a Yoshi en submarino.", "Transform Yoshi into a submarine.", "Transforme Yoshi em submarino.")
    };

    static final int ALL_IDS_MASK = (1 << DEFINITIONS.length) - 1;
    static final int EXPANDED_IDS_MASK = ALL_IDS_MASK & ~((1 << LEGACY_COUNT) - 1);

    static final class State {
        final long[] metrics;
        final long[] unlockedAt;
        final boolean notifications;
        final int recognizedOnUpgrade;
        State(long[] metrics, long[] unlockedAt, boolean notifications) {
            this(metrics, unlockedAt, notifications, 0);
        }
        State(long[] metrics, long[] unlockedAt, boolean notifications, int recognizedOnUpgrade) {
            validateMetrics(metrics);
            if (unlockedAt == null || unlockedAt.length != DEFINITIONS.length)
                throw new IllegalArgumentException("Achievement date count");
            if ((recognizedOnUpgrade & ~EXPANDED_IDS_MASK) != 0)
                throw new IllegalArgumentException("Invalid expanded achievement mask");
            this.metrics = metrics.clone(); this.unlockedAt = unlockedAt.clone();
            this.notifications = notifications;
            this.recognizedOnUpgrade = recognizedOnUpgrade;
            for (int i = 0; i < unlockedAt.length; ++i) {
                if (unlockedAt[i] < 0 || (unlockedAt[i] > 0) != (DEFINITIONS[i].progress(this) == DEFINITIONS[i].target))
                    throw new IllegalArgumentException("Achievement date and evidence mismatch");
                if ((recognizedOnUpgrade & (1 << i)) != 0 && unlockedAt[i] == 0)
                    throw new IllegalArgumentException("Expanded recognition without evidence");
            }
        }
        int unlocked() {
            int count = 0;
            for (long date : unlockedAt) if (date > 0) ++count;
            return count;
        }
        State notifications(boolean enabled) { return new State(metrics, unlockedAt, enabled, recognizedOnUpgrade); }
        boolean same(State other) {
            return other != null && notifications == other.notifications && recognizedOnUpgrade == other.recognizedOnUpgrade
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
            if (dates[i] == 0 && def.progress(values) == def.target) dates[i] = Math.max(1, now);
        }
        return new State(values, dates, previous.notifications, previous.recognizedOnUpgrade);
    }

    /** Recognize only new IDs from previously saved local evidence, preserving original dates. */
    static State expandLegacy(long[] metrics, long[] oldDates, boolean notifications, long now) {
        validateMetrics(metrics);
        if (oldDates == null || oldDates.length != LEGACY_COUNT)
            throw new IllegalArgumentException("Legacy achievement date count");
        long[] dates = Arrays.copyOf(oldDates, DEFINITIONS.length);
        for (int i = 0; i < LEGACY_COUNT; ++i)
            if (dates[i] < 0 || (dates[i] > 0) != (DEFINITIONS[i].progress(metrics) == DEFINITIONS[i].target))
                throw new IllegalArgumentException("Legacy achievement evidence mismatch");
        int recognized = 0;
        for (int i = LEGACY_COUNT; i < DEFINITIONS.length; ++i) {
            if (DEFINITIONS[i].progress(metrics) == DEFINITIONS[i].target) {
                dates[i] = Math.max(1, now);
                recognized |= 1 << i;
            }
        }
        return new State(metrics, dates, notifications, recognized);
    }

    static int newUnlocks(State saved, State next) {
        int mask = 0;
        for (int i = 0; i < DEFINITIONS.length; ++i)
            if (saved.unlockedAt[i] == 0 && next.unlockedAt[i] > 0) mask |= 1 << i;
        return mask;
    }

    private AchievementsModel() {}
}
