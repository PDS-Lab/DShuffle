package pdsl.dpx.type;

import java.lang.reflect.*;
import java.util.Arrays;
import java.util.Objects;

/*
 * TypeTrait will save the all the information we want.
 * usage: new TypeTrait<Type-You-Want-To-Register>() {}
 */
public abstract class TypeTraits<T> {
    protected Type t;
    {
        Type superClass = getClass().getGenericSuperclass();
        t = ((ParameterizedType) superClass).getActualTypeArguments()[0];
    }

    public Type getType() {
        return t;
    }

    @Override
    public boolean equals(Object o) {
        if (o instanceof Type) {
            return equals(t, (Type) o);
        }
        return o instanceof TypeTraits<?> && equals(t, ((TypeTraits<?>) o).getType());
    }

    @Override
    public int hashCode() {
        return t.getTypeName().hashCode();
    }

    private boolean equals(Type a, Type b) {
        if (a == b) {
            return true;
        } else if (a instanceof Class) {
            return a.equals(b);
        } else if (a instanceof ParameterizedType) {
            if (!(b instanceof ParameterizedType)) {
                return false;
            }
            ParameterizedType pa = (ParameterizedType) a;
            ParameterizedType pb = (ParameterizedType) b;
            return Objects.equals(pa.getOwnerType(), pb.getOwnerType())
                    && pa.getRawType().equals(pb.getRawType())
                    && Arrays.equals(pa.getActualTypeArguments(), pb.getActualTypeArguments());

        } else {
            // unsupported
            return false;
        }
    }
}
