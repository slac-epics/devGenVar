/* $Id */
#ifndef DEV_GEN_VAR_SUP_H
#define DEV_GEN_VAR_SUP_H

/*
 * Device-support interface to devGenVar. Developers who write device support
 * for records not yet supported by devGenVar use this API.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <devGenVar.h>
#include <dbAccess.h>


/* Struct attached to DPVT */

#define FLG_NCONV    (1<<0)        /* They want 'no-conversion'                        */
#define FLG_NCSUP    (1<<1)        /* 'no-conversion' supported by record but not used */

typedef struct DevGenVarPvtRec_ {
	DevGenVar  gv;
	int        flags;
	dbAddr     dbaddr;
} DevGenVarPvtRec, *DevGenVarPvt;

/*
 * To be called by input-record's init_record() method.
 * 
 *  - verify parameters and some record fields (e.g., INP must be VMEIO etc.)
 *  - create and initialize a DevGenVarPvtRec and attach it to DPVT.
 *
 * The record's INP field has the following format:
 *
 *       #CxSy@gv_name
 *
 * 'gv_name' is the name of an array of DevGenVarRec's which was registered by
 * the low-level code: devGenVarRegister("gv_name", &my_gv, x). 'x' is the
 * index into the array of DevGenVarRec's. If there is only a single DevGenVarRec
 * then 'x' is usually 0.
 *
 * The 'y' parameter may be non-zero and indicates that the user does not
 * want to use the record's 'conversion' feature (only for records which support
 * such a feature).
 *
 * Arguments: 
 *     'l':         pointer to record's INP link.
 *     'prec':      pointer to record
 *     'fldOff:     field offset of record field that should be connected
 *                  to GenVar. You may pass a negative value which selects
 *                  the 'VAL' field.
 *     'rawFldOff:  field offset of record field that should be connected
 *                  to GenVar if the user desires 'no-conversion' (nonzero
 *                  'S' parameter in INP field).
 *                  Pass a negative value for 'rawFldOff' if your record
 *                  does not support conversion anyways.
 *
 * Note: the field-offset indices used for 'fldOff' and 'rawFldOff', respectively
 *       are defined in the record specific headers, e.g., <xyzRecord.h>.
 *
 * RETURNS: zero on success, nonzero error-code on failure (with prec->PACT set).
 */
long
devGenVarInitInpRec(DBLINK *l, dbCommon *prec, int fldOff, int rawFldOff);

/*
 * Mostly indentical to devGenVarInitInpRec() but also handle some
 * additional details relevant for output records:
 *
 * If PINI is set then assume the user wants the initial 'VAL' to
 * be written out. Therefore -- if and only if the record supports
 * conversion, i.e., you pass non-negative rawFldOff -- this routine
 * returns '2' (which you should pass back out of 'init_record') so
 * that record support does *NOT* convert back the initial RVAL to VAL
 * (thus wiping out the initial VAL from the database).
 *
 * If PINI is not set then the current value of the GenVar is read
 * back into the record.
 *
 * RETURNS: success (0/2) or error code. Return value should be passed
 *          out of 'init_record()'.
 */
long 
devGenVarInitOutRec(DBLINK *l, dbCommon *prec, int fldOff, int rawFldOff);

/*
 * Lock GenVar's mutex, read from GenVar into record, unlock mutex.
 * For simple records this can be used verbatim for the 'read_xxx()'
 * devsup method.
 *
 * If an error occurs then alarms are set (severity INVALID) and the
 * routine returns an error code.
 * On success the routine returns zero or '2' (if record supports
 * 'no-conversion' and this was requested by the user by giving
 * non-zero 'S' parameter in INP field).
 *
 * The return value should be passed out of the record's 'read_xxx()'
 * devsup method.
 */
long 
devGenVarGet(dbCommon *prec);

/*
 * Like 'devGenVarGet' but omits locking. If you need to do
 * more complex computations then you usually do
 *
 *   devGenVarLock( ((DevGenVarPvt)prec->dpvt)->gv );
 *
 *      if ( 0 == (status = devGenVarGet_nolock( prec )) ) {
 *         further_processing()
 *      }
 *
 *   devGenVarUnlock( ((DevGenVarPvt)prec->dpvt)->gv );
 *
 *   return status;
 */     
long 
devGenVarGet_nolock(dbCommon *prec);

/*
 * Very similar to devGenVarGet_nolock() but
 *  - set alarm severity to MAJOR instead of INVALID.
 *  - never returns '2'.
 *
 * This routine is intended to be used by output records
 * which need to retrieve the current value of a DevGenVar
 * during a 'read-modify-write' type operation.
 */
long
devGenVarReadback_nolock(dbCommon *prec);

/*
 * Lock mutex, write value; on failure set alarm (INVALID severity),
 * on success post DevGenVar's 'evt'. Unlock and leave.
 *
 * RETURNS: zero on success, nonzero error code on failure. The
 *          status should be passed out of 'write_xxx()' devsup
 *          method.
 * 
 * NOTE:    routine can be used verbatim for simple record's 
 *          'write_xxx()' method.
 */
long
devGenVarPut(dbCommon *prec);

/*
 * Like devGenVarPut() but omit locking/unlocking. May be
 * used if you need to perform more complex operations
 * e.g., read-modify-write. A typical read-modify-write
 * operation does:
 *    lock mutex (if present)
 *    save_value
 *    devGenVarReadback_nolock():
 *    merge saved_value and read-back value
 *    devGenVarPut_nolock()
 *    unlock mutex (if present)
 */
long
devGenVarPut_nolock(dbCommon *prec);

/*
 * Generic get_ioint_info() routine that may be used by device-support
 * modules supporting other records than the ones that come with this
 * library.
 */
long
devGenVarGetIointInfo(int delFrom, dbCommon *prec, IOSCANPVT *ppvt);

#ifdef __cplusplus
}
#endif

#endif
