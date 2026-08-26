#ifndef TESTER_TYPES_H
#define TESTER_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TESTER_CONTACT_COUNT 24u
#define TESTER_END_COUNT 2u
#define TESTER_ENDPOINT_COUNT (TESTER_CONTACT_COUNT * TESTER_END_COUNT)
#define TESTER_ENDPOINT_BITMAP_BYTES ((TESTER_ENDPOINT_COUNT + 7u) / 8u)
#define TESTER_CONTACT_BITMAP_BYTES ((TESTER_CONTACT_COUNT + 7u) / 8u)

typedef enum {
    TESTER_END_J1 = 0,
    TESTER_END_J2 = 1
} tester_end_t;

typedef enum {
    TESTER_CONTACT_A1 = 0,
    TESTER_CONTACT_A2,
    TESTER_CONTACT_A3,
    TESTER_CONTACT_A4,
    TESTER_CONTACT_A5,
    TESTER_CONTACT_A6,
    TESTER_CONTACT_A7,
    TESTER_CONTACT_A8,
    TESTER_CONTACT_A9,
    TESTER_CONTACT_A10,
    TESTER_CONTACT_A11,
    TESTER_CONTACT_A12,
    TESTER_CONTACT_B1,
    TESTER_CONTACT_B2,
    TESTER_CONTACT_B3,
    TESTER_CONTACT_B4,
    TESTER_CONTACT_B5,
    TESTER_CONTACT_B6,
    TESTER_CONTACT_B7,
    TESTER_CONTACT_B8,
    TESTER_CONTACT_B9,
    TESTER_CONTACT_B10,
    TESTER_CONTACT_B11,
    TESTER_CONTACT_B12
} tester_contact_t;

typedef struct {
    uint8_t row[TESTER_ENDPOINT_COUNT][TESTER_ENDPOINT_BITMAP_BYTES];
} tester_matrix_t;

typedef struct {
    tester_matrix_t low;
    tester_matrix_t unstable;
} tester_observation_t;

static inline uint8_t tester_endpoint(tester_end_t end, tester_contact_t contact)
{
    return (uint8_t)(((uint8_t)end * TESTER_CONTACT_COUNT) + (uint8_t)contact);
}

static inline tester_contact_t tester_endpoint_contact(uint8_t endpoint)
{
    return (tester_contact_t)(endpoint % TESTER_CONTACT_COUNT);
}

static inline tester_end_t tester_endpoint_end(uint8_t endpoint)
{
    return (tester_end_t)(endpoint / TESTER_CONTACT_COUNT);
}

void tester_matrix_clear(tester_matrix_t *matrix);
void tester_matrix_fill(tester_matrix_t *matrix);
void tester_matrix_set(tester_matrix_t *matrix, uint8_t row, uint8_t column, bool value);
bool tester_matrix_get(const tester_matrix_t *matrix, uint8_t row, uint8_t column);
void tester_bitmap_set(uint8_t *bitmap, uint8_t bit, bool value);
bool tester_bitmap_get(const uint8_t *bitmap, uint8_t bit);
const char *tester_contact_name(tester_contact_t contact);
const char *tester_endpoint_name(uint8_t endpoint, char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif
