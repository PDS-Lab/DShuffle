#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int16_t class_id_t;
typedef uint16_t flag_t;
typedef int8_t basic_type_t;

// copy from jvm hotspot
#define T_BOOLEAN 4
#define T_CHAR 5
#define T_FLOAT 6
#define T_DOUBLE 7
#define T_BYTE 8
#define T_SHORT 9
#define T_INT 10
#define T_LONG 11

#define T_OBJECT 12
#define T_ARRAY 13

#define T_VOID 14
#define T_ADDRESS 15
#define T_NARROWOOP 16
#define T_METADATA 17
#define T_NARROWKLASS 18
#define T_CONFLICT 19
#define T_ILLEGAL 20

// NOTICE:
//  1. class id is all positive, the negtive means null
//  2. class id must be larger than legal basic type
#define MIN_CLASS_ID T_ILLEGAL
#define MAX_CLASS_ID INT16_MAX
#define UNREGISTERED_CLASS_ID -1

#define CTRL_FLAG_MASK ((flag_t)0x0F)
#define NULL_FLAG ((flag_t)0x1)
#define ENUM_FLAG ((flag_t)0x2)
#define REDIRECT_FLAG ((flag_t)0x3)

#define TYPE_FLAG_MASK ((flag_t)0xF0)
#define OBJECT_FLAG ((flag_t)0x10)
#define ARRAY_FLAG ((flag_t)0x20)

#ifdef __cplusplus
}
#endif
