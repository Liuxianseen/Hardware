#ifndef CABLE_PROFILE_H
#define CABLE_PROFILE_H

#include "tester_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CABLE_KIND_DISCOVERY = 0,
    CABLE_KIND_USB2_UNMARKED,
    CABLE_KIND_USB2_EMARKED,
    CABLE_KIND_FULL_UNMARKED,
    CABLE_KIND_FULL_EMARKED,
    CABLE_KIND_STRAIGHT_24_FIXTURE,
    CABLE_KIND_AUTO,
    CABLE_KIND_COUNT
} cable_kind_t;

typedef struct {
    cable_kind_t kind;
    bool j1_flipped;
    bool j2_flipped;
    tester_matrix_t required;
    tester_matrix_t allowed;
} cable_profile_t;

bool cable_profile_build(
    cable_profile_t *profile,
    cable_kind_t kind,
    bool j1_flipped,
    bool j2_flipped);
const char *cable_kind_name(cable_kind_t kind);
bool cable_kind_parse(const char *text, cable_kind_t *kind);

#ifdef __cplusplus
}
#endif

#endif
