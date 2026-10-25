// Support code is embedded in the submission: no jars or native libraries.
interface JppCopyable<T> {
    T copy();
}

interface JppAssignable<T> extends JppCopyable<T> {
    T jppAssignFrom(T source);
}
interface JppUninitialized {}

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

static class JppPair<A, B> implements Comparable<JppPair<A, B>>, JppAssignable<JppPair<A, B>> {
    A first;
    B second;
    JppPair(A a, B b) {
        first = a;
        second = b;
    }
    public JppPair<A, B> copy() {
        return new JppPair<A, B>(jppCopy(first), jppCopy(second));
    }
    public JppPair<A, B> jppAssignFrom(JppPair<A, B> source) {
        first = Jpp.assignValue(first, source.first);
        second = Jpp.assignValue(second, source.second);
        return this;
    }
    public int compareTo(JppPair<A, B> other) {
        int c = Jpp.compare(first, other.first);
        return c != 0 ? c : Jpp.compare(second, other.second);
    }
    public boolean equals(Object o) {
        if (!(o instanceof JppPair))
            return false;
        JppPair<?, ?> p = (JppPair<?, ?>)o;
        return Objects.equals(first, p.first) && Objects.equals(second, p.second);
    }
    public int hashCode() {
        return Objects.hash(first, second);
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
    JppPtr(List<T> values, int index, Object identity) {
        super(values, index, 1, identity);
    }
    JppPtr<T> plus(int offset) {
        return new JppPtr<T>(list, pos + offset, storage);
    }
    boolean same(JppIter<?> other) {
        if (other == null)
            return false;
        if (pos == other.pos && (storage == other.storage ||
                                 (storage instanceof JppAddress && storage.equals(other.storage))))
            return true;
        // A method's this pointer and an array element can name the same record.
        return list != null && other.list != null && pos >= 0 && pos < list.size() &&
            other.pos >= 0 && other.pos < other.list.size() && get() instanceof JppCopyable &&
            get() == other.get();
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

static class JppMapEntry<K, V> extends JppPair<K, V> {
    JppMap<K, V> owner;
    JppMapEntry(JppMap<K, V> map, K key, V value) {
        super(key, value);
        owner = map;
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
            current == null ? map : current.owner, current, step);
        copy.unordered = unordered;
        copy.unorderedIndex = unorderedIndex;
        return copy.advance(offset);
    }
    JppIter<JppPair<K, V>> advance(int offset) {
        int direction = offset < 0 ? -step : step;
        for (long n = Math.abs((long)offset); n > 0; --n) {
            if (current != null)
                map = current.owner;
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
        map = current.owner;
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
                entry.getValue().owner = this;
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
            for (JppMapEntry<K, V> entry : data.values())
                entry.owner = this;
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

static class JppDeque<T> implements Iterable<T>, JppCopyable<JppDeque<T>> {
    final ArrayDeque<T> data = new ArrayDeque<T>();
    void push(T value) {
        data.addLast(jppCopy(value));
    }
    void push_back(T value) {
        push(value);
    }
    void push_front(T value) {
        data.addFirst(jppCopy(value));
    }
    void pop() {
        data.removeFirst();
    }
    void pop_front() {
        data.removeFirst();
    }
    void pop_back() {
        data.removeLast();
    }
    T front() {
        return data.getFirst();
    }
    T back() {
        return data.getLast();
    }
    T top() {
        return data.getLast();
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
    public Iterator<T> iterator() {
        return data.iterator();
    }
    public JppDeque<T> copy() {
        JppDeque<T> d = new JppDeque<T>();
        for (T v : data)
            d.push(v);
        return d;
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
        if (value instanceof Boolean)
            output.print(boolalpha ? value : ((Boolean)value ? 1 : 0));
        else if (value instanceof Double || value instanceof Float) {
            String special = Jpp.nonfinite(((Number)value).doubleValue(), false, false, false);
            if (special != null) {
                output.print(special);
                if (unitBuffered)
                    output.flush();
                return this;
            }
            double number = ((Number)value).doubleValue();
            if (Double.doubleToRawLongBits(number) < 0)
                output.print('-');
            output.print(Jpp.floating(Math.abs(number), formatMode,
                                      formatMode == 'a' ? -1 : precision, false));
        } else
            output.print(value);
        if (unitBuffered)
            output.flush();
        return this;
    }
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
        print('\n');
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
        return at == null ? null : new JppPtr<T>(at.pointerStorage(), at.pos, at.pointerIdentity());
    }
    static <T> JppPtr<T> address(JppRef<T> value, Object object, Object field) {
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
    static <T> JppPtr<T> pointerValue(T value) {
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
        return left == null ? right == null : pointer(left).same(right);
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
    static <T> int swap(JppRef<T> a, JppRef<T> b) {
        T value = a.get();
        a.set(b.get());
        b.set(value);
        return 0;
    }
    static int isdigit(int c) {
        return c >= '0' && c <= '9' ? 1 : 0;
    }
    static <T> Comparator<T> comparator(java.util.function.BiPredicate<? super T, ? super T> less) {
        return (a, b) -> less.test(a, b) ? -1 : less.test(b, a) ? 1 : 0;
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
    static <T> void resizeDefault(ArrayList<T> a, int n, java.util.function.Supplier<T> factory) {
        if (n < a.size())
            a.subList(n, a.size()).clear();
        while (a.size() < n)
            a.add(factory.get());
    }
    static <T> void resize(ArrayList<T> a, int n, T value) {
        resizeDefault(a, n, () -> jppCopy(value));
    }
    static <T> void assign(ArrayList<T> a, int n, T value) {
        a.clear();
        resizeDefault(a, n, () -> jppCopy(value));
    }
    static <T> JppIter<T> erase(ArrayList<T> a, JppIter<T> first, JppIter<T> last) {
        a.subList(first.pos, last.pos).clear();
        return first;
    }
    static <T> JppIter<T> erase(ArrayList<T> a, JppIter<T> at) {
        a.remove(at.pos);
        return at;
    }
    static <T> JppIter<T> insert(ArrayList<T> a, JppIter<T> at, T value) {
        a.add(at.pos, jppCopy(value));
        return at;
    }
    static <T> JppIter<T> begin(List<T> a) {
        return new JppIter<T>(a, 0, 1);
    }
    static <T> JppIter<T> end(List<T> a) {
        return new JppIter<T>(a, a.size(), 1);
    }
    static <T> JppIter<T> rbegin(List<T> a) {
        return new JppIter<T>(a, a.size() - 1, -1);
    }
    static <T> JppIter<T> rend(List<T> a) {
        return new JppIter<T>(a, -1, -1);
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
    static <T> void sort(JppIter<T> first, JppIter<T> last) {
        sort(first, last, (a, b) -> compare(a, b) < 0);
    }
    static <T> void sort(JppIter<T> first, JppIter<T> last,
                         java.util.function.BiPredicate<? super T, ? super T> less) {
        ArrayList<T> values = new ArrayList<T>();
        for (int i = first.pos; i != last.pos; i += first.step)
            values.add(first.list.get(i));
        values.sort((a, b) -> less.test(a, b) ? -1 : less.test(b, a) ? 1 : 0);
        for (int i = 0, p = first.pos; i < values.size(); ++i, p += first.step)
            first.list.set(p, values.get(i));
    }
    static <T> void reverse(JppIter<T> first, JppIter<T> last) {
        for (int l = first.pos, r = last.pos - first.step; (r - l) * first.step > 0;
             l += first.step, r -= first.step)
            Collections.swap(first.list, l, r);
    }
    static <T> void fill(JppIter<T> first, JppIter<T> last, T value) {
        for (int i = first.pos; i != last.pos; i += first.step)
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
        int out = first.pos;
        for (int i = first.pos + first.step; i != last.pos; i += first.step)
            if (compare(first.list.get(out), first.list.get(i)) != 0) {
                out += first.step;
                first.list.set(out, jppCopy(first.list.get(i)));
            }
        return new JppIter<T>(first.list, out + first.step, first.step);
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
        for (int i = first.pos; i != last.pos; i += first.step)
            init += first.list.get(i).longValue();
        return init;
    }
    static <T extends Number> int accumulate(JppIter<T> first, JppIter<T> last, int init) {
        for (int i = first.pos; i != last.pos; i += first.step)
            init += first.list.get(i).intValue();
        return init;
    }
    static <T extends Number> double accumulate(JppIter<T> first, JppIter<T> last, double init) {
        for (int i = first.pos; i != last.pos; i += first.step)
            init += first.list.get(i).doubleValue();
        return init;
    }
    static void iota(JppIter<Integer> first, JppIter<Integer> last, int value) {
        for (int i = first.pos; i != last.pos; i += first.step)
            first.list.set(i, value++);
    }
    static void iota(JppIter<Long> first, JppIter<Long> last, long value) {
        for (int i = first.pos; i != last.pos; i += first.step)
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
        Collections.swap(first.list, first.plus(i).pos, first.plus(j).pos);
        reverse(first.plus(i + 1), last);
        return true;
    }
    static int putchar(int c) {
        jppOut.print((char)c);
        return c;
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
        jppOut.print(cString(s)).print('\n');
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
