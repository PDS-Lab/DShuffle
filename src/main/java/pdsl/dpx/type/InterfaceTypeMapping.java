package pdsl.dpx.type;

import java.util.HashMap;
import java.util.Map;
import java.lang.reflect.Type;

// NOTICE:
// something like List<String> to ArrayList<String>
public class InterfaceTypeMapping {
    public Map<TypeTraits<?>, TypeTraits<?>> m = new HashMap<TypeTraits<?>, TypeTraits<?>>();

    private static class TypeTraitsQueryHandle extends TypeTraits<Void> {
        TypeTraitsQueryHandle(Type t) {
            this.t = t;
        }
    }

    public void add(TypeTraits<?> i, TypeTraits<?> o) {
        m.put(i, o);
    }

    public TypeTraits<?> find(Type i) {
        return m.get(new TypeTraitsQueryHandle(i));
    }
}
