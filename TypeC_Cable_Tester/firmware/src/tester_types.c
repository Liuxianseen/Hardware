#include "tester_types.h"

#include <stdio.h>
#include <string.h>

static const char *const contact_names[TESTER_CONTACT_COUNT] = {
    "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "A10", "A11", "A12",
    "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "B10", "B11", "B12"
};

void tester_matrix_clear(tester_matrix_t *matrix)
{
    if (matrix != NULL) {
        memset(matrix, 0, sizeof(*matrix));
    }
}

void tester_matrix_fill(tester_matrix_t *matrix)
{
    if (matrix != NULL) {
        memset(matrix, 0xFF, sizeof(*matrix));
    }
}

void tester_matrix_set(tester_matrix_t *matrix, uint8_t row, uint8_t column, bool value)
{
    uint8_t mask;

    if ((matrix == NULL) || (row >= TESTER_ENDPOINT_COUNT) || (column >= TESTER_ENDPOINT_COUNT)) {
        return;
    }
    mask = (uint8_t)(1u << (column & 7u));
    if (value) {
        matrix->row[row][column >> 3u] |= mask;
    } else {
        matrix->row[row][column >> 3u] &= (uint8_t)~mask;
    }
}

bool tester_matrix_get(const tester_matrix_t *matrix, uint8_t row, uint8_t column)
{
    if ((matrix == NULL) || (row >= TESTER_ENDPOINT_COUNT) || (column >= TESTER_ENDPOINT_COUNT)) {
        return false;
    }
    return (matrix->row[row][column >> 3u] & (uint8_t)(1u << (column & 7u))) != 0u;
}

void tester_bitmap_set(uint8_t *bitmap, uint8_t bit, bool value)
{
    uint8_t mask;

    if (bitmap == NULL) {
        return;
    }
    mask = (uint8_t)(1u << (bit & 7u));
    if (value) {
        bitmap[bit >> 3u] |= mask;
    } else {
        bitmap[bit >> 3u] &= (uint8_t)~mask;
    }
}

bool tester_bitmap_get(const uint8_t *bitmap, uint8_t bit)
{
    if (bitmap == NULL) {
        return false;
    }
    return (bitmap[bit >> 3u] & (uint8_t)(1u << (bit & 7u))) != 0u;
}

const char *tester_contact_name(tester_contact_t contact)
{
    if ((uint8_t)contact >= TESTER_CONTACT_COUNT) {
        return "?";
    }
    return contact_names[(uint8_t)contact];
}

const char *tester_endpoint_name(uint8_t endpoint, char *buffer, size_t buffer_size)
{
    const char *end_name;

    if ((buffer == NULL) || (buffer_size == 0u) || (endpoint >= TESTER_ENDPOINT_COUNT)) {
        return "?";
    }
    end_name = (tester_endpoint_end(endpoint) == TESTER_END_J1) ? "J1." : "J2.";
    (void)snprintf(buffer, buffer_size, "%s%s", end_name, tester_contact_name(tester_endpoint_contact(endpoint)));
    return buffer;
}
