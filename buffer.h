//
//  buffer.h
//  TacoQuest
//
//  Created by Thomas Foster on 5/26/25.
//

#ifndef buffer_h
#define buffer_h

#include "ints.h"
#include <stdbool.h>

#define BUFFER_CAPACITY 8192

typedef struct {
    U8 data[BUFFER_CAPACITY];
    size_t size; // Number of bytes currently in the buffer.
} Buffer;

/// Copy `size` bytes from `source` to the end of `buffer`.
/// - returns: Returns `false` if the write would result in an overflow.
/// (No operation is performed.)
size_t buffer_insert(Buffer* buffer, const void* source, size_t size);

/// Copy `size` bytes from the front of `buffer` to into `dest`, removing
/// data from the buffer in the process.
/// - returns: `false` if the read would result in an underflow. (No operation
/// is performed.)
size_t buffer_remove(Buffer* buffer, void* dest, size_t size);

#endif /* buffer_h */
