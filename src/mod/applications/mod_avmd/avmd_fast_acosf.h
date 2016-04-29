#ifndef WIN32   /* currently we support fast acosf computation only on UNIX/Linux */
/*
 * @brief   Fast arithmetic using precomputed arc cosine table.
 * Contributor(s):
 *
 * Eric des Courtis <eric.des.courtis@benbria.com>
 * Piotr Gregor <piotrek.gregor gmail.com>
 */


#ifndef __AVMD_FAST_ACOSF_H__
#define __AVMD_FAST_ACOSF_H__


#define ACOS_TABLE_FILENAME "/tmp/acos_table.dat"


/*! \brief Arc cosine table initialization.
 *
 * @return 0 on success, negative value otherwise:
 *          -1 can't access arc cos table with error != NOENT,
 *          -2 table creation failed (compute_table)
 *          -3 can access table but fopen failed
 *          -4 mmap failed
 */
extern int init_fast_acosf(void);

/*! \brief Arc cosine table deinitialization.
 *
 * @return 0 on success, negative value otherwise:
 *          -1 munmap failed,
 *          -2 close failed
 */
extern int destroy_fast_acosf(void);

/*! \brief  Return arc cos for this argument.
 * @details Uses previously created and mmapped file.
 */
extern float fast_acosf(float x);

/*! \brief Arc cosine table creation.
 *
 * @return 0 on success, negative value otherwise:
 *          -1 fwrite failed,
 *          -2 fclose failed
 */
extern int compute_table(void);

#endif /* __AVMD_FAST_ACOSF_H__ */
#endif
