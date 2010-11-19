#ifndef DEV_GEN_VAR_H
#define DEV_GEN_VAR_H

#include <dbAddr.h>
#include <dbScan.h>
#include <dbCommon.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Basic data structure describing a 'generic variable'.
 *
 * For each variable that is to be connected to the EPICS database
 * one DevGenVarRec must be created and initialized.
 *
 * 1) Allocate memory    (either statically or via malloc)
 * 2) Initialize memory: devGenVarInit( gv, num_elements );
 *    (devGenVarInit() can initialize an array of DevGenVarRec's
 *    in one sweep).
 * 3) Fill structure elements:
 *
 *       scan_p:   (optional) pointer to IOSCANPVT. If this is used
 *                 then you must initialize the IOSCANPVT object
 *                 yourself (scanIoInit())!. Note that multiple
 *                 DevGenVarRec's may point to the same scan-list
 *                 (which is to be initialized only once).
 *
 *                 If you initialize this field then your code may
 *                 cause the attached record(s) to scan by issuing
 *                 scanIoRequest() on the IOSCANPVT that scan_p is
 *                 pointing to. This causes all records connected
 *                 to all DevGenVarRec's pointing to the same IOSCANPVT
 *                 to process.
 * 
 *       mtx:      (optional) mutex used to synchronize access to the 
 *                 *data_p object. All access (read / write / read-modify-write)
 *                 of this device-support module acquires and releases
 *                 this mutex (if present).
 *
 *       evt:      (optional) event that can be used to notify
 *                 your code that a generic-variable was written
 *                 by an output record.
 *
 *       data_p:   (mandatory) opaque pointer to your generic-variable.
 *
 *
 *       dbr_t:    (mandatory) EPICS DBR type of the generic-varibale/object.               
 *
 *  NOTE: Only the mandatory and optional fields that you intend to use 
 *        need to be filled by you. Unused optional fields may remain
 *        as written by devGenVarInit().
 */

typedef epicsEventId DevGenVarEvt; 
typedef epicsMutexId DevGenVarMtx;

typedef struct DevGenVarRec_ {
	IOSCANPVT      *scan_p;        /* scanlist (may be NULL)            */
	DevGenVarMtx    mtx;           /* protection (may be NULL)          */
	DevGenVarEvt    evt;           /* synchronization (may be NULL)     */
	volatile void  *data_p;        /* data we want to transfer          */
	unsigned        dbr_t;         /* DBR type of data we want to transfer from/to field */
} DevGenVarRec, *DevGenVar;

/*
 * Initialize an array of DevGenVarRec's. Must be called
 * before you set individual fields.
 * Static DevGenVarRec's may be initialized using the DEV_GEN_VAR_INIT()
 * macro below.
 */
static __inline__ void
devGenVarInit( DevGenVar p, int n_entries )
{
	memset( p, 0, n_entries * sizeof( *p ) );
}

/*
 * Alternate initializer for static DevGenVarRec's. You should
 * always use this macro (in case more fields are added in the future)
 * in order to produce portable code. Initialize unused, optional fields
 * with 0.
 *
 * Example:
 *
 *    DevGenVarRec myVars[] = {
 *      DEV_GEN_VAR_INIT( &my_scanlist, 0, 0, &my_data,       DBR_LONG ),
 *      DEV_GEN_VAR_INIT( &my_scanlist, 0, 0, &my_other_data, DBR_LONG ),
 *    };
 *
 * In this example both variables are connected to the same scan-list.
 */
#define DEV_GEN_VAR_INIT( scan, mutx, evnt, data, type ) \
	{ scan_p: (scan), mtx: (mutx), evt: (evnt), data_p: (data), dbr_t: (type) }

/*
 * Register an array of DevGenVarRec's so that the device-support module
 * may find them.
 *
 * RETURNS: zero on success, nonzero on failure.
 */
long
devGenVarRegister(const char *registryEntry, DevGenVar p, int n_entries);

/*
 * Create an event and attach to 'p'. Always use this routine - the
 * underlying object may change in the future!
 */

long
devGenVarEvtCreate(DevGenVar p);

/* Block (with timeout) until devsup has written to generic variable.
 * Useful to synchronize low-level code with EPICS writing to an 
 * output record.
 *
 * NOTES: Zero timeout returns immediately, negative timeout blocks
 *        indefinitely.
 *       
 *        Only supported for output records (which *write* to a GenVar).
 *        The usual mechanism for letting an input record know that
 *        the underlying GenVar changed is calling scanIoRequest().
 *
 *        ALWAYS use this routine, NEVER epicsEventWaitxxx() directly
 *        because the implementation may change, moving away from 
 *        epics events!
 */
#define DEV_GEN_VAR_OK	       0
#define DEV_GEN_VAR_TIMEDOUT   1
#define DEV_GEN_VAR_ERRWAIT    2    /* blocking operation returned error    */
#define DEV_GEN_VAR_ERRNOEVT  -1	/* blocking not supported by this GenVar */

static __inline__ long
devGenVarWait(DevGenVar p, double timeout)
{
	if ( !p || !p->evt )
		return DEV_GEN_VAR_ERRNOEVT;

	return timeout < 0. ? 
	           epicsEventWait( p->evt )  :
               epicsEventWaitWithTimeout( p->evt, timeout );
}

/*
 * Create a lock and attach to 'p'. Always use this routine - the
 * underlying implementation may change in the future!
 */

long
devGenVarLockCreate(DevGenVar p);


/* Serialize access to underlying variable.
 * ALWAYS use these inlines - implementation of lock may change!
 */
static __inline__ void
devGenVarLock(DevGenVar p)
{
	if ( p->mtx )
		epicsMutexMustLock( p->mtx );
}

static __inline__ void
devGenVarUnlock(DevGenVar p)
{
	if ( p->mtx )
		epicsMutexUnlock( p->mtx );
}

#ifdef __cplusplus
}
#endif

#endif
