// Support code is embedded in the submission: no jars or native libraries.
interface JppCopyable<T> {
    T copy();
}

static abstract class JppRef<T> {
    abstract T get();
    abstract T set(T value);
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
        return new JppRef<T>() {
            T get() {
                return get.get();
            }
            T set(T v) {
                set.accept(v);
                return v;
            }
        };
    }
    static <O, T> JppRef<T> member(O object, java.util.function.Function<O, T> get,
                                   java.util.function.BiConsumer<O, T> set) {
        return of(() -> get.apply(object), v -> set.accept(object, v));
    }
    static <T> JppRef<T> at(List<T> a, int i) {
        return of(() -> a.get(i), v -> a.set(i, v));
    }
    static <T> JppRef<T> at(T[] a, int i) {
        return of(() -> a[i], v -> a[i] = v);
    }
    static <K, V> JppRef<V> at(JppMap<K, V> a, K i) {
        return of(() -> a.get(i), v -> a.put(i, v));
    }
    static JppRef<Integer> at(int[] a, int i) {
        return of(() -> a[i], v -> a[i] = v);
    }
    static JppRef<Long> at(long[] a, int i) {
        return of(() -> a[i], v -> a[i] = v);
    }
    static JppRef<Double> at(double[] a, int i) {
        return of(() -> a[i], v -> a[i] = v);
    }
    static JppRef<Float> at(float[] a, int i) {
        return of(() -> a[i], v -> a[i] = v);
    }
    static JppRef<Boolean> at(boolean[] a, int i) {
        return of(() -> a[i], v -> a[i] = v);
    }
    static JppRef<Character> at(char[] a, int i) {
        return of(() -> a[i], v -> a[i] = v);
    }
    static JppRef<Character> at(JppString a, int i) {
        return of(() -> a.get(i), v -> a.put(i, v));
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

static class JppPair<A, B> implements Comparable<JppPair<A, B>>, JppCopyable<JppPair<A, B>> {
    A first;
    B second;
    JppPair(A a, B b) {
        first = a;
        second = b;
    }
    public JppPair<A, B> copy() {
        return new JppPair<A, B>(jppCopy(first), jppCopy(second));
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

static class JppString
    implements Comparable<JppString>, Iterable<Character>, JppCopyable<JppString> {
    private final StringBuilder data;
    JppString() {
        data = new StringBuilder();
    }
    JppString(Object value) {
        data = new StringBuilder(String.valueOf(value));
    }
    JppString(int count, char value) {
        data = new StringBuilder(count);
        while (count-- > 0)
            data.append(value);
    }
    public JppString copy() {
        return new JppString(data);
    }
    int size() {
        return data.length();
    }
    int length() {
        return size();
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
        data.append(value);
        return this;
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
    JppString substr(int start) {
        return new JppString(data.substring(start));
    }
    JppString substr(int start, int length) {
        return new JppString(data.substring(start, Math.min(size(), start + length)));
    }
    long find(Object value) {
        int n = data.indexOf(value.toString());
        return n < 0 ? -1L : n;
    }
    long find(Object value, int from) {
        int n = data.indexOf(value.toString(), from);
        return n < 0 ? -1L : n;
    }
    JppIter<Character> begin() {
        return new JppIter<Character>(view(), 0, 1);
    }
    JppIter<Character> end() {
        return new JppIter<Character>(view(), size(), 1);
    }
    JppIter<Character> rbegin() {
        return new JppIter<Character>(view(), size() - 1, -1);
    }
    JppIter<Character> rend() {
        return new JppIter<Character>(view(), -1, -1);
    }
    private List<Character> view() {
        return new AbstractList<Character>() {
            public int size() {
                return data.length();
            }
            public Character get(int i) {
                return data.charAt(i);
            }
            public Character set(int i, Character c) {
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
    int pos;
    final int step;
    JppIter(List<T> values, int index, int direction) {
        list = values;
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
    JppIter<T> plus(int offset) {
        return new JppIter<T>(list, pos + offset * step, step);
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

// TreeMap supplies the ordering and logarithmic lookup; the cursor adds C++
// bidirectional-iterator behavior, including repeated values in a multiset.
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
        return other instanceof JppTreeIter && Objects.equals(key, ((JppTreeIter<?>)other).key) &&
            duplicate == ((JppTreeIter<?>)other).duplicate;
    }
}

static class JppMap<K, V> implements Iterable<JppPair<K, V>>, JppCopyable<JppMap<K, V>> {
    final Map<K, V> data;
    final java.util.function.Supplier<V> factory;
    JppMap(boolean ordered, java.util.function.Supplier<V> init) {
        data = ordered ? new TreeMap<K, V>(Jpp::compare) : new HashMap<K, V>();
        factory = init;
    }
    V get(K key) {
        if (!data.containsKey(key))
            data.put(jppCopy(key), factory.get());
        return data.get(key);
    }
    V at(K key) {
        if (!data.containsKey(key))
            throw new NoSuchElementException();
        return data.get(key);
    }
    V put(K key, V value) {
        data.put(jppCopy(key), value);
        return value;
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
        for (Map.Entry<K, V> e : data.entrySet())
            result.add(new JppPair<K, V>(e.getKey(), e.getValue()));
        return result;
    }
    public Iterator<JppPair<K, V>> iterator() {
        return entries().iterator();
    }
    JppIter<JppPair<K, V>> begin() {
        return Jpp.begin(entries());
    }
    JppIter<JppPair<K, V>> end() {
        return Jpp.end(entries());
    }
    JppIter<JppPair<K, V>> find(K key) {
        ArrayList<JppPair<K, V>> v = entries();
        for (int i = 0; i < v.size(); ++i)
            if (Objects.equals(v.get(i).first, key))
                return Jpp.begin(v).plus(i);
        return Jpp.end(v);
    }
    public JppMap<K, V> copy() {
        JppMap<K, V> result = new JppMap<K, V>(data instanceof TreeMap, factory);
        for (Map.Entry<K, V> e : data.entrySet())
            result.data.put(jppCopy(e.getKey()), jppCopy(e.getValue()));
        return result;
    }
}

static class JppSet<T> implements Iterable<T>, JppCopyable<JppSet<T>> {
    final NavigableMap<T, Integer> data = new TreeMap<T, Integer>(Jpp::compare);
    final boolean multiple;
    int count;
    JppSet(boolean multi) {
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
    public JppSet<T> copy() {
        JppSet<T> s = new JppSet<T>(multiple);
        s.data.putAll(data);
        s.count = count;
        return s;
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

static class JppInput {
    private final InputStream input = System.in;
    private final byte[] buffer = new byte[1 << 16];
    private int pos, size, saved = -2;
    boolean good = true;
    private int readByte() {
        if (saved != -2) {
            int c = saved;
            saved = -2;
            return c;
        }
        if (pos >= size) {
            try {
                size = input.read(buffer);
                pos = 0;
            } catch (IOException e) {
                throw new UncheckedIOException(e);
            }
            if (size < 0)
                return -1;
        }
        return buffer[pos++] & 255;
    }
    private int skip() {
        int c;
        do {
            c = readByte();
        } while (c >= 0 && c <= 32);
        return c;
    }
    boolean hasNext() {
        int c = skip();
        saved = c;
        good = c >= 0;
        return good;
    }
    long nextLong() {
        int c = skip();
        if (c < 0) {
            good = false;
            return 0;
        }
        boolean neg = c == '-';
        if (neg || c == '+')
            c = readByte();
        long n = 0;
        while (c >= '0' && c <= '9') {
            n = n * 10 + c - '0';
            c = readByte();
        }
        saved = c;
        return neg ? -n : n;
    }
    int nextInt() {
        return (int)nextLong();
    }
    char nextChar() {
        int c = skip();
        if (c < 0)
            good = false;
        return (char)c;
    }
    JppString nextString() {
        int c = skip();
        if (c < 0) {
            good = false;
            return new JppString();
        }
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        while (c > 32) {
            bytes.write(c);
            c = readByte();
        }
        saved = c;
        return new JppString(
            new String(bytes.toByteArray(), java.nio.charset.StandardCharsets.UTF_8));
    }
    double nextDouble() {
        return Double.parseDouble(nextString().toString());
    }
    JppString line() {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        int c;
        while ((c = readByte()) >= 0 && c != '\n')
            if (c != '\r')
                bytes.write(c);
        if (c < 0)
            good = false;
        return new JppString(
            new String(bytes.toByteArray(), java.nio.charset.StandardCharsets.UTF_8));
    }
}

static class JppOutput {
    private final PrintWriter output;
    int precision = 6;
    boolean fixed, boolalpha;
    JppOutput(OutputStream stream) {
        output = new PrintWriter(new BufferedWriter(
            new OutputStreamWriter(stream, java.nio.charset.StandardCharsets.UTF_8)));
    }
    JppOutput print(Object value) {
        if (value instanceof Boolean)
            output.print(boolalpha ? value : ((Boolean)value ? 1 : 0));
        else if (value instanceof Double || value instanceof Float) {
            String formatted = String.format(
                Locale.ROOT, fixed ? "%." + precision + "f" : "%." + Math.max(1, precision) + "g",
                value);
            if (!fixed) {
                int exp = formatted.indexOf('e');
                String tail = exp < 0 ? "" : formatted.substring(exp);
                String head = exp < 0 ? formatted : formatted.substring(0, exp);
                if (head.indexOf('.') >= 0) {
                    while (head.endsWith("0"))
                        head = head.substring(0, head.length() - 1);
                    if (head.endsWith("."))
                        head = head.substring(0, head.length() - 1);
                }
                formatted = head + tail;
            }
            output.print(formatted);
        } else
            output.print(value);
        return this;
    }
    JppOutput flush() {
        output.flush();
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
        fixed = on;
        return this;
    }
    JppOutput boolalpha(boolean on) {
        boolalpha = on;
        return this;
    }
    int printf(String format, Object... args) {
        format = format.replaceAll("%([0-9.+\\-# ]*)ll([diuoxX])", "%$1$2")
                     .replace("%ld", "%d")
                     .replace("%zu", "%d")
                     .replace("%Lf", "%f")
                     .replace("%u", "%d");
        String text = String.format(Locale.ROOT, format, args);
        output.print(text);
        return text.length();
    }
}

static final JppInput jppIn = new JppInput();
static final JppOutput jppOut = new JppOutput(System.out), jppErr = new JppOutput(System.err);

static class Jpp {
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
    static <T> Comparator<T> comparator(java.util.function.BiPredicate<T, T> less) {
        return (a, b) -> less.test(a, b) ? -1 : less.test(b, a) ? 1 : 0;
    }
    static <T> Comparator<T> heapComparator(java.util.function.BiPredicate<T, T> less) {
        return (a, b) -> less.test(a, b) ? 1 : less.test(b, a) ? -1 : 0;
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
        return new JppString(String.valueOf(a) + b);
    }
    static <T> JppIter<T> begin(T[] a) {
        return new JppIter<T>(Arrays.asList(a), 0, 1);
    }
    static <T> JppIter<T> end(T[] a) {
        return new JppIter<T>(Arrays.asList(a), a.length, 1);
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
        return new JppIter<Integer>(Jpp.<Integer>arrayView(a), 0, 1);
    }
    static JppIter<Long> begin(long[] a) {
        return new JppIter<Long>(Jpp.<Long>arrayView(a), 0, 1);
    }
    static JppIter<Double> begin(double[] a) {
        return new JppIter<Double>(Jpp.<Double>arrayView(a), 0, 1);
    }
    static JppIter<Character> begin(char[] a) {
        return new JppIter<Character>(Jpp.<Character>arrayView(a), 0, 1);
    }
    static JppIter<Boolean> begin(boolean[] a) {
        return new JppIter<Boolean>(Jpp.<Boolean>arrayView(a), 0, 1);
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
                         java.util.function.BiPredicate<T, T> less) {
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
            first.list.set(i, jppCopy(value));
    }
    static <T> JppIter<T> lower_bound(JppIter<T> first, JppIter<T> last, T value) {
        return lower_bound(first, last, value, (a, b) -> compare(a, b) < 0);
    }
    static <T> JppIter<T> lower_bound(JppIter<T> first, JppIter<T> last, T value,
                                      java.util.function.BiPredicate<T, T> less) {
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
    static <T> JppIter<T> upper_bound(JppIter<T> first, JppIter<T> last, T value) {
        int lo = 0, hi = last.minus(first);
        while (lo < hi) {
            int mid = (lo + hi) >>> 1;
            if (compare(value, first.plus(mid).get()) >= 0)
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
        System.exit(status);
        return 0;
    }
    static int getchar() {
        return jppIn.readByte();
    }
    static int puts(Object s) {
        jppOut.print(s).print('\n');
        return 0;
    }
    static int ignore(Object... values) {
        return 0;
    }
    static boolean input(Object... values) {
        return jppIn.good;
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
