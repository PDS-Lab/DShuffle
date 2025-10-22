#pragma once

#include "sd/common/basic_type.h"
#include "util/logger.hxx"
#include "util/unreachable.hxx"

namespace dpx::sd {

inline bool is_primitive_type(basic_type_t t) { return t >= T_BOOLEAN && t <= T_LONG; }

inline bool is_reference_type(basic_type_t t) { return t >= T_OBJECT && t <= T_ARRAY; }

inline uint32_t type_size(uint16_t t) {
  switch (t) {
    case T_BOOLEAN:
    case T_BYTE:
      return 1;
    case T_CHAR:
    case T_SHORT:
      return 2;
    case T_INT:
    case T_FLOAT:
      return 4;
    case T_LONG:
    case T_DOUBLE:
      return 8;
    case T_OBJECT:
    case T_ARRAY:
      return 4;  // WARN we assume that CompressedOops is on
    case T_VOID:
    case T_ADDRESS:
    case T_NARROWOOP:
    case T_METADATA:
    case T_NARROWKLASS:
    case T_CONFLICT:
    case T_ILLEGAL:
      return 0;
  }
  unreachable();
}

inline basic_type_t char2type(char c) {
  switch (c) {
    case 'B':
      return T_BYTE;
    case 'C':
      return T_CHAR;
    case 'D':
      return T_DOUBLE;
    case 'F':
      return T_FLOAT;
    case 'I':
      return T_INT;
    case 'J':
      return T_LONG;
    case 'S':
      return T_SHORT;
    case 'Z':
      return T_BOOLEAN;
    case 'V':
      return T_VOID;
    case 'L':
      return T_OBJECT;
    case '[':
      return T_ARRAY;
  }
  unreachable();
}

inline const char *type2sig(basic_type_t t) {
  switch (t) {
    case T_BOOLEAN:
      return "Z";
    case T_CHAR:
      return "C";
    case T_FLOAT:
      return "F";
    case T_DOUBLE:
      return "D";
    case T_BYTE:
      return "B";
    case T_SHORT:
      return "S";
    case T_INT:
      return "I";
    case T_LONG:
      return "J";
    case T_OBJECT:
    case T_ARRAY:
    case T_VOID:
    case T_ADDRESS:
    case T_NARROWOOP:
    case T_METADATA:
    case T_NARROWKLASS:
    case T_CONFLICT:
    case T_ILLEGAL:
  }
  // INFO("{}", t);
  unreachable();
}

inline const char *type2str(basic_type_t t) {
  switch (t) {
    case T_BOOLEAN:
      return "Boolean";
    case T_CHAR:
      return "Char";
    case T_FLOAT:
      return "Float";
    case T_DOUBLE:
      return "Double";
    case T_BYTE:
      return "Byte";
    case T_SHORT:
      return "Short";
    case T_INT:
      return "Int";
    case T_LONG:
      return "Long";
    case T_OBJECT:
      return "Object";
    case T_ARRAY:
      return "Array";
    case T_VOID:
      // return "Void";
    case T_ADDRESS:
    case T_NARROWOOP:
    case T_METADATA:
    case T_NARROWKLASS:
    case T_CONFLICT:
    case T_ILLEGAL:
      return "unsupported";
  }
  // INFO("{}", t);
  unreachable();
}

inline bool is_null_f(flag_t f) { return (f & CTRL_FLAG_MASK) == NULL_FLAG; }
inline bool is_object_f(flag_t f) { return (f & TYPE_FLAG_MASK) == OBJECT_FLAG; }
inline bool is_array_f(flag_t f) { return (f & TYPE_FLAG_MASK) == ARRAY_FLAG; }
inline bool is_enum_f(flag_t f) { return is_object_f(f) && (f & CTRL_FLAG_MASK) == ENUM_FLAG; }
inline bool is_redirect_f(flag_t f) { return (f & CTRL_FLAG_MASK) == REDIRECT_FLAG; }

}  // namespace dpx::sd
