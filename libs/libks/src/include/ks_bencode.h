/*
 * libbencodetools
 *
 * Written by Heikki Orsila <heikki.orsila@iki.fi> and
 * Janne Kulmala <janne.t.kulmala@tut.fi> in 2011.
 */

#ifndef TYPEVALIDATOR_BENCODE_H
#define TYPEVALIDATOR_BENCODE_H

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Used to verify format strings in compile time */
#ifdef __GNUC__
#define BEN_CHECK_FORMAT(...)  __attribute__ ((format( __VA_ARGS__ )))
#else
#define BEN_CHECK_FORMAT(args)
#endif

enum {
	BENCODE_BOOL = 1,
	BENCODE_DICT,
	BENCODE_INT,
	BENCODE_LIST,
	BENCODE_STR,
	BENCODE_USER,
};

enum {
	BEN_OK = 0, /* No errors. Set to zero. Non-zero implies an error. */
	BEN_INVALID,      /* Invalid data was given to decoder */
	BEN_INSUFFICIENT, /* Insufficient amount of data for decoding */
	BEN_NO_MEMORY,    /* Memory allocation failed */
	BEN_MISMATCH,     /* A given structure did not match unpack format */
};

struct bencode {
	char type;
};

struct bencode_bool {
	char type;
	char b;
};

struct bencode_dict_node {
	long long hash;
	struct bencode *key;
	struct bencode *value;
	size_t next;
};

struct bencode_dict {
	char type;
	char shared; /* non-zero means that the internal data is shared with
			other instances and should not be freed */
	size_t n;
	size_t alloc;
	size_t *buckets;
	struct bencode_dict_node *nodes;
};

struct bencode_int {
	char type;
	long long ll;
};

struct bencode_list {
	char type;
	char shared; /* non-zero means that the internal data is shared with
			other instances and should not be freed */
	size_t n;
	size_t alloc;
	struct bencode **values;
};

struct bencode_str {
	char type;
	size_t len;
	char *s;
};

struct ben_decode_ctx;
struct ben_encode_ctx;

struct bencode_type {
	size_t size;
	struct bencode *(*decode) (struct ben_decode_ctx *ctx);
	int (*encode) (struct ben_encode_ctx *ctx, const struct bencode *b);
	size_t (*get_size) (const struct bencode *b);
	void (*free) (struct bencode *b);
	int (*cmp) (const struct bencode *a, const struct bencode *b);
};

struct bencode_user {
	char type;
	struct bencode_type *info;
};

struct bencode_error {
	int error;  /* 0 if no errors */
	int line;   /* Error line: 0 is the first line */
	size_t off; /* Error offset in bytes from the start */
};

/* Allocate an instance of a user-defined type */
void *ben_alloc_user(struct bencode_type *type);

/*
 * Try to set capacity of a list or a dict to 'n' objects.
 * The function does nothing if 'n' is less than or equal to the number of
 * objects in 'b'. That is, nothing happens if n <= ben_{dict|list}_len(b).
 *
 * This function is used only for advice. The implementation need not obey it.
 *
 * The function returns 0 if the new capacity is used, otherwise -1.
 *
 * Note: This can be used to make construction of lists and dicts
 * more efficient when the number of inserted items is known in advance.
 */
int ben_allocate(struct bencode *b, size_t n);

/*
 * Returns an identical but a separate copy of structure b. Returns NULL if
 * there is no memory to make a copy. The copy is made recursively.
 */
struct bencode *ben_clone(const struct bencode *b);

/*
 * Returns a weak reference copy of structure b. Only a minimum amount of
 * data is copied because the returned structure references to the same
 * internal data as the original structure. As a result, the original
 * structure must remain valid until the copy is destroyed.
 *
 * This function is used for optimization for special cases.
 */
struct bencode *ben_shared_clone(const struct bencode *b);

/*
 * ben_cmp() is similar to strcmp(). It compares integers, strings and lists
 * similar to Python. User-defined types can be also compared.
 * Note: an integer is always less than a string.
 *
 * ben_cmp(a, b) returns a negative value if "a < b", 0 if "a == b",
 * or a positive value if "a > b".
 *
 * Algorithm for comparing dictionaries is:
 * If 'a' and 'b' have different number of keys or keys have different values,
 * a non-zero value is returned. Otherwise, they have the exact same keys
 * and comparison is done in ben_cmp() order of keys. The value for each key
 * is compared, and the first inequal value (ben_cmp() != 0) defines the
 * return value of the comparison.
 *
 * Note: recursive dictionaries in depth have the same issues.
 */
int ben_cmp(const struct bencode *a, const struct bencode *b);

/* Same as ben_cmp(), but the second argument is a C string */
int ben_cmp_with_str(const struct bencode *a, const char *s);

/*
 * Comparison function suitable for qsort(). Uses ben_cmp(), so this can be
 * used to order both integer and string arrays.
 */
int ben_cmp_qsort(const void *a, const void *b);

/*
 * Decode 'data' with 'len' bytes of data. Returns NULL on error.
 * The encoded data must be exactly 'len' bytes (not less), otherwise NULL
 * is returned. ben_decode2() function supports partial decoding ('len' is
 * larger than actual decoded message) and gives more accurate error reports.
 */
struct bencode *ben_decode(const void *data, size_t len);

/*
 * Same as ben_decode(), but allows one to set start offset for decoding with
 * 'off' and reports errors more accurately.
 *
 * '*off' must point to decoding start offset inside 'data'.
 * If decoding is successful, '*off' is updated to point to the next byte
 * after the decoded message.
 *
 * If 'error != NULL', it is updated according to the success and error of
 * the decoding. BEN_OK is success, BEN_INVALID means invalid data.
 * BEN_INSUFFICIENT means data is invalid but could be valid if more data
 * was given for decoding. BEN_NO_MEMORY means decoding ran out of memory.
 */
struct bencode *ben_decode2(const void *data, size_t len, size_t *off, int *error);

/*
 * Same as ben_decode2(), but allows one to define user types.
 */
struct bencode *ben_decode3(const void *data, size_t len, size_t *off, int *error, struct bencode_type *types[128]);

/*
 * Same as ben_decode(), but decodes data encoded with ben_print(). This is
 * whitespace tolerant, so intended Python syntax can also be read.
 * The decoder skips comments that begin with a '#' character.
 * The comment starts from '#' character and ends at the end of the same line.
 *
 * For example, this can be used to read in config files written as a Python
 * dictionary.
 *
 * ben_decode_printed2() fills information about the error in
 * struct bencode_error.
 * error->error is 0 on success, otherwise it is an error code
 * (see ben_decode2()).
 * error->line is the line number where error occured.
 * error->off is the byte offset of error (approximation).
 */
struct bencode *ben_decode_printed(const void *data, size_t len);
struct bencode *ben_decode_printed2(const void *data, size_t len, size_t *off, struct bencode_error *error);

/* Get the serialization size of bencode structure 'b' */
size_t ben_encoded_size(const struct bencode *b);

/* encode 'b'. Return encoded data with a pointer, and length in '*len' */
void *ben_encode(size_t *len, const struct bencode *b);

/*
 * encode 'b' into 'data' buffer with at most 'maxlen' bytes.
 * Returns the size of encoded data.
 */
size_t ben_encode2(char *data, size_t maxlen, const struct bencode *b);

/*
 * You must use ben_free() for all allocated bencode structures after use.
 * If b == NULL, ben_free does nothing.
 *
 * ben_free() frees all the objects contained within the bencoded structure.
 * It recursively iterates over lists and dictionaries and frees objects.
 */
void ben_free(struct bencode *b);

long long ben_str_hash(const struct bencode *b);
long long ben_int_hash(const struct bencode *b);
long long ben_hash(const struct bencode *b);

/* Create a string from binary data with len bytes */
struct bencode *ben_blob(const void *data, size_t len);

/* Create a boolean from integer */
struct bencode *ben_bool(int b);

/* Create an empty dictionary */
struct bencode *ben_dict(void);

/*
 * Try to locate 'key' in dictionary. Returns the associated value, if found.
 * Returns NULL if the key does not exist.
 */
struct bencode *ben_dict_get(const struct bencode *d, const struct bencode *key);

struct bencode *ben_dict_get_by_str(const struct bencode *d, const char *key);
struct bencode *ben_dict_get_by_int(const struct bencode *d, long long key);

struct bencode_keyvalue {
	struct bencode *key;
	struct bencode *value;
};

/*
 * Returns an array of key-value pairs in key order as defined by ben_cmp().
 * Array elements are struct bencode_keyvalue members. Returns NULL if
 * the array can not be allocated or the bencode object is not a dictionary.
 * The returned array must be freed by using free(). The length of the
 * array can be determined with ben_dict_len(d).
 *
 * Warning: key and value pointers in the array are pointers to exact same
 * objects in the dictionary. Therefore, the dictionary and its key-values
 * must exist while the same keys and values are accessed from the array.
 */
struct bencode_keyvalue *ben_dict_ordered_items(const struct bencode *d);

/*
 * Try to locate 'key' in dictionary. Returns the associated value, if found.
 * The value must be later freed with ben_free(). Returns NULL if the key
 * does not exist.
 */
struct bencode *ben_dict_pop(struct bencode *d, const struct bencode *key);

struct bencode *ben_dict_pop_by_str(struct bencode *d, const char *key);
struct bencode *ben_dict_pop_by_int(struct bencode *d, long long key);

/*
 * Set 'key' in dictionary to be 'value'. An old value exists for the key
 * is freed if it exists. 'key' and 'value' are owned by the dictionary
 * after a successful call (one may not call ben_free() for 'key' or
 * 'value'). One may free 'key' and 'value' if the call is unsuccessful.
 *
 * Returns 0 on success, -1 on failure (no memory).
 */
int ben_dict_set(struct bencode *d, struct bencode *key, struct bencode *value);

/* Same as ben_dict_set(), but the key is a C string */
int ben_dict_set_by_str(struct bencode *d, const char *key, struct bencode *value);

/* Same as ben_dict_set(), but the key and value are C strings */
int ben_dict_set_str_by_str(struct bencode *d, const char *key, const char *value);

struct bencode *ben_int(long long ll);

/* Create an empty list */
struct bencode *ben_list(void);

/*
 * Append 'b' to 'list'. Returns 0 on success, -1 on failure (no memory).
 * One may not call ben_free(b) after a successful call, because the list owns
 * the object 'b'.
 */
int ben_list_append(struct bencode *list, struct bencode *b);

int ben_list_append_str(struct bencode *list, const char *s);
int ben_list_append_int(struct bencode *list, long long ll);

/* Remove and return value at position 'pos' in list */
struct bencode *ben_list_pop(struct bencode *list, size_t pos);

/*
 * Returns a Python formatted C string representation of 'b' on success,
 * NULL on failure. The returned string should be freed with free().
 *
 * Note: The string is terminated with '\0'. All instances of '\0' bytes in
 * the bencoded data are escaped so that there is only one '\0' byte
 * in the generated string at the end.
 */
char *ben_print(const struct bencode *b);

/* Create a string from C string (note bencode string may contain '\0'. */
struct bencode *ben_str(const char *s);

/* Return a human readable explanation of error returned with ben_decode2() */
const char *ben_strerror(int error);

/*
 * Unpack a Bencoded structure similar to scanf(). Takes a format string and
 * a list of pointers as variable arguments. The given b structure is checked
 * against the format and values are unpacked using the given specifiers.
 * A specifier begins with a percent (%) that follows a string of specifier
 * characters documented below.
 * The syntax is similar to Python format for recursive data structures, and
 * consists of tokens {, }, [, ] with any number of spaces between them.
 * The keys of a dictionary are given as literal strings or integers and
 * matched against the keys of the Bencoded structure.
 *
 * Unpack modifiers:
 *     l    The integer is of type long or unsigned long, and the type of the
 *          argument is expected to be long * or unsigned long *.
 *     ll   The integer is a long long or an unsigned long long, and the
 *          argument is long long * or unsigned long long *.
 *     L    Same as ll.
 *     q    Same as ll.
 *
 * Unpack specifiers:
 *     %ps  The Bencode value must be a string and a pointer to a string
 *          (char **) is expected to be given as arguments. Note, returns a
 *          reference to the internal string buffer. The returned memory should
 *          not be freed and it has the same life time as the Bencode string.
 *
 *     %pb  Takes any structure and writes a pointer given as an argument.
 *          The argument is expected to be "struct bencode **". Note, returns a
 *          reference to the value inside the structure passed to ben_unpack().
 *          The returned memory should not be freed and it has the same life
 *          time as the original structure.
 *
 *     %d   The bencode value is expected to be a (signed) integer. The
 *          preceeding conversion modifiers define the type of the given
 *          pointer.

 *     %u   The bencode value is expected to be an unsigned integer. The
 *          preceeding conversion modifiers define the type of the given
 *          pointer.
 */
int ben_unpack(const struct bencode *b, const char *fmt, ...)
	BEN_CHECK_FORMAT(scanf, 2, 3);

int ben_unpack2(const struct bencode *b, size_t *off, struct bencode_error *error, const char *fmt, ...)
	BEN_CHECK_FORMAT(scanf, 4, 5);

/*
 * Pack a Bencoded structure similar to printf(). Takes a format string and
 * a list of values as variable arguments.
 * Works similarly to ben_decode_printed(), but allows the string to values
 * specifiers which are replaced with values given as arguments.
 * A specifier begins with a percent (%) that follows a string of specifier
 * characters documented below.
 *
 * Value modifiers:
 *     l    The integer is of type long or unsigned long.
 *     ll   The integer is a long long or an unsigned long long.
 *     L    Same as ll.
 *     q    Same as ll.
 *
 * Value specifiers:
 *     %s   A string pointer (char *) expected to be given as argument. A new
 *          Bencode string is constructed from the given string.
 *
 *     %pb  A Bencode structure (struct bencode *) is expected to be given as
 *          argument. Note, takes ownership of the structure, even when an
 *          error is returned.
 *
 *     %d   Constructs a new integer from the given (signed) integer. The
 *          preceeding conversion modifiers define the type of the value.
 *
 *     %u   Constructs a new integer from the given unsigned integer. The
 *          preceeding conversion modifiers define the type of the value.
 */
struct bencode *ben_pack(const char *fmt, ...)
	BEN_CHECK_FORMAT(printf, 1, 2);

/* ben_is_bool() returns 1 iff b is a boolean, 0 otherwise */
static inline int ben_is_bool(const struct bencode *b)
{
	return b->type == BENCODE_BOOL;
}
static inline int ben_is_dict(const struct bencode *b)
{
	return b->type == BENCODE_DICT;
}
static inline int ben_is_int(const struct bencode *b)
{
	return b->type == BENCODE_INT;
}
static inline int ben_is_list(const struct bencode *b)
{
	return b->type == BENCODE_LIST;
}
static inline int ben_is_str(const struct bencode *b)
{
	return b->type == BENCODE_STR;
}
static inline int ben_is_user(const struct bencode *b)
{
	return b->type == BENCODE_USER;
}

/*
 * ben_bool_const_cast(b) returns "(const struct bencode_bool *) b" if the
 * underlying object is a boolean, NULL otherwise.
 */
static inline const struct bencode_bool *ben_bool_const_cast(const struct bencode *b)
{
	return b->type == BENCODE_BOOL ? ((const struct bencode_bool *) b) : NULL;
}

/*
 * ben_bool_cast(b) returns "(struct bencode_bool *) b" if the
 * underlying object is a boolean, NULL otherwise.
 */
static inline struct bencode_bool *ben_bool_cast(struct bencode *b)
{
	return b->type == BENCODE_BOOL ? ((struct bencode_bool *) b) : NULL;
}

static inline const struct bencode_dict *ben_dict_const_cast(const struct bencode *b)
{
	return b->type == BENCODE_DICT ? ((const struct bencode_dict *) b) : NULL;
}
static inline struct bencode_dict *ben_dict_cast(struct bencode *b)
{
	return b->type == BENCODE_DICT ? ((struct bencode_dict *) b) : NULL;
}

static inline const struct bencode_int *ben_int_const_cast(const struct bencode *i)
{
	return i->type == BENCODE_INT ? ((const struct bencode_int *) i) : NULL;
}
static inline struct bencode_int *ben_int_cast(struct bencode *i)
{
	return i->type == BENCODE_INT ? ((struct bencode_int *) i) : NULL;
}

static inline const struct bencode_list *ben_list_const_cast(const struct bencode *list)
{
	return list->type == BENCODE_LIST ? ((const struct bencode_list *) list) : NULL;
}
static inline struct bencode_list *ben_list_cast(struct bencode *list)
{
	return list->type == BENCODE_LIST ? ((struct bencode_list *) list) : NULL;
}

static inline const struct bencode_str *ben_str_const_cast(const struct bencode *str)
{
	return str->type == BENCODE_STR ? ((const struct bencode_str *) str) : NULL;
}
static inline struct bencode_str *ben_str_cast(struct bencode *str)
{
	return str->type == BENCODE_STR ? ((struct bencode_str *) str) : NULL;
}

static inline const struct bencode_user *ben_user_const_cast(const struct bencode *user)
{
	return user->type == BENCODE_USER ? ((const struct bencode_user *) user) : NULL;
}
static inline struct bencode_user *ben_user_cast(struct bencode *user)
{
	return user->type == BENCODE_USER ? ((struct bencode_user *) user) : NULL;
}

static inline int ben_is_user_type(const struct bencode *b, struct bencode_type *type)
{
	return b->type == BENCODE_USER ? ((const struct bencode_user *) b)->info == type : 0;
}

static inline const void *ben_user_type_const_cast(const struct bencode *b, struct bencode_type *type)
{
	return (b->type == BENCODE_USER && ((const struct bencode_user *) b)->info == type) ? b : NULL;
}
static inline void *ben_user_type_cast(struct bencode *b, struct bencode_type *type)
{
	return (b->type == BENCODE_USER && ((const struct bencode_user *) b)->info == type) ? b : NULL;
}

/* Return the number of keys in a dictionary 'b' */
static inline size_t ben_dict_len(const struct bencode *b)
{
	return ben_dict_const_cast(b)->n;
}

/* Return the number of items in a list 'b' */
static inline size_t ben_list_len(const struct bencode *b)
{
	return ben_list_const_cast(b)->n;
}

/* ben_list_get(list, i) returns object at position i in list */
static inline struct bencode *ben_list_get(const struct bencode *list, size_t i)
{
	const struct bencode_list *l = ben_list_const_cast(list);
	if (i >= l->n) {
		fprintf(stderr, "bencode: List index out of bounds\n");
		abort();
	}
	return l->values[i];
}

/*
 * ben_list_set(list, i, b) sets object b to list at position i.
 * The old value at position i is freed.
 * The program aborts if position i is out of bounds.
 */
void ben_list_set(struct bencode *list, size_t i, struct bencode *b);

/* Return the number of bytes in a string 'b' */
static inline size_t ben_str_len(const struct bencode *b)
{
	return ben_str_const_cast(b)->len;
}

/* Return boolean value (0 or 1) of 'b' */
static inline int ben_bool_val(const struct bencode *b)
{
	return ben_bool_const_cast(b)->b ? 1 : 0;
}

/* Return integer value of 'b' */
static inline long long ben_int_val(const struct bencode *b)
{
	return ben_int_const_cast(b)->ll;
}

/*
 * Note: the string is always zero terminated. Also, the string may
 * contain more than one zero.
 * bencode strings are not compatible with C strings.
 */
static inline const char *ben_str_val(const struct bencode *b)
{
	return ben_str_const_cast(b)->s;
}

/*
 * ben_list_for_each() is an iterator macro for bencoded lists.
 *
 * Note, it is not allowed to change the list while iterating except by
 * using ben_list_pop_current().
 *
 * pos is a size_t.
 *
 * Example:
 *
 * size_t pos;
 * struct bencode *list = xxx;
 * struct bencode *value;
 * ben_list_for_each(value, pos, list) {
 *         inspect(value);
 * }
 */
#define ben_list_for_each(value, pos, l) \
	for ((pos) = (size_t) 0;		    \
	     (pos) < (ben_list_const_cast(l))->n && \
	     ((value) = ((const struct bencode_list *) (l))->values[(pos)]) != NULL ; \
	     (pos)++)

/*
 * ben_list_pop_current() returns and removes the current item at 'pos'
 * while iterating the list with ben_list_for_each().
 * It can be used more than once per walk, but only once per item.
 * Example below:
 *
 * Filter out all items from list whose string value does not begin with "foo".
 *
 * ben_list_for_each(value, pos, list) {
 *         if (strncmp(ben_str_val(value), "foo", 3) != 0)
 *                 ben_free(ben_list_pop_current(&pos, list));
 * }
 */
static inline struct bencode *ben_list_pop_current(struct bencode *list,
						   size_t *pos)
{
	struct bencode *value = ben_list_pop(list, *pos);
	(*pos)--;
	return value;
}

/*
 * ben_dict_for_each() is an iterator macro for bencoded dictionaries.
 *
 * Note, it is not allowed to change the dictionary while iterating except
 * by using ben_dict_pop_current().
 *
 * struct bencode *dict = ben_dict();
 * size_t pos;
 * struct bencode *key;
 * struct bencode *value;
 * ben_dict_set_str_by_str(dict, "foo", "bar");
 *
 * ben_dict_for_each(key, value, pos, dict) {
 *     use(key, value);
 * }
 *
 * pos is a size_t.
 */
#define ben_dict_for_each(bkey, bvalue, pos, d) \
	for ((pos) = 0; \
	     (pos) < (ben_dict_const_cast(d))->n && \
	     ((bkey) = ((const struct bencode_dict *) (d))->nodes[(pos)].key) != NULL && \
	     ((bvalue) = ((const struct bencode_dict *) (d))->nodes[(pos)].value) != NULL; \
	     (pos)++)

/*
 * ben_dict_pop_current() deletes the current item at 'pos' while iterating
 * the dictionary with ben_dict_for_each(). It can be used more than once
 * per walk, but only once per item. Example below:
 *
 * Filter out all items from dictionary whose key does not begin with "foo".
 *
 * ben_dict_for_each(key, value, pos, dict) {
 *     if (strncmp(ben_str_val(key), "foo", 3) != 0)
 *         ben_free(ben_dict_pop_current(dict, &pos));
 * }
 */
struct bencode *ben_dict_pop_current(struct bencode *dict, size_t *pos);

/* Report an error while decoding. Returns NULL. */
void *ben_insufficient_ptr(struct ben_decode_ctx *ctx);
void *ben_invalid_ptr(struct ben_decode_ctx *ctx);
void *ben_oom_ptr(struct ben_decode_ctx *ctx);

/*
 * Decode from the current position of 'ctx'.
 *
 * This function is used to implement decoders for user-defined types.
 */
struct bencode *ben_ctx_decode(struct ben_decode_ctx *ctx);

/*
 * Test whether the input of 'ctx' has at least n bytes left.
 * Returns 0 when there is enough bytes left and -1 when there isn't.
 *
 * This function is used to implement decoders for user-defined types.
 */
int ben_need_bytes(const struct ben_decode_ctx *ctx, size_t n);

/*
 * Returns the character in current position of 'ctx'.
 *
 * This function is used to implement decoders for user-defined types.
 */
char ben_current_char(const struct ben_decode_ctx *ctx);

/*
 * Get the next n bytes from input.
 * Returns pointer to the data or NULL when there aren't enough bytes left.
 *
 * This function is used to implement decoders for user-defined types.
 */
const char *ben_current_buf(const struct ben_decode_ctx *ctx, size_t n);

/*
 * Increments current position by n.
 *
 * This function is used to implement decoders for user-defined types.
 */
void ben_skip(struct ben_decode_ctx *ctx, size_t n);

/*
 * Encode to the output of 'ctx'. The size of the encoded data can be obtained
 * with ben_encoded_size().
 *
 * This function is used to implement encoders for user-defined types.
 */
int ben_ctx_encode(struct ben_encode_ctx *ctx, const struct bencode *b);

/*
 * Append one character to output of 'ctx'. The amount of bytes written to the
 * output must be the same as returned by get_size().
 *
 * This function is used to implement encoders for user-defined types.
 */
int ben_put_char(struct ben_encode_ctx *ctx, char c);

/*
 * Append data to output of 'ctx'. The amount of bytes written to the output
 * must be the same as returned by get_size().
 *
 * This function is used to implement encoders for user-defined types.
 */
int ben_put_buffer(struct ben_encode_ctx *ctx, const void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
