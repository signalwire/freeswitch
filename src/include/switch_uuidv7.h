/*
 * switch_uuidv7.h uuidv7
*/
#include <switch.h>

#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L

#include <stddef.h>
#include <stdint.h>


/**
 * Indicates that the `unix_ts_ms` passed was used because no preceding UUID was
 * specified.
 */
#define UUIDV7_STATUS_UNPRECEDENTED (0)

/**
 * Indicates that the `unix_ts_ms` passed was used because it was greater than
 * the previous one.
 */
#define UUIDV7_STATUS_NEW_TIMESTAMP (1)

/**
 * Indicates that the counter was incremented because the `unix_ts_ms` passed
 * was no greater than the previous one.
 */
#define UUIDV7_STATUS_COUNTER_INC (2)

/**
 * Indicates that the previous `unix_ts_ms` was incremented because the counter
 * reached its maximum value.
 */
#define UUIDV7_STATUS_TIMESTAMP_INC (3)

/**
 * Indicates that the monotonic order of generated UUIDs was broken because the
 * `unix_ts_ms` passed was less than the previous one by more than ten seconds.
 */
#define UUIDV7_STATUS_CLOCK_ROLLBACK (4)

/** Indicates that an invalid `unix_ts_ms` is passed. */
#define UUIDV7_STATUS_ERR_TIMESTAMP (-1)

/**
 * Indicates that the attempt to increment the previous `unix_ts_ms` failed
 * because it had reached its maximum value.
 */
#define UUIDV7_STATUS_ERR_TIMESTAMP_OVERFLOW (-2)


#ifdef __cplusplus
extern "C" {
#endif


/**
 * Generates a new UUIDv7 from the given Unix time, random bytes, and previous
 * UUID.
 *
 * @param uuid_out    16-byte byte array where the generated UUID is stored.
 * @param unix_ts_ms  Current Unix time in milliseconds.
 * @param rand_bytes  At least 10-byte byte array filled with random bytes. This
 *                    function consumes the leading 4 bytes or the whole 10
 *                    bytes per call depending on the conditions.
 *                    `uuidv7_status_n_rand_consumed()` maps the return value of
 *                    this function to the number of random bytes consumed.
 * @param uuid_prev   16-byte byte array representing the immediately preceding
 *                    UUID, from which the previous timestamp and counter are
 *                    extracted. This may be NULL if the caller does not care
 *                    the ascending order of UUIDs within the same timestamp.
 *                    This may point to the same location as `uuid_out`; this
 *                    function reads the value before writing.
 * @return            One of the `UUIDV7_STATUS_*` codes that describe the
 *                    characteristics of generated UUIDs. Callers can usually
 *                    ignore the status unless they need to guarantee the
 *                    monotonic order of UUIDs or fine-tune the generation
 *                    process.
 */

static inline int8_t uuidv7_generate(uint8_t *uuid_out, uint64_t unix_ts_ms,const uint8_t *rand_bytes,const uint8_t *uuid_prev) {
  int8_t status;
  uint64_t timestamp = 0;
  static const uint64_t MAX_TIMESTAMP = ((uint64_t)1 << 48) - 1;
  static const uint64_t MAX_COUNTER = ((uint64_t)1 << 42) - 1;

  if (unix_ts_ms > MAX_TIMESTAMP) {
    return UUIDV7_STATUS_ERR_TIMESTAMP;
  }


  if (uuid_prev == NULL) {
    status = UUIDV7_STATUS_UNPRECEDENTED;
    timestamp = unix_ts_ms;
  } else {
    for (int i = 0; i < 6; i++) {
      timestamp = (timestamp << 8) | uuid_prev[i];
    }

    if (unix_ts_ms > timestamp) {
      status = UUIDV7_STATUS_NEW_TIMESTAMP;
      timestamp = unix_ts_ms;
    } else if (unix_ts_ms + 10000 < timestamp) {
      // ignore prev if clock moves back by more than ten seconds
      status = UUIDV7_STATUS_CLOCK_ROLLBACK;
      timestamp = unix_ts_ms;
    } else {
      // increment prev counter
      uint64_t counter = uuid_prev[6] & 0x0f; // skip ver
      counter = (counter << 8) | uuid_prev[7];
      counter = (counter << 6) | (uuid_prev[8] & 0x3f); // skip var
      counter = (counter << 8) | uuid_prev[9];
      counter = (counter << 8) | uuid_prev[10];
      counter = (counter << 8) | uuid_prev[11];

      if (counter++ < MAX_COUNTER) {
        status = UUIDV7_STATUS_COUNTER_INC;
        uuid_out[6] = counter >> 38; // ver + bits 0-3
        uuid_out[7] = counter >> 30; // bits 4-11
        uuid_out[8] = counter >> 24; // var + bits 12-17
        uuid_out[9] = counter >> 16; // bits 18-25
        uuid_out[10] = counter >> 8; // bits 26-33
        uuid_out[11] = counter;      // bits 34-41
      } else {
        // increment prev timestamp at counter overflow
        status = UUIDV7_STATUS_TIMESTAMP_INC;
        timestamp++;
        if (timestamp > MAX_TIMESTAMP) {
          return UUIDV7_STATUS_ERR_TIMESTAMP_OVERFLOW;
        }
      }
    }
  }

  uuid_out[0] = timestamp >> 40;
  uuid_out[1] = timestamp >> 32;
  uuid_out[2] = timestamp >> 24;
  uuid_out[3] = timestamp >> 16;
  uuid_out[4] = timestamp >> 8;
  uuid_out[5] = timestamp;

  for (int i = (status == UUIDV7_STATUS_COUNTER_INC) ? 12 : 6; i < 16; i++) {
    uuid_out[i] = *rand_bytes++;
  }

  uuid_out[6] = 0x70 | (uuid_out[6] & 0x0f); // set ver
  uuid_out[8] = 0x80 | (uuid_out[8] & 0x3f); // set var

  return status;
}

/**
 * Determines the number of random bytes consumsed by `uuidv7_generate()` from
 * the `UUIDV7_STATUS_*` code returned.
 *
 * @param status  `UUIDV7_STATUS_*` code returned by `uuidv7_generate()`.
 * @return        `4` if `status` is `UUIDV7_STATUS_COUNTER_INC` or `10`
 *                otherwise.
 */
static inline int uuidv7_status_n_rand_consumed(int8_t status) {
  return status == UUIDV7_STATUS_COUNTER_INC ? 4 : 10;
}


/**
 * @name High-level APIs that require platform integration
 */

/**
 * Generates a new UUIDv7 with the current Unix time.
 *
 * This declaration defines the interface to generate a new UUIDv7 with the
 * current time, default random number generator, and global shared state
 * holding the previously generated UUID. Since this single-file library does
 * not provide platform-specific implementations, users need to prepare a
 * concrete implementation (if necessary) by integrating a real-time clock,
 * cryptographically strong random number generator, and shared state storage
 * available in the target platform.
 *
 * @param uuid_out  16-byte byte array where the generated UUID is stored.
 * @return          One of the `UUIDV7_STATUS_*` codes that describe the
 *                  characteristics of generated UUIDs or an
 *                  implementation-dependent code. Callers can usually ignore
 *                  the `UUIDV7_STATUS_*` code unless they need to guarantee the
 *                  monotonic order of UUIDs or fine-tune the generation
 *                  process. The implementation-dependent code must be out of
 *                  the range of `int8_t` and negative if it reports an error.
 */

SWITCH_DECLARE(int) uuidv7_new(uint8_t *uuid_out);


#ifdef __cplusplus
} /* extern "C" { */
#endif
