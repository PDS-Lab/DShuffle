package pdsl.dpx.type;

import java.lang.reflect.*;
import java.util.*;

public class RelatedClassCollector {
    public static Set<Class<?>> collect(TypeTraits<?> mainTraits, InterfaceTypeMapping m) {
        Type t = mainTraits.getType();
        Set<Class<?>> relatedClasses = collect(t, m);
        for (Class<?> rc : relatedClasses) {
            if (rc.isEnum()) { // NOTICE: force instantiation
                rc.getEnumConstants();
            }
        }
        return relatedClasses;
    }

    private static Set<Class<?>> collect(Type mainType, InterfaceTypeMapping m) {
        Set<Class<?>> relatedClasses = new HashSet<Class<?>>();
        do_collect(mainType, m, relatedClasses);
        return relatedClasses;
    }

    private static void do_collect(Type t, InterfaceTypeMapping m, Set<Class<?>> result) {
        if (t instanceof ParameterizedType) { // Generic Type
            Class<?> c = (Class<?>) ((ParameterizedType) t).getRawType();
            if (c.isInterface()) { // TODO: we may need to handle abstract class
                if (m == null) {
                    throw new IllegalArgumentException("need mapping");
                }
                result.add(c); // NOTICE: interface not used
                Type mt = m.find(t).getType();
                do_collect(mt, m, result);
            } else {
                do_collect(c, m, result);
            }
            Type[] pts = ((ParameterizedType) t).getActualTypeArguments();
            for (Type pt : pts) {
                do_collect(pt, m, result);
            }
        } else if (t instanceof Class<?>) { // Simple Type
            Class<?> c = (Class<?>) t;
            if (c.isInterface()) {
                if (m == null) {
                    throw new IllegalArgumentException("need mapping");
                }
                // do mapping here
                result.add(c);
                Type mt = m.find(t).getType();
                do_collect(mt, m, result);
                return;
            }
            if (result.contains(c) || c.isPrimitive()) {
                return;
            }
            result.add(c);
            for (Field f : c.getDeclaredFields()) {
                if (Modifier.isStatic(f.getModifiers())) {
                    continue;
                }
                do_collect(f.getGenericType(), m, result);
            }
            if (c.isArray()) {
                do_collect(c.getComponentType(), m, result);
            }
        } else {
            throw new IllegalArgumentException("unsupported");
        }
    }
}
