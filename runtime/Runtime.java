// Support code is embedded in the submission: no jars or native libraries.
interface JppCopyable<T> {
    T copy();
}

interface JppAssignable<T> extends JppCopyable<T> {
    T jppAssignFrom(T source);
}
interface JppUninitialized {}
interface JppMove {}
interface JppTrivialValue {}

// The C handle keeps its identity while BigInteger supplies the integer value.
static class JppGmpInteger {
    BigInteger value = BigInteger.ZERO;
    int limbCount(int width) {
        int bits = value.abs().bitLength();
        int count = bits == 0 ? 0 : (bits - 1) / width + 1;
        return value.signum() < 0 ? -count : count;
    }
    static void initialize(JppPtr<JppGmpInteger> target) {
        target.get().value = BigInteger.ZERO;
    }
    static void clear(JppPtr<JppGmpInteger> target) {
        target.get().value = BigInteger.ZERO;
    }
    static void set(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> source) {
        target.get().value = source.get().value;
    }
    static void setInteger(JppPtr<JppGmpInteger> target, BigInteger source) {
        target.get().value = source;
    }
    static void add(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> left,
                    JppPtr<JppGmpInteger> right) {
        target.get().value = left.get().value.add(right.get().value);
    }
    static void subtract(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> left,
                         JppPtr<JppGmpInteger> right) {
        target.get().value = left.get().value.subtract(right.get().value);
    }
    static void multiply(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> left,
                         JppPtr<JppGmpInteger> right) {
        target.get().value = left.get().value.multiply(right.get().value);
    }
    static void addInteger(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> left,
                           BigInteger right) {
        target.get().value = left.get().value.add(right);
    }
    static void subtractInteger(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> left,
                                BigInteger right) {
        target.get().value = left.get().value.subtract(right);
    }
    static void integerSubtract(JppPtr<JppGmpInteger> target, BigInteger left,
                                JppPtr<JppGmpInteger> right) {
        target.get().value = left.subtract(right.get().value);
    }
    static void multiplyInteger(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> left,
                                BigInteger right) {
        target.get().value = left.get().value.multiply(right);
    }
    static void negate(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> source) {
        target.get().value = source.get().value.negate();
    }
    static void absolute(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> source) {
        target.get().value = source.get().value.abs();
    }
    static void swap(JppPtr<JppGmpInteger> left, JppPtr<JppGmpInteger> right) {
        JppGmpInteger a = left.get(), b = right.get();
        BigInteger value = a.value;
        a.value = b.value;
        b.value = value;
    }
    static BigInteger[] division(BigInteger numerator, BigInteger denominator, int mode) {
        BigInteger[] parts = numerator.divideAndRemainder(denominator);
        if (parts[1].signum() != 0) {
            int sign = numerator.signum() * denominator.signum();
            if (mode < 0 && sign < 0) {
                parts[0] = parts[0].subtract(BigInteger.ONE);
                parts[1] = parts[1].add(denominator);
            } else if (mode > 0 && sign > 0) {
                parts[0] = parts[0].add(BigInteger.ONE);
                parts[1] = parts[1].subtract(denominator);
            }
        }
        return parts;
    }
    static void divide(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> numerator,
                       JppPtr<JppGmpInteger> denominator, int mode) {
        target.get().value = division(numerator.get().value, denominator.get().value, mode)[0];
    }
    static void remainder(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> numerator,
                          JppPtr<JppGmpInteger> denominator, int mode) {
        target.get().value = division(numerator.get().value, denominator.get().value, mode)[1];
    }
    static void divideRemainder(JppPtr<JppGmpInteger> quotient, JppPtr<JppGmpInteger> remainder,
                                JppPtr<JppGmpInteger> numerator,
                                JppPtr<JppGmpInteger> denominator, int mode) {
        BigInteger[] parts = division(numerator.get().value, denominator.get().value, mode);
        quotient.get().value = parts[0];
        remainder.get().value = parts[1];
    }
    static void modulo(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> numerator,
                       JppPtr<JppGmpInteger> denominator) {
        target.get().value = numerator.get().value.mod(denominator.get().value.abs());
    }
    static int compare(JppPtr<JppGmpInteger> left, JppPtr<JppGmpInteger> right) {
        return left.get().value.compareTo(right.get().value);
    }
    static int compareInteger(JppPtr<JppGmpInteger> left, BigInteger right) {
        return left.get().value.compareTo(right);
    }
    static int compareAbsolute(JppPtr<JppGmpInteger> left, JppPtr<JppGmpInteger> right) {
        return left.get().value.abs().compareTo(right.get().value.abs());
    }
    static int compareAbsoluteInteger(JppPtr<JppGmpInteger> left, BigInteger right) {
        return left.get().value.abs().compareTo(right);
    }
    static BigInteger unsignedValue(JppPtr<JppGmpInteger> source) {
        return source.get().value.abs();
    }
    static int testBit(JppPtr<JppGmpInteger> source, BigInteger index) {
        BigInteger value = source.get().value;
        return index.bitLength() > 31 ? (value.signum() < 0 ? 1 : 0)
                                     : (value.testBit(index.intValue()) ? 1 : 0);
    }
    static void multiplyPowerOfTwo(JppPtr<JppGmpInteger> target,
                                   JppPtr<JppGmpInteger> source, BigInteger exponent) {
        BigInteger value = source.get().value;
        target.get().value = value.signum() == 0 ? value : value.shiftLeft(exponent.intValueExact());
    }
    static void and(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> left,
                    JppPtr<JppGmpInteger> right) {
        target.get().value = left.get().value.and(right.get().value);
    }
    static void or(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> left,
                   JppPtr<JppGmpInteger> right) {
        target.get().value = left.get().value.or(right.get().value);
    }
    static void xor(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> left,
                    JppPtr<JppGmpInteger> right) {
        target.get().value = left.get().value.xor(right.get().value);
    }
    static void complement(JppPtr<JppGmpInteger> target, JppPtr<JppGmpInteger> source) {
        target.get().value = source.get().value.not();
    }
}

static class JppIntegerOverflow {
    static BigInteger unsigned(BigInteger value, int width) {
        return value.and(BigInteger.ONE.shiftLeft(width).subtract(BigInteger.ONE));
    }
    static <T> boolean store(BigInteger value, int width, boolean signed, JppPtr<T> destination,
                              java.util.function.Function<BigInteger, T> convert) {
        BigInteger modulus = BigInteger.ONE.shiftLeft(width);
        BigInteger narrowed = value.and(modulus.subtract(BigInteger.ONE));
        if (signed && narrowed.testBit(width - 1))
            narrowed = narrowed.subtract(modulus);
        destination.put(convert.apply(narrowed));
        return !value.equals(narrowed);
    }
}

// Narrow character classification in the initial C locale.
static class JppCtype {
    static boolean isalnum(int value) {
        return isalpha(value) || isdigit(value);
    }
    static boolean isalpha(int value) {
        return isupper(value) || islower(value);
    }
    static boolean isblank(int value) {
        return value == ' ' || value == '\t';
    }
    static boolean iscntrl(int value) {
        return (value >= 0 && value < 32) || value == 127;
    }
    static boolean isdigit(int value) {
        return value >= '0' && value <= '9';
    }
    static boolean isgraph(int value) {
        return value >= '!' && value <= '~';
    }
    static boolean islower(int value) {
        return value >= 'a' && value <= 'z';
    }
    static boolean isprint(int value) {
        return value >= ' ' && value <= '~';
    }
    static boolean ispunct(int value) {
        return isgraph(value) && !isalnum(value);
    }
    static boolean isspace(int value) {
        return value == ' ' || (value >= '\t' && value <= '\r');
    }
    static boolean isupper(int value) {
        return value >= 'A' && value <= 'Z';
    }
    static boolean isxdigit(int value) {
        return isdigit(value) || (value >= 'a' && value <= 'f') ||
               (value >= 'A' && value <= 'F');
    }
}

// The default GNU libc rand/random stream has 31 words of shared state.
static class JppGlibcRandom {
    static final int[] words = new int[31];
    static int front, rear;
    static { seed(1); }
    static synchronized void seed(int bits) {
        long value = bits == 0 ? 1 : bits;
        words[0] = (int)value;
        for (int i = 1; i < words.length; ++i) {
            value = value * 16807 % 2147483647;
            if (value < 0)
                value += 2147483647;
            words[i] = (int)value;
        }
        front = 3;
        rear = 0;
        for (int i = 0; i < words.length * 10; ++i)
            next();
    }
    static synchronized int next() {
        int value = words[front] + words[rear];
        words[front] = value;
        if (++front == words.length)
            front = 0;
        if (++rear == words.length)
            rear = 0;
        return value >>> 1;
    }
}

// C++ handlers catch only translated C++ exceptions. Nonlocal jumps and JVM
// failures keep their own control paths.
static class JppException extends RuntimeException {
    final String type;
    final String[] handlers;
    final JppRef<Object> value;
    JppException(String type, Object value, String[] handlers) {
        super(null, null, false, false);
        this.type = type;
        this.handlers = handlers;
        this.value = JppRef.cell(value);
    }
    boolean matches(String handler) {
        for (String candidate : handlers)
            if (candidate.equals(handler))
                return true;
        return false;
    }
    <T> JppRef<T> reference() {
        return (JppRef<T>)(JppRef<?>)value;
    }
}
static class JppExceptions {
    static final ThreadLocal<JppException> CURRENT = new ThreadLocal<JppException>();
    static JppException activate(JppException exception) {
        JppException previous = CURRENT.get();
        CURRENT.set(exception);
        return previous;
    }
    static void restore(JppException previous) {
        if (previous == null)
            CURRENT.remove();
        else
            CURRENT.set(previous);
    }
    static JppException rethrow() {
        JppException exception = CURRENT.get();
        if (exception == null)
            throw terminate();
        return exception;
    }
    static <T> T raise(JppException exception) {
        throw exception;
    }
    static JppException terminate() {
        Runtime.getRuntime().halt(134);
        throw new AssertionError("C++ termination returned");
    }
}

// x87 values are immutable scalars. BigInteger supplies exact intermediates;
// every arithmetic operation rounds once to the target's 64-bit significand.
static class JppLongDouble implements Comparable<JppLongDouble> {
    static final int MIN_POWER = -16445;
    static final JppLongDouble ZERO = new JppLongDouble(false, BigInteger.ZERO, 0, 0);
    static final JppLongDouble ONE = integer(BigInteger.ONE);
    static final JppLongDouble TWO = integer(BigInteger.valueOf(2));
    static final JppLongDouble MAX = new JppLongDouble(
        false, BigInteger.ONE.shiftLeft(64).subtract(BigInteger.ONE), 16320, 0);
    final boolean negative;
    final BigInteger significand;
    final int power;
    final int special; // 0 finite, 1 infinity, 2 quiet NaN
    JppLongDouble(boolean negative, BigInteger significand, int power, int special) {
        this.negative = negative;
        this.significand = significand;
        this.power = power;
        this.special = special;
    }
    static JppLongDouble zero(boolean negative) {
        return negative ? new JppLongDouble(true, BigInteger.ZERO, 0, 0) : ZERO;
    }
    static JppLongDouble infinity(boolean negative) {
        return new JppLongDouble(negative, BigInteger.ZERO, 0, 1);
    }
    static JppLongDouble nan(boolean negative) {
        return new JppLongDouble(negative, BigInteger.ZERO, 0, 2);
    }
    static BigInteger roundedQuotient(BigInteger numerator, BigInteger denominator) {
        BigInteger[] parts = numerator.divideAndRemainder(denominator);
        int half = parts[1].shiftLeft(1).compareTo(denominator);
        return half > 0 || (half == 0 && parts[0].testBit(0))
            ? parts[0].add(BigInteger.ONE) : parts[0];
    }
    static JppLongDouble rounded(BigInteger numerator, BigInteger denominator, long power,
                                 boolean negative) {
        if (numerator.signum() == 0)
            return zero(negative);
        int difference = numerator.bitLength() - denominator.bitLength();
        long top = power + difference;
        int comparison = difference >= 0
            ? numerator.compareTo(denominator.shiftLeft(difference))
            : numerator.shiftLeft(-difference).compareTo(denominator);
        if (comparison < 0)
            --top;
        if (top > 16383)
            return infinity(negative);
        if (top < MIN_POWER - 1)
            return zero(negative);
        int quantum = (int)Math.max(top - 63, MIN_POWER);
        long shift = power - quantum;
        BigInteger rounded = shift >= 0
            ? roundedQuotient(numerator.shiftLeft(Math.toIntExact(shift)), denominator)
            : roundedQuotient(numerator, denominator.shiftLeft(Math.toIntExact(-shift)));
        if (rounded.signum() == 0)
            return zero(negative);
        if (rounded.bitLength() > 64) {
            rounded = rounded.shiftRight(1);
            ++quantum;
        }
        if ((long)quantum + rounded.bitLength() - 1 > 16383)
            return infinity(negative);
        return new JppLongDouble(negative, rounded, quantum, 0);
    }
    static JppLongDouble integer(BigInteger value) {
        return rounded(value.abs(), BigInteger.ONE, 0, value.signum() < 0);
    }
    static JppLongDouble integer(long value) {
        return integer(BigInteger.valueOf(value));
    }
    static JppLongDouble floating(double value) {
        long bits = Double.doubleToRawLongBits(value);
        boolean negative = bits < 0;
        int exponent = (int)((bits >>> 52) & 2047);
        long fraction = bits & 0xfffffffffffffL;
        if (exponent == 2047)
            return fraction == 0 ? infinity(negative) : nan(negative);
        if (exponent != 0)
            fraction |= 1L << 52;
        return rounded(BigInteger.valueOf(fraction), BigInteger.ONE,
                       exponent == 0 ? -1074 : exponent - 1075, negative);
    }
    static long exponent(String text) {
        try {
            return Long.parseLong(text);
        } catch (NumberFormatException error) {
            if (!text.matches("[+-]?[0-9]+"))
                throw error;
            return text.startsWith("-") ? Long.MIN_VALUE : Long.MAX_VALUE;
        }
    }
    static JppLongDouble parse(String source) {
        String text = source.toLowerCase(Locale.ROOT);
        boolean negative = text.startsWith("-");
        if (text.startsWith("-") || text.startsWith("+"))
            text = text.substring(1);
        if (text.equals("inf") || text.equals("infinity"))
            return infinity(negative);
        if (text.equals("nan"))
            return nan(negative);
        if (text.matches("0x(?:[0-9a-f]+(?:\\.[0-9a-f]*)?|\\.[0-9a-f]+)[p][+-]?[0-9]+")) {
            int marker = text.indexOf('p'), point = text.indexOf('.');
            String digits = text.substring(2, marker).replace(".", "");
            long scale = exponent(text.substring(marker + 1));
            scale = Math.max(-1000000000L, Math.min(1000000000L, scale));
            if (point >= 0)
                scale -= 4L * (marker - point - 1);
            return rounded(new BigInteger(digits, 16), BigInteger.ONE, scale, negative);
        }
        if (!text.matches("(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:e[+-]?[0-9]+)?"))
            throw new NumberFormatException(source);
        int marker = text.indexOf('e');
        String mantissa = marker >= 0 ? text.substring(0, marker) : text;
        String digits = mantissa.replace(".", "");
        int first = 0;
        while (first < digits.length() && digits.charAt(first) == '0')
            ++first;
        if (first == digits.length())
            return zero(negative);
        long exponent = marker >= 0 ? exponent(text.substring(marker + 1)) : 0;
        // No Java string has enough digits to cancel an exponent this large.
        if (exponent > 10000000000L || exponent < -10000000000L)
            return exponent > 0 ? infinity(negative) : zero(negative);
        int point = mantissa.indexOf('.');
        long leading = digits.length() - first - 1L + exponent -
            (point >= 0 ? mantissa.length() - point - 1L : 0);
        if (leading > 5000)
            return infinity(negative);
        if (leading < -5000)
            return zero(negative);
        BigDecimal decimal = new BigDecimal(text);
        BigInteger numerator = decimal.unscaledValue();
        if (numerator.signum() == 0)
            return zero(negative);
        long magnitude = (long)decimal.precision() - decimal.scale();
        if (magnitude > 5000)
            return infinity(negative);
        if (magnitude < -5000)
            return zero(negative);
        int scale = decimal.scale();
        return scale >= 0
            ? rounded(numerator, BigInteger.TEN.pow(scale), 0, negative)
            : rounded(numerator.multiply(BigInteger.TEN.pow(-scale)), BigInteger.ONE, 0, negative);
    }
    boolean isZero() {
        return special == 0 && significand.signum() == 0;
    }
    boolean isNormal() {
        return special == 0 && significand.bitLength() == 64;
    }
    JppLongDouble negate() {
        return new JppLongDouble(!negative, significand, power, special);
    }
    JppLongDouble abs() {
        return negative ? negate() : this;
    }
    JppLongDouble copySign(JppLongDouble other) {
        return negative == other.negative ? this : negate();
    }
    JppLongDouble add(JppLongDouble other) {
        if (special == 2 || other.special == 2)
            return special == 2 ? this : other;
        if (special == 1 || other.special == 1) {
            if (special == 1 && other.special == 1 && negative != other.negative)
                return nan(true);
            return special == 1 ? this : other;
        }
        int common = Math.min(power, other.power);
        BigInteger left = significand.shiftLeft(power - common);
        BigInteger right = other.significand.shiftLeft(other.power - common);
        BigInteger sum = (negative ? left.negate() : left).add(
            other.negative ? right.negate() : right);
        return rounded(sum.abs(), BigInteger.ONE, common,
                       sum.signum() < 0 || (sum.signum() == 0 && negative && other.negative));
    }
    JppLongDouble subtract(JppLongDouble other) {
        return other.special == 2 ? other : add(other.negate());
    }
    JppLongDouble multiply(JppLongDouble other) {
        if (special == 2 || other.special == 2)
            return special == 2 ? this : other;
        if (special == 1 || other.special == 1)
            return isZero() || other.isZero() ? nan(true) : infinity(negative != other.negative);
        return rounded(significand.multiply(other.significand), BigInteger.ONE,
                       (long)power + other.power, negative != other.negative);
    }
    JppLongDouble divide(JppLongDouble other) {
        if (special == 2 || other.special == 2)
            return special == 2 ? this : other;
        if ((special == 1 && other.special == 1) || (isZero() && other.isZero()))
            return nan(true);
        if (special == 1 || other.isZero())
            return infinity(negative != other.negative);
        if (other.special == 1)
            return zero(negative != other.negative);
        return rounded(significand, other.significand, (long)power - other.power,
                       negative != other.negative);
    }
    JppLongDouble scaleB(int exponent) {
        return special != 0 ? this
            : rounded(significand, BigInteger.ONE, (long)power + exponent, negative);
    }
    // Modes: truncate, floor, ceiling, halfway away from zero, ties to even.
    JppLongDouble integral(int mode) {
        if (special != 0 || power >= 0 || isZero())
            return this;
        BigInteger whole = significand.shiftRight(-power);
        BigInteger fraction = significand.subtract(whole.shiftLeft(-power));
        if (fraction.signum() != 0) {
            int half = fraction.shiftLeft(1).compareTo(BigInteger.ONE.shiftLeft(-power));
            if ((mode == 1 && negative) || (mode == 2 && !negative) ||
                (mode >= 3 && (half > 0 || (half == 0 && (mode == 3 || whole.testBit(0))))))
                whole = whole.add(BigInteger.ONE);
        }
        return rounded(whole, BigInteger.ONE, 0, negative);
    }
    BigInteger integerValue() {
        if (special != 0)
            throw new ArithmeticException("nonfinite long double to integer conversion");
        BigInteger value = power >= 0 ? significand.shiftLeft(power)
                                     : significand.shiftRight(-power);
        return negative ? value.negate() : value;
    }
    static BigInteger integerSqrt(BigInteger value) {
        if (value.signum() == 0)
            return value;
        BigInteger result = BigInteger.ONE.shiftLeft((value.bitLength() + 1) / 2);
        while (true) {
            BigInteger next = result.add(value.divide(result)).shiftRight(1);
            if (next.compareTo(result) >= 0)
                return result;
            result = next;
        }
    }
    JppLongDouble sqrt() {
        if (special == 2 || isZero())
            return this;
        if (negative)
            return nan(true);
        if (special == 1)
            return this;
        int top = power + significand.bitLength() - 1;
        int quantum = Math.floorDiv(top, 2) - 63;
        int shift = power - 2 * quantum;
        BigInteger numerator = shift >= 0 ? significand.shiftLeft(shift) : significand;
        BigInteger denominator = shift >= 0 ? BigInteger.ONE : BigInteger.ONE.shiftLeft(-shift);
        BigInteger root = integerSqrt(numerator.divide(denominator));
        BigInteger midpoint = root.shiftLeft(1).add(BigInteger.ONE);
        int comparison = numerator.shiftLeft(2).compareTo(
            midpoint.multiply(midpoint).multiply(denominator));
        if (comparison > 0 || (comparison == 0 && root.testBit(0)))
            root = root.add(BigInteger.ONE);
        return rounded(root, BigInteger.ONE, quantum, false);
    }
    int magnitudeCompare(JppLongDouble other) {
        if (isZero() || other.isZero())
            return Integer.compare(significand.signum(), other.significand.signum());
        int top = power + significand.bitLength(), otherTop = other.power + other.significand.bitLength();
        if (top != otherTop)
            return Integer.compare(top, otherTop);
        int common = Math.min(power, other.power);
        return significand.shiftLeft(power - common).compareTo(
            other.significand.shiftLeft(other.power - common));
    }
    public int compareTo(JppLongDouble other) {
        if (special == 2 || other.special == 2 || (isZero() && other.isZero()))
            return 0;
        if (negative != other.negative)
            return negative ? -1 : 1;
        int order = special == 1 || other.special == 1
            ? Integer.compare(special, other.special) : magnitudeCompare(other);
        return negative ? -order : order;
    }
    boolean test(JppLongDouble other, String operation) {
        if (special == 2 || other.special == 2)
            return operation.equals("!=");
        int order = compareTo(other);
        switch (operation) {
        case "==": return order == 0;
        case "!=": return order != 0;
        case "<": return order < 0;
        case "<=": return order <= 0;
        case ">": return order > 0;
        case ">=": return order >= 0;
        default: throw new IllegalArgumentException(operation);
        }
    }
    long floatingBits(int precision, int minimum, int maximum, int bias, int fractionBits) {
        long sign = negative ? 1L << (fractionBits + (precision == 53 ? 11 : 8)) : 0;
        long infinity = ((1L << (precision == 53 ? 11 : 8)) - 1) << fractionBits;
        if (special != 0)
            return sign | infinity | (special == 2 ? 1L << (fractionBits - 1) : 0);
        if (isZero())
            return sign;
        int top = power + significand.bitLength() - 1;
        int quantum = Math.max(top - precision + 1, minimum - precision + 1);
        int shift = power - quantum;
        BigInteger value = shift >= 0 ? significand.shiftLeft(shift)
            : roundedQuotient(significand, BigInteger.ONE.shiftLeft(-shift));
        if (value.signum() == 0)
            return sign;
        if (value.bitLength() > precision) {
            value = value.shiftRight(1);
            ++quantum;
        }
        top = quantum + value.bitLength() - 1;
        if (top > maximum)
            return sign | infinity;
        if (top < minimum)
            return sign | value.longValue();
        return sign | ((long)(top + bias) << fractionBits) |
            (value.longValue() & ((1L << fractionBits) - 1));
    }
    double doubleValue() {
        return Double.longBitsToDouble(floatingBits(53, -1022, 1023, 1023, 52));
    }
    float floatValue() {
        return Float.intBitsToFloat((int)floatingBits(24, -126, 127, 127, 23));
    }
}

static class JppJumpTarget {
    boolean active = true;
}

static class JppJumpBuffer {
    JppJumpTarget target;
}

static class JppLongJump extends Error {
    final JppJumpTarget target;
    final int value;
    JppLongJump(JppJumpTarget target, int value) {
        super(null, null, false, false);
        this.target = target;
        this.value = value == 0 ? 1 : value;
    }
}

// Clone the callable's Java class, then give its owned captures independent cells.
static abstract class JppClosure<F> implements JppCopyable<F>, java.lang.Cloneable {
    protected abstract void jppCopyCaptures();
    @SuppressWarnings("unchecked")
    public F copy() {
        try {
            JppClosure<F> result = (JppClosure<F>)super.clone();
            result.jppCopyCaptures();
            return (F)result;
        } catch (java.lang.CloneNotSupportedException error) {
            throw new AssertionError(error);
        }
    }
}

interface JppListOwner<T> {
    List<T> jppStorage();
}

// ArrayList owns all allocation and element operations. The outer object stays
// stable when C++ swaps or moves its allocation to another vector.
static class JppVector<T> extends ArrayList<T>
        implements JppAssignable<ArrayList<T>>, JppListOwner<T> {
    ArrayList<T> data;
    JppVector() { data = new ArrayList<T>(); }
    JppVector(ArrayList<T> storage) { data = storage; }
    public List<T> jppStorage() { return data; }
    public int size() { return data.size(); }
    public boolean isEmpty() { return data.isEmpty(); }
    public boolean contains(Object value) { return data.contains(value); }
    public int indexOf(Object value) { return data.indexOf(value); }
    public int lastIndexOf(Object value) { return data.lastIndexOf(value); }
    public T get(int index) { return data.get(index); }
    public T set(int index, T value) { return data.set(index, value); }
    public boolean add(T value) { return data.add(value); }
    public void add(int index, T value) { data.add(index, value); }
    public boolean addAll(Collection<? extends T> values) { return data.addAll(values); }
    public boolean addAll(int index, Collection<? extends T> values) {
        return data.addAll(index, values);
    }
    public T remove(int index) { return data.remove(index); }
    public boolean remove(Object value) { return data.remove(value); }
    public void clear() { data.clear(); }
    public void ensureCapacity(int capacity) { data.ensureCapacity(capacity); }
    public void trimToSize() { data.trimToSize(); }
    public boolean removeAll(Collection<?> values) { return data.removeAll(values); }
    public boolean retainAll(Collection<?> values) { return data.retainAll(values); }
    protected void removeRange(int first, int last) { data.subList(first, last).clear(); }
    public Object[] toArray() { return data.toArray(); }
    public <U> U[] toArray(U[] destination) { return data.toArray(destination); }
    public Iterator<T> iterator() { return data.iterator(); }
    public ListIterator<T> listIterator() { return data.listIterator(); }
    public ListIterator<T> listIterator(int index) { return data.listIterator(index); }
    public List<T> subList(int first, int last) { return data.subList(first, last); }
    public Spliterator<T> spliterator() { return data.spliterator(); }
    public void forEach(java.util.function.Consumer<? super T> action) { data.forEach(action); }
    public boolean removeIf(java.util.function.Predicate<? super T> predicate) {
        return data.removeIf(predicate);
    }
    public void replaceAll(java.util.function.UnaryOperator<T> action) { data.replaceAll(action); }
    public void sort(Comparator<? super T> compare) { data.sort(compare); }
    public boolean equals(Object other) { return other == this || data.equals(other); }
    public int hashCode() { return data.hashCode(); }
    public String toString() { return data.toString(); }
    public Object clone() { return new JppVector<T>(new ArrayList<T>(data)); }
    // These names also override the sequenced-list methods on newer JDKs.
    public T getFirst() { return data.get(0); }
    public T getLast() { return data.get(data.size() - 1); }
    public void addFirst(T value) { data.add(0, value); }
    public void addLast(T value) { data.add(value); }
    public T removeFirst() { return data.remove(0); }
    public T removeLast() { return data.remove(data.size() - 1); }
    public ArrayList<T> copy() {
        JppVector<T> result = new JppVector<T>();
        for (T value : data) result.add(jppCopy(value));
        return result;
    }
    public ArrayList<T> jppAssignFrom(ArrayList<T> source) {
        if (source != this) {
            ArrayList<T> replacement = new ArrayList<T>(source.size());
            for (T value : source) replacement.add(jppCopy(value));
            data = replacement;
        }
        return this;
    }
}

static class JppStatic<T> extends JppRef<T> {
    private T value;
    private boolean initialized;
    private final boolean ownsResource;
    JppStatic(T zero, boolean ownsResource) {
        value = zero;
        this.ownsResource = ownsResource;
    }
    synchronized JppStatic<T> initialize(java.util.function.Supplier<T> initializer) {
        if (!initialized) {
            T created = initializer.get();
            if (value != null && value.getClass().isArray())
                Jpp.arrayInit(value, created);
            else
                value = created;
            if (ownsResource && value instanceof AutoCloseable)
                JppStaticResources.track((AutoCloseable)value);
            initialized = true;
        }
        return this;
    }
    T get() {
        return value;
    }
    T set(T next) {
        value = next;
        return next;
    }
}

static class JppStaticResources {
    private static final ArrayList<AutoCloseable> resources = new ArrayList<AutoCloseable>();
    private static boolean registered;
    static synchronized<T extends AutoCloseable> T track(T resource) {
        if (!registered) {
            Runtime.getRuntime().addShutdownHook(new Thread(JppStaticResources::closeAll));
            registered = true;
        }
        resources.add(resource);
        return resource;
    }
    static <T extends AutoCloseable> JppRef<T> temporary(JppRef<T> reference) {
        track(reference.get());
        return reference;
    }
    static synchronized void closeAll() {
        for (int i = resources.size(); i-- > 0;)
            try {
                resources.get(i).close();
            } catch (Exception exception) {
                throw new IllegalStateException(exception);
            }
        resources.clear();
    }
}

static abstract class JppRef<T> {
    abstract T get();
    abstract T set(T value);
    JppPtr<T> pointer() {
        return Jpp.address(this, this, "");
    }
    static <T> JppRef<T> cell(T value) {
        return new JppRef<T>() {
            T data = value;
            T get() {
                return data;
            }
            T set(T v) {
                data = v;
                return v;
            }
        };
    }
    static <T> JppRef<T> of(java.util.function.Supplier<T> get,
                            java.util.function.Consumer<T> set) {
        return of(get, set, (java.util.function.Supplier<JppPtr<T>>)null);
    }
    static <T> JppRef<T> object(T object) {
        return of(() -> object, value -> Jpp.assignValue(object, value),
                  () -> Jpp.pointerValue(object));
    }
    static <T> JppRef<T> of(java.util.function.Supplier<T> get, java.util.function.Consumer<T> set,
                            java.util.function.Supplier<JppPtr<T>> location) {
        return new JppRef<T>() {
            T get() {
                return get.get();
            }
            T set(T v) {
                set.accept(v);
                return v;
            }
            JppPtr<T> pointer() {
                return location == null ? super.pointer() : location.get();
            }
        };
    }
    static <T> JppRef<T> of(java.util.function.Supplier<T> get, java.util.function.Consumer<T> set,
                            Object object, Object field) {
        return new JppRef<T>() {
            T get() {
                return get.get();
            }
            T set(T v) {
                set.accept(v);
                return v;
            }
            JppPtr<T> pointer() {
                return Jpp.address(this, object, field);
            }
        };
    }
    static <O, T> JppRef<T> member(O object, String field, java.util.function.Function<O, T> get,
                                   java.util.function.BiConsumer<O, T> set) {
        return of(() -> get.apply(object), v -> set.accept(object, v), object, field);
    }
    static <T> JppRef<T> at(List<T> a, int i) {
        List<T> storage = Jpp.listStorage(a);
        if (storage != a)
            return at(storage, i);
        if (a instanceof JppDeque)
            return ((JppDeque<T>)a).data.reference(i);
        if (a instanceof JppDequeStorage)
            return ((JppDequeStorage<T>)a).reference(i);
        return of(() -> a.get(i), v -> a.set(i, v), () -> Jpp.pointer(Jpp.begin(a).plus(i)));
    }
    static <T> JppRef<T> front(List<T> values) {
        return at(values, 0);
    }
    static <T> JppRef<T> back(List<T> values) {
        return at(values, values.size() - 1);
    }
    static JppRef<Character> front(JppString value) {
        return at(value, 0);
    }
    static JppRef<Character> back(JppString value) {
        return at(value, value.size() - 1);
    }
    static <T> JppRef<T> at(T[] a, int i) {
        return of(() -> a[i], v -> a[i] = v, () -> Jpp.pointer(Jpp.begin(a).plus(i)));
    }
    static <T> JppRef<T> at(JppIter<T> iterator) {
        if (iterator.list instanceof JppDequeStorage)
            return ((JppDequeStorage<T>)iterator.list).reference(iterator.index());
        JppIter<T> position = iterator.plus(0);
        return of(position::get, position::put, () -> Jpp.pointer(position));
    }
    static <K, V> JppRef<V> at(JppMap<K, V> a, K i) {
        return member(a.entry(i), "second", node -> node.second, (node, value) -> node.second = value);
    }
    static <K, V> JppRef<V> mapAt(JppMap<K, V> map, K key) {
        map.at(key);
        return at(map, key);
    }
    static JppRef<Integer> at(int[] a, int i) {
        return of(() -> a[i], v -> a[i] = v, () -> Jpp.pointer(Jpp.begin(a).plus(i)));
    }
    static JppRef<Long> at(long[] a, int i) {
        return of(() -> a[i], v -> a[i] = v, () -> Jpp.pointer(Jpp.begin(a).plus(i)));
    }
    static JppRef<Double> at(double[] a, int i) {
        return of(() -> a[i], v -> a[i] = v, () -> Jpp.pointer(Jpp.begin(a).plus(i)));
    }
    static JppRef<Float> at(float[] a, int i) {
        return of(() -> a[i], v -> a[i] = v, () -> Jpp.pointer(Jpp.begin(a).plus(i)));
    }
    static JppRef<Boolean> at(boolean[] a, int i) {
        return of(() -> a[i], v -> a[i] = v, () -> Jpp.pointer(Jpp.begin(a).plus(i)));
    }
    static JppRef<Character> at(char[] a, int i) {
        return of(() -> a[i], v -> a[i] = v, () -> Jpp.pointer(Jpp.begin(a).plus(i)));
    }
    static JppRef<Character> at(JppString a, int i) {
        return of(() -> a.get(i), v -> a.put(i, v), () -> Jpp.pointer(a.begin().plus(i)));
    }
}

@SuppressWarnings("unchecked")
static <T> T jppCopy(T value) {
    if (value instanceof JppCopyable)
        return (T)((JppCopyable<?>)value).copy();
    if (value instanceof ArrayList) {
        ArrayList<Object> result = new ArrayList<Object>();
        for (Object item : (ArrayList<?>)value)
            result.add(jppCopy(item));
        return (T)result;
    }
    if (value instanceof ArrayDeque) {
        ArrayDeque<Object> result = new ArrayDeque<Object>();
        for (Object item : (ArrayDeque<?>)value)
            result.add(jppCopy(item));
        return (T)result;
    }
    if (value instanceof PriorityQueue) {
        PriorityQueue source = (PriorityQueue)value;
        PriorityQueue result = new PriorityQueue(source.comparator());
        for (Object item : source)
            result.add(jppCopy(item));
        return (T)result;
    }
    if (value != null && value.getClass().isArray()) {
        int n = java.lang.reflect.Array.getLength(value);
        Object result = java.lang.reflect.Array.newInstance(value.getClass().getComponentType(), n);
        for (int i = 0; i < n; ++i)
            java.lang.reflect.Array.set(result, i, jppCopy(java.lang.reflect.Array.get(value, i)));
        return (T)result;
    }
    return value;
}

interface JppProduct {
    int arity();
    boolean binding(int index);
    Object raw(int index);
    void raw(int index, Object value);
    String memberName(int index);
}

static class JppIgnore implements JppAssignable<JppIgnore> {
    static final JppIgnore INSTANCE = new JppIgnore();
    public JppIgnore copy() { return new JppIgnore(); }
    public JppIgnore jppAssignFrom(JppIgnore other) { return this; }
}

static class JppOrdering implements JppAssignable<JppOrdering> {
    int rank;
    private JppOrdering(int rank) { this.rank = rank; }
    public JppOrdering copy() { return new JppOrdering(rank); }
    public JppOrdering jppAssignFrom(JppOrdering source) { rank = source.rank; return this; }
    JppOrdering reversed() { return new JppOrdering(rank == 2 ? 2 : -rank); }
    static final JppOrdering LESS = new JppOrdering(-1), EQUIVALENT = new JppOrdering(0),
                             GREATER = new JppOrdering(1), UNORDERED = new JppOrdering(2);
    static final JppOrdering STRONG_LESS = new JppOrdering(-1), STRONG_EQUAL = new JppOrdering(0),
                             STRONG_EQUIVALENT = new JppOrdering(0), STRONG_GREATER = new JppOrdering(1);
    static final JppOrdering WEAK_LESS = new JppOrdering(-1), WEAK_EQUIVALENT = new JppOrdering(0),
                             WEAK_GREATER = new JppOrdering(1);
    static final JppOrdering PARTIAL_LESS = new JppOrdering(-1), PARTIAL_EQUIVALENT = new JppOrdering(0),
                             PARTIAL_GREATER = new JppOrdering(1), PARTIAL_UNORDERED = new JppOrdering(2);
    boolean is_eq() { return rank == 0; }
    boolean is_neq() { return rank != 0; }
    boolean is_lt() { return rank == -1; }
    boolean is_lteq() { return rank == -1 || rank == 0; }
    boolean is_gt() { return rank == 1; }
    boolean is_gteq() { return rank == 0 || rank == 1; }
}

static class JppPair<A, B> implements Comparable<JppPair<A, B>>, JppAssignable<JppPair<A, B>>, JppProduct {
    A first;
    B second;
    final boolean[] referenceKinds;
    JppPair(A a, B b) {
        this(null, a, b);
    }
    JppPair(boolean[] referenceKinds, A a, B b) {
        this.referenceKinds = referenceKinds;
        first = a;
        second = b;
    }
    public JppPair<A, B> copy() {
        return new JppPair<A, B>(referenceKinds, jppCopy(first), jppCopy(second));
    }
    public JppPair<A, B> jppAssignFrom(JppPair<A, B> source) {
        first = (A)Jpp.assignProductField(first, Jpp.productValue(source, 0), binding(0));
        second = (B)Jpp.assignProductField(second, Jpp.productValue(source, 1), binding(1));
        return this;
    }
    public int arity() { return 2; }
    public boolean binding(int index) { return referenceKinds != null && referenceKinds[index]; }
    public String memberName(int index) { return index == 0 ? "first" : "second"; }
    public Object raw(int index) {
        switch (index) {
        case 0: return first;
        case 1: return second;
        default: throw new IndexOutOfBoundsException();
        }
    }
    public void raw(int index, Object value) {
        switch (index) {
        case 0: first = (A)value; return;
        case 1: second = (B)value; return;
        default: throw new IndexOutOfBoundsException();
        }
    }
    public int compareTo(JppPair<A, B> other) {
        int c = Jpp.compare(Jpp.productValue(this, 0), Jpp.productValue(other, 0));
        return c != 0 ? c : Jpp.compare(Jpp.productValue(this, 1), Jpp.productValue(other, 1));
    }
    public boolean equals(Object o) {
        if (!(o instanceof JppPair))
            return false;
        JppPair<?, ?> p = (JppPair<?, ?>)o;
        return Jpp.valueEquals(Jpp.productValue(this, 0), Jpp.productValue(p, 0)) &&
            Jpp.valueEquals(Jpp.productValue(this, 1), Jpp.productValue(p, 1));
    }
    public int hashCode() {
        return 31 * Jpp.valueHash(Jpp.productValue(this, 0)) + Jpp.valueHash(Jpp.productValue(this, 1));
    }
}

static class JppBitset implements JppAssignable<JppBitset> {
    final int length;
    final BitSet bits;
    JppBitset(int size) {
        length = size;
        bits = new BitSet(size);
    }
    JppBitset(int size, long value) {
        length = size;
        bits = BitSet.valueOf(new long[] {value});
        if (bits.length() > size)
            bits.clear(size, bits.length());
    }
    JppBitset(int size, String text, long position, long count, char zero, char one) {
        this(size);
        if (Long.compareUnsigned(position, text.length()) > 0)
            throw new IndexOutOfBoundsException();
        int start = (int)position, available = text.length() - start;
        int selected = Long.compareUnsigned(count, available) < 0 ? (int)count : available;
        for (int i = 0; i < selected; ++i) {
            char c = text.charAt(start + i);
            if (c != zero && c != one)
                throw new IllegalArgumentException("invalid bitset character");
        }
        int used = Math.min(size, selected);
        for (int i = 0; i < used; ++i)
            if (text.charAt(start + used - i - 1) != zero)
                bits.set(i);
    }
    int index(long position) {
        if (Long.compareUnsigned(position, length) >= 0)
            throw new IndexOutOfBoundsException();
        return (int)position;
    }
    boolean get(long position) {
        return bits.get(index(position));
    }
    boolean test(long position) {
        return get(position);
    }
    JppBitRef ref(long position) {
        return new JppBitRef(this, index(position));
    }
    int size() {
        return length;
    }
    int count() {
        return bits.cardinality();
    }
    int findFirst() {
        int result = bits.nextSetBit(0);
        return result < 0 ? length : result;
    }
    int findNext(long position) {
        long start = position + 1;
        if (Long.compareUnsigned(start, length) >= 0)
            return length;
        int result = bits.nextSetBit((int)start);
        return result < 0 ? length : result;
    }
    boolean any() {
        return !bits.isEmpty();
    }
    boolean none() {
        return bits.isEmpty();
    }
    boolean all() {
        return bits.nextClearBit(0) >= length;
    }
    JppBitset set() {
        bits.set(0, length);
        return this;
    }
    JppBitset set(long position) {
        return set(position, true);
    }
    JppBitset set(long position, boolean value) {
        bits.set(index(position), value);
        return this;
    }
    JppBitset reset() {
        bits.clear();
        return this;
    }
    JppBitset reset(long position) {
        return set(position, false);
    }
    JppBitset flip() {
        bits.flip(0, length);
        return this;
    }
    JppBitset flip(long position) {
        bits.flip(index(position));
        return this;
    }
    long toLong(int width) {
        if (bits.nextSetBit(width) >= 0)
            throw new ArithmeticException("bitset integer overflow");
        long[] words = bits.toLongArray();
        return words.length == 0 ? 0 : words[0];
    }
    JppString to_string(char zero, char one) {
        StringBuilder text = new StringBuilder(length);
        for (int i = length; i-- > 0;)
            text.append(bits.get(i) ? one : zero);
        return new JppString(text);
    }
    public String toString() {
        return to_string('0', '1').toString();
    }
    public boolean equals(Object other) {
        return other instanceof JppBitset && length == ((JppBitset)other).length &&
            bits.equals(((JppBitset)other).bits);
    }
    public int hashCode() {
        return 31 * length + bits.hashCode();
    }
    public JppBitset copy() {
        JppBitset result = new JppBitset(length);
        result.bits.or(bits);
        return result;
    }
    public JppBitset jppAssignFrom(JppBitset source) {
        if (source != this) {
            bits.clear();
            bits.or(source.bits);
        }
        return this;
    }
    static void swap(JppBitset left, JppBitset right) {
        JppBitset temporary = left.copy();
        left.jppAssignFrom(right);
        right.jppAssignFrom(temporary);
    }
    static JppBitset andAssign(JppBitset source, JppBitset target) {
        target.bits.and(source.bits);
        return target;
    }
    static JppBitset orAssign(JppBitset source, JppBitset target) {
        target.bits.or(source.bits);
        return target;
    }
    static JppBitset xorAssign(JppBitset source, JppBitset target) {
        target.bits.xor(source.bits);
        return target;
    }
    JppBitset and(JppBitset other) {
        return andAssign(other, copy());
    }
    JppBitset or(JppBitset other) {
        return orAssign(other, copy());
    }
    JppBitset xor(JppBitset other) {
        return xorAssign(other, copy());
    }
    JppBitset complement() {
        return copy().flip();
    }
    private JppBitset shift(long amount, boolean left) {
        if (Long.compareUnsigned(amount, length) >= 0) {
            bits.clear();
            return this;
        }
        if (amount == 0 || bits.isEmpty())
            return this;
        long[] source = bits.toLongArray();
        long[] result = new long[(int)((length + 63L) / 64)];
        int words = (int)(amount >>> 6), remainder = (int)(amount & 63);
        for (int i = left ? 0 : words; i < source.length; ++i) {
            int target = left ? i + words : i - words;
            if (target < result.length)
                result[target] |= left ? source[i] << remainder : source[i] >>> remainder;
            int carry = target + (left ? 1 : -1);
            if (remainder != 0 && carry >= 0 && carry < result.length)
                result[carry] |= left ? source[i] >>> (64 - remainder)
                                     : source[i] << (64 - remainder);
        }
        if ((length & 63) != 0)
            result[result.length - 1] &= (1L << (length & 63)) - 1;
        bits.clear();
        bits.or(BitSet.valueOf(result));
        return this;
    }
    JppBitset shiftLeft(long amount) {
        return copy().shift(amount, true);
    }
    JppBitset shiftRight(long amount) {
        return copy().shift(amount, false);
    }
    static JppBitset shiftLeftAssign(long amount, JppBitset target) {
        return target.shift(amount, true);
    }
    static JppBitset shiftRightAssign(long amount, JppBitset target) {
        return target.shift(amount, false);
    }
}

static class JppBitRef implements JppCopyable<JppBitRef> {
    final JppBitset owner;
    final int index;
    JppBitRef(JppBitset value, int position) {
        owner = value;
        index = position;
    }
    boolean get() {
        return owner.bits.get(index);
    }
    JppBitRef assign(boolean value) {
        owner.bits.set(index, value);
        return this;
    }
    JppBitRef assign(JppBitRef other) {
        return assign(other.get());
    }
    static JppBitRef assign(boolean value, JppBitRef target) {
        return target.assign(value);
    }
    static JppBitRef assign(JppBitRef source, JppBitRef target) {
        return target.assign(source);
    }
    JppBitRef flip() {
        owner.bits.flip(index);
        return this;
    }
    public JppBitRef copy() {
        return new JppBitRef(owner, index);
    }
}

static class JppDequeStorage<T> extends AbstractList<T> implements RandomAccess {
    ArrayList<T> ring = new ArrayList<T>(Collections.nCopies(16, (T)null));
    int head, count;
    long first;
    public int size() {
        return count;
    }
    private int slot(int index) {
        return index >= ring.size() - head ? index - (ring.size() - head) : head + index;
    }
    private void check(int index) {
        if (index < 0 || index >= count)
            throw new IndexOutOfBoundsException();
    }
    public T get(int index) {
        check(index);
        return ring.get(slot(index));
    }
    public T set(int index, T value) {
        check(index);
        return ring.set(slot(index), value);
    }
    private void grow() {
        if (count < ring.size())
            return;
        int capacity =
            ring.size() <= (Integer.MAX_VALUE - 8) / 2 ? ring.size() * 2 : Integer.MAX_VALUE - 8;
        if (capacity <= count)
            throw new OutOfMemoryError("deque exceeds Java's indexing limit");
        ArrayList<T> next = new ArrayList<T>(Collections.nCopies(capacity, (T)null));
        for (int i = 0; i < count; ++i)
            next.set(i, get(i));
        ring = next;
        head = 0;
    }
    public void add(int index, T value) {
        if (index < 0 || index > count)
            throw new IndexOutOfBoundsException();
        grow();
        if (index == 0) {
            head = head == 0 ? ring.size() - 1 : head - 1;
            --first;
        } else
            for (int i = count; i > index; --i)
                ring.set(slot(i), ring.get(slot(i - 1)));
        ring.set(slot(index), value);
        ++count;
        ++modCount;
    }
    public T remove(int index) {
        T old = get(index);
        removeRange(index, index + 1);
        return old;
    }
    protected void removeRange(int begin, int end) {
        if (begin < 0 || end < begin || end > count)
            throw new IndexOutOfBoundsException();
        int removed = end - begin;
        if (removed == 0)
            return;
        if (begin == 0 || begin < count - end) {
            for (int i = begin - 1; i >= 0; --i)
                ring.set(slot(i + removed), ring.get(slot(i)));
            for (int i = 0; i < removed; ++i)
                ring.set(slot(i), null);
            head = slot(removed);
            first += removed;
        } else {
            for (int i = end; i < count; ++i)
                ring.set(slot(i - removed), ring.get(slot(i)));
            for (int i = count - removed; i < count; ++i)
                ring.set(slot(i), null);
        }
        count -= removed;
        ++modCount;
    }
    public void clear() {
        for (int i = 0; i < count; ++i)
            ring.set(slot(i), null);
        first += count;
        head = count = 0;
        ++modCount;
    }
    JppRef<T> reference(int index) {
        check(index);
        final long address = first + index;
        return JppRef.of(()
                             -> get((int)(address - first)),
                         value -> set((int)(address - first), value), this, address);
    }
    public Iterator<T> iterator() {
        return new Iterator<T>() {
            long next = first;
            public boolean hasNext() {
                return Long.compareUnsigned(next - first, count) < 0;
            }
            public T next() {
                if (!hasNext())
                    throw new NoSuchElementException();
                return get((int)(next++ - first));
            }
        };
    }
}

static class JppDeque<T> extends AbstractList<T> implements JppAssignable<JppDeque<T>> {
    JppDequeStorage<T> data = new JppDequeStorage<T>();
    JppDeque() {}
    JppDeque(Collection<? extends T> values) {
        for (T value : values)
            addLast(jppCopy(value));
    }
    public int size() {
        return data.size();
    }
    public T get(int index) {
        return data.get(index);
    }
    public T set(int index, T value) {
        return data.set(index, value);
    }
    public void add(int index, T value) {
        data.add(index, value);
    }
    public T remove(int index) {
        return data.remove(index);
    }
    public void clear() {
        data.clear();
    }
    public Iterator<T> iterator() {
        return data.iterator();
    }
    public void addFirst(T value) {
        data.add(0, value);
    }
    public void addLast(T value) {
        data.add(size(), value);
    }
    public T getFirst() {
        return get(0);
    }
    public T getLast() {
        return get(size() - 1);
    }
    public T removeFirst() {
        return data.remove(0);
    }
    public T removeLast() {
        return data.remove(size() - 1);
    }
    void swap(JppDeque<T> other) {
        JppDequeStorage<T> saved = data;
        data = other.data;
        other.data = saved;
    }
    public JppDeque<T> copy() {
        return new JppDeque<T>(this);
    }
    public JppDeque<T> jppAssignFrom(JppDeque<T> other) {
        if (other != this)
            data = other.copy().data;
        return this;
    }
}

static class JppDequeIter<T> extends JppIter<T> {
    final JppDequeStorage<T> deque;
    long address;
    JppDequeIter(JppDequeStorage<T> storage, long address, int direction) {
        super(storage, (int)(address - storage.first), direction, storage);
        deque = storage;
        this.address = address;
    }
    int index() {
        return (int)(address - deque.first);
    }
    T get() {
        return deque.get(index());
    }
    T put(T value) {
        deque.set(index(), value);
        return value;
    }
    JppIter<T> plus(int offset) {
        return new JppDequeIter<T>(deque, address + (long)offset * step, step);
    }
    JppIter<T> advance(int offset) {
        address += (long)offset * step;
        return this;
    }
    int minus(JppIter<?> other) {
        return (int)((address - ((JppDequeIter<?>)other).address) * step);
    }
    boolean same(JppIter<?> other) {
        return other instanceof JppDequeIter && deque == ((JppDequeIter<?>)other).deque &&
            address == ((JppDequeIter<?>)other).address;
    }
}

static class JppSeedSeq {
    private final int[] words;
    JppSeedSeq() {
        words = new int[0];
    }
    JppSeedSeq(int[] values) {
        words = values.clone();
    }
    static <T> JppSeedSeq from(JppIter<T> first, JppIter<T> last,
                                 java.util.function.Function<T, Integer> convert) {
        ArrayList<Integer> values = new ArrayList<Integer>();
        for (JppIter<T> at = first.plus(0); !at.same(last); at.advance(1))
            values.add(convert.apply(at.get()));
        int[] words = new int[values.size()];
        for (int i = 0; i < words.length; ++i)
            words[i] = values.get(i);
        return new JppSeedSeq(words);
    }
    int size() {
        return words.length;
    }
    private static int wrap(int index, int offset, int size) {
        return index >= size - offset ? index - (size - offset) : index + offset;
    }
    int[] generate(int size) {
        int[] values = new int[size];
        if (size == 0)
            return values;
        Arrays.fill(values, 0x8b8b8b8b);
        int spread = size >= 623 ? 11 : size >= 68 ? 7 : size >= 39 ? 5 : size >= 7 ? 3
                                                                                        : (size - 1) / 2;
        int firstOffset = (size - spread) / 2, secondOffset = firstOffset + spread;
        long rounds = Math.max((long)words.length + 1, size);
        for (long round = 0; round < rounds; ++round) {
            int index = (int)(round % size);
            int first = wrap(index, firstOffset, size), second = wrap(index, secondOffset, size);
            int previous = index == 0 ? size - 1 : index - 1;
            int mixed = values[index] ^ values[first] ^ values[previous];
            int a = 1664525 * (mixed ^ (mixed >>> 27));
            int b = a + index + (round == 0 ? words.length
                                      : round <= words.length ? words[(int)round - 1] : 0);
            values[first] += a;
            values[second] += b;
            values[index] = b;
        }
        for (long round = rounds; round < rounds + size; ++round) {
            int index = (int)(round % size);
            int first = wrap(index, firstOffset, size), second = wrap(index, secondOffset, size);
            int previous = index == 0 ? size - 1 : index - 1;
            int mixed = values[index] + values[first] + values[previous];
            int a = 1566083941 * (mixed ^ (mixed >>> 27)), b = a - index;
            values[first] ^= a;
            values[second] ^= b;
            values[index] = b;
        }
        return values;
    }
    <T> void generate(JppIter<T> first, JppIter<T> last,
                        java.util.function.Function<Integer, T> convert) {
        int[] generated = generate(last.minus(first));
        for (int i = 0; i < generated.length; ++i)
            first.plus(i).put(convert.apply(generated[i]));
    }
    <T> void param(JppIter<T> destination, java.util.function.Function<Integer, T> convert) {
        for (int i = 0; i < words.length; ++i)
            destination.plus(i).put(convert.apply(words[i]));
    }
}

static class JppMtEngine implements JppAssignable<JppMtEngine> {
    final int width, middle, separation, u, s, t, l;
    final long a, d, b, c, multiplier, mask;
    long[] state;
    int position;
    JppMtEngine(int width, int size, int middle, int separation, long a, int u, long d,
                  int s, long b, int t, long c, int l, long multiplier, long seed) {
        this.width = width;
        this.middle = middle;
        this.separation = separation;
        this.a = a;
        this.u = u;
        this.d = d;
        this.s = s;
        this.b = b;
        this.t = t;
        this.c = c;
        this.l = l;
        this.multiplier = multiplier;
        mask = width == 64 ? -1L : (1L << width) - 1;
        state = new long[size];
        seed(seed);
    }
    JppMtEngine(int width, int size, int middle, int separation, long a, int u, long d,
                  int s, long b, int t, long c, int l, long multiplier, JppSeedSeq seed) {
        this(width, size, middle, separation, a, u, d, s, b, t, c, l, multiplier, 0L);
        seed(seed);
    }
    private JppMtEngine(JppMtEngine other) {
        width = other.width;
        middle = other.middle;
        separation = other.separation;
        a = other.a;
        u = other.u;
        d = other.d;
        s = other.s;
        b = other.b;
        t = other.t;
        c = other.c;
        l = other.l;
        multiplier = other.multiplier;
        mask = other.mask;
        state = other.state.clone();
        position = other.position;
    }
    void seed(long value) {
        state[0] = value & mask;
        for (int i = 1; i < state.length; ++i) {
            long previous = state[i - 1];
            state[i] = (multiplier * (previous ^ (previous >>> (width - 2))) + i) & mask;
        }
        position = state.length;
    }
    void seed(JppSeedSeq sequence) {
        int words = (width + 31) / 32;
        int[] generated = sequence.generate(Math.multiplyExact(words, state.length));
        boolean empty = true;
        long lower = separation == 64 ? -1L : (1L << separation) - 1;
        for (int i = 0; i < state.length; ++i) {
            long value = Integer.toUnsignedLong(generated[words * i]);
            if (words == 2)
                value |= Integer.toUnsignedLong(generated[words * i + 1]) << 32;
            state[i] = value & mask;
            if ((i == 0 ? state[i] & (mask ^ lower) : state[i]) != 0)
                empty = false;
        }
        if (empty)
            state[0] = 1L << (width - 1);
        position = state.length;
    }
    private static long right(long value, int amount) {
        return amount == 64 ? 0 : value >>> amount;
    }
    private static long left(long value, int amount) {
        return amount == 64 ? 0 : value << amount;
    }
    private void twist() {
        long lower = separation == 64 ? -1L : (1L << separation) - 1;
        long upper = mask ^ lower;
        for (int i = 0; i < state.length; ++i) {
            long combined = (state[i] & upper) | (state[(i + 1) % state.length] & lower);
            int shifted = i >= state.length - middle ? i - (state.length - middle) : i + middle;
            state[i] = (state[shifted] ^ (combined >>> 1) ^
                         ((combined & 1) == 0 ? 0 : a)) & mask;
        }
        position = 0;
    }
    long next() {
        if (position == state.length)
            twist();
        long value = state[position++];
        value ^= right(value, u) & d;
        value ^= left(value, s) & b;
        value ^= left(value, t) & c;
        value ^= right(value, l);
        return value & mask;
    }
    void discard(long count) {
        while (Long.compareUnsigned(count, state.length - position) > 0) {
            count -= state.length - position;
            twist();
        }
        position += (int)count;
    }
    boolean sameState(JppMtEngine other) {
        return position == other.position && Arrays.equals(state, other.state);
    }
    public JppMtEngine copy() {
        return new JppMtEngine(this);
    }
    public JppMtEngine jppAssignFrom(JppMtEngine other) {
        if (other != this) {
            state = other.state.clone();
            position = other.position;
        }
        return this;
    }
}

static class JppString
    implements Comparable<JppString>, Iterable<Character>, JppAssignable<JppString> {
    private final StringBuilder data;
    JppString() {
        data = new StringBuilder();
    }
    JppString(Object value) {
        data = new StringBuilder(value instanceof JppIter || value instanceof char[]
                                     ? Jpp.cString(value)
                                     : String.valueOf(value));
    }
    JppString(Collection<Character> values) {
        data = new StringBuilder(values.size());
        for (char value : values)
            data.append(value);
    }
    JppString(JppIter<Character> source, long count) {
        int size = Jpp.stringSize(count);
        data = new StringBuilder(size);
        for (int i = 0; i < size; ++i)
            data.append((char)source.plus(i).get());
    }
    JppString(JppIter<Character> first, JppIter<Character> last) {
        data = new StringBuilder();
        for (JppIter<Character> at = first.plus(0); !at.same(last); at.advance(1))
            data.append((char)at.get());
    }
    JppString(long count, char value) {
        int length = Jpp.stringSize(count);
        data = new StringBuilder(length);
        while (length-- > 0)
            data.append(value);
    }
    public JppString copy() {
        return new JppString(data);
    }
    public JppString jppAssignFrom(JppString source) {
        if (source != this) {
            data.setLength(0);
            data.append(source.data);
        }
        return this;
    }
    JppString jppSetText(String text) {
        data.setLength(0);
        data.append(text);
        return this;
    }
    int size() {
        return data.length();
    }
    int length() {
        return size();
    }
    int capacity() {
        return data.capacity();
    }
    void reserve(long count) {
        data.ensureCapacity(Jpp.stringSize(count));
    }
    void resize(long count, char fill) {
        int length = Jpp.stringSize(count);
        if (fill == '\0' || length <= size())
            data.setLength(length);
        else {
            data.ensureCapacity(length);
            while (size() < length)
                data.append(fill);
        }
    }
    boolean empty() {
        return size() == 0;
    }
    char get(int index) {
        return data.charAt(index);
    }
    char put(int index, char value) {
        data.setCharAt(index, value);
        return value;
    }
    JppString append(Object value) {
        data.append(Jpp.cString(value));
        return this;
    }
    JppString append(long count, char value) {
        return append(new JppString(count, value));
    }
    JppString append(JppIter<Character> source, long count) {
        return append(new JppString(source, count));
    }
    JppString append(JppIter<Character> first, JppIter<Character> last) {
        return append(new JppString(first, last));
    }
    JppString append(Collection<Character> values) {
        return append(new JppString(values));
    }
    JppString append(JppString source, long start) {
        return append(source.substr(start));
    }
    JppString append(JppString source, long start, long count) {
        return append(source.substr(start, count));
    }
    JppString assign(Object value) {
        return jppSetText(Jpp.cString(value));
    }
    JppString assign(long count, char value) {
        return jppAssignFrom(new JppString(count, value));
    }
    JppString assign(JppIter<Character> source, long count) {
        return jppAssignFrom(new JppString(source, count));
    }
    JppString assign(JppIter<Character> first, JppIter<Character> last) {
        return jppAssignFrom(new JppString(first, last));
    }
    JppString assign(Collection<Character> values) {
        return jppAssignFrom(new JppString(values));
    }
    JppString assign(JppString source, long start) {
        return jppAssignFrom(source.substr(start));
    }
    JppString assign(JppString source, long start, long count) {
        return jppAssignFrom(source.substr(start, count));
    }
    void push_back(char c) {
        data.append(c);
    }
    void pop_back() {
        data.setLength(size() - 1);
    }
    void clear() {
        data.setLength(0);
    }
    char front() {
        return get(0);
    }
    char back() {
        return get(size() - 1);
    }
    JppString substr(long start) {
        return substr(start, -1L);
    }
    JppString substr(long start, long length) {
        if (Long.compareUnsigned(start, size()) > 0)
            throw new IndexOutOfBoundsException();
        int available = size() - (int)start;
        int count = Long.compareUnsigned(length, available) < 0 ? (int)length : available;
        return new JppString(data.substring((int)start, (int)start + count));
    }
    long find(Object value) {
        int n = data.indexOf(Jpp.cString(value));
        return n < 0 ? -1L : n;
    }
    long find(Object value, long from) {
        if (Long.compareUnsigned(from, size()) > 0)
            return -1L;
        int n = data.indexOf(Jpp.cString(value), (int)from);
        return n < 0 ? -1L : n;
    }
    long find(JppIter<Character> value, long from, long count) {
        return find(new JppString(value, count), from);
    }
    JppIter<Character> begin() {
        return new JppIter<Character>(view(), 0, 1, this);
    }
    JppIter<Character> end() {
        return new JppIter<Character>(view(), size(), 1, this);
    }
    JppIter<Character> rbegin() {
        return new JppIter<Character>(view(), size() - 1, -1, this);
    }
    JppIter<Character> rend() {
        return new JppIter<Character>(view(), -1, -1, this);
    }
    JppPtr<Character> c_str() {
        return new JppPtr<Character>(view(), 0, this);
    }
    private List<Character> view() {
        return new AbstractList<Character>() {
            public int size() {
                return data.length();
            }
            public Character get(int i) {
                return i == data.length() ? '\0' : data.charAt(i);
            }
            public Character set(int i, Character c) {
                if (i == data.length() && c == '\0')
                    return c;
                char old = data.charAt(i);
                data.setCharAt(i, c);
                return old;
            }
        };
    }
    public Iterator<Character> iterator() {
        return view().iterator();
    }
    public int compareTo(JppString other) {
        return toString().compareTo(other.toString());
    }
    public boolean equals(Object other) {
        return other instanceof JppString && toString().equals(other.toString());
    }
    public int hashCode() {
        return toString().hashCode();
    }
    public String toString() {
        return data.toString();
    }
}

static class JppDuration implements JppAssignable<JppDuration>, Comparable<JppDuration> {
    long ticks;
    final long numerator, denominator;
    JppDuration(long ticks, long numerator, long denominator) {
        this.ticks = ticks;
        long divisor = Jpp.gcd(numerator, denominator);
        this.numerator = numerator / divisor;
        this.denominator = denominator / divisor;
    }
    long count() {
        return ticks;
    }
    public JppDuration copy() {
        return new JppDuration(ticks, numerator, denominator);
    }
    public JppDuration jppAssignFrom(JppDuration source) {
        ticks = source.convert(numerator, denominator, "cast").ticks;
        return this;
    }
    JppDuration convert(long targetNumerator, long targetDenominator, String rounding) {
        BigInteger top = BigInteger.valueOf(ticks)
                             .multiply(BigInteger.valueOf(numerator))
                             .multiply(BigInteger.valueOf(targetDenominator));
        BigInteger bottom =
            BigInteger.valueOf(denominator).multiply(BigInteger.valueOf(targetNumerator));
        BigInteger[] division = top.divideAndRemainder(bottom);
        BigInteger result = division[0];
        int sign = division[1].signum();
        if ((rounding.equals("floor") && sign < 0) || (rounding.equals("ceil") && sign > 0))
            result = result.add(BigInteger.valueOf(sign));
        if (rounding.equals("round")) {
            int comparison = division[1].abs().shiftLeft(1).compareTo(bottom);
            if (comparison > 0 || (comparison == 0 && result.testBit(0)))
                result = result.add(BigInteger.valueOf(sign));
        }
        return new JppDuration(result.longValue(), targetNumerator, targetDenominator);
    }
    JppDuration add(JppDuration other, boolean subtract, long targetNumerator,
                    long targetDenominator) {
        long left = convert(targetNumerator, targetDenominator, "cast").ticks;
        long right = other.convert(targetNumerator, targetDenominator, "cast").ticks;
        return new JppDuration(subtract ? left - right : left + right, targetNumerator,
                               targetDenominator);
    }
    JppDuration scale(long value, String operation) {
        return new JppDuration(operation.equals("*")   ? ticks * value
                               : operation.equals("/") ? ticks / value
                                                       : ticks % value,
                               numerator, denominator);
    }
    JppDuration remainder(JppDuration other, long numerator, long denominator) {
        long left = convert(numerator, denominator, "cast").ticks;
        long right = other.convert(numerator, denominator, "cast").ticks;
        return new JppDuration(left % right, numerator, denominator);
    }
    long quotient(JppDuration other) {
        long numerator = Jpp.gcd(this.numerator, other.numerator);
        long denominator =
            BigInteger.valueOf(this.denominator)
                .divide(BigInteger.valueOf(Jpp.gcd(this.denominator, other.denominator)))
                .multiply(BigInteger.valueOf(other.denominator))
                .longValueExact();
        return convert(numerator, denominator, "cast").ticks /
            other.convert(numerator, denominator, "cast").ticks;
    }
    static JppDuration addAssign(JppDuration right, JppDuration left, boolean subtract) {
        return left.jppAssignFrom(left.add(right, subtract, left.numerator, left.denominator));
    }
    static JppDuration scaleAssign(long right, JppDuration left, String operation) {
        return left.jppAssignFrom(left.scale(right, operation));
    }
    JppDuration negate() {
        return new JppDuration(-ticks, numerator, denominator);
    }
    JppDuration absolute() {
        return ticks < 0 ? negate() : copy();
    }
    JppDuration update(int increment, boolean post) {
        JppDuration previous = post ? copy() : this;
        ticks += increment;
        return previous;
    }
    int compare(JppDuration other) {
        BigInteger left = BigInteger.valueOf(ticks)
                              .multiply(BigInteger.valueOf(numerator))
                              .multiply(BigInteger.valueOf(other.denominator));
        BigInteger right = BigInteger.valueOf(other.ticks)
                               .multiply(BigInteger.valueOf(other.numerator))
                               .multiply(BigInteger.valueOf(denominator));
        return left.compareTo(right);
    }
    public int compareTo(JppDuration other) {
        return compare(other);
    }
    String suffix() {
        if (numerator == 1) {
            long[] divisors = {1000000000000000000L,
                               1000000000000000L,
                               1000000000000L,
                               1000000000L,
                               1000000L,
                               1000L,
                               100L,
                               10L,
                               1L};
            String[] names = {"as", "fs", "ps", "ns", "us", "ms", "cs", "ds", "s"};
            for (int i = 0; i < divisors.length; ++i)
                if (denominator == divisors[i])
                    return names[i];
        }
        if (denominator == 1) {
            long[] multiples = {10L,
                                100L,
                                1000L,
                                1000000L,
                                1000000000L,
                                1000000000000L,
                                1000000000000000L,
                                1000000000000000000L,
                                60L,
                                3600L,
                                86400L};
            String[] names = {"das", "hs", "ks", "Ms", "Gs", "Ts", "Ps", "Es", "min", "h", "d"};
            for (int i = 0; i < multiples.length; ++i)
                if (numerator == multiples[i])
                    return names[i];
        }
        return "[" + numerator + (denominator == 1 ? "" : "/" + denominator) + "]s";
    }
    public String toString() {
        return Long.toString(ticks) + suffix();
    }
}

static class JppTimePoint implements JppAssignable<JppTimePoint>, Comparable<JppTimePoint> {
    final JppDuration elapsed;
    JppTimePoint(JppDuration duration) {
        elapsed = duration.copy();
    }
    public JppTimePoint copy() {
        return new JppTimePoint(elapsed);
    }
    public JppTimePoint jppAssignFrom(JppTimePoint source) {
        elapsed.jppAssignFrom(source.elapsed);
        return this;
    }
    JppDuration time_since_epoch() {
        return elapsed.copy();
    }
    JppTimePoint shift(JppDuration amount, boolean subtract, long numerator, long denominator) {
        return new JppTimePoint(elapsed.add(amount, subtract, numerator, denominator));
    }
    JppDuration difference(JppTimePoint other, long numerator, long denominator) {
        return elapsed.add(other.elapsed, true, numerator, denominator);
    }
    static JppTimePoint shiftAssign(JppDuration right, JppTimePoint left, boolean subtract) {
        left.elapsed.jppAssignFrom(
            left.elapsed.add(right, subtract, left.elapsed.numerator, left.elapsed.denominator));
        return left;
    }
    int compare(JppTimePoint other) {
        return elapsed.compare(other.elapsed);
    }
    public int compareTo(JppTimePoint other) {
        return compare(other);
    }
    static JppTimePoint now(boolean steady) {
        long nanoseconds;
        if (steady)
            nanoseconds = System.nanoTime();
        else {
            java.time.Instant instant = java.time.Instant.now();
            nanoseconds = instant.getEpochSecond() * 1000000000L + instant.getNano();
        }
        return new JppTimePoint(new JppDuration(nanoseconds, 1L, 1000000000L));
    }
    static void sleep(JppDuration duration) {
        if (duration.ticks <= 0)
            return;
        BigInteger nanos = BigInteger.valueOf(duration.ticks)
                               .multiply(BigInteger.valueOf(duration.numerator))
                               .multiply(BigInteger.valueOf(1000000000L))
                               .add(BigInteger.valueOf(duration.denominator - 1))
                               .divide(BigInteger.valueOf(duration.denominator));
        BigInteger[] parts = nanos.divideAndRemainder(BigInteger.valueOf(1000000L));
        try {
            Thread.sleep(parts[0].longValueExact(), parts[1].intValue());
        } catch (InterruptedException interrupted) {
            Thread.currentThread().interrupt();
            throw new IllegalStateException(interrupted);
        }
    }
}

static class JppIotaFactory implements JppCopyable<JppIotaFactory> {
    static final JppIotaFactory INSTANCE = new JppIotaFactory();
    public JppIotaFactory copy() { return new JppIotaFactory(); }
}

static class JppRangesForEach implements JppCopyable<JppRangesForEach> {
    static final JppRangesForEach INSTANCE = new JppRangesForEach();
    public JppRangesForEach copy() { return new JppRangesForEach(); }
}

static class JppRangeResult<I, F> implements JppAssignable<JppRangeResult<I, F>> {
    I in;
    F fun;
    JppRangeResult(I in, F fun) { this.in = in; this.fun = fun; }
    public JppRangeResult<I, F> copy() {
        return new JppRangeResult<I, F>(jppCopy(in), jppCopy(fun));
    }
    public JppRangeResult<I, F> jppAssignFrom(JppRangeResult<I, F> other) {
        in = Jpp.assignValue(in, other.in);
        fun = Jpp.assignValue(fun, other.fun);
        return this;
    }
}

static class JppIotaIter<T> implements JppAssignable<JppIotaIter<T>>, Comparable<JppIotaIter<T>> {
    BigInteger position;
    java.util.function.Function<BigInteger, T> decode;
    int width;
    boolean signed;
    JppIotaIter(BigInteger position, java.util.function.Function<BigInteger, T> decode,
                int width, boolean signed) {
        this.position = canonical(position, width, signed);
        this.decode = decode;
        this.width = width;
        this.signed = signed;
    }
    static BigInteger canonical(BigInteger value, int width, boolean signed) {
        if (signed ? value.bitLength() < width : value.signum() >= 0 && value.bitLength() <= width)
            return value;
        BigInteger modulus = BigInteger.ONE.shiftLeft(width);
        BigInteger low = value.and(modulus.subtract(BigInteger.ONE));
        return signed && low.testBit(width - 1) ? low.subtract(modulus) : low;
    }
    T get() { return decode.apply(position); }
    T get(BigInteger offset) { return decode.apply(canonical(position.add(offset), width, signed)); }
    JppIotaIter<T> plus(BigInteger offset) {
        return new JppIotaIter<T>(position.add(offset), decode, width, signed);
    }
    JppIotaIter<T> advance(BigInteger offset) {
        position = canonical(position.add(offset), width, signed);
        return this;
    }
    JppIotaIter<T> post(BigInteger offset) {
        JppIotaIter<T> old = copy();
        advance(offset);
        return old;
    }
    public JppIotaIter<T> copy() { return plus(BigInteger.ZERO); }
    public int compareTo(JppIotaIter<T> other) { return position.compareTo(other.position); }
    public JppIotaIter<T> jppAssignFrom(JppIotaIter<T> other) {
        position = other.position;
        decode = other.decode;
        width = other.width;
        signed = other.signed;
        return this;
    }
}

// Iota stores its endpoints, independent of the number of generated values.
static class JppIota<T> implements JppAssignable<JppIota<T>>, Iterable<T> {
    BigInteger first, last;
    java.util.function.Function<BigInteger, T> decode;
    int width;
    boolean signed;
    JppIota(BigInteger first, BigInteger last,
             java.util.function.Function<BigInteger, T> decode, int width, boolean signed) {
        if (first.compareTo(last) > 0)
            throw new IllegalArgumentException("invalid iota endpoint order");
        this.first = first;
        this.last = last;
        this.decode = decode;
        this.width = width;
        this.signed = signed;
    }
    JppIotaIter<T> begin() { return new JppIotaIter<T>(first, decode, width, signed); }
    JppIotaIter<T> end() { return new JppIotaIter<T>(last, decode, width, signed); }
    BigInteger size() { return last.subtract(first); }
    boolean empty() { return first.equals(last); }
    T front() { return decode.apply(first); }
    T back() { return decode.apply(last.subtract(BigInteger.ONE)); }
    T get(BigInteger offset) { return decode.apply(JppIotaIter.canonical(first.add(offset), width, signed)); }
    public JppIota<T> copy() { return new JppIota<T>(first, last, decode, width, signed); }
    public JppIota<T> jppAssignFrom(JppIota<T> other) {
        first = other.first;
        last = other.last;
        decode = other.decode;
        width = other.width;
        signed = other.signed;
        return this;
    }
    public Iterator<T> iterator() {
        BigInteger endpoint = last;
        JppIotaIter<T> current = begin();
        return new Iterator<T>() {
            public boolean hasNext() { return !current.position.equals(endpoint); }
            public T next() {
                if (!hasNext())
                    throw new NoSuchElementException();
                T result = current.get();
                current.advance(BigInteger.ONE);
                return result;
            }
        };
    }
    static <T, F> JppRangeResult<JppIotaIter<T>, F> forEach(
        JppIota<T> range, F argument, java.util.function.UnaryOperator<F> move,
        java.util.function.BiConsumer<F, T> action) {
        F function = move.apply(argument);
        JppIotaIter<T> iterator = range.begin();
        BigInteger end = range.last;
        while (!iterator.position.equals(end)) {
            action.accept(function, iterator.get());
            iterator.advance(BigInteger.ONE);
        }
        return new JppRangeResult<JppIotaIter<T>, F>(iterator, move.apply(function));
    }
}

static class JppIter<T> implements JppCopyable<JppIter<T>> {
    final List<T> list;
    final Object storage;
    int pos;
    final int step;
    JppIter() {
        this(null, 0, 1);
    }
    JppIter(List<T> values, int index, int direction) {
        this(values, index, direction, values);
    }
    JppIter(List<T> values, int index, int direction, Object identity) {
        list = values;
        storage = identity;
        pos = index;
        step = direction;
    }
    public JppIter<T> copy() {
        return plus(0);
    }
    T get() {
        return list.get(pos);
    }
    int index() {
        return pos;
    }
    T put(T value) {
        list.set(pos, value);
        return value;
    }
    List<T> pointerStorage() {
        return list;
    }
    Object pointerIdentity() {
        return storage;
    }
    JppIter<T> plus(int offset) {
        return new JppIter<T>(list, pos + offset * step, step, storage);
    }
    JppIter<T> advance(int offset) {
        pos += offset * step;
        return this;
    }
    JppIter<T> post(int offset) {
        JppIter<T> old = plus(0);
        advance(offset);
        return old;
    }
    int minus(JppIter<?> other) {
        return (pos - other.pos) * step;
    }
    boolean same(JppIter<?> other) {
        return pos == other.pos;
    }
}

static class JppAddress {
    final Object object;
    final Object field;
    JppAddress(Object object, Object field) {
        this.object = object;
        this.field = field;
    }
    public boolean equals(Object other) {
        if (!(other instanceof JppAddress))
            return false;
        JppAddress address = (JppAddress)other;
        return object == address.object && Objects.equals(field, address.field);
    }
    public int hashCode() {
        return 31 * System.identityHashCode(object) + Objects.hashCode(field);
    }
}

// A typed view of existing storage. Collections remain Java's standard collections.
static class JppPtr<T> extends JppIter<T> {
    JppMemoryType elementType;
    boolean littleEndian;
    JppMemory memory;
    JppPtr(List<T> values, int index, Object identity) {
        super(values, index, 1, identity);
    }
    JppPtr<T> plus(int offset) {
        JppPtr<T> result = new JppPtr<T>(list, Math.addExact(pos, offset), storage);
        result.elementType = elementType;
        result.littleEndian = littleEndian;
        result.memory = memory;
        return result;
    }
    boolean same(JppIter<?> other) {
        if (other == null)
            return false;
        if (elementType != null && other instanceof JppPtr &&
            ((JppPtr<?>)other).elementType != null)
            return Jpp.sameMemoryAddress(this, other);
        if (pos == other.pos && (storage == other.storage ||
                                 (storage instanceof JppAddress && storage.equals(other.storage))))
            return true;
        // A returned object reference and a stored element can name the same C++ object.
        return list != null && other.list != null && pos >= 0 && pos < list.size() &&
            other.pos >= 0 && other.pos < other.list.size() &&
            (get() instanceof JppCopyable || get() instanceof Collection) &&
            get() == other.get();
    }
}

// Integer pointer values can be retained as sentinels without inventing an allocation.
static class JppIntegerPtr<T> extends JppPtr<T> {
    final BigInteger address;
    JppIntegerPtr(BigInteger address) {
        super(null, 0, address);
        this.address = address;
    }
    JppIntegerPtr<T> plus(int offset) {
        if (offset != 0)
            throw new UnsupportedOperationException("arithmetic on an integer-derived C++ address");
        return new JppIntegerPtr<T>(address);
    }
    boolean same(JppIter<?> other) {
        return other instanceof JppIntegerPtr && address.equals(((JppIntegerPtr<?>)other).address);
    }
    T get() {
        throw new UnsupportedOperationException("dereferencing an integer-derived C++ address");
    }
    T put(T value) {
        throw new UnsupportedOperationException("dereferencing an integer-derived C++ address");
    }
}

static abstract class JppRecord {
    JppRecordPosition jppPosition;
    abstract JppMemoryType jppLayout();
}

static class JppRecordPosition {
    final JppMemory memory;
    final long offset;
    JppRecordPosition(JppMemory memory, long offset) {
        this.memory = memory;
        this.offset = offset;
    }
}

static class JppMemoryField<O extends JppRecord, T> {
    final String name;
    final long offset;
    final JppMemoryType type;
    final java.util.function.Function<O, T> getter;
    final java.util.function.BiConsumer<O, T> setter;
    JppMemoryField(String name, long offset, JppMemoryType type,
                   java.util.function.Function<O, T> getter,
                   java.util.function.BiConsumer<O, T> setter) {
        this.name = name;
        this.offset = offset;
        this.type = type;
        this.getter = getter;
        this.setter = setter;
    }
    @SuppressWarnings("unchecked")
    Object get(JppRecord object) {
        return getter.apply((O)object);
    }
    @SuppressWarnings("unchecked")
    void set(JppRecord object, Object value) {
        setter.accept((O)object, (T)value);
    }
}

// Clang supplies the C++ width and signedness: Java boxes alone cannot distinguish
// a C++ short from an int, or a narrow character from char16_t.
static class JppMemoryType {
    final int bytes;
    final long size;
    final char kind;
    final boolean signed;
    final JppMemoryType element;
    final int count;
    final JppMemoryField<?, ?>[] fields;
    final java.util.function.Supplier<? extends JppRecord> factory;
    final boolean recordLittleEndian;
    private JppMemoryType(int bytes, char kind, boolean signed) {
        this.bytes = bytes;
        size = bytes;
        this.kind = kind;
        this.signed = signed;
        element = null;
        count = 0;
        fields = null;
        factory = null;
        recordLittleEndian = false;
    }
    private JppMemoryType(JppMemoryType element, int count) {
        bytes = 0;
        size = Math.multiplyExact(element.size, count);
        kind = 'a';
        signed = false;
        this.element = element;
        this.count = count;
        fields = null;
        factory = null;
        recordLittleEndian = false;
    }
    private JppMemoryType(long size, boolean littleEndian,
                           java.util.function.Supplier<? extends JppRecord> factory,
                           JppMemoryField<?, ?>[] fields) {
        bytes = 0;
        this.size = size;
        kind = 'r';
        signed = false;
        element = null;
        count = 0;
        this.fields = fields;
        this.factory = factory;
        recordLittleEndian = littleEndian;
    }
    static JppMemoryType record(long size, boolean littleEndian,
                                java.util.function.Supplier<? extends JppRecord> factory,
                                JppMemoryField<?, ?>... fields) {
        return new JppMemoryType(size, littleEndian, factory, fields);
    }
    static JppMemoryType array(JppMemoryType element, int count) {
        return new JppMemoryType(element, count);
    }
    JppMemoryType scalar() {
        return element == null ? this : element.scalar();
    }
    boolean sameType(JppMemoryType other) {
        return this == other || (element != null && other.element != null && count == other.count &&
                                  element.sameType(other.element));
    }
    static final JppMemoryType I8 = new JppMemoryType(1, 'i', true);
    static final JppMemoryType U8 = new JppMemoryType(1, 'i', false);
    static final JppMemoryType I16 = new JppMemoryType(2, 'i', true);
    static final JppMemoryType U16 = new JppMemoryType(2, 'i', false);
    static final JppMemoryType I32 = new JppMemoryType(4, 'i', true);
    static final JppMemoryType U32 = new JppMemoryType(4, 'i', false);
    static final JppMemoryType I64 = new JppMemoryType(8, 'i', true);
    static final JppMemoryType U64 = new JppMemoryType(8, 'i', false);
    static final JppMemoryType I128 = new JppMemoryType(16, 'n', true);
    static final JppMemoryType U128 = new JppMemoryType(16, 'n', false);
    static final JppMemoryType C8 = new JppMemoryType(1, 'c', false);
    static final JppMemoryType C16 = new JppMemoryType(2, 'c', false);
    static final JppMemoryType BOOL = new JppMemoryType(1, 'b', false);
    static final JppMemoryType F32 = new JppMemoryType(4, 'f', false);
    static final JppMemoryType F64 = new JppMemoryType(8, 'd', false);
    long bits(Object value) {
        switch (kind) {
        case 'c':
            return (Character)value;
        case 'b':
            return (Boolean)value ? 1L : 0L;
        case 'f':
            return Integer.toUnsignedLong(Float.floatToRawIntBits((Float)value));
        case 'd':
            return Double.doubleToRawLongBits((Double)value);
        default:
            return ((Number)value).longValue();
        }
    }
    Object value(long bits) {
        switch (kind) {
        case 'c':
            return Character.valueOf((char)bits);
        case 'b':
            if (bits != 0 && bits != 1)
                throw new UnsupportedOperationException("noncanonical bool object representation");
            return Boolean.valueOf(bits != 0);
        case 'f':
            return Float.valueOf(Float.intBitsToFloat((int)bits));
        case 'd':
            return Double.valueOf(Double.longBitsToDouble(bits));
        default:
            if (bytes == 8)
                return Long.valueOf(bits);
            if (bytes < 4)
                bits = signed ? (bits << (64 - bytes * 8)) >> (64 - bytes * 8)
                              : bits & ((1L << (bytes * 8)) - 1);
            return Integer.valueOf((int)bits);
        }
    }
    int byteAt(Object value, int index, boolean littleEndian) {
        int shift = 8 * (littleEndian ? index : bytes - index - 1);
        return kind == 'n' ? ((BigInteger)value).shiftRight(shift).intValue() & 255
                           : (int)(bits(value) >>> shift) & 255;
    }
    Object decode(byte[] source, int offset, boolean littleEndian) {
        if (kind == 'n') {
            byte[] magnitude = new byte[bytes];
            for (int i = 0; i < bytes; ++i)
                magnitude[bytes - i - 1] = source[offset + (littleEndian ? i : bytes - i - 1)];
            return signed ? new BigInteger(magnitude) : new BigInteger(1, magnitude);
        }
        long bits = 0;
        for (int i = 0; i < bytes; ++i)
            bits |= (long)(source[offset + i] & 255) << (8 * (littleEndian ? i : bytes - i - 1));
        return value(bits);
    }
}

// Fields stay ordinary Java fields. A view overlays their current values on the
// allocation's bytes and updates them when a byte operation writes into a field.
static class JppRecordView {
    final JppRecord object;
    final JppMemoryType type;
    final long offset;
    final boolean littleEndian;
    final byte[] padding;
    JppRecordView(JppRecord object, JppMemoryType type, long offset, boolean littleEndian,
                   boolean allocated) {
        this.object = object;
        this.type = type;
        this.offset = offset;
        this.littleEndian = littleEndian;
        padding = allocated ? null : new byte[Math.toIntExact(type.size)];
    }
    int getByte(long relative, byte[] allocation) {
        for (JppMemoryField<?, ?> field : type.fields)
            if (relative >= field.offset && relative - field.offset < field.type.size)
                return field.type.byteAt(field.get(object), (int)(relative - field.offset),
                                          littleEndian);
        return (padding == null ? allocation[Math.toIntExact(offset + relative)]
                                 : padding[Math.toIntExact(relative)]) & 255;
    }
    void overlay(long start, byte[] result, int index, int count) {
        for (JppMemoryField<?, ?> field : type.fields) {
            long first = Math.max(start, offset + field.offset);
            long last = Math.min(start + count, offset + field.offset + field.type.size);
            if (first >= last)
                continue;
            Object value = field.get(object);
            for (long at = first; at < last; ++at)
                result[index + (int)(at - start)] = (byte)field.type.byteAt(
                    value, (int)(at - offset - field.offset), littleEndian);
        }
    }
    void write(long start, byte[] source, int index, int count) {
        if (padding != null)
            System.arraycopy(source, index, padding, Math.toIntExact(start - offset), count);
        for (JppMemoryField<?, ?> field : type.fields) {
            long first = Math.max(start, offset + field.offset);
            long last = Math.min(start + count, offset + field.offset + field.type.size);
            if (first >= last)
                continue;
            byte[] bytes = new byte[field.type.bytes];
            Object previous = field.get(object);
            for (int i = 0; i < bytes.length; ++i)
                bytes[i] = (byte)field.type.byteAt(previous, i, littleEndian);
            System.arraycopy(source, index + (int)(first - start), bytes,
                               (int)(first - offset - field.offset), (int)(last - first));
            field.set(object, field.type.decode(bytes, 0, littleEndian));
            Object stored = field.get(object);
            for (int i = 0; i < bytes.length; ++i)
                if (field.type.byteAt(stored, i, littleEndian) != (bytes[i] & 255))
                    throw new UnsupportedOperationException(
                        "JVM changed a floating object representation");
        }
    }
}

// A byte view retains the real Java array/list/reference cell. Changes through
// this view and ordinary generated Java accesses therefore see the same storage.
static class JppMemory {
    final Object identity;
    final List<Object> values;
    final Object array;
    final JppMemoryType arrayType;
    final JppMemoryType elementType;
    final boolean littleEndian;
    byte[] allocation;
    boolean freed;
    TreeMap<Long, JppRecordView> records;
    @SuppressWarnings("unchecked")
    JppMemory(JppPtr<?> pointer) {
        identity = pointer.storage;
        if (pointer.elementType.element == null) {
            values = (List<Object>)(List<?>)pointer.list;
            array = null;
            arrayType = null;
        } else {
            values = null;
            array = pointer.list;
            arrayType = JppMemoryType.array(pointer.elementType, pointer.list.size());
        }
        elementType = pointer.elementType.scalar();
        littleEndian = pointer.littleEndian;
        if (elementType.kind == 'r' && values != null)
            for (int i = 0; i < values.size(); ++i) {
                JppRecord object = (JppRecord)values.get(i);
                JppRecordPosition previous = object.jppPosition;
                long offset = (long)i * elementType.size;
                object.jppPosition = new JppRecordPosition(this, offset);
                if (previous != null && previous.memory.records != null) {
                    JppRecordView old = previous.memory.records.get(previous.offset);
                    if (old != null && old.padding != null) {
                        if (records == null)
                            records = new TreeMap<Long, JppRecordView>();
                        JppRecordView view = new JppRecordView(object, elementType, offset,
                                                                littleEndian, false);
                        System.arraycopy(old.padding, 0, view.padding, 0, view.padding.length);
                        records.put(offset, view);
                    }
                }
            }
    }
    JppMemory(byte[] bytes, boolean littleEndian) {
        // Saved pointer values must not retain an allocation after free.
        identity = new Object();
        allocation = bytes;
        values = null;
        array = null;
        arrayType = null;
        elementType = JppMemoryType.U8;
        this.littleEndian = littleEndian;
    }
    JppMemory(Object array, JppMemoryType type, boolean littleEndian) {
        identity = array;
        values = null;
        this.array = array;
        arrayType = type;
        elementType = type.scalar();
        this.littleEndian = littleEndian;
    }
    boolean allocated() {
        return values == null && array == null;
    }
    Object arrayElement(Object selected, int index) {
        return selected instanceof List ? ((List<?>)selected).get(index)
                                        : java.lang.reflect.Array.get(selected, index);
    }
    long size() {
        if (freed)
            throw new IllegalStateException("access to freed C++ allocation");
        if (array != null)
            return arrayType.size;
        if (allocated())
            return allocation.length;
        // A string's terminating NUL is addressable, but is not part of size().
        return ((long)values.size() + (identity instanceof JppString ? 1 : 0)) * elementType.size;
    }
    JppRecordView recordView(long offset, JppMemoryType type) {
        check(offset, type.size);
        if (records == null)
            records = new TreeMap<Long, JppRecordView>();
        JppRecordView existing = records.get(offset);
        if (existing != null) {
            if (existing.type != type)
                throw new UnsupportedOperationException("overlapping C++ record types");
            return existing;
        }
        Map.Entry<Long, JppRecordView> previous = records.floorEntry(offset);
        Map.Entry<Long, JppRecordView> next = records.ceilingEntry(offset);
        if ((previous != null && previous.getKey() + previous.getValue().type.size > offset) ||
            (next != null && next.getKey() < offset + type.size))
            throw new UnsupportedOperationException("overlapping C++ record layouts");
        JppRecord object;
        if (allocated()) {
            object = type.factory.get();
            for (JppMemoryField<?, ?> field : type.fields)
                field.set(object, field.type.decode(allocation,
                    Math.toIntExact(offset + field.offset), littleEndian));
        } else {
            if (elementType != type || offset % type.size != 0)
                throw new UnsupportedOperationException("this C++ storage has a different record layout");
            object = (JppRecord)scalarValue(offset / type.size);
        }
        JppRecordView view = new JppRecordView(object, type, offset, littleEndian, allocated());
        records.put(offset, view);
        if (object.jppPosition == null || allocated() || values == null || values.size() != 1)
            object.jppPosition = new JppRecordPosition(this, offset);
        return view;
    }
    Map.Entry<Long, JppRecordView> firstRecord(long offset) {
        if (records == null)
            return null;
        Map.Entry<Long, JppRecordView> entry = records.floorEntry(offset);
        if (entry == null || entry.getKey() + entry.getValue().type.size <= offset)
            entry = records.ceilingEntry(offset);
        return entry;
    }
    void bindRecord(long offset) {
        if (!allocated() && elementType.kind == 'r' && offset >= 0 && offset < size())
            recordView(offset - offset % elementType.size, elementType);
    }
    Object scalarValue(long index) {
        if (array == null)
            return values.get(Math.toIntExact(index));
        Object selected = array;
        JppMemoryType type = arrayType;
        long offset = index * elementType.bytes;
        while (type.element.element != null) {
            selected = arrayElement(selected, (int)(offset / type.element.size));
            offset %= type.element.size;
            type = type.element;
        }
        return java.lang.reflect.Array.get(selected, (int)(offset / elementType.bytes));
    }
    void scalarValue(long index, Object value) {
        if (array == null) {
            values.set(Math.toIntExact(index), value);
            return;
        }
        Object selected = array;
        JppMemoryType type = arrayType;
        long offset = index * elementType.bytes;
        while (type.element.element != null) {
            selected = arrayElement(selected, (int)(offset / type.element.size));
            offset %= type.element.size;
            type = type.element;
        }
        java.lang.reflect.Array.set(selected, (int)(offset / elementType.bytes), value);
    }
    Object arrayValue(long offset, JppMemoryType wanted) {
        check(offset, wanted.size);
        Object selected = array;
        JppMemoryType type = arrayType;
        while (selected != null && type.element != null) {
            if (offset == 0 && type.sameType(wanted))
                return selected;
            if (type.element.size == 0)
                break;
            selected = arrayElement(selected, (int)(offset / type.element.size));
            offset %= type.element.size;
            type = type.element;
        }
        throw new UnsupportedOperationException("this array lvalue requires its address view");
    }
    void check(long offset, long count) {
        long size = size();
        if (offset < 0 || count < 0 || offset > size || count > size - offset)
            throw new IndexOutOfBoundsException("C++ memory range");
    }
    int getByte(long offset) {
        check(offset, 1);
        bindRecord(offset);
        Map.Entry<Long, JppRecordView> entry = firstRecord(offset);
        if (entry != null && entry.getKey() <= offset)
            return entry.getValue().getByte(offset - entry.getKey(), allocation);
        if (allocated())
            return allocation[(int)offset] & 255;
        return elementType.byteAt(scalarValue(offset / elementType.bytes),
                                  (int)(offset % elementType.bytes), littleEndian);
    }
    void read(long offset, byte[] result, int start, int count) {
        check(offset, count);
        if (allocated()) {
            System.arraycopy(allocation, (int)offset, result, start, count);
            for (Map.Entry<Long, JppRecordView> entry = firstRecord(offset);
                 entry != null && entry.getKey() < offset + count;
                 entry = records.higherEntry(entry.getKey()))
                entry.getValue().overlay(offset, result, start, count);
            return;
        }
        for (int i = 0; i < count; ++i)
            result[start + i] = (byte)getByte(offset + i);
    }
    void write(long offset, byte[] source, int start, int count) {
        check(offset, count);
        if (allocated()) {
            System.arraycopy(source, start, allocation, (int)offset, count);
            for (Map.Entry<Long, JppRecordView> entry = firstRecord(offset);
                 entry != null && entry.getKey() < offset + count;
                 entry = records.higherEntry(entry.getKey())) {
                JppRecordView view = entry.getValue();
                long first = Math.max(offset, view.offset);
                long last = Math.min(offset + count, view.offset + view.type.size);
                view.write(first, source, start + (int)(first - offset), (int)(last - first));
            }
            return;
        }
        if (elementType.kind == 'r') {
            while (count > 0) {
                long origin = offset - offset % elementType.size;
                JppRecordView view = recordView(origin, elementType);
                int part = (int)Math.min(count, origin + elementType.size - offset);
                view.write(offset, source, start, part);
                offset += part;
                start += part;
                count -= part;
            }
            return;
        }
        int width = elementType.bytes;
        byte[] representation = new byte[width];
        while (count > 0) {
            long index = offset / width;
            int first = (int)(offset % width);
            int part = Math.min(count, width - first);
            Object previous = scalarValue(index);
            for (int i = 0; i < width; ++i)
                representation[i] = (byte)elementType.byteAt(previous, i, littleEndian);
            System.arraycopy(source, start, representation, first, part);
            scalarValue(index, elementType.decode(representation, 0, littleEndian));
            // A JVM may quiet a signaling NaN when it materializes a floating
            // value. Reject that boundary explicitly instead of losing bytes.
            Object stored = scalarValue(index);
            for (int i = 0; i < width; ++i)
                if (elementType.byteAt(stored, i, littleEndian) != (representation[i] & 255))
                    throw new UnsupportedOperationException(
                        "JVM changed a floating object representation");
            offset += part;
            start += part;
            count -= part;
        }
    }
    Object load(long offset, JppMemoryType type) {
        if (type.kind == 'r')
            return recordView(offset, type).object;
        if (type.element != null)
            return arrayValue(offset, type);
        check(offset, type.bytes);
        if (!allocated() && type == elementType && offset % type.bytes == 0)
            return scalarValue(offset / type.bytes);
        byte[] representation = new byte[type.bytes];
        read(offset, representation, 0, representation.length);
        return type.decode(representation, 0, littleEndian);
    }
    void store(long offset, JppMemoryType type, Object value) {
        if (type.kind == 'r') {
            JppRecord target = recordView(offset, type).object;
            for (JppMemoryField<?, ?> field : type.fields)
                field.set(target, field.get((JppRecord)value));
            return;
        }
        if (type.element != null)
            throw new IllegalArgumentException("C++ arrays cannot be assigned as scalar values");
        byte[] representation = new byte[type.bytes];
        for (int i = 0; i < representation.length; ++i)
            representation[i] = (byte)type.byteAt(value, i, littleEndian);
        write(offset, representation, 0, representation.length);
    }
}

static class JppMemoryPtr<T> extends JppPtr<T> {
    final long origin;
    @SuppressWarnings("unchecked")
    JppMemoryPtr(JppMemory memory, long offset, JppMemoryType type) {
        super(new AbstractList<T>() {
            final long unit = Math.max(1L, type.size);
            final long remainder = offset % unit;
            public int size() {
                return (int)Math.min(Integer.MAX_VALUE, (memory.size() - remainder) / unit);
            }
            public T get(int i) {
                return (T)memory.load(remainder + (long)i * type.size, type);
            }
            public T set(int i, T value) {
                T old = get(i);
                memory.store(remainder + (long)i * type.size, type, value);
                return old;
            }
        }, Math.toIntExact(offset / Math.max(1L, type.size)), memory.identity);
        this.memory = memory;
        elementType = type;
        littleEndian = memory.littleEndian;
        origin = offset % Math.max(1L, type.size);
    }
    long byteOffset() {
        return origin + (long)pos * elementType.size;
    }
    JppPtr<T> plus(int offset) {
        return new JppMemoryPtr<T>(memory, byteOffset() + (long)offset * elementType.size,
                                   elementType);
    }
    int minus(JppIter<?> other) {
        return Math.toIntExact(Jpp.pointerDifference(this, other));
    }
    boolean same(JppIter<?> other) {
        return other != null && Jpp.sameMemoryAddress(this, other);
    }
}

// TreeMap supplies the ordering and logarithmic lookup; the cursor adds C++
// bidirectional-iterator behavior, including repeated values in a multiset.
static class JppBackInserter<T> extends JppIter<T> {
    final Collection<T> target;
    JppBackInserter(Collection<T> target) {
        super(Collections.<T>emptyList(), 0, 1);
        this.target = target;
    }
    T put(T value) {
        target.add(value);
        return value;
    }
    JppIter<T> plus(int offset) {
        return new JppBackInserter<T>(target);
    }
    JppIter<T> advance(int offset) {
        return this;
    }
}

static class JppTreeIter<T> extends JppIter<T> {
    final NavigableMap<T, Integer> tree;
    T key;
    int duplicate;
    JppTreeIter(NavigableMap<T, Integer> values, T at, int direction) {
        super(null, 0, direction);
        tree = values;
        key = at;
    }
    T get() {
        if (key == null)
            throw new NoSuchElementException();
        return key;
    }
    List<T> pointerStorage() {
        return Collections.singletonList(get());
    }
    Object pointerIdentity() {
        return new JppAddress(tree, key);
    }
    JppIter<T> plus(int offset) {
        JppTreeIter<T> p = new JppTreeIter<T>(tree, key, step);
        p.duplicate = duplicate;
        return p.advance(offset);
    }
    JppIter<T> advance(int offset) {
        int direction = offset < 0 ? -step : step;
        for (long n = Math.abs((long)offset); n > 0; --n) {
            if (key == null) {
                if (tree.isEmpty())
                    throw new NoSuchElementException();
                key = direction < 0 ? tree.lastKey() : tree.firstKey();
                duplicate = direction < 0 ? tree.get(key) - 1 : 0;
            } else if (direction > 0) {
                if (++duplicate >= tree.get(key)) {
                    key = tree.higherKey(key);
                    duplicate = 0;
                }
            } else if (duplicate > 0)
                --duplicate;
            else {
                key = tree.lowerKey(key);
                duplicate = key == null ? 0 : tree.get(key) - 1;
            }
        }
        return this;
    }
    boolean same(JppIter<?> other) {
        return other instanceof JppTreeIter && tree == ((JppTreeIter<?>)other).tree &&
            Objects.equals(key, ((JppTreeIter<?>)other).key) &&
            duplicate == ((JppTreeIter<?>)other).duplicate;
    }
    private int rank() {
        int index = 0;
        for (Map.Entry<T, Integer> entry : tree.entrySet()) {
            if (key != null && Objects.equals(key, entry.getKey()))
                return index + duplicate;
            index += entry.getValue();
        }
        if (key == null)
            return step > 0 ? index : -1;
        throw new NoSuchElementException();
    }
    int minus(JppIter<?> other) {
        return (rank() - ((JppTreeIter<?>)other).rank()) * step;
    }
}

static class JppMapOwner<K, V> {
    JppMap<K, V> map;
    JppMapOwner(JppMap<K, V> value) {
        map = value;
    }
}

static class JppMapEntry<K, V> extends JppPair<K, V> {
    JppMapOwner<K, V> owner;
    JppMapEntry(JppMap<K, V> map, K key, V value) {
        super(key, value);
        owner = map.ownership;
    }
}

static class JppMapIter<K, V> extends JppIter<JppPair<K, V>> {
    JppMap<K, V> map;
    JppMapEntry<K, V> current;
    List<JppMapEntry<K, V>> unordered;
    int unorderedIndex;
    JppMapIter(JppMap<K, V> source, JppMapEntry<K, V> entry, int direction) {
        super(null, 0, direction, source);
        map = source;
        current = entry;
    }
    JppPair<K, V> get() {
        if (current == null)
            throw new NoSuchElementException();
        return current;
    }
    List<JppPair<K, V>> pointerStorage() {
        return Collections.singletonList(get());
    }
    Object pointerIdentity() {
        return new JppAddress(current, "");
    }
    JppIter<JppPair<K, V>> plus(int offset) {
        JppMapIter<K, V> copy = new JppMapIter<K, V>(
            current == null ? map : current.owner.map, current, step);
        copy.unordered = unordered;
        copy.unorderedIndex = unorderedIndex;
        return copy.advance(offset);
    }
    JppIter<JppPair<K, V>> advance(int offset) {
        int direction = offset < 0 ? -step : step;
        for (long n = Math.abs((long)offset); n > 0; --n) {
            if (current != null)
                map = current.owner.map;
            if (map.data instanceof NavigableMap) {
                NavigableMap<K, JppMapEntry<K, V>> tree =
                    (NavigableMap<K, JppMapEntry<K, V>>)map.data;
                Map.Entry<K, JppMapEntry<K, V>> next =
                    current == null ? (direction < 0 ? tree.lastEntry() : tree.firstEntry())
                    : direction < 0 ? tree.lowerEntry(current.first)
                                    : tree.higherEntry(current.first);
                current = next == null ? null : next.getValue();
            } else {
                if (unordered == null) {
                    unordered = new ArrayList<JppMapEntry<K, V>>(map.data.values());
                    unorderedIndex = current == null ? (direction < 0 ? unordered.size() : -1)
                                                     : unordered.indexOf(current);
                }
                unorderedIndex += direction;
                current = unorderedIndex < 0 || unorderedIndex >= unordered.size()
                              ? null : unordered.get(unorderedIndex);
            }
        }
        return this;
    }
    boolean same(JppIter<?> other) {
        if (!(other instanceof JppMapIter))
            return false;
        JppMapIter<?, ?> right = (JppMapIter<?, ?>)other;
        return current == right.current && (current != null || map == right.map);
    }
    private int rank() {
        if (current == null)
            return step > 0 ? map.size() : -1;
        map = current.owner.map;
        int index = 0;
        for (JppMapEntry<K, V> entry : map.data.values()) {
            if (entry == current)
                return index;
            ++index;
        }
        throw new NoSuchElementException();
    }
    int minus(JppIter<?> other) {
        return (rank() - ((JppMapIter<?, ?>)other).rank()) * step;
    }
}

static class JppMap<K, V> implements Iterable<JppPair<K, V>>, JppAssignable<JppMap<K, V>> {
    Map<K, JppMapEntry<K, V>> data;
    JppMapOwner<K, V> ownership = new JppMapOwner<K, V>(this);
    final java.util.function.Supplier<V> factory;
    Comparator<? super K> comparator;
    JppMap(boolean ordered, java.util.function.Supplier<V> init) {
        this(ordered, init, Jpp::compare);
    }
    JppMap(boolean ordered, java.util.function.Supplier<V> init, Comparator<? super K> comparison) {
        comparator = comparison;
        data = ordered ? new TreeMap<K, JppMapEntry<K, V>>(comparison)
                       : new HashMap<K, JppMapEntry<K, V>>();
        factory = init;
    }
    V get(K key) {
        JppMapEntry<K, V> node = data.get(key);
        if (node == null) {
            K stored = jppCopy(key);
            node = new JppMapEntry<K, V>(this, stored, factory.get());
            data.put(stored, node);
        }
        return node.second;
    }
    V at(K key) {
        if (!data.containsKey(key))
            throw new NoSuchElementException();
        return data.get(key).second;
    }
    V put(K key, V value) {
        JppMapEntry<K, V> node = data.get(key);
        if (node == null) {
            K stored = jppCopy(key);
            data.put(stored, new JppMapEntry<K, V>(this, stored, value));
        } else
            node.second = value;
        return value;
    }
    JppMapEntry<K, V> entry(K key) {
        get(key);
        return data.get(key);
    }
    JppPair<JppIter<JppPair<K, V>>, Boolean> emplace(K key, V value) {
        boolean inserted = !data.containsKey(key);
        if (inserted)
            put(key, value);
        return new JppPair<JppIter<JppPair<K, V>>, Boolean>(find(key), inserted);
    }
    JppPair<JppIter<JppPair<K, V>>, Boolean> insert(JppPair<K, V> value) {
        if (data.containsKey(value.first))
            return new JppPair<JppIter<JppPair<K, V>>, Boolean>(find(value.first), false);
        return emplace(value.first, jppCopy(value.second));
    }
    void merge(JppMap<K, V> source) {
        if (source == this)
            return;
        Iterator<Map.Entry<K, JppMapEntry<K, V>>> entries = source.data.entrySet().iterator();
        while (entries.hasNext()) {
            Map.Entry<K, JppMapEntry<K, V>> entry = entries.next();
            if (!data.containsKey(entry.getKey())) {
                data.put(entry.getKey(), entry.getValue());
                entry.getValue().owner = ownership;
                entries.remove();
            }
        }
    }
    int count(K key) {
        return data.containsKey(key) ? 1 : 0;
    }
    boolean contains(K key) {
        return data.containsKey(key);
    }
    int erase(K key) {
        return data.remove(key) == null ? 0 : 1;
    }
    JppIter<JppPair<K, V>> eraseIterator(JppIter<JppPair<K, V>> position) {
        JppIter<JppPair<K, V>> next = position.plus(1);
        erase(position.get().first);
        return next;
    }
    JppIter<JppPair<K, V>> eraseRange(JppIter<JppPair<K, V>> first, JppIter<JppPair<K, V>> last) {
        JppIter<JppPair<K, V>> current = first.plus(0);
        while (!current.same(last))
            current = eraseIterator(current);
        return last.plus(0);
    }
    int size() {
        return data.size();
    }
    boolean empty() {
        return data.isEmpty();
    }
    void clear() {
        data.clear();
    }
    void swap(JppMap<K, V> other) {
        if (other == this)
            return;
        Map<K, JppMapEntry<K, V>> savedData = data;
        data = other.data;
        other.data = savedData;
        Comparator<? super K> savedComparator = comparator;
        comparator = other.comparator;
        other.comparator = savedComparator;
        JppMapOwner<K, V> savedOwner = ownership;
        ownership = other.ownership;
        other.ownership = savedOwner;
        ownership.map = this;
        other.ownership.map = other;
    }
    ArrayList<JppPair<K, V>> entries() {
        ArrayList<JppPair<K, V>> result = new ArrayList<JppPair<K, V>>();
        result.addAll(data.values());
        return result;
    }
    public Iterator<JppPair<K, V>> iterator() {
        return new Iterator<JppPair<K, V>>() {
            JppIter<JppPair<K, V>> position = begin();
            boolean pending;
            private void prepare() {
                if (pending) {
                    position.advance(1);
                    pending = false;
                }
            }
            public boolean hasNext() {
                prepare();
                return !position.same(end());
            }
            public JppPair<K, V> next() {
                prepare();
                JppPair<K, V> value = position.get();
                pending = true;
                return value;
            }
        };
    }
    JppIter<JppPair<K, V>> begin() {
        return new JppMapIter<K, V>(this, data.isEmpty() ? null : data.values().iterator().next(), 1);
    }
    JppIter<JppPair<K, V>> end() {
        return new JppMapIter<K, V>(this, null, 1);
    }
    JppIter<JppPair<K, V>> find(K key) {
        return new JppMapIter<K, V>(this, data.get(key), 1);
    }
    JppIter<JppPair<K, V>> lower_bound(K key) {
        Map.Entry<K, JppMapEntry<K, V>> at =
            ((NavigableMap<K, JppMapEntry<K, V>>)data).ceilingEntry(key);
        return new JppMapIter<K, V>(this, at == null ? null : at.getValue(), 1);
    }
    JppIter<JppPair<K, V>> upper_bound(K key) {
        Map.Entry<K, JppMapEntry<K, V>> at =
            ((NavigableMap<K, JppMapEntry<K, V>>)data).higherEntry(key);
        return new JppMapIter<K, V>(this, at == null ? null : at.getValue(), 1);
    }
    JppPair<JppIter<JppPair<K, V>>, JppIter<JppPair<K, V>>> equal_range(K key) {
        return new JppPair<JppIter<JppPair<K, V>>, JppIter<JppPair<K, V>>>(lower_bound(key), upper_bound(key));
    }
    JppIter<JppPair<K, V>> rbegin() {
        Map.Entry<K, JppMapEntry<K, V>> at =
            ((NavigableMap<K, JppMapEntry<K, V>>)data).lastEntry();
        return new JppMapIter<K, V>(this, at == null ? null : at.getValue(), -1);
    }
    JppIter<JppPair<K, V>> rend() {
        return new JppMapIter<K, V>(this, null, -1);
    }
    public JppMap<K, V> copy() {
        JppMap<K, V> result = new JppMap<K, V>(data instanceof TreeMap, factory, comparator);
        for (JppMapEntry<K, V> entry : data.values())
            result.put(entry.first, jppCopy(entry.second));
        return result;
    }
    public JppMap<K, V> jppAssignFrom(JppMap<K, V> source) {
        if (source != this) {
            JppMap<K, V> snapshot = source.copy();
            data = snapshot.data;
            comparator = snapshot.comparator;
            ownership = snapshot.ownership;
            ownership.map = this;
        }
        return this;
    }
}

static class JppSet<T> implements Iterable<T>, JppAssignable<JppSet<T>> {
    NavigableMap<T, Integer> data;
    final boolean multiple;
    int count;
    JppSet(boolean multi) {
        this(multi, Jpp::compare);
    }
    JppSet(boolean multi, Comparator<? super T> comparison) {
        data = new TreeMap<T, Integer>(comparison);
        multiple = multi;
    }
    JppPair<JppIter<T>, Boolean> insert(T value) {
        int old = count(value);
        if (multiple || old == 0) {
            data.put(jppCopy(value), old + 1);
            ++count;
        }
        return new JppPair<JppIter<T>, Boolean>(find(value), old == 0);
    }
    JppIter<T> insertMultiple(T value) {
        return insert(value).first;
    }
    void merge(JppSet<T> source) {
        if (source == this)
            return;
        Iterator<Map.Entry<T, Integer>> entries = source.data.entrySet().iterator();
        while (entries.hasNext()) {
            Map.Entry<T, Integer> entry = entries.next();
            int moved = multiple ? entry.getValue() : contains(entry.getKey()) ? 0 : 1;
            if (moved == 0)
                continue;
            data.put(entry.getKey(), count(entry.getKey()) + moved);
            count += moved;
            source.count -= moved;
            int remaining = entry.getValue() - moved;
            if (remaining == 0)
                entries.remove();
            else
                entry.setValue(remaining);
        }
    }
    int count(T value) {
        Integer n = data.get(value);
        return n == null ? 0 : n;
    }
    boolean contains(T value) {
        return data.containsKey(value);
    }
    int erase(T value) {
        Integer n = data.remove(value);
        if (n == null)
            return 0;
        count -= n;
        return n;
    }
    JppIter<T> erase(JppIter<T> it) {
        T value = it.get();
        int n = count(value);
        if (n == 1)
            data.remove(value);
        else
            data.put(value, n - 1);
        --count;
        return lower_bound(value);
    }
    int size() {
        return count;
    }
    boolean empty() {
        return count == 0;
    }
    void clear() {
        data.clear();
        count = 0;
    }
    ArrayList<T> values() {
        ArrayList<T> result = new ArrayList<T>();
        for (Map.Entry<T, Integer> e : data.entrySet())
            for (int n = e.getValue(); n > 0; --n)
                result.add(e.getKey());
        return result;
    }
    public Iterator<T> iterator() {
        return new Iterator<T>() {
            final Iterator<Map.Entry<T, Integer>> entries = data.entrySet().iterator();
            T key;
            int remaining;
            public boolean hasNext() {
                return remaining > 0 || entries.hasNext();
            }
            public T next() {
                if (remaining == 0) {
                    Map.Entry<T, Integer> e = entries.next();
                    key = e.getKey();
                    remaining = e.getValue();
                }
                --remaining;
                return key;
            }
        };
    }
    JppIter<T> begin() {
        return new JppTreeIter<T>(data, data.isEmpty() ? null : data.firstKey(), 1);
    }
    JppIter<T> end() {
        return new JppTreeIter<T>(data, null, 1);
    }
    JppIter<T> rbegin() {
        JppTreeIter<T> p = new JppTreeIter<T>(data, data.isEmpty() ? null : data.lastKey(), -1);
        p.duplicate = data.isEmpty() ? 0 : data.get(p.key) - 1;
        return p;
    }
    JppIter<T> rend() {
        return new JppTreeIter<T>(data, null, -1);
    }
    JppIter<T> find(T value) {
        return new JppTreeIter<T>(data, contains(value) ? data.ceilingKey(value) : null, 1);
    }
    JppIter<T> lower_bound(T value) {
        return new JppTreeIter<T>(data, data.ceilingKey(value), 1);
    }
    JppIter<T> upper_bound(T value) {
        return new JppTreeIter<T>(data, data.higherKey(value), 1);
    }
    JppPair<JppIter<T>, JppIter<T>> equal_range(T value) {
        return new JppPair<JppIter<T>, JppIter<T>>(lower_bound(value), upper_bound(value));
    }
    public JppSet<T> copy() {
        JppSet<T> s = new JppSet<T>(multiple, data.comparator());
        s.data.putAll(data);
        s.count = count;
        return s;
    }
    public JppSet<T> jppAssignFrom(JppSet<T> source) {
        if (source != this) {
            JppSet<T> snapshot = source.copy();
            data = snapshot.data;
            count = snapshot.count;
        }
        return this;
    }
}

// Java has no standard indexed ordered set. AVL nodes carry subtree sizes for
// logarithmic PBDS rank/selection, and retain their identity during rotations.
static class JppOrderNode<T> {
    final T key;
    JppOrderNode<T> left, right, parent;
    int height = 1, size = 1;
    JppOrderNode(T key, JppOrderNode<T> parent) {
        this.key = key;
        this.parent = parent;
    }
}

static class JppOrderState<T> {
    final Comparator<? super T> comparison;
    JppOrderNode<T> root;
    JppOrderState(Comparator<? super T> comparison) {
        this.comparison = comparison;
    }
    static int size(JppOrderNode<?> node) {
        return node == null ? 0 : node.size;
    }
    static int height(JppOrderNode<?> node) {
        return node == null ? 0 : node.height;
    }
    static void update(JppOrderNode<?> node) {
        node.size = 1 + size(node.left) + size(node.right);
        node.height = 1 + Math.max(height(node.left), height(node.right));
    }
    static <T> JppOrderNode<T> extreme(JppOrderNode<T> node, int direction) {
        if (node != null)
            while ((direction > 0 ? node.left : node.right) != null)
                node = direction > 0 ? node.left : node.right;
        return node;
    }
    static <T> JppOrderNode<T> next(JppOrderNode<T> node, int direction) {
        JppOrderNode<T> child = direction > 0 ? node.right : node.left;
        if (child != null)
            return extreme(child, direction);
        while (node.parent != null &&
               node == (direction > 0 ? node.parent.right : node.parent.left))
            node = node.parent;
        return node.parent;
    }
    void replace(JppOrderNode<T> old, JppOrderNode<T> replacement) {
        if (old.parent == null)
            root = replacement;
        else if (old == old.parent.left)
            old.parent.left = replacement;
        else
            old.parent.right = replacement;
        if (replacement != null)
            replacement.parent = old.parent;
    }
    JppOrderNode<T> rotateLeft(JppOrderNode<T> node) {
        JppOrderNode<T> top = node.right;
        replace(node, top);
        node.right = top.left;
        if (node.right != null)
            node.right.parent = node;
        top.left = node;
        node.parent = top;
        update(node);
        update(top);
        return top;
    }
    JppOrderNode<T> rotateRight(JppOrderNode<T> node) {
        JppOrderNode<T> top = node.left;
        replace(node, top);
        node.left = top.right;
        if (node.left != null)
            node.left.parent = node;
        top.right = node;
        node.parent = top;
        update(node);
        update(top);
        return top;
    }
    void rebalance(JppOrderNode<T> node) {
        while (node != null) {
            update(node);
            int balance = height(node.left) - height(node.right);
            if (balance > 1) {
                if (height(node.left.left) < height(node.left.right))
                    rotateLeft(node.left);
                node = rotateRight(node);
            } else if (balance < -1) {
                if (height(node.right.right) < height(node.right.left))
                    rotateRight(node.right);
                node = rotateLeft(node);
            }
            node = node.parent;
        }
    }
    JppOrderNode<T> find(T key, int bound) {
        JppOrderNode<T> node = root, candidate = null;
        while (node != null) {
            int order = comparison.compare(key, node.key);
            if (order == 0 && bound == 0)
                return node;
            if (order < 0 || (order == 0 && bound < 0)) {
                candidate = node;
                node = node.left;
            } else
                node = node.right;
        }
        return bound == 0 ? null : candidate;
    }
    void erase(JppOrderNode<T> node) {
        JppOrderNode<T> start;
        if (node.left == null || node.right == null) {
            start = node.parent;
            replace(node, node.left == null ? node.right : node.left);
        } else {
            JppOrderNode<T> successor = extreme(node.right, 1);
            if (successor.parent == node)
                start = successor;
            else {
                start = successor.parent;
                replace(successor, successor.right);
                successor.right = node.right;
                successor.right.parent = successor;
            }
            replace(node, successor);
            successor.left = node.left;
            successor.left.parent = successor;
        }
        rebalance(start);
    }
    JppOrderNode<T> copyNodes(JppOrderNode<T> node, JppOrderNode<T> parent) {
        if (node == null)
            return null;
        JppOrderNode<T> result = new JppOrderNode<T>(jppCopy(node.key), parent);
        result.left = copyNodes(node.left, result);
        result.right = copyNodes(node.right, result);
        update(result);
        return result;
    }
}

static class JppOrderIter<T> extends JppIter<T> {
    final JppOrderState<T> owner;
    JppOrderNode<T> node;
    JppOrderIter(JppOrderState<T> owner, JppOrderNode<T> node, int direction) {
        super(null, 0, direction);
        this.owner = owner;
        this.node = node;
    }
    T get() {
        if (node == null)
            throw new NoSuchElementException();
        return node.key;
    }
    List<T> pointerStorage() {
        return Collections.singletonList(get());
    }
    Object pointerIdentity() {
        return node;
    }
    JppOrderIter<T> plus(int offset) {
        return new JppOrderIter<T>(owner, node, step).advance(offset);
    }
    JppOrderIter<T> advance(int offset) {
        int direction = offset < 0 ? -step : step;
        for (long count = Math.abs((long)offset); count > 0; --count) {
            if (node == null) {
                if (offset > 0)
                    throw new NoSuchElementException();
                node = JppOrderState.extreme(owner.root, direction);
                if (node == null)
                    throw new NoSuchElementException();
            } else
                node = JppOrderState.next(node, direction);
        }
        return this;
    }
    boolean same(JppIter<?> other) {
        return other instanceof JppOrderIter && owner == ((JppOrderIter<?>)other).owner &&
            node == ((JppOrderIter<?>)other).node;
    }
    int rank() {
        if (node == null)
            return JppOrderState.size(owner.root);
        int rank = JppOrderState.size(node.left);
        for (JppOrderNode<T> at = node; at.parent != null; at = at.parent)
            if (at == at.parent.right)
                rank += 1 + JppOrderState.size(at.parent.left);
        return rank;
    }
    int minus(JppIter<?> other) {
        if (!(other instanceof JppOrderIter) || owner != ((JppOrderIter<?>)other).owner)
            throw new IllegalArgumentException("iterators from different policy trees");
        return (rank() - ((JppOrderIter<?>)other).rank()) * step;
    }
}

static class JppOrderSet<T> implements Iterable<T>, JppAssignable<JppOrderSet<T>> {
    JppOrderState<T> state;
    JppOrderSet(Comparator<? super T> comparison) {
        state = new JppOrderState<T>(comparison);
    }
    int size() {
        return JppOrderState.size(state.root);
    }
    boolean empty() {
        return state.root == null;
    }
    void clear() {
        state.root = null;
    }
    JppOrderIter<T> cursor(JppOrderNode<T> node, int direction) {
        return new JppOrderIter<T>(state, node, direction);
    }
    JppPair<JppIter<T>, Boolean> insert(T value) {
        JppOrderNode<T> parent = null, node = state.root;
        int comparison = 0;
        while (node != null) {
            parent = node;
            comparison = state.comparison.compare(value, node.key);
            if (comparison == 0)
                return new JppPair<JppIter<T>, Boolean>(cursor(node, 1), false);
            node = comparison < 0 ? node.left : node.right;
        }
        node = new JppOrderNode<T>(jppCopy(value), parent);
        if (parent == null)
            state.root = node;
        else if (comparison < 0)
            parent.left = node;
        else
            parent.right = node;
        state.rebalance(parent);
        return new JppPair<JppIter<T>, Boolean>(cursor(node, 1), true);
    }
    boolean erase(T value) {
        JppOrderNode<T> node = state.find(value, 0);
        if (node == null)
            return false;
        state.erase(node);
        return true;
    }
    JppIter<T> erase(JppIter<T> position) {
        JppOrderIter<T> iterator = (JppOrderIter<T>)position;
        if (iterator.owner != state || iterator.node == null)
            throw new IllegalArgumentException("invalid policy-tree erase iterator");
        JppOrderIter<T> next = iterator.plus(1);
        state.erase(iterator.node);
        return next;
    }
    int order_of_key(T key) {
        int rank = 0;
        for (JppOrderNode<T> node = state.root; node != null;) {
            if (state.comparison.compare(node.key, key) < 0) {
                rank += 1 + JppOrderState.size(node.left);
                node = node.right;
            } else
                node = node.left;
        }
        return rank;
    }
    JppIter<T> find_by_order(long order) {
        if (Long.compareUnsigned(order, (long)size()) >= 0)
            return end();
        int rank = (int)order;
        for (JppOrderNode<T> node = state.root; node != null;) {
            int left = JppOrderState.size(node.left);
            if (rank == left)
                return cursor(node, 1);
            if (rank < left)
                node = node.left;
            else {
                rank -= left + 1;
                node = node.right;
            }
        }
        throw new IllegalStateException("inconsistent policy-tree rank");
    }
    JppIter<T> find(T key) {
        return cursor(state.find(key, 0), 1);
    }
    JppIter<T> lower_bound(T key) {
        return cursor(state.find(key, -1), 1);
    }
    JppIter<T> upper_bound(T key) {
        return cursor(state.find(key, 1), 1);
    }
    JppIter<T> begin() {
        return cursor(JppOrderState.extreme(state.root, 1), 1);
    }
    JppIter<T> end() {
        return cursor(null, 1);
    }
    JppIter<T> rbegin() {
        return cursor(JppOrderState.extreme(state.root, -1), -1);
    }
    JppIter<T> rend() {
        return cursor(null, -1);
    }
    public Iterator<T> iterator() {
        return new Iterator<T>() {
            final JppIter<T> position = begin();
            public boolean hasNext() {
                return ((JppOrderIter<T>)position).node != null;
            }
            public T next() {
                T result = position.get();
                position.advance(1);
                return result;
            }
        };
    }
    void swap(JppOrderSet<T> other) {
        JppOrderState<T> previous = state;
        state = other.state;
        other.state = previous;
    }
    public JppOrderSet<T> copy() {
        JppOrderSet<T> result = new JppOrderSet<T>(state.comparison);
        result.state.root = state.copyNodes(state.root, null);
        return result;
    }
    public JppOrderSet<T> jppAssignFrom(JppOrderSet<T> source) {
        if (source != this)
            state = source.copy().state;
        return this;
    }
}

static class JppHeap<T> implements JppCopyable<JppHeap<T>> {
    final PriorityQueue<T> data;
    JppHeap(boolean minimum) {
        data = new PriorityQueue<T>((a, b) -> minimum ? Jpp.compare(a, b) : Jpp.compare(b, a));
    }
    JppHeap(java.util.function.BiPredicate<T, T> cmp) {
        data = new PriorityQueue<T>((a, b) -> cmp.test(a, b) ? 1 : cmp.test(b, a) ? -1 : 0);
    }
    void push(T value) {
        data.add(jppCopy(value));
    }
    void pop() {
        data.remove();
    }
    T top() {
        return data.element();
    }
    int size() {
        return data.size();
    }
    boolean empty() {
        return data.isEmpty();
    }
    private JppHeap(Comparator<? super T> cmp) {
        data = new PriorityQueue<T>(cmp);
    }
    public JppHeap<T> copy() {
        JppHeap<T> h = new JppHeap<T>(data.comparator());
        for (T v : data)
            h.data.add(jppCopy(v));
        return h;
    }
}

static class JppStreamState {
    boolean defaultStdoutTie;
    private JppOutput tied;
    JppPtr<JppOutput> tie() {
        JppOutput current = defaultStdoutTie ? jppOut : tied;
        return current == null ? null : Jpp.pointerValue(current);
    }
    JppPtr<JppOutput> tie(JppPtr<JppOutput> replacement) {
        JppPtr<JppOutput> previous = tie();
        defaultStdoutTie = false;
        tied = replacement == null ? null : replacement.get();
        return previous;
    }
    void flushTie() {
        JppOutput current = defaultStdoutTie ? jppOut : tied;
        if (current != null)
            current.flush();
    }
}

static class JppIoState {
    private static boolean synchronizedStdio = true;
    static boolean sync(boolean enabled) {
        boolean previous = synchronizedStdio;
        // libstdc++ keeps synchronization disabled after the first disabling call.
        synchronizedStdio &= enabled;
        return previous;
    }
}

static class JppInput extends JppStreamState implements AutoCloseable {
    private InputStream input;
    private final byte[] buffer = new byte[1 << 16];
    private int pos, size, saved = -2;
    boolean good = true, ended;
    JppInput() {
        this(System.in);
        defaultStdoutTie = true;
    }
    JppInput(InputStream stream) {
        input = stream;
    }
    JppInput(String path) {
        this(path, 8);
    }
    JppInput(String path, int mode) {
        if (path != null)
            open(path, mode);
    }
    void open(String path) {
        open(path, 8);
    }
    void open(String path, int mode) {
        if (input != null) {
            good = false;
            return;
        }
        try {
            input = new FileInputStream(Jpp.filePath(path));
            pos = size = 0;
            saved = -2;
            good = true;
            ended = false;
        } catch (IOException e) {
            good = false;
        }
    }
    boolean is_open() {
        return input != null;
    }
    public void close() {
        if (input == null) {
            good = false;
            return;
        }
        try {
            input.close();
        } catch (IOException e) {
            good = false;
        }
        input = null;
    }
    void clear() {
        good = true;
        ended = false;
    }
    JppInput ignore() {
        return ignore(1, -1);
    }
    JppInput ignore(long count) {
        return ignore(count, -1);
    }
    JppInput ignore(long count, int delimiter) {
        flushTie();
        if (good)
            while (count-- > 0) {
                int c = readByte();
                if (c < 0 || c == delimiter)
                    break;
            }
        return this;
    }
    private int readByte() {
        if (saved != -2) {
            int c = saved;
            saved = -2;
            return c;
        }
        if (input == null) {
            good = false;
            return -1;
        }
        if (pos >= size) {
            try {
                size = input.read(buffer);
                pos = 0;
            } catch (IOException e) {
                throw new UncheckedIOException(e);
            }
            if (size < 0) {
                ended = true;
                return -1;
            }
        }
        return buffer[pos++] & 255;
    }
    private int skip() {
        flushTie();
        int c;
        do {
            c = readByte();
        } while (c == ' ' || (c >= 9 && c <= 13));
        return c;
    }
    boolean hasNext() {
        int c = skip();
        saved = c;
        good = c >= 0;
        return good;
    }
    long nextLong() {
        return nextLong(0);
    }
    long nextLong(long previous) {
        return nextInteger(previous, 64, false);
    }
    long nextUnsignedLong(long previous) {
        return nextInteger(previous, 64, true);
    }
    private long nextInteger(long previous, int bits, boolean unsigned) {
        if (!good)
            return previous;
        int c = skip();
        if (c < 0) {
            good = false;
            return previous;
        }
        boolean neg = c == '-';
        if (neg || c == '+')
            c = readByte();
        long maximum = unsigned     ? bits == 64 ? -1L : (1L << bits) - 1
                       : bits == 64 ? Long.MAX_VALUE
                                    : (1L << (bits - 1)) - 1;
        long bound = !unsigned && neg ? maximum + 1 : maximum;
        long quotient = Long.divideUnsigned(bound, 10), remainder =
                                                            Long.remainderUnsigned(bound, 10);
        long n = 0;
        boolean digits = false, overflow = false;
        while (c >= '0' && c <= '9') {
            digits = true;
            int digit = c - '0';
            if (Long.compareUnsigned(n, quotient) > 0 || (n == quotient && digit > remainder))
                overflow = true;
            if (!overflow)
                n = n * 10 + digit;
            c = readByte();
        }
        saved = c;
        if (!digits) {
            good = false;
            return 0;
        }
        if (overflow) {
            good = false;
            return unsigned || !neg ? maximum : -bound;
        }
        long value = neg ? -n : n;
        return unsigned && bits < 64 ? value & ((1L << bits) - 1) : value;
    }
    int nextInt() {
        return nextInt(0);
    }
    int nextInt(int previous) {
        return (int)nextInteger(previous, 32, false);
    }
    int nextUnsignedInt(int previous) {
        return (int)nextInteger(previous, 32, true);
    }
    int nextShort(int previous) {
        return (int)nextInteger(previous, 16, false);
    }
    int nextUnsignedShort(int previous) {
        return (int)nextInteger(previous, 16, true);
    }
    int nextByte(int previous) {
        return (int)nextInteger(previous, 8, false);
    }
    int nextUnsignedByte(int previous) {
        return (int)nextInteger(previous, 8, true);
    }
    boolean nextBoolean(boolean previous) {
        long value = nextLong(previous ? 1 : 0);
        if (value != 0 && value != 1)
            good = false;
        return value != 0;
    }
    char nextChar() {
        return nextChar('\0');
    }
    char nextChar(char previous) {
        if (!good)
            return previous;
        int c = skip();
        if (c < 0) {
            good = false;
            return previous;
        }
        return (char)c;
    }
    JppString nextString() {
        return nextString(new JppString());
    }
    JppString nextString(JppString previous) {
        if (!good)
            return previous;
        int c = skip();
        if (c < 0) {
            good = false;
            return previous;
        }
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        while (c >= 0 && c != ' ' && (c < 9 || c > 13)) {
            bytes.write(c);
            c = readByte();
        }
        saved = c;
        String text = new String(bytes.toByteArray(), java.nio.charset.StandardCharsets.ISO_8859_1);
        return previous == null ? new JppString(text) : previous.jppSetText(text);
    }
    double nextDouble() {
        return nextDouble(0);
    }
    private String floatingToken() {
        if (!good)
            return null;
        int c = skip();
        if (c < 0) {
            good = false;
            return null;
        }
        StringBuilder text = new StringBuilder();
        if (c == '+' || c == '-') {
            text.append((char)c);
            c = readByte();
        }
        boolean point = false, exponent = false, exponentSign = false, digits = false;
        while (c >= 0) {
            if (c >= '0' && c <= '9') {
                exponentSign = false;
                digits = true;
            } else if (c == '.' && !point && !exponent)
                point = true;
            else if ((c == 'e' || c == 'E') && !exponent && digits) {
                exponent = true;
                exponentSign = true;
            } else if ((c == '+' || c == '-') && exponentSign)
                exponentSign = false;
            else
                break;
            text.append((char)c);
            c = readByte();
        }
        saved = c;
        return text.toString();
    }
    double nextDouble(double previous) {
        String token = floatingToken();
        if (token == null)
            return previous;
        try {
            double value = Double.parseDouble(token);
            if (Double.isInfinite(value)) {
                good = false;
                return Math.copySign(Double.MAX_VALUE, value);
            }
            return value;
        } catch (NumberFormatException e) {
            good = false;
            return 0;
        }
    }
    float nextFloat(float previous) {
        String token = floatingToken();
        if (token == null)
            return previous;
        try {
            float value = Float.parseFloat(token);
            if (Float.isInfinite(value)) {
                good = false;
                return Math.copySign(Float.MAX_VALUE, value);
            }
            return value;
        } catch (NumberFormatException e) {
            good = false;
            return 0;
        }
    }
    JppString line() {
        return line(new JppString(), '\n');
    }
    JppString line(JppString previous, int delimiter) {
        flushTie();
        if (!good || ended) {
            good = false;
            return previous;
        }
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        int c;
        while ((c = readByte()) >= 0 && c != delimiter)
            bytes.write(c);
        if (c < 0 && bytes.size() == 0)
            good = false;
        String text = new String(bytes.toByteArray(), java.nio.charset.StandardCharsets.ISO_8859_1);
        return previous == null ? new JppString(text) : previous.jppSetText(text);
    }
}

interface JppScanTarget {
    void accept(String text, int radix, char conversion);
}

static class JppScanner {
    static final java.util.regex.Pattern floating = java.util.regex.Pattern.compile(
        "[+-]?(?:(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?"
            + "|0x(?:[0-9a-f]+(?:\\.[0-9a-f]*)?|\\.[0-9a-f]+)(?:p[+-]?[0-9]+)?"
            + "|inf(?:inity)?|nan)",
        java.util.regex.Pattern.CASE_INSENSITIVE);
    final JppInput input;
    int consumed;
    JppScanner(JppInput source) {
        input = source;
    }
    int read() {
        int c = input.readByte();
        if (c >= 0)
            ++consumed;
        return c;
    }
    void unread(int c) {
        input.saved = c;
        if (c >= 0)
            --consumed;
    }
    static boolean space(int c) {
        return c == 32 || (c >= 9 && c <= 13);
    }
    int skip() {
        int c;
        do {
            c = read();
        } while (space(c));
        return c;
    }
    static double parseFloating(String text) {
        String lower = text.toLowerCase(Locale.ROOT);
        if (lower.equals("nan") || lower.equals("+nan") || lower.equals("-nan"))
            return lower.startsWith("-") ? Double.longBitsToDouble(0xfff8000000000000L)
                                         : Double.NaN;
        if (lower.matches("[+-]?inf(?:inity)?"))
            return lower.startsWith("-") ? Double.NEGATIVE_INFINITY : Double.POSITIVE_INFINITY;
        if (lower.indexOf("0x") >= 0 && lower.indexOf('p') < 0)
            text += "p0";
        return Double.parseDouble(text);
    }
    int scan(String format, JppScanTarget... targets) {
        int assigned = 0, argument = 0;
        for (int at = 0; at < format.length();) {
            char spec = format.charAt(at++);
            if (space(spec)) {
                int c = skip();
                unread(c);
                continue;
            }
            if (spec != '%') {
                int c = read();
                if (c != spec) {
                    unread(c);
                    return c < 0 && assigned == 0 ? -1 : assigned;
                }
                continue;
            }
            if (format.charAt(at) == '%') {
                ++at;
                int c = read();
                if (c != '%') {
                    unread(c);
                    return c < 0 && assigned == 0 ? -1 : assigned;
                }
                continue;
            }
            boolean suppressed = format.charAt(at) == '*';
            if (suppressed)
                ++at;
            int width = 0;
            while (at < format.length() && Character.isDigit(format.charAt(at)))
                width = width * 10 + format.charAt(at++) - '0';
            if (width == 0)
                width = Integer.MAX_VALUE;
            String length = "";
            while (at < format.length() && "hljztL".indexOf(format.charAt(at)) >= 0)
                length += format.charAt(at++);
            spec = format.charAt(at++);
            if (spec == 'n') {
                if (!suppressed)
                    targets[argument++].accept(Integer.toString(consumed), 10, spec);
                continue;
            }
            String set = "";
            boolean inverse = false;
            if (spec == '[') {
                inverse = format.charAt(at) == '^';
                if (inverse)
                    ++at;
                if (format.charAt(at) == ']')
                    set += format.charAt(at++);
                while (at < format.length() && format.charAt(at) != ']')
                    set += format.charAt(at++);
                ++at;
            }
            if (length.indexOf('l') >= 0 && (spec == 's' || spec == 'c' || spec == '['))
                throw new UnsupportedOperationException("wide-character scanf conversion");
            int c = spec == 'c' || spec == '[' ? read() : skip();
            if (c < 0)
                return assigned == 0 ? -1 : assigned;
            StringBuilder text = new StringBuilder();
            int radix = 10;
            if (spec == 's' || spec == 'c' || spec == '[') {
                if (spec == 'c' && width == Integer.MAX_VALUE)
                    width = 1;
                while (c >= 0 && width > 0) {
                    boolean take = spec == 'c' || (spec == 's' && !space(c));
                    if (spec == '[') {
                        boolean found = false;
                        for (int i = 0; i < set.length(); ++i) {
                            if (i + 2 < set.length() && set.charAt(i + 1) == '-') {
                                found |= c >= set.charAt(i) && c <= set.charAt(i + 2);
                                i += 2;
                            } else
                                found |= c == set.charAt(i);
                        }
                        take = found != inverse;
                    }
                    if (!take)
                        break;
                    text.append((char)c);
                    --width;
                    if (width == 0) {
                        c = -2;
                        break;
                    }
                    c = read();
                }
                if (c != -2)
                    unread(c);
            } else if ("diuoxX".indexOf(spec) >= 0) {
                radix = spec == 'o' ? 8 : spec == 'x' || spec == 'X' ? 16 : 10;
                boolean negative = c == '-';
                if (negative || c == '+') {
                    if (negative)
                        text.append('-');
                    --width;
                    c = width > 0 ? read() : -2;
                }
                if (width > 0 && c == '0' && (spec == 'i' || radix == 16)) {
                    text.append('0');
                    --width;
                    c = width > 0 ? read() : -2;
                    if (width > 0 && (c == 'x' || c == 'X')) {
                        radix = 16;
                        --width;
                        c = width > 0 ? read() : -2;
                    } else if (spec == 'i')
                        radix = 8;
                }
                while (width > 0 && c >= 0 && Character.digit((char)c, radix) >= 0) {
                    text.append((char)c);
                    --width;
                    c = width > 0 ? read() : -2;
                }
                if (c != -2)
                    unread(c);
                if (text.length() == 1 && text.charAt(0) == '-')
                    text.setLength(0);
            } else if ("aAeEfFgG".indexOf(spec) >= 0) {
                while (c >= 0 && width > 0) {
                    String candidate = text.toString() + (char)c;
                    java.util.regex.Matcher match = floating.matcher(candidate);
                    if (!match.matches() && !match.hitEnd())
                        break;
                    text.append((char)c);
                    --width;
                    c = width > 0 ? read() : -2;
                }
                if (c != -2)
                    unread(c);
                if (!floating.matcher(text).matches())
                    return assigned;
            } else
                throw new UnsupportedOperationException("scanf conversion: " + spec);
            if (text.length() == 0)
                return assigned;
            if (!suppressed) {
                targets[argument++].accept(text.toString(), radix, spec);
                ++assigned;
            }
        }
        return assigned;
    }
}

static class JppOutput extends JppStreamState implements AutoCloseable {
    private PrintWriter output;
    int precision = 6;
    char formatMode = 'g';
    long fieldWidth;
    char fillCharacter = ' ', adjustment = 'r';
    boolean boolalpha, good = true, ended, unitBuffered;
    JppOutput(OutputStream stream) {
        output = new PrintWriter(new BufferedWriter(
            new OutputStreamWriter(stream, java.nio.charset.StandardCharsets.ISO_8859_1)));
    }
    JppOutput(OutputStream stream, boolean errorStream) {
        this(stream);
        defaultStdoutTie = errorStream;
        unitBuffered = errorStream;
    }
    JppOutput(JppOutput shared) {
        output = shared.output;
    }
    JppOutput unitbuf(boolean enabled) {
        unitBuffered = enabled;
        return this;
    }
    JppOutput(String path) {
        this(path, 16);
    }
    JppOutput(String path, int mode) {
        if (path != null)
            open(path, mode);
    }
    void open(String path) {
        open(path, 16);
    }
    void open(String path, int mode) {
        if (output != null) {
            good = false;
            return;
        }
        try {
            output = new PrintWriter(new BufferedWriter(
                new OutputStreamWriter(new FileOutputStream(Jpp.filePath(path), (mode & 1) != 0),
                                       java.nio.charset.StandardCharsets.ISO_8859_1)));
            good = true;
        } catch (IOException e) {
            good = false;
        }
    }
    boolean is_open() {
        return output != null;
    }
    public void close() {
        if (output == null) {
            good = false;
            return;
        }
        output.close();
        if (output.checkError())
            good = false;
        output = null;
    }
    void clear() {
        good = true;
    }
    JppOutput print(Object value) {
        if (!good || output == null)
            return this;
        flushTie();
        String text;
        if (value instanceof Boolean)
            text = boolalpha ? value.toString() : ((Boolean)value ? "1" : "0");
        else if (value instanceof Double || value instanceof Float) {
            String special = Jpp.nonfinite(((Number)value).doubleValue(), false, false, false);
            if (special != null)
                text = special;
            else {
                double number = ((Number)value).doubleValue();
                text = (Double.doubleToRawLongBits(number) < 0 ? "-" : "") +
                    Jpp.floating(Math.abs(number), formatMode,
                                 formatMode == 'a' ? -1 : precision, false);
            }
        } else
            text = String.valueOf(value);
        long padding = fieldWidth > text.length() ? fieldWidth - text.length() : 0;
        fieldWidth = 0;
        int prefix = 0;
        if (adjustment == 'i' && value instanceof Number) {
            if (text.startsWith("-") || text.startsWith("+"))
                prefix = 1;
            else if (text.startsWith("0x") || text.startsWith("0X"))
                prefix = 2;
        }
        if (adjustment == 'l') {
            output.print(text);
            padding(padding);
        } else {
            output.print(text.substring(0, prefix));
            padding(padding);
            output.print(text.substring(prefix));
        }
        if (unitBuffered)
            output.flush();
        return this;
    }
    private void padding(long count) {
        if (count == 0)
            return;
        char[] buffer = new char[256];
        Arrays.fill(buffer, fillCharacter);
        while (count > 0) {
            int length = (int)Math.min(count, buffer.length);
            output.write(buffer, 0, length);
            count -= length;
        }
    }
    JppOutput raw(Object value) {
        if (!good || output == null)
            return this;
        flushTie();
        output.print(value);
        if (unitBuffered)
            output.flush();
        return this;
    }
    JppOutput put(char value) {
        return raw(value);
    }
    JppOutput write(JppPtr<Character> source, long count) {
        if (!good || output == null)
            return this;
        flushTie();
        if (count < 0) {
            good = false;
            return this;
        }
        int length = Math.toIntExact(count);
        for (int i = 0; i < length; ++i)
            output.print(source.plus(i).get());
        if (unitBuffered)
            output.flush();
        return this;
    }
    long width() { return fieldWidth; }
    long width(long value) {
        long previous = fieldWidth;
        fieldWidth = value;
        return previous;
    }
    char fill() { return fillCharacter; }
    char fill(char value) {
        char previous = fillCharacter;
        fillCharacter = value;
        return previous;
    }
    JppOutput fieldWidth(long value) { fieldWidth = value; return this; }
    JppOutput fieldFill(char value) { fillCharacter = value; return this; }
    JppOutput adjust(char value) { adjustment = value; return this; }
    JppOutput flush() {
        flushTie();
        if (output != null) {
            output.flush();
            if (output.checkError())
                good = false;
        }
        return this;
    }
    JppOutput endl() {
        put('\n');
        return flush();
    }
    JppOutput precision(int n) {
        precision = n;
        return this;
    }
    JppOutput fixed(boolean on) {
        formatMode = on ? 'f' : 'g';
        return this;
    }
    JppOutput scientific() {
        formatMode = 'e';
        return this;
    }
    JppOutput hexadecimal() {
        formatMode = 'a';
        return this;
    }
    JppOutput boolalpha(boolean on) {
        boolalpha = on;
        return this;
    }
    int printf(String format, Object... args) {
        if (!good || output == null)
            return -1;
        StringBuilder result = new StringBuilder();
        int argument = 0;
        for (int at = 0; at < format.length();) {
            char c = format.charAt(at++);
            if (c != '%') {
                result.append(c);
                continue;
            }
            if (format.charAt(at) == '%') {
                result.append('%');
                ++at;
                continue;
            }
            String flags = "";
            while (at < format.length() && "-+ #0".indexOf(format.charAt(at)) >= 0)
                flags += format.charAt(at++);
            int width = 0;
            if (format.charAt(at) == '*') {
                width = ((Number)args[argument++]).intValue();
                ++at;
                if (width < 0) {
                    flags += '-';
                    width = -width;
                }
            } else
                while (at < format.length() && Character.isDigit(format.charAt(at)))
                    width = width * 10 + format.charAt(at++) - '0';
            int precision = -1;
            if (format.charAt(at) == '.') {
                ++at;
                precision = 0;
                if (format.charAt(at) == '*') {
                    precision = ((Number)args[argument++]).intValue();
                    ++at;
                } else
                    while (at < format.length() && Character.isDigit(format.charAt(at)))
                        precision = precision * 10 + format.charAt(at++) - '0';
            }
            String length = "";
            while (at < format.length() && "hljztL".indexOf(format.charAt(at)) >= 0)
                length += format.charAt(at++);
            c = format.charAt(at++);
            Object value = args[argument++];
            String text;
            boolean left = flags.indexOf('-') >= 0;
            if ("diuoxX".indexOf(c) >= 0) {
                int bits = length.equals("hh")  ? 8
                           : length.equals("h") ? 16
                           : length.isEmpty()   ? 32
                                                : 64;
                long raw =
                    value instanceof Character ? (Character)value : ((Number)value).longValue();
                boolean signed = c == 'd' || c == 'i';
                BigInteger number;
                if (signed) {
                    if (bits < 64) {
                        int shift = 64 - bits;
                        raw = (raw << shift) >> shift;
                    }
                    number = BigInteger.valueOf(raw);
                } else
                    number = bits == 64 ? new BigInteger(Long.toUnsignedString(raw))
                                        : BigInteger.valueOf(raw & ((1L << bits) - 1));
                int radix = c == 'o' ? 8 : c == 'x' || c == 'X' ? 16 : 10;
                String digits = number.abs().toString(radix);
                if (c == 'X')
                    digits = digits.toUpperCase(Locale.ROOT);
                if (precision == 0 && number.signum() == 0)
                    digits = "";
                while (digits.length() < precision)
                    digits = '0' + digits;
                String prefix = number.signum() < 0                 ? "-"
                                : signed && flags.indexOf('+') >= 0 ? "+"
                                : signed && flags.indexOf(' ') >= 0 ? " "
                                                                    : "";
                if (flags.indexOf('#') >= 0) {
                    if (radix == 16 && number.signum() != 0)
                        prefix += c == 'X' ? "0X" : "0x";
                    if (radix == 8 && !digits.startsWith("0"))
                        digits = '0' + digits;
                }
                if (!left && precision < 0 && flags.indexOf('0') >= 0)
                    while (prefix.length() + digits.length() < width)
                        digits = '0' + digits;
                text = prefix + digits;
            } else if (c == 's') {
                text = Jpp.cString(value);
                if (precision >= 0 && text.length() > precision)
                    text = text.substring(0, precision);
            } else if (c == 'c') {
                int code =
                    value instanceof Character ? (Character)value : ((Number)value).intValue();
                text = Character.toString((char)(code & 255));
            } else if (c == 'n') {
                int count = result.length();
                Object countValue = length.isEmpty() || length.equals("h") || length.equals("hh")
                                        ? (Object)Integer.valueOf(count)
                                        : (Object)Long.valueOf(count);
                ((JppRef)value).set(countValue);
                continue;
            } else if ("aAeEfFgG".indexOf(c) >= 0) {
                String special =
                    Jpp.nonfinite(((Number)value).doubleValue(), Character.isUpperCase(c),
                                  flags.indexOf('+') >= 0, flags.indexOf(' ') >= 0);
                if (special != null) {
                    text = special;
                } else {
                    double number = ((Number)value).doubleValue();
                    String prefix = Double.doubleToRawLongBits(number) < 0 ? "-"
                                    : flags.indexOf('+') >= 0              ? "+"
                                    : flags.indexOf(' ') >= 0              ? " "
                                                                           : "";
                    String digits =
                        Jpp.floating(Math.abs(number), c, precision, flags.indexOf('#') >= 0);
                    if (flags.indexOf('0') >= 0 && !left) {
                        if ((c == 'a' || c == 'A') && digits.length() >= 2) {
                            prefix += digits.substring(0, 2);
                            digits = digits.substring(2);
                        }
                        while (prefix.length() + digits.length() < width)
                            digits = '0' + digits;
                    }
                    text = prefix + digits;
                }
            } else
                throw new UnsupportedOperationException("printf conversion: " + c);
            int padding = Math.max(0, width - text.length());
            if (!left)
                while (padding-- > 0)
                    result.append(' ');
            result.append(text);
            if (left)
                while (padding-- > 0)
                    result.append(' ');
        }
        String text = result.toString();
        output.print(text);
        return text.length();
    }
}

static final JppInput jppIn = new JppInput();
static final JppOutput jppOut = new JppOutput(System.out),
                      jppErr = new JppOutput(System.err, true),
                      jppLog = new JppOutput(jppErr);

static class Jpp {
    static int toupper(int value) {
        return value >= 'a' && value <= 'z' ? value - ('a' - 'A') : value;
    }
    static int tolower(int value) {
        return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
    }
    static JppPtr<JppPtr<Character>> arguments(String program, String[] arguments) {
        ArrayList<JppPtr<Character>> values = new ArrayList<JppPtr<Character>>();
        values.add(argumentString(program));
        for (String argument : arguments)
            values.add(argumentString(argument));
        values.add(null);
        return pointer(begin(values));
    }
    static JppPtr<Character> argumentString(String value) {
        String bytes = new String(value.getBytes(java.nio.charset.StandardCharsets.UTF_8),
                                  java.nio.charset.StandardCharsets.ISO_8859_1);
        return new JppString(bytes).c_str();
    }
    static long byteIoCount(long size, long count) {
        if (size == 0 || count == 0)
            return 0;
        if (size < 0 || count < 0 || size > Long.MAX_VALUE / count)
            throw new IllegalArgumentException("C byte-I/O request exceeds indexed storage limits");
        return size * count;
    }
    static long fread(JppPtr<?> destination, long size, long count, JppInput input) {
        long requested = byteIoCount(size, count);
        if (requested == 0)
            return 0;
        JppMemory storage = memory(destination);
        long offset = byteOffset(destination), copied = 0;
        byte[] buffer = new byte[(int)Math.min(requested, 8192L)];
        while (copied < requested) {
            int length = (int)Math.min((long)buffer.length, requested - copied), available = 0;
            try {
                while (available < length) {
                    int value = input.readByte();
                    if (value < 0)
                        break;
                    buffer[available++] = (byte)value;
                }
            } catch (UncheckedIOException failure) {
                // Bytes obtained before an I/O error still belong to this read.
            }
            if (available != 0) {
                storage.write(offset + copied, buffer, 0, available);
                copied += available;
            }
            if (available < length)
                break;
        }
        return copied / size;
    }
    static long fwrite(JppPtr<?> source, long size, long count, JppOutput output) {
        long requested = byteIoCount(size, count);
        if (requested == 0 || !output.good)
            return 0;
        JppMemory storage = memory(source);
        long offset = byteOffset(source), copied = 0;
        byte[] buffer = new byte[(int)Math.min(requested, 8192L)];
        while (copied < requested) {
            int length = (int)Math.min((long)buffer.length, requested - copied);
            storage.read(offset + copied, buffer, 0, length);
            output.raw(new String(buffer, 0, length,
                                    java.nio.charset.StandardCharsets.ISO_8859_1));
            copied += length;
            if (!output.good)
                break;
        }
        return copied / size;
    }
    static <T> T assignUsing(T source, T target, java.util.function.BiConsumer<T, T> assign) {
        assign.accept(source, target);
        return target;
    }
    static <T> JppRef<T> assignUsingRef(T source, JppRef<T> target,
            java.util.function.BiConsumer<T, T> assign) {
        assign.accept(source, target.get());
        return target;
    }
    static <T> List<T> reflectedArray(Object source) {
        return new AbstractList<T>() {
            public int size() { return java.lang.reflect.Array.getLength(source); }
            public T get(int index) { return (T)java.lang.reflect.Array.get(source, index); }
            public T set(int index, T value) {
                T previous = get(index);
                java.lang.reflect.Array.set(source, index, value);
                return previous;
            }
        };
    }
    static <A, T> A constructArray(A destination, A source,
            java.util.function.Function<JppRef<T>, T> construct) {
        List<T> elements = reflectedArray(source);
        for (int i = 0; i < elements.size(); ++i) {
            JppRef<T> element = JppRef.at(new JppPtr<T>(elements, i, source));
            java.lang.reflect.Array.set(destination, i, construct.apply(element));
        }
        return destination;
    }
    static <A, S, T> A assignArray(A source, A target,
            java.util.function.BiConsumer<JppRef<S>, JppRef<T>> assign) {
        List<S> from = reflectedArray(source);
        List<T> to = reflectedArray(target);
        for (int i = 0; i < to.size(); ++i)
            assign.accept(JppRef.at(new JppPtr<S>(from, i, source)),
                          JppRef.at(new JppPtr<T>(to, i, target)));
        return target;
    }
    static <S, T> ArrayList<T> assignStandardArray(ArrayList<S> source, ArrayList<T> target,
            java.util.function.BiConsumer<JppRef<S>, JppRef<T>> assign) {
        for (int i = 0; i < target.size(); ++i)
            assign.accept(JppRef.at(source, i), JppRef.at(target, i));
        return target;
    }
    static <S, T> JppRef<ArrayList<T>> assignStandardArrayRef(ArrayList<S> source,
            JppRef<ArrayList<T>> target,
            java.util.function.BiConsumer<JppRef<S>, JppRef<T>> assign) {
        assignStandardArray(source, target.get(), assign);
        return target;
    }
    static <S, T> ArrayList<T> constructStandardArray(ArrayList<S> source,
            java.util.function.Function<JppRef<S>, T> construct) {
        ArrayList<T> result = new ArrayList<T>(source.size());
        for (int i = 0; i < source.size(); ++i)
            result.add(construct.apply(JppRef.at(source, i)));
        return result;
    }
    static <T> ArrayList<T> movableVector(ArrayList<T> values) {
        return values instanceof JppVector ? values : new JppVector<T>(values);
    }
    static <T> void swapVectors(ArrayList<T> left, ArrayList<T> right) {
        JppVector<T> a = (JppVector<T>)left, b = (JppVector<T>)right;
        ArrayList<T> storage = a.data;
        a.data = b.data;
        b.data = storage;
    }
    static <T> ArrayList<T> moveVector(ArrayList<T> source) {
        JppVector<T> original = (JppVector<T>)source;
        JppVector<T> result = new JppVector<T>(original.data);
        original.data = new ArrayList<T>();
        return result;
    }
    static <T> ArrayList<T> moveAssignVector(ArrayList<T> source, ArrayList<T> target) {
        JppVector<T> from = (JppVector<T>)source, to = (JppVector<T>)target;
        to.data = from.data;
        from.data = new ArrayList<T>();
        return target;
    }
    static <T> JppRef<ArrayList<T>> moveAssignVectorRef(ArrayList<T> source,
            JppRef<ArrayList<T>> target) {
        moveAssignVector(source, target.get());
        return target;
    }
    static Object productValue(JppProduct product, int index) {
        Object value = product.raw(index);
        return product.binding(index) ? ((JppRef<?>)value).get() : value;
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    static Object assignProductField(Object target, Object value, boolean reference) {
        if (reference) {
            JppRef binding = (JppRef)target;
            binding.set(assignProductField(binding.get(), value, false));
            return target;
        }
        if (target instanceof JppIgnore)
            return target;
        return assignValue(target, value);
    }
    static void storeProductField(JppProduct product, int index, Object value) {
        product.raw(index, assignProductField(product.raw(index), value, product.binding(index)));
    }
    @SuppressWarnings("unchecked")
    static <T> JppRef<T> productReference(JppProduct product, int index) {
        Object value = product.raw(index);
        if (product.binding(index))
            return (JppRef<T>)value;
        return JppRef.of(() -> (T)product.raw(index), v -> product.raw(index, v),
                         product, product.memberName(index));
    }
    static <T> JppRef<T> arrayReference(JppRef<T> reference, JppMemoryType type, boolean littleEndian) {
        return JppRef.of(reference::get, reference::set,
                         () -> Jpp.<T>typedPointer(reference.pointer(), type, littleEndian));
    }
    static <S, T> T assignProduct(S source, T target, java.util.function.BiConsumer<S, T> assign) {
        assign.accept(source, target);
        return target;
    }
    static <S, T> T convertProduct(S source, java.util.function.Function<S, T> convert) {
        return convert.apply(source);
    }
    static <A, B> boolean testProducts(A left, B right, java.util.function.BiPredicate<A, B> test) {
        return test.test(left, right);
    }
    static <A, B> JppOrdering orderValues(A left, B right,
            java.util.function.BiFunction<A, B, JppOrdering> compare) {
        return compare.apply(left, right).copy();
    }
    static boolean equalProducts(JppProduct left, JppProduct right,
            java.util.function.BiPredicate<Object, Object> equal) {
        for (int i = 0; i < left.arity(); ++i)
            if (!equal.test(productValue(left, i), productValue(right, i))) return false;
        return true;
    }
    static JppOrdering orderProducts(JppProduct left, JppProduct right,
            java.util.function.BiFunction<Object, Object, JppOrdering> compare) {
        for (int i = 0; i < left.arity(); ++i) {
            JppOrdering result = compare.apply(productValue(left, i), productValue(right, i));
            if (result.rank != 0) return result;
        }
        return JppOrdering.EQUIVALENT.copy();
    }
    static <A, B> boolean sequenceEqual(List<A> left, List<B> right,
            java.util.function.BiPredicate<A, B> equal) {
        if (left.size() != right.size()) return false;
        for (int i = 0; i < left.size(); ++i)
            if (!equal.test(left.get(i), right.get(i))) return false;
        return true;
    }
    static boolean valueEquals(Object left, Object right) {
        if ((left instanceof Float || left instanceof Double) && right instanceof Number)
            return ((Number)left).doubleValue() == ((Number)right).doubleValue();
        if (left instanceof List && right instanceof List) {
            List<?> a = (List<?>)left, b = (List<?>)right;
            if (a.size() != b.size()) return false;
            for (int i = 0; i < a.size(); ++i)
                if (!valueEquals(a.get(i), b.get(i))) return false;
            return true;
        }
        return Objects.equals(left, right);
    }
    static int valueHash(Object value) {
        if (value instanceof Double && (Double)value == 0.0) return 0;
        if (value instanceof Float && (Float)value == 0.0f) return 0;
        return Objects.hashCode(value);
    }
    static int stringSize(long size) {
        if (Long.compareUnsigned(size, Integer.MAX_VALUE) > 0)
            throw new IllegalArgumentException("string length exceeds Java's limit");
        return (int)size;
    }
    static String filePath(String bytes) {
        int zero = bytes.indexOf('\0');
        if (zero >= 0)
            bytes = bytes.substring(0, zero);
        return new String(bytes.getBytes(java.nio.charset.StandardCharsets.ISO_8859_1),
                          java.nio.charset.StandardCharsets.UTF_8);
    }
    static <T> JppRef<T> storeReference(T value, JppRef<T> target) {
        target.set(value);
        return target;
    }
    static <T> JppRef<T> assignReference(T source, JppRef<T> target) {
        assignRecord(source, target);
        return target;
    }
    static <T> JppRef<T> updatedReference(JppRef<T> target, int amount) {
        update(target, amount, false);
        return target;
    }
    static <T> JppRef<JppPtr<T>> updatedPointerReference(JppRef<JppPtr<T>> target, int amount) {
        updatePointer(target, amount, false);
        return target;
    }
    @SuppressWarnings({"rawtypes", "unchecked"})
    static <T> T assignValue(T target, T source) {
        if (target instanceof JppAssignable) {
            ((JppAssignable)target).jppAssignFrom(source);
            return target;
        }
        if (target instanceof Collection && source instanceof Collection) {
            if (target == source)
                return target;
            Collection snapshot = (Collection)jppCopy(source);
            ((Collection)target).clear();
            ((Collection)target).addAll(snapshot);
            return target;
        }
        if (target != null && source != null && target.getClass().isArray()) {
            for (int i = 0; i < java.lang.reflect.Array.getLength(target); ++i)
                java.lang.reflect.Array.set(target, i,
                                            assignValue(java.lang.reflect.Array.get(target, i),
                                                        java.lang.reflect.Array.get(source, i)));
            return target;
        }
        return jppCopy(source);
    }
    static <T> T assignRecord(T source, JppRef<T> target) {
        T result = assignValue(target.get(), source);
        target.set(result);
        return result;
    }
    static <T> JppPtr<T> pointer(JppIter<T> at) {
        if (at != null && at.list instanceof JppDequeStorage)
            return ((JppDequeStorage<T>)at.list).reference(at.index()).pointer();
        if (at instanceof JppPtr)
            return ((JppPtr<T>)at).plus(0);
        return at == null ? null : new JppPtr<T>(at.pointerStorage(), at.pos, at.pointerIdentity());
    }
    static <T> JppPtr<T> integerPointer(BigInteger value, int width) {
        BigInteger address = value.and(BigInteger.ONE.shiftLeft(width).subtract(BigInteger.ONE));
        return address.signum() == 0 ? null : new JppIntegerPtr<T>(address);
    }
    static int arrayBound(long value) {
        if (value < 0 || value > Integer.MAX_VALUE - 8)
            throw new IllegalArgumentException("C++ array bound exceeds Java's indexing limit");
        return (int)value;
    }
    static <T> JppPtr<T> arrayAddress(T array, JppMemoryType type, boolean littleEndian) {
        return new JppMemoryPtr<T>(new JppMemory(array, type, littleEndian), 0, type);
    }
    @SuppressWarnings("unchecked")
    static <T> JppPtr<T> arrayDecay(JppPtr<?> array) {
        return new JppMemoryPtr<T>(memory(array), byteOffset(array), array.elementType.element);
    }
    static <T> JppPtr<T> arrayEnd(JppPtr<?> array) {
        return Jpp.<T>arrayDecay(array).plus(array.elementType.count);
    }
    static int arrayLength(JppPtr<?> array) {
        return array.elementType.count;
    }
    static long arrayBytes(JppPtr<?> array) {
        return array.elementType.size;
    }
    static <T> T arrayCopy(T destination, JppPtr<?> source, JppMemoryType type, boolean littleEndian) {
        memcpy(arrayAddress(destination, type, littleEndian), source, type.size);
        return destination;
    }
    @SuppressWarnings("unchecked")
    static <T> JppPtr<T> typedPointer(JppIter<?> at, JppMemoryType type, boolean littleEndian) {
        if (at == null)
            return null;
        JppPtr<?> pointer = at instanceof JppPtr ? (JppPtr<?>)at : pointer(at);
        if (pointer instanceof JppIntegerPtr)
            return (JppPtr<T>)pointer;
        if (pointer.elementType == null) {
            pointer = pointer.plus(0);
            pointer.elementType = type;
            pointer.littleEndian = littleEndian;
        } else if (!pointer.elementType.sameType(type))
            pointer = new JppMemoryPtr<Object>(memory(pointer), byteOffset(pointer), type);
        if (type.kind == 'r' && !(pointer instanceof JppMemoryPtr)) {
            JppMemory storage = pointer.memory;
            if (storage == null && !pointer.list.isEmpty()) {
                JppRecord object = (JppRecord)pointer.list.get(0);
                JppRecordPosition binding = object.jppPosition;
                if (binding != null && binding.memory.elementType == type &&
                    sameStorage(binding.memory.identity, pointer.storage))
                    storage = binding.memory;
            }
            if (storage == null)
                storage = memory(pointer);
            pointer = new JppMemoryPtr<Object>(storage, byteOffset(pointer), type);
        }
        return (JppPtr<T>)pointer;
    }
    static JppMemory memory(JppPtr<?> pointer) {
        if (pointer.elementType == null)
            throw new UnsupportedOperationException("byte access to this C++ object layout");
        if (pointer.memory == null)
            pointer.memory = new JppMemory(pointer);
        return pointer.memory;
    }
    static JppPtr<?> voidPointer(JppIter<?> at) {
        if (at == null)
            return null;
        JppPtr<?> pointer = pointer(at);
        return pointer.elementType == null
            ? pointer
            : Jpp.<Integer>typedPointer(pointer, JppMemoryType.U8, pointer.littleEndian);
    }
    static JppPtr<?> voidPlus(JppPtr<?> pointer, int offset) {
        if (pointer == null && offset == 0)
            return null;
        if (pointer.elementType == null)
            throw new UnsupportedOperationException(
                "void-pointer arithmetic on this C++ object layout");
        return voidPointer(pointer).plus(offset);
    }
    static JppPtr<?> updateVoidPointer(JppRef<JppPtr<?>> target, int amount, boolean post) {
        JppPtr<?> previous = target.get();
        JppPtr<?> next = voidPlus(previous, amount);
        target.set(next);
        return post ? voidPointer(previous) : next;
    }
    static long byteOffset(JppPtr<?> pointer) {
        return pointer instanceof JppMemoryPtr ? ((JppMemoryPtr<?>)pointer).byteOffset()
                                               : (long)pointer.pos * pointer.elementType.size;
    }
    static boolean sameStorage(Object left, Object right) {
        return left == right || (left instanceof JppAddress && left.equals(right));
    }
    static boolean sameMemoryAddress(JppPtr<?> left, JppIter<?> right) {
        if (right instanceof JppPtr && ((JppPtr<?>)right).elementType != null) {
            JppPtr<?> pointer = (JppPtr<?>)right;
            if (sameStorage(left.storage, pointer.storage))
                return byteOffset(left) == byteOffset(pointer);
            prepareRecordAddress(left);
            prepareRecordAddress(pointer);
            JppRecordPosition a = memoryPosition(left), b = memoryPosition(pointer);
            return sameStorage(a.memory.identity, b.memory.identity) && a.offset == b.offset;
        }
        return sameStorage(left.storage, right.storage) && left.pos == right.pos;
    }
    static void prepareRecordAddress(JppPtr<?> pointer) {
        memory(pointer).bindRecord(byteOffset(pointer));
    }
    static JppRecordPosition memoryPosition(JppPtr<?> pointer) {
        JppMemory memory = memory(pointer);
        long offset = byteOffset(pointer);
        // A field address can precede discovery of the containing array. Follow
        // the object's current binding when its earlier view was a singleton.
        while (!memory.allocated() && memory.elementType.kind == 'r' &&
               memory.values != null && memory.values.size() == 1) {
            JppRecord object = (JppRecord)memory.values.get(0);
            JppRecordPosition binding = object.jppPosition;
            if (binding == null || binding.memory == memory)
                break;
            memory = binding.memory;
            offset += binding.offset;
        }
        return new JppRecordPosition(memory, offset);
    }
    static long pointerDifference(JppIter<?> left, JppIter<?> right) {
        if (left == null && right == null)
            return 0;
        if (left instanceof JppPtr && right instanceof JppPtr) {
            JppPtr<?> a = (JppPtr<?>)left, b = (JppPtr<?>)right;
            if (a.elementType != null && b.elementType != null) {
                if (sameStorage(a.storage, b.storage))
                    return (byteOffset(a) - byteOffset(b)) / a.elementType.size;
                prepareRecordAddress(a);
                prepareRecordAddress(b);
                JppRecordPosition from = memoryPosition(a), to = memoryPosition(b);
                if (!sameStorage(from.memory.identity, to.memory.identity))
                    throw new IllegalArgumentException(
                        "subtracting pointers to different allocations");
                return (from.offset - to.offset) / a.elementType.size;
            }
        }
        return (long)left.pos - right.pos;
    }
    static JppPtr<?> malloc(long size, boolean littleEndian) {
        if (size < 0 || size > Integer.MAX_VALUE - 8)
            return null;
        try {
            JppMemory memory = new JppMemory(new byte[(int)size], littleEndian);
            return new JppMemoryPtr<Integer>(memory, 0, JppMemoryType.U8);
        } catch (OutOfMemoryError failure) {
            return null;
        }
    }
    static JppPtr<?> calloc(long count, long size, boolean littleEndian) {
        if (count != 0 && Long.compareUnsigned(size, Long.divideUnsigned(-1L, count)) > 0)
            return null;
        return malloc(count * size, littleEndian);
    }
    static void free(JppPtr<?> pointer) {
        if (pointer == null)
            return;
        JppMemory memory = memory(pointer);
        if (!memory.allocated() || byteOffset(pointer) != 0 || memory.freed)
            throw new IllegalArgumentException("free requires a live allocation base");
        memory.freed = true;
        memory.allocation = null;
        if (memory.records != null)
            memory.records.clear();
    }
    static JppPtr<?> realloc(JppPtr<?> pointer, long size, boolean littleEndian) {
        if (pointer == null)
            return malloc(size, littleEndian);
        JppMemory old = memory(pointer);
        if (!old.allocated() || byteOffset(pointer) != 0 || old.freed)
            throw new IllegalArgumentException("realloc requires a live allocation base");
        if (size == 0) {
            free(pointer);
            return null;
        }
        JppPtr<?> result = malloc(size, littleEndian);
        if (result != null) {
            memcpy(result, pointer, Math.min(old.size(), size));
            free(pointer);
        }
        return result;
    }
    static JppPtr<?> memcpy(JppPtr<?> destination, JppPtr<?> source, long count) {
        return memoryCopy(destination, source, count, false);
    }
    static JppPtr<?> memmove(JppPtr<?> destination, JppPtr<?> source, long count) {
        return memoryCopy(destination, source, count, true);
    }
    static JppPtr<?> memoryCopy(JppPtr<?> destination, JppPtr<?> source, long count, boolean move) {
        JppMemory to = memory(destination), from = memory(source);
        long target = byteOffset(destination), origin = byteOffset(source);
        to.check(target, count);
        from.check(origin, count);
        boolean backwards = false;
        if (move) {
            if (sameStorage(to.identity, from.identity))
                backwards = target > origin && target - origin < count;
            else {
                prepareRecordAddress(destination);
                prepareRecordAddress(source);
                JppRecordPosition targetAddress = memoryPosition(destination);
                JppRecordPosition sourceAddress = memoryPosition(source);
                backwards = sameStorage(targetAddress.memory.identity, sourceAddress.memory.identity) &&
                    targetAddress.offset > sourceAddress.offset &&
                    targetAddress.offset - sourceAddress.offset < count;
            }
        }
        byte[] buffer = new byte[(int)Math.min(count, 16384)];
        long done = 0;
        while (done < count) {
            int part = (int)Math.min(count - done, buffer.length);
            long offset = backwards ? count - done - part : done;
            from.read(origin + offset, buffer, 0, part);
            to.write(target + offset, buffer, 0, part);
            done += part;
        }
        return pointer(destination);
    }
    static JppPtr<?> memset(JppPtr<?> destination, int byteValue, long count) {
        JppMemory to = memory(destination);
        long offset = byteOffset(destination);
        to.check(offset, count);
        byte[] buffer = new byte[(int)Math.min(count, 16384)];
        Arrays.fill(buffer, (byte)byteValue);
        for (long done = 0; done < count;) {
            int part = (int)Math.min(count - done, buffer.length);
            to.write(offset + done, buffer, 0, part);
            done += part;
        }
        return pointer(destination);
    }
    static int memcmp(JppPtr<?> left, JppPtr<?> right, long count) {
        JppMemory a = memory(left), b = memory(right);
        long x = byteOffset(left), y = byteOffset(right);
        a.check(x, count);
        b.check(y, count);
        for (long i = 0; i < count; ++i) {
            int difference = a.getByte(x + i) - b.getByte(y + i);
            if (difference != 0)
                return difference;
        }
        return 0;
    }
    static JppPtr<?> memchr(JppPtr<?> source, int byteValue, long count) {
        JppMemory from = memory(source);
        long offset = byteOffset(source);
        from.check(offset, count);
        for (long i = 0; i < count; ++i)
            if (from.getByte(offset + i) == (byteValue & 255))
                return new JppMemoryPtr<Integer>(from, offset + i, JppMemoryType.U8);
        return null;
    }
    static <T> JppPtr<T> address(JppRef<T> value, Object object, Object field) {
        if (object instanceof JppRecord) {
            JppRecord record = (JppRecord)object;
            Jpp.pointerValue(record);
            JppRecordPosition position = record.jppPosition;
            for (JppMemoryField<?, ?> member : record.jppLayout().fields)
                if (member.name.equals(field))
                    return new JppMemoryPtr<T>(position.memory, position.offset + member.offset,
                                               member.type);
        }
        List<T> view = new AbstractList<T>() {
            public int size() {
                return 1;
            }
            public T get(int index) {
                if (index != 0)
                    throw new IndexOutOfBoundsException();
                return value.get();
            }
            public T set(int index, T item) {
                T old = get(index);
                value.set(item);
                return old;
            }
        };
        return new JppPtr<T>(view, 0, new JppAddress(object, field));
    }
    @SuppressWarnings("unchecked")
    static <T> JppPtr<T> pointerValue(T value) {
        if (value instanceof JppRecord) {
            JppRecord object = (JppRecord)value;
            JppMemoryType type = object.jppLayout();
            if (object.jppPosition == null) {
                JppPtr<T> pointer = new JppPtr<T>(Collections.singletonList(value), 0, value);
                pointer.elementType = type;
                pointer.littleEndian = type.recordLittleEndian;
                JppMemory storage = memory(pointer);
                storage.recordView(0, type);
            }
            JppRecordPosition position = object.jppPosition;
            return new JppMemoryPtr<T>(position.memory, position.offset, type);
        }
        return address(JppRef.cell(value), value, "");
    }
    static <T> JppPtr<T> allocate(int count, java.util.function.IntFunction<T> initializer) {
        ArrayList<T> storage = new ArrayList<T>(count);
        for (int i = 0; i < count; ++i)
            storage.add(initializer.apply(i));
        return new JppPtr<T>(storage, 0, storage);
    }
    static <T> JppPtr<T> updatePointer(JppRef<JppPtr<T>> target, int amount, boolean post) {
        JppPtr<T> old = target.get();
        JppPtr<T> next = old.plus(amount);
        target.set(next);
        return post ? old : next;
    }
    static boolean samePointer(JppIter<?> left, JppIter<?> right) {
        if (left == null || right == null)
            return left == right;
        if (left instanceof JppPtr && ((JppPtr<?>)left).elementType != null &&
            right instanceof JppPtr && ((JppPtr<?>)right).elementType != null)
            return sameMemoryAddress((JppPtr<?>)left, right);
        return pointer(left).same(right);
    }
    static void delete(JppPtr<?> pointer) {}
    static double unsignedDouble(long value) {
        return value >= 0 ? (double)value : (double)((value >>> 1) | (value & 1)) * 2.0;
    }
    static float unsignedFloat(long value) {
        return value >= 0 ? (float)value : (float)((value >>> 1) | (value & 1)) * 2.0f;
    }
    static BigInteger unsignedBigInteger(long value) {
        BigInteger magnitude = BigInteger.valueOf(value & Long.MAX_VALUE);
        return value < 0 ? magnitude.setBit(63) : magnitude;
    }
    static BigInteger floatingInteger(double value) {
        return new BigDecimal(value).toBigInteger();
    }
    static String decimalPoint(String text, boolean alternate) {
        return alternate && text.indexOf('.') < 0 ? text + '.' : text;
    }
    static String trimFraction(String text) {
        if (text.indexOf('.') < 0)
            return text;
        int end = text.length();
        while (end > 0 && text.charAt(end - 1) == '0')
            --end;
        if (end > 0 && text.charAt(end - 1) == '.')
            --end;
        return text.substring(0, end);
    }
    static String exponent(int value, char marker) {
        int magnitude = Math.abs(value);
        return "" + marker + (value < 0 ? '-' : '+') + (magnitude < 10 ? "0" : "") + magnitude;
    }
    static String hexadecimal(double value, int precision, boolean alternate) {
        long raw = Double.doubleToRawLongBits(value);
        int encodedExponent = (int)((raw >>> 52) & 2047);
        int exponent = encodedExponent == 0 ? value == 0 ? 0 : -1022 : encodedExponent - 1023;
        long significand = (raw & ((1L << 52) - 1)) | (encodedExponent == 0 ? 0 : 1L << 52);
        String digits;
        if (precision >= 0 && precision < 13) {
            int discarded = 52 - 4 * precision;
            long rounded = significand >>> discarded;
            long remainder = significand & ((1L << discarded) - 1), halfway = 1L << (discarded - 1);
            if (remainder > halfway || (remainder == halfway && (rounded & 1) != 0))
                ++rounded;
            digits = Long.toHexString(rounded);
            while (digits.length() < precision + 1)
                digits = '0' + digits;
        } else {
            digits = Long.toHexString(significand);
            while (digits.length() < 14)
                digits = '0' + digits;
            if (precision < 0) {
                while (digits.length() > 1 && digits.endsWith("0"))
                    digits = digits.substring(0, digits.length() - 1);
            } else
                while (digits.length() < precision + 1)
                    digits += '0';
        }
        String fraction = digits.length() > 1 ? "." + digits.substring(1) : alternate ? "." : "";
        return "0x" + digits.charAt(0) + fraction + 'p' + (exponent < 0 ? "" : "+") + exponent;
    }
    static String floating(double value, char conversion, int precision, boolean alternate) {
        char kind = Character.toLowerCase(conversion);
        String result;
        if (kind == 'a')
            result = hexadecimal(value, precision, alternate);
        else {
            int p = precision < 0 ? 6 : precision;
            if (kind == 'g')
                p = Math.max(1, p);
            BigDecimal exact = new BigDecimal(value);
            if (kind == 'f')
                result = decimalPoint(exact.setScale(p, RoundingMode.HALF_EVEN).toPlainString(),
                                      alternate);
            else {
                int significant = kind == 'e' ? p + 1 : p;
                BigDecimal rounded =
                    exact.round(new MathContext(significant, RoundingMode.HALF_EVEN));
                int exponent = value == 0 ? 0 : rounded.precision() - rounded.scale() - 1;
                boolean scientific = kind == 'e' || exponent < -4 || exponent >= p;
                if (scientific) {
                    int fraction = kind == 'e' ? p : p - 1;
                    String mantissa = rounded.scaleByPowerOfTen(-exponent)
                                          .setScale(fraction, RoundingMode.HALF_EVEN)
                                          .toPlainString();
                    if (kind == 'g' && !alternate)
                        mantissa = trimFraction(mantissa);
                    result = decimalPoint(mantissa, alternate) + exponent(exponent, 'e');
                } else {
                    result = rounded.setScale(Math.max(0, p - exponent - 1), RoundingMode.HALF_EVEN)
                                 .toPlainString();
                    result = alternate ? decimalPoint(result, true) : trimFraction(result);
                }
            }
        }
        return Character.isUpperCase(conversion) ? result.toUpperCase(Locale.ROOT) : result;
    }
    static String nonfinite(double value, boolean uppercase, boolean plus, boolean space) {
        if (Double.isFinite(value))
            return null;
        String sign = Double.doubleToRawLongBits(value) < 0 ? "-" : plus ? "+" : space ? " " : "";
        return sign + (Double.isNaN(value) ? uppercase ? "NAN" : "nan" : uppercase ? "INF" : "inf");
    }
    static double log2(double value) {
        if (Double.isNaN(value))
            return quietNaN(value);
        if (value < 0)
            return Double.longBitsToDouble(0xfff8000000000000L);
        if (value == 0)
            return Double.NEGATIVE_INFINITY;
        if (Double.isInfinite(value))
            return value;
        int adjustment = 0;
        if (value < Double.MIN_NORMAL) {
            value = Math.scalb(value, 54);
            adjustment = -54;
        }
        int exponent = Math.getExponent(value);
        return exponent + adjustment +
            Math.log(Math.scalb(value, -exponent)) / 0x1.62e42fefa39efp-1;
    }
    static double round(double value) {
        if (Double.isNaN(value))
            return quietNaN(value);
        double magnitude = Math.abs(value);
        if (!Double.isFinite(value) || magnitude >= 0x1.0p52)
            return value;
        double integral = Math.floor(magnitude);
        if (magnitude - integral >= 0.5)
            ++integral;
        return Math.copySign(integral, value);
    }
    static float round(float value) {
        if (Float.isNaN(value))
            return quietNaN(value);
        return (float)round((double)value);
    }
    static double quietNaN(double value) {
        return Double.longBitsToDouble(Double.doubleToRawLongBits(value) | 0x0008000000000000L);
    }
    static float quietNaN(float value) {
        return Float.intBitsToFloat(Float.floatToRawIntBits(value) | 0x00400000);
    }
    static double rint(double value) {
        return Double.isNaN(value) ? quietNaN(value) : Math.rint(value);
    }
    static float rint(float value) {
        return Float.isNaN(value) ? quietNaN(value) : (float)Math.rint((double)value);
    }
    static double trunc(double value) {
        if (Double.isNaN(value))
            return quietNaN(value);
        return Math.copySign(Math.floor(Math.abs(value)), value);
    }
    static float trunc(float value) {
        return Float.isNaN(value) ? quietNaN(value) : (float)trunc((double)value);
    }
    static <T> JppIter<T> advance(int amount, JppIter<T> position) {
        return position.advance(amount);
    }
    static <T> T extract(JppInput source, JppRef<T> destination,
                         java.util.function.BiFunction<JppInput, T, T> reader) {
        return destination.set(reader.apply(source, destination.get()));
    }
    static char[] charArray(String value, int size) {
        return Arrays.copyOf(value.toCharArray(), size);
    }
    static long strlen(Object value) {
        if (value instanceof JppIter) {
            JppIter<?> at = ((JppIter<?>)value).plus(0);
            long length = 0;
            while ((Character)at.get() != '\0') {
                ++length;
                at.advance(1);
            }
            return length;
        }
        return cString(value).length();
    }
    static String cString(Object value) {
        if (value instanceof String) {
            String text = (String)value;
            int end = text.indexOf('\0');
            return end < 0 ? text : text.substring(0, end);
        }
        if (value instanceof JppIter) {
            JppIter<?> at = ((JppIter<?>)value).plus(0);
            StringBuilder result = new StringBuilder();
            for (char c; (c = (Character)at.get()) != '\0'; at.advance(1))
                result.append(c);
            return result.toString();
        }
        if (value instanceof char[]) {
            char[] chars = (char[])value;
            int length = 0;
            while (length < chars.length && chars[length] != '\0')
                ++length;
            return new String(chars, 0, length);
        }
        return value.toString();
    }
    static <T> T sequence(java.util.function.Supplier<T> action) {
        return action.get();
    }
    static JppScanTarget scanInt(JppRef<Integer> target, int bits, boolean unsigned) {
        return (text, radix, conversion) -> {
            int value = new BigInteger(text, radix).intValue();
            if (bits < 32) {
                int shift = 32 - bits;
                value = unsigned ? value & (int)((1L << bits) - 1) : (value << shift) >> shift;
            }
            target.set(value);
        };
    }
    static JppScanTarget scanLong(JppRef<Long> target) {
        return (text, radix, conversion) -> target.set(new BigInteger(text, radix).longValue());
    }
    static JppScanTarget scanFloat(JppRef<Float> target) {
        return (text, radix, conversion) -> target.set((float)JppScanner.parseFloating(text));
    }
    static JppScanTarget scanDouble(JppRef<Double> target) {
        return (text, radix, conversion) -> target.set(JppScanner.parseFloating(text));
    }
    static JppScanTarget scanChar(JppRef<Character> target) {
        return (text, radix, conversion)
                   -> target.set(conversion == 'c' ? text.charAt(0)
                                                   : (char)new BigInteger(text, radix).intValue());
    }
    static JppScanTarget scanChars(char[] target) {
        return (text, radix, conversion) -> {
            for (int i = 0; i < text.length(); ++i)
                target[i] = text.charAt(i);
            if (conversion != 'c')
                target[text.length()] = '\0';
        };
    }
    static JppScanTarget scanChars(JppIter<Character> target) {
        JppIter<Character> start = target.plus(0);
        return (text, radix, conversion) -> {
            for (int i = 0; i < text.length(); ++i)
                start.plus(i).put(text.charAt(i));
            if (conversion != 'c')
                start.plus(text.length()).put('\0');
        };
    }
    static JppInput reopenInput(JppInput stream, String path, String mode) {
        if (!mode.startsWith("r"))
            throw new UnsupportedOperationException("input freopen mode: " + mode);
        stream.close();
        stream.open(path);
        return stream.good ? stream : null;
    }
    static JppOutput reopenOutput(JppOutput stream, String path, String mode) {
        if (!mode.startsWith("w") && !mode.startsWith("a"))
            throw new UnsupportedOperationException("output freopen mode: " + mode);
        stream.close();
        stream.open(path, mode.startsWith("a") ? 17 : 16);
        return stream.good ? stream : null;
    }
    static <T> T append(List<T> values, T value) {
        values.add(value);
        return value;
    }
    static <T> JppIter<T> backInserter(Collection<T> target) {
        return new JppBackInserter<T>(target);
    }
    static <S, T> JppIter<T> copy(JppIter<S> first, JppIter<S> last, JppIter<T> result,
                                  java.util.function.Function<S, T> convert) {
        JppIter<T> out = result.plus(0);
        for (JppIter<S> at = first.plus(0); !at.same(last); at.advance(1)) {
            out.put(jppCopy(convert.apply(at.get())));
            out.advance(1);
        }
        return out;
    }
    static <S, T> JppIter<T> copy_n(JppIter<S> first, int count, JppIter<T> result,
                                    java.util.function.Function<S, T> convert) {
        return copy(first, first.plus(Math.max(0, count)), result, convert);
    }
    static <S, T> JppIter<T> copy_backward(JppIter<S> first, JppIter<S> last, JppIter<T> result,
                                           java.util.function.Function<S, T> convert) {
        JppIter<S> in = last.plus(0);
        JppIter<T> out = result.plus(0);
        while (!in.same(first)) {
            in.advance(-1);
            out.advance(-1);
            out.put(jppCopy(convert.apply(in.get())));
        }
        return out;
    }
    static <A, B, T> JppIter<T> merge(JppIter<A> first, JppIter<A> last, JppIter<B> second,
                                      JppIter<B> secondLast, JppIter<T> result,
                                      java.util.function.Function<A, T> left,
                                      java.util.function.Function<B, T> right) {
        return merge(first, last, second, secondLast, result, left, right,
                     (a, b) -> compare(a, b) < 0);
    }
    static <A, B, T> JppIter<T> merge(JppIter<A> first, JppIter<A> last, JppIter<B> second,
                                      JppIter<B> secondLast, JppIter<T> result,
                                      java.util.function.Function<A, T> left,
                                      java.util.function.Function<B, T> right,
                                      java.util.function.BiPredicate<B, A> less) {
        JppIter<A> a = first.plus(0);
        JppIter<B> b = second.plus(0);
        JppIter<T> out = result.plus(0);
        while (!a.same(last) && !b.same(secondLast)) {
            if (less.test(b.get(), a.get())) {
                out.put(jppCopy(right.apply(b.get())));
                b.advance(1);
            } else {
                out.put(jppCopy(left.apply(a.get())));
                a.advance(1);
            }
            out.advance(1);
        }
        out = copy(a, last, out, left);
        return copy(b, secondLast, out, right);
    }
    static <T, F> F for_each(JppIter<T> first, JppIter<T> last, F function,
                             java.util.function.BiConsumer<F, JppIter<T>> invoke) {
        for (JppIter<T> at = first.plus(0); !at.same(last); at.advance(1))
            invoke.accept(function, at);
        return function;
    }
    static <A, B, C, D> JppPair<C, D> convertPair(JppPair<A, B> value,
                                                  java.util.function.Function<A, C> first,
                                                  java.util.function.Function<B, D> second) {
        return new JppPair<C, D>(jppCopy(first.apply(value.first)),
                                 jppCopy(second.apply(value.second)));
    }
    static int reverseCompare(Object a, Object b) {
        return compare(b, a);
    }
    static int bitCeil(int value) {
        return value == 0 || value == 1 ? 1 : Integer.highestOneBit(value - 1) << 1;
    }
    static long bitCeil(long value) {
        return value == 0 || value == 1 ? 1L : Long.highestOneBit(value - 1) << 1;
    }
    // Argument order preserves C++17's RHS-before-lvalue sequencing.
    static <T> T store(T value, JppRef<T> target) {
        return target.set(value);
    }
    static <L, R> L compound(R right, JppRef<L> target,
                             java.util.function.BiFunction<L, R, L> operation) {
        return target.set(operation.apply(target.get(), right));
    }
    static <L, R> JppRef<L> compoundReference(R right, JppRef<L> target,
                                              java.util.function.BiFunction<L, R, L> operation) {
        target.set(operation.apply(target.get(), right));
        return target;
    }
    static <T> int swap(JppRef<T> a, JppRef<T> b) {
        T value = a.get();
        a.set(b.get());
        b.set(value);
        return 0;
    }
    static <T> Comparator<T> comparator(java.util.function.BiPredicate<? super T, ? super T> less) {
        return (a, b) -> less.test(a, b) ? -1 : less.test(b, a) ? 1 : 0;
    }
    static <T> JppRef<T> minReference(JppRef<T> first, JppRef<T> second,
                                      java.util.function.BiPredicate<T, T> less) {
        return less.test(second.get(), first.get()) ? second : first;
    }
    static <T> JppRef<T> maxReference(JppRef<T> first, JppRef<T> second,
                                      java.util.function.BiPredicate<T, T> less) {
        return less.test(first.get(), second.get()) ? second : first;
    }
    static <K, V> JppMap<K, V> mapFrom(JppMap<K, V> map,
                                       Iterable<? extends JppPair<K, V>> values) {
        for (JppPair<K, V> value : values)
            map.insert(value);
        return map;
    }
    static <A, K, V> JppMap<K, V> mapFrom(JppMap<K, V> map, JppIter<A> first, JppIter<A> last,
                                          java.util.function.Function<A, JppPair<K, V>> convert) {
        for (JppIter<A> at = first.plus(0); !at.same(last); at.advance(1))
            map.insert(convert.apply(at.get()));
        return map;
    }
    static <T> Comparator<T> heapComparator(java.util.function.BiPredicate<? super T, ? super T> less) {
        return (a, b) -> less.test(a, b) ? 1 : less.test(b, a) ? -1 : 0;
    }
    static <T> PriorityQueue<T> heapFrom(PriorityQueue<T> heap, Iterable<? extends T> values) {
        for (T value : values)
            heap.add(jppCopy(value));
        return heap;
    }
    static <A, T> PriorityQueue<T> heapFrom(PriorityQueue<T> heap, JppIter<A> first, JppIter<A> last,
                                            java.util.function.Function<A, T> convert) {
        for (JppIter<A> at = first.plus(0); !at.same(last); at.advance(1))
            heap.add(jppCopy(convert.apply(at.get())));
        return heap;
    }
    static <T> ArrayList<T> vectorDefault(int n, java.util.function.Supplier<T> factory) {
        ArrayList<T> a = new ArrayList<T>(n);
        while (n-- > 0)
            a.add(factory.get());
        return a;
    }
    static <T> ArrayList<T> vector(int n, T value) {
        return vectorDefault(n, () -> jppCopy(value));
    }
    static <T> ArrayList<T> vector(JppIter<T> first, JppIter<T> last) {
        ArrayList<T> a = new ArrayList<T>();
        for (JppIter<T> p = first.plus(0); !p.same(last); p.advance(1))
            a.add(jppCopy(p.get()));
        return a;
    }
    static <T> ArrayList<T> vectorInitialized(int size, java.util.function.Supplier<T> zero,
                                            List<T> initial) {
        ArrayList<T> result = new ArrayList<T>(size);
        result.addAll(initial);
        while (result.size() < size)
            result.add(zero.get());
        return result;
    }
    static <T> T set(List<T> a, int i, T value) {
        a.set(i, value);
        return value;
    }
    static <T> JppDeque<T> dequeDefault(int n, java.util.function.Supplier<T> factory) {
        JppDeque<T> result = new JppDeque<T>();
        resizeDefault(result, n, factory);
        return result;
    }
    static <T> JppDeque<T> dequeFill(int n, T value) {
        return dequeDefault(n, () -> jppCopy(value));
    }
    static <A, T> JppDeque<T> dequeFrom(JppIter<A> first, JppIter<A> last,
                                        java.util.function.Function<A, T> convert) {
        JppDeque<T> result = new JppDeque<T>();
        for (JppIter<A> at = first.plus(0); !at.same(last); at.advance(1))
            result.addLast(jppCopy(convert.apply(at.get())));
        return result;
    }
    static <T> List<T> listStorage(List<T> values) {
        if (values instanceof JppListOwner)
            return ((JppListOwner<T>)values).jppStorage();
        return values instanceof JppDeque ? ((JppDeque<T>)values).data : values;
    }
    static <T> void resizeDefault(List<T> a, int n, java.util.function.Supplier<T> factory) {
        if (n < a.size())
            a.subList(n, a.size()).clear();
        while (a.size() < n)
            a.add(factory.get());
    }
    static <T> void resize(List<T> a, int n, T value) {
        resizeDefault(a, n, () -> jppCopy(value));
    }
    static <T> void assign(List<T> a, int n, T value) {
        a.clear();
        resizeDefault(a, n, () -> jppCopy(value));
    }
    static <T> JppIter<T> erase(List<T> a, JppIter<T> first, JppIter<T> last) {
        int index = first.index();
        listStorage(a).subList(index, last.index()).clear();
        return begin(a).plus(index);
    }
    static <T> JppIter<T> erase(List<T> a, JppIter<T> at) {
        int index = at.index();
        listStorage(a).remove(index);
        return begin(a).plus(index);
    }
    static <T> JppIter<T> insert(List<T> a, JppIter<T> at, T value) {
        int index = at.index();
        listStorage(a).add(index, jppCopy(value));
        return begin(a).plus(index);
    }
    static <T> JppIter<T> begin(List<T> a) {
        List<T> storage = listStorage(a);
        if (storage instanceof JppDequeStorage) {
            JppDequeStorage<T> data = (JppDequeStorage<T>)storage;
            return new JppDequeIter<T>(data, data.first, 1);
        }
        return new JppIter<T>(storage, 0, 1);
    }
    static <T> JppIter<T> end(List<T> a) {
        return begin(a).plus(a.size());
    }
    static <T> JppIter<T> rbegin(List<T> a) {
        List<T> storage = listStorage(a);
        if (storage instanceof JppDequeStorage) {
            JppDequeStorage<T> data = (JppDequeStorage<T>)storage;
            return new JppDequeIter<T>(data, data.first + data.size() - 1, -1);
        }
        return new JppIter<T>(storage, a.size() - 1, -1);
    }
    static <T> JppIter<T> rend(List<T> a) {
        List<T> storage = listStorage(a);
        if (storage instanceof JppDequeStorage) {
            JppDequeStorage<T> data = (JppDequeStorage<T>)storage;
            return new JppDequeIter<T>(data, data.first - 1, -1);
        }
        return new JppIter<T>(storage, -1, -1);
    }
    static <T> T arrayInit(T a, T initial) {
        for (int i = 0; i < java.lang.reflect.Array.getLength(initial); ++i) {
            Object v = java.lang.reflect.Array.get(initial, i);
            Object old = java.lang.reflect.Array.get(a, i);
            if (v != null && v.getClass().isArray() && old != null)
                arrayInit(old, v);
            else
                java.lang.reflect.Array.set(a, i, v);
        }
        return a;
    }
    static <T> T objects(T a, java.util.function.Supplier<?> factory) {
        for (int i = 0; i < java.lang.reflect.Array.getLength(a); ++i) {
            Object item = java.lang.reflect.Array.get(a, i);
            if (item != null && item.getClass().isArray())
                objects(item, factory);
            else
                java.lang.reflect.Array.set(a, i, factory.get());
        }
        return a;
    }
    @SuppressWarnings("unchecked")
    static <T> T update(JppRef<T> ref, int delta, boolean post) {
        T old = ref.get();
        Object value;
        if (old instanceof Integer)
            value = (Integer)old + delta;
        else if (old instanceof Long)
            value = (Long)old + delta;
        else if (old instanceof Character)
            value = (char)((Character)old + delta);
        else if (old instanceof Double)
            value = (Double)old + delta;
        else if (old instanceof Float)
            value = (Float)old + delta;
        else if (old instanceof BigInteger)
            value = ((BigInteger)old).add(BigInteger.valueOf(delta));
        else
            throw new IllegalArgumentException("unsupported scalar update");
        ref.set((T)value);
        return post ? old : (T)value;
    }
    @SuppressWarnings({"rawtypes", "unchecked"})
    static int compare(Object a, Object b) {
        if (a instanceof List && b instanceof List) {
            List<?> x = (List<?>)a, y = (List<?>)b;
            for (int i = 0; i < Math.min(x.size(), y.size()); ++i) {
                int c = compare(x.get(i), y.get(i));
                if (c != 0)
                    return c;
            }
            return Integer.compare(x.size(), y.size());
        }
        if (a instanceof Number && b instanceof Number) {
            if (a instanceof Double || a instanceof Float || b instanceof Double ||
                b instanceof Float)
                return Double.compare(((Number)a).doubleValue(), ((Number)b).doubleValue());
            if (a instanceof BigInteger || b instanceof BigInteger)
                return new BigInteger(a.toString()).compareTo(new BigInteger(b.toString()));
            return Long.compare(((Number)a).longValue(), ((Number)b).longValue());
        }
        return ((Comparable)a).compareTo(b);
    }
    static <T> T min(T a, T b) {
        return compare(a, b) <= 0 ? a : b;
    }
    static <T> T min(T a, T b, java.util.function.BiPredicate<? super T, ? super T> less) {
        return less.test(b, a) ? b : a;
    }
    static <T> T max(T a, T b, java.util.function.BiPredicate<? super T, ? super T> less) {
        return less.test(a, b) ? b : a;
    }
    static <T> T min(List<T> values, java.util.function.BiPredicate<? super T, ? super T> less) {
        T result = values.get(0);
        for (int i = 1; i < values.size(); ++i)
            result = min(result, values.get(i), less);
        return result;
    }
    static <T> T max(List<T> values, java.util.function.BiPredicate<? super T, ? super T> less) {
        T result = values.get(0);
        for (int i = 1; i < values.size(); ++i)
            result = max(result, values.get(i), less);
        return result;
    }
    static <T> T clamp(T value, T low, T high,
                      java.util.function.BiPredicate<? super T, ? super T> less) {
        return less.test(value, low) ? low : less.test(high, value) ? high : value;
    }
    static <T> JppIter<T> orderIterator(JppIter<T> source, int direction) {
        JppOrderIter<T> iterator = (JppOrderIter<T>)source;
        return new JppOrderIter<T>(iterator.owner, iterator.node, direction);
    }
    static <S, T> JppOrderSet<T> orderSetFrom(JppOrderSet<T> destination, JppIter<S> first,
                                            JppIter<S> last,
                                            java.util.function.Function<S, T> convert) {
        for (JppIter<S> position = first.copy(); !position.same(last); position.advance(1))
            destination.insert(convert.apply(position.get()));
        return destination;
    }
    static <T> void swapOrderSets(JppOrderSet<T> first, JppOrderSet<T> second) {
        // PBDS declares a copy constructor and assignment, so generic std::swap
        // performs copies. Its member swap transfers nodes and has a separate path.
        JppOrderSet<T> temporary = first.copy();
        first.jppAssignFrom(second);
        second.jppAssignFrom(temporary);
    }
    static JppJumpTarget checkpoint(JppPtr<JppJumpBuffer> buffer) {
        JppJumpTarget target = new JppJumpTarget();
        buffer.get().target = target;
        return target;
    }
    static JppLongDouble readLongDouble(JppInput source, JppLongDouble previous) {
        String token = source.floatingToken();
        if (token == null)
            return previous;
        try {
            JppLongDouble value = JppLongDouble.parse(token);
            if (value.special == 1) {
                source.good = false;
                return JppLongDouble.MAX.copySign(value);
            }
            return value;
        } catch (NumberFormatException error) {
            source.good = false;
            return JppLongDouble.ZERO;
        }
    }
    static JppLongDouble longDoubleFrexp(JppLongDouble value, JppPtr<Integer> exponent) {
        int power = value.special != 0 || value.isZero() ? 0
            : value.power + value.significand.bitLength();
        exponent.put(power);
        return value.scaleB(-power);
    }
    static JppLongDouble updateLongDouble(JppRef<JppLongDouble> target, int amount, boolean post) {
        JppLongDouble previous = target.get();
        JppLongDouble value = amount > 0 ? previous.add(JppLongDouble.ONE)
                                       : previous.subtract(JppLongDouble.ONE);
        target.set(value);
        return post ? previous : value;
    }
    static void longJump(JppPtr<JppJumpBuffer> buffer, int value) {
        JppJumpTarget target = buffer.get().target;
        if (target == null || !target.active)
            throw new IllegalStateException("longjmp requires a live C++ checkpoint");
        throw new JppLongJump(target, value);
    }
    static <T> JppSet<T> setFrom(JppSet<T> destination, JppIter<T> first, JppIter<T> last) {
        for (JppIter<T> position = first.plus(0); !position.same(last); position.advance(1))
            destination.insert(position.get());
        return destination;
    }
    static <T> JppSet<T> setFrom(JppSet<T> destination, List<T> values) {
        for (T value : values)
            destination.insert(value);
        return destination;
    }
    static <T> T max(T a, T b) {
        return compare(a, b) >= 0 ? a : b;
    }
    static <T> T min(List<T> v) {
        return Collections.min(v, Jpp::compare);
    }
    static <T> T max(List<T> v) {
        return Collections.max(v, Jpp::compare);
    }
    static <T> T clamp(T x, T lo, T hi) {
        return min(max(x, lo), hi);
    }
    static long gcd(long a, long b) {
        while (b != 0) {
            long t = a % b;
            a = b;
            b = t;
        }
        return Math.abs(a);
    }
    static int gcd(int a, int b) {
        return (int)gcd((long)a, b);
    }
    static long lcm(long a, long b) {
        return a == 0 || b == 0 ? 0 : Math.abs(a / gcd(a, b) * b);
    }
    static int lcm(int a, int b) {
        return (int)lcm((long)a, b);
    }
    static JppString concat(Object a, Object b) {
        return new JppString(cString(a) + cString(b));
    }
    static <T> JppIter<T> begin(T[] a) {
        return new JppIter<T>(Arrays.asList(a), 0, 1, a);
    }
    static <T> JppIter<T> end(T[] a) {
        return begin(a).plus(a.length);
    }
    static JppIter<Character> begin(String a) {
        JppIter<Character> characters = begin(charArray(a, a.length() + 1));
        return new JppIter<Character>(characters.list, 0, 1, a);
    }
    @SuppressWarnings("unchecked")
    static <T> List<T> arrayView(final Object a) {
        return new AbstractList<T>() {
            public int size() {
                return java.lang.reflect.Array.getLength(a);
            }
            public T get(int i) {
                return (T)java.lang.reflect.Array.get(a, i);
            }
            public T set(int i, T v) {
                T old = get(i);
                java.lang.reflect.Array.set(a, i, v);
                return old;
            }
        };
    }
    static JppIter<Integer> begin(int[] a) {
        return new JppIter<Integer>(Jpp.<Integer>arrayView(a), 0, 1, a);
    }
    static JppIter<Long> begin(long[] a) {
        return new JppIter<Long>(Jpp.<Long>arrayView(a), 0, 1, a);
    }
    static JppIter<Double> begin(double[] a) {
        return new JppIter<Double>(Jpp.<Double>arrayView(a), 0, 1, a);
    }
    static JppIter<Character> begin(char[] a) {
        return new JppIter<Character>(Jpp.<Character>arrayView(a), 0, 1, a);
    }
    static JppIter<Boolean> begin(boolean[] a) {
        return new JppIter<Boolean>(Jpp.<Boolean>arrayView(a), 0, 1, a);
    }
    static JppIter<Float> begin(float[] a) {
        return new JppIter<Float>(Jpp.<Float>arrayView(a), 0, 1, a);
    }
    static JppIter<Float> end(float[] a) {
        return begin(a).plus(a.length);
    }
    static JppIter<Integer> end(int[] a) {
        return begin(a).plus(a.length);
    }
    static JppIter<Long> end(long[] a) {
        return begin(a).plus(a.length);
    }
    static JppIter<Double> end(double[] a) {
        return begin(a).plus(a.length);
    }
    static JppIter<Character> end(char[] a) {
        return begin(a).plus(a.length);
    }
    static JppIter<Boolean> end(boolean[] a) {
        return begin(a).plus(a.length);
    }
    static <T> void sortList(List<T> values, Comparator<? super T> comparison) {
        if (!values.isEmpty() && values.get(0) instanceof JppTrivialValue)
            sort(begin(values), end(values), (a, b) -> comparison.compare(a, b) < 0);
        else
            values.sort(comparison);
    }
    static <T> void reverseList(List<T> values) {
        if (!values.isEmpty() && values.get(0) instanceof JppTrivialValue)
            reverse(begin(values), end(values));
        else
            Collections.reverse(values);
    }
    static <T> void sort(JppIter<T> first, JppIter<T> last) {
        sort(first, last, (a, b) -> compare(a, b) < 0);
    }
    static <T> void sort(JppIter<T> first, JppIter<T> last,
                         java.util.function.BiPredicate<? super T, ? super T> less) {
        ArrayList<T> values = new ArrayList<T>();
        for (int i = first.index(); i != last.index(); i += first.step) {
            T value = first.list.get(i);
            values.add(value instanceof JppTrivialValue ? jppCopy(value) : value);
        }
        values.sort((a, b) -> less.test(a, b) ? -1 : less.test(b, a) ? 1 : 0);
        for (int i = 0, p = first.index(); i < values.size(); ++i, p += first.step) {
            T previous = first.list.get(p), value = values.get(i);
            first.list.set(p, previous instanceof JppTrivialValue ? assignValue(previous, value) : value);
        }
    }
    static <T> void reverse(JppIter<T> first, JppIter<T> last) {
        for (int l = first.index(), r = last.index() - first.step; (r - l) * first.step > 0;
             l += first.step, r -= first.step) {
            T left = first.list.get(l), right = first.list.get(r);
            if (left instanceof JppTrivialValue) {
                T saved = jppCopy(left);
                first.list.set(l, assignValue(left, right));
                first.list.set(r, assignValue(right, saved));
            } else
                Collections.swap(first.list, l, r);
        }
    }
    static <T> void fill(JppIter<T> first, JppIter<T> last, T value) {
        for (int i = first.index(); i != last.index(); i += first.step)
            first.list.set(i, assignValue(first.list.get(i), value));
    }
    static <T> void fill(List<T> values, T value) {
        fill(begin(values), end(values), value);
    }
    static <T, U> boolean lexicographicalLess(List<T> left, List<U> right,
                         java.util.function.BiPredicate<T, U> less,
                         java.util.function.BiPredicate<U, T> reverseLess) {
        for (int i = 0; i < Math.min(left.size(), right.size()); ++i) {
            if (less.test(left.get(i), right.get(i)))
                return true;
            if (reverseLess.test(right.get(i), left.get(i)))
                return false;
        }
        return left.size() < right.size();
    }
    static <T, U> JppIter<T> lower_bound(JppIter<T> first, JppIter<T> last, U value) {
        return lower_bound(first, last, value, (a, b) -> compare(a, b) < 0);
    }
    static <T, U> JppIter<T> lower_bound(JppIter<T> first, JppIter<T> last, U value,
                                      java.util.function.BiPredicate<? super T, ? super U> less) {
        int lo = 0, hi = last.minus(first);
        while (lo < hi) {
            int mid = (lo + hi) >>> 1;
            if (less.test(first.plus(mid).get(), value))
                lo = mid + 1;
            else
                hi = mid;
        }
        return first.plus(lo);
    }
    static <T, U> JppIter<T> upper_bound(JppIter<T> first, JppIter<T> last, U value) {
        return upper_bound(first, last, value, (a, b) -> compare(a, b) < 0);
    }
    static <T, U> JppIter<T> upper_bound(JppIter<T> first, JppIter<T> last, U value,
                                      java.util.function.BiPredicate<? super U, ? super T> less) {
        int lo = 0, hi = last.minus(first);
        while (lo < hi) {
            int mid = (lo + hi) >>> 1;
            if (!less.test(value, first.plus(mid).get()))
                lo = mid + 1;
            else
                hi = mid;
        }
        return first.plus(lo);
    }
    static <T> boolean binary_search(JppIter<T> first, JppIter<T> last, T value) {
        JppIter<T> p = lower_bound(first, last, value);
        return !p.same(last) && compare(p.get(), value) == 0;
    }
    static <T, U> boolean binary_search(JppIter<T> first, JppIter<T> last, U value,
                            java.util.function.BiPredicate<? super T, ? super U> less,
                            java.util.function.BiPredicate<? super U, ? super T> reverseLess) {
        JppIter<T> position = lower_bound(first, last, value, less);
        return !position.same(last) && !reverseLess.test(value, position.get());
    }
    static <T> JppIter<T> unique(JppIter<T> first, JppIter<T> last) {
        if (first.same(last))
            return last;
        int out = first.index();
        for (int i = first.index() + first.step; i != last.index(); i += first.step)
            if (compare(first.list.get(out), first.list.get(i)) != 0) {
                out += first.step;
                first.list.set(out, jppCopy(first.list.get(i)));
            }
        return first.plus((out - first.index()) * first.step + 1);
    }
    static <T> JppIter<T> find(JppIter<T> first, JppIter<T> last, T value) {
        JppIter<T> p = first.plus(0);
        while (!p.same(last) && !Objects.equals(p.get(), value))
            p.advance(1);
        return p;
    }
    static <T> int count(JppIter<T> first, JppIter<T> last, T value) {
        int n = 0;
        for (JppIter<T> p = first.plus(0); !p.same(last); p.advance(1))
            if (Objects.equals(p.get(), value))
                ++n;
        return n;
    }
    static <T> JppIter<T> min_element(JppIter<T> first, JppIter<T> last) {
        JppIter<T> best = first;
        for (JppIter<T> p = first.plus(0); !p.same(last); p.advance(1))
            if (compare(p.get(), best.get()) < 0)
                best = p.plus(0);
        return best;
    }
    static <T> JppIter<T> max_element(JppIter<T> first, JppIter<T> last) {
        JppIter<T> best = first;
        for (JppIter<T> p = first.plus(0); !p.same(last); p.advance(1))
            if (compare(p.get(), best.get()) > 0)
                best = p.plus(0);
        return best;
    }
    static <T extends Number> long accumulate(JppIter<T> first, JppIter<T> last, long init) {
        for (int i = first.index(); i != last.index(); i += first.step)
            init += first.list.get(i).longValue();
        return init;
    }
    static <T extends Number> int accumulate(JppIter<T> first, JppIter<T> last, int init) {
        for (int i = first.index(); i != last.index(); i += first.step)
            init += first.list.get(i).intValue();
        return init;
    }
    static <T extends Number> double accumulate(JppIter<T> first, JppIter<T> last, double init) {
        for (int i = first.index(); i != last.index(); i += first.step)
            init += first.list.get(i).doubleValue();
        return init;
    }
    static void iota(JppIter<Integer> first, JppIter<Integer> last, int value) {
        for (int i = first.index(); i != last.index(); i += first.step)
            first.list.set(i, value++);
    }
    static void iota(JppIter<Long> first, JppIter<Long> last, long value) {
        for (int i = first.index(); i != last.index(); i += first.step)
            first.list.set(i, value++);
    }
    static <T> boolean next_permutation(JppIter<T> first, JppIter<T> last) {
        int n = last.minus(first), i = n - 2;
        while (i >= 0 && compare(first.plus(i).get(), first.plus(i + 1).get()) >= 0)
            --i;
        if (i < 0) {
            reverse(first, last);
            return false;
        }
        int j = n - 1;
        while (compare(first.plus(i).get(), first.plus(j).get()) >= 0)
            --j;
        Collections.swap(first.list, first.plus(i).index(), first.plus(j).index());
        reverse(first.plus(i + 1), last);
        return true;
    }
    static int putchar(int c) {
        int octet = c & 255;
        jppOut.raw((char)octet);
        return octet;
    }
    static int exit(int status) {
        jppOut.flush();
        jppErr.flush();
        jppLog.flush();
        System.exit(status);
        return 0;
    }
    static int getchar() {
        return jppIn.readByte();
    }
    static int puts(Object s) {
        jppOut.raw(cString(s)).raw('\n');
        return 0;
    }
    static int ignore(Object... values) {
        return 0;
    }
    static JppInput input(JppInput source, Object... values) {
        return source;
    }
    static <T> T last(Object unused, T result) {
        return result;
    }
    static int trap() {
        throw new IllegalStateException("C++ trap or unreachable executed");
    }
    static int assertFail() {
        throw new AssertionError("C++ assertion failed");
    }
    static void check(boolean condition) {
        if (!condition)
            throw new AssertionError("C++ assertion failed");
    }
}
