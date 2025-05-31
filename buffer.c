//
//  buffer.c
//  TacoQuest
//
//  Created by Thomas Foster on 5/26/25.
//

#include "buffer.h"
#include "assert.h"

#include <string.h>

size_t buffer_insert(Buffer* buffer, const void* source, size_t size) {

    size_t available = BUFFER_CAPACITY - buffer->size;

    if ( available < size ) {
        assert("buffer_write overflow");
        return 0;
    }

    memcpy(buffer->data + buffer->size, source, size);
    buffer->size += size;

    return size;
}

size_t buffer_remove(Buffer* buffer, void* dest, size_t size) {

    if ( buffer->size < size ) {
        assert("buffer_read underflow");
        return 0;
    }

    memcpy(dest, buffer->data, size);

    // Move all subsequent data to the front.
    memmove(buffer->data, buffer->data + size, buffer->size - size);
    buffer->size -= size;

    return size;
}

void buffer_clear(Buffer* buffer)
{
    buffer->size = 0;
}
