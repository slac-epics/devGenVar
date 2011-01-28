
#include <dbAccess.h>
#include <dbConvertFast.h>
#include <devSup.h>
#include <dbCommon.h>
#include <dbBase.h>
#include <errlog.h>
#include <stdio.h> /* required by gpHash.h; epics 3.14.11 */
#include <gpHash.h>
#include <recGbl.h>
#include <alarm.h>
#include <epicsExport.h>
#include <cantProceed.h>
#include <iocsh.h>

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <epicsVersion.h>

#include "devGenVar.h"

#if   ! defined(EPICS_VERSION)      \
   || ! defined(EPICS_REVISION)     \
   || ! defined(EPICS_MODIFICATION)
#error "Unknown EPICS Version"
#endif

/* I Loooooove changing APIs. Really, I do... */
#if      EPICS_VERSION > 3 \
    || ( EPICS_VERSION == 3 && EPICS_REVISION > 14 ) \
    || ( EPICS_VERSION == 3 && EPICS_REVISION == 14 && EPICS_MODIFICATION > 10 )
#define HAVE_EPICS_31411
#endif

#define REG_LD_TBL_SZ_DEFAULT 9

#define FLG_NCONV    (1<<0)
#define FLG_NCSUP    (1<<1)

typedef struct DevGenVarPvtRec_ {
	DevGenVar  gv;
	int        flags;
	dbAddr     dbaddr;
} DevGenVarPvtRec, *DevGenVarPvt;

typedef struct RegHeadRec_ {
	DevGenVar  gv;
	int        n_entries;
} RegHeadRec, *RegHead;

#ifdef HAVE_EPICS_31411
struct gphPvt
#else
void
#endif
*devGenVarRegistry  = 0;

static unsigned regLdTblSz = REG_LD_TBL_SZ_DEFAULT;

int
devGenVarConfig(unsigned ldTblSz)
{
	if ( ldTblSz < 8 || ldTblSz > 16 ) {
		errlogPrintf("devGenVarConfig(): ldTableSize argument must be in 8..16\n");
		return (1 << regLdTblSz);
	}
	if ( devGenVarRegistry ) {
		/* Already initialized; return current size */
		return (1 << regLdTblSz);
	}
	regLdTblSz = ldTblSz;
	return 0;
}

long
devGenVarInitDevSup(int pass)
{
	/* Assume this is executed during early init from
	 * a single thread and that no locking is required.
	 */
	if ( ! devGenVarRegistry ) {
		gphInitPvt( &devGenVarRegistry, (1 << regLdTblSz) );

		if ( ! devGenVarRegistry ) 
			cantProceed("devGenVar: Unable to create hash table\n");
	}
	return 0;
}

long
devGenVarRegister(const char *registryEntry, DevGenVar gv, int n_entries)
{
RegHead   h = 0;
GPHENTRY *he;
	
	if ( ! (h = malloc(sizeof(*h))) ) {
		errlogPrintf("devGenVarRegister: no memory\n");
		return -1;
	}

	h->n_entries = n_entries;
	h->gv        = gv;

	if ( ! (he = gphAdd(devGenVarRegistry, registryEntry, devGenVarRegistry)) ) {
		errlogPrintf("devGenVarRegister: Unable to add entry '%s'\n", registryEntry);
		free(h);
		return -1;
	}

	he->userPvt = h;
	return 0;
}

long 
devGenVarGet_nolock(dbCommon *prec)
{
DevGenVarPvt       p = prec->dpvt;
DevGenVar         gv = p->gv;
unsigned short dbf_t = p->dbaddr.field_type;
unsigned short dbr_t = gv->dbr_t;
long          status;

	if ( dbf_t > DBF_DEVICE || dbr_t > DBR_ENUM )
		return -1;

	/* 'put' from outside data buffer to rec. field */
	status = (* (dbFastPutConvertRoutine[dbr_t][dbf_t]))(gv->data_p, p->dbaddr.pfield, &p->dbaddr);

	if ( status )
		recGblSetSevr( prec, READ_ALARM, INVALID_ALARM );
	else if ( (p->flags & FLG_NCONV) ) {
		/* No conversion */
		prec->udf = FALSE;
		status = 2;
	}

	return status;
}

long
devGenVarReadback_nolock(dbCommon *prec)
{
DevGenVarPvt       p = prec->dpvt;
DevGenVar         gv = p->gv;
unsigned short dbf_t = p->dbaddr.field_type;
unsigned short dbr_t = gv->dbr_t;
long          status;

	if ( dbf_t > DBF_DEVICE || dbr_t > DBR_ENUM )
		return -1;

	status = (* (dbFastPutConvertRoutine[dbr_t][dbf_t]))(gv->data_p, p->dbaddr.pfield, &p->dbaddr);

	if ( status ) {
		recGblRecordError(status, prec, "Unable to read current value back\n");
		recGblSetSevr(prec, READ_ALARM, MAJOR_ALARM);
	}

	return status;
}

long
devGenVarGet(dbCommon *prec)
{
DevGenVarPvt       p = prec->dpvt;
DevGenVar         gv = p->gv;
long          status;

	if ( ! gv->mtx )
		return devGenVarGet_nolock( prec );

	epicsMutexMustLock( gv->mtx );

		status = devGenVarGet_nolock( prec );

	epicsMutexUnlock( gv->mtx );

	return status;
}

long
devGenVarPut_nolock(dbCommon *prec)
{
DevGenVarPvt       p = prec->dpvt;
DevGenVar         gv = p->gv;
unsigned short dbf_t = p->dbaddr.field_type;
unsigned short dbr_t = gv->dbr_t;
long     status;

	if ( dbf_t > DBF_DEVICE || dbr_t > DBR_ENUM )
		return -1;

	status = (* (dbFastGetConvertRoutine[dbf_t][dbr_t]))(p->dbaddr.pfield, gv->data_p, &p->dbaddr);

	if ( status ) {
		recGblSetSevr( prec, WRITE_ALARM, INVALID_ALARM );
	} else {
		if ( gv->evt )
			epicsEventSignal( gv->evt );
	}

	return status;
}

long
devGenVarPut(dbCommon *prec)
{
DevGenVarPvt       p = prec->dpvt;
DevGenVar         gv = p->gv;
long          status;

	if ( ! gv->mtx )
		return devGenVarPut_nolock( prec );

	epicsMutexMustLock( gv->mtx );

		status = devGenVarPut_nolock( prec );

	epicsMutexUnlock( gv->mtx );

	return status;
}

long
devGenVarInitScanPvt( DevGenVar p, int n_entries )
{
int i;

	devGenVarInit( p, n_entries );

	for ( i = 0; i < n_entries; i++ ) {
		if ( ! (p[i].scan_p = malloc( sizeof(IOSCANPVT) )) )
			return -1;
		scanIoInit( p[i].scan_p );
	}
	return 0;
}


long
devGenVarGetIointInfo(int delFrom, dbCommon *prec, IOSCANPVT *ppvt)
{
DevGenVar gv = ((DevGenVarPvt)prec->dpvt)->gv;

	if ( ! (gv->scan_p) )
		return -1;
	*ppvt = *gv->scan_p;
	return 0;
}

static void *
findEntry(const char *name)
{
GPHENTRY *he;
	if ( ! name )
		return 0;
	if ( ! (he = gphFind( devGenVarRegistry, name, devGenVarRegistry )) )
		return 0;
	return he->userPvt;
}


static long
devGenVarInitRec(DBLINK *l, dbCommon *prec, int fldOff, int rawFldOff)
{
RegHead       h;
DevGenVarPvt  p;
char        *nm = 0;
long       rval = -1;
dbFldDes *fldD;

	if ( VME_IO != l->type ) {
		errlogPrintf("devGenVarInitRec(%s): link must be of type VME_IO\n", prec->name);
		rval = S_dev_badBus;
		goto bail;
	}

	if ( ! ( h = findEntry( l->value.vmeio.parm ) ) ) {
		errlogPrintf("devGenVarInitRec(%s): no registry entry found for %s\n", prec->name, l->value.vmeio.parm);
		rval = S_dev_noDeviceFound;
		goto bail;
	}

	if ( l->value.vmeio.card >= h->n_entries ) {
		errlogPrintf("devGenVarInitRec(%s): invalid card # %u; only up to %i supported\n", prec->name, l->value.vmeio.card, h->n_entries - 1);
		rval = S_dev_badCard;
		goto bail;
	}

	if ( ! ( p = calloc( 1, sizeof(*p) ) ) ) {
		errlogPrintf("devGenVarInitRec(%s): no memory for DPVT\n", prec->name);
		rval = S_db_noMemory;
		goto bail;
	}

	p->gv    = h->gv + l->value.vmeio.card;
	/* no conversion; use raw field */
	p->flags = 0;

	if ( rawFldOff >= 0 ) {
		p->flags |= FLG_NCSUP;
		if ( l->value.vmeio.signal) {
			p->flags |= FLG_NCONV;
			fldOff = rawFldOff;
		}
	}

	prec->dpvt = p;

	if ( fldOff >= prec->rdes->no_fields ) {
		errlogPrintf("devGenVarInitRec(%s): fldOff(%i) out of range\n", prec->name, fldOff);
		rval = S_db_errArg;
		goto bail;
	}

	fldD = fldOff < 0 ? prec->rdes->pvalFldDes : prec->rdes->papFldDes[fldOff];

	if ( ! (nm = malloc(strlen(prec->name) + 1 + strlen(fldD->name) + 1 )) ) {
		errlogPrintf("devGenVarInitRec(%s): no memory\n", prec->name);
		rval = S_db_noMemory;
		goto bail;
	}

	strcpy(nm, prec->name);
	strcat(nm, ".");
	strcat(nm, fldD->name);

	if ( dbNameToAddr(nm, &p->dbaddr) ) {
		errlogPrintf("devGenVarInitRec(%s): dbNameToAddr() failure\n", prec->name);
		rval = S_db_notFound;
		goto bail;
	}

	rval = 0;

bail:
	free ( nm );
	if ( rval ) {
		prec->pact = TRUE;
		recGblRecordError(rval, prec, "devGenVarInitRec failed\n");
	}
	return rval;
}

long
devGenVarInitInpRec(DBLINK *l, dbCommon *prec, int fldOff, int rawFldOff)
{
	return devGenVarInitRec(l, prec, fldOff, rawFldOff);
}

long 
devGenVarInitOutRec(DBLINK *l, dbCommon *prec, int fldOff, int rawFldOff)
{
long status;

	status = devGenVarInitRec(l, prec, fldOff, rawFldOff);
	if ( status ) goto bail;

	if ( prec->pini ) {
		/* Assume they want to write initial 'VAL' out. 
		 * In this case we just initialize but don't read back.
		 * return '2' for 'no-convert' so that the aoRecord
		 * doesn't convert initial RVAL -> VAL before PINI
		 * happens...
		 * Do this only for records which support conversion...
		 */
		if ( (((DevGenVarPvt)prec->dpvt)->flags & FLG_NCSUP) )
			status = 2;
	} else {
		/* Read current value into record */
		status = devGenVarGet(prec);
		if ( status >= 0 )
			recGblResetAlarms(prec);
	}
bail:
	return status;
}

long
devGenVarEvtCreate(DevGenVar p)
{
	if ( p->evt )
		return -1;

	p->evt = epicsEventMustCreate( epicsEventEmpty );
	return 0;
}

long
devGenVarLockCreate(DevGenVar p)
{
	if ( p->mtx )
		return -1;

	p->mtx = epicsMutexMustCreate();
	return 0;
}


#include <aiRecord.h>

static long init_rec_ai(aiRecord *prec)
{
long status;

	status = devGenVarInitInpRec( &prec->inp, (dbCommon*)prec, aiRecordRVAL, aiRecordVAL );

	if ( status ) {
		recGblRecordError(status, (void*)prec, "devGenVar(ai): init_record failed\n");
		return status;
	}
	return 0;
}

static long read_ai(aiRecord *pai)
{
long status = devGenVarGet((dbCommon*)pai);

	if ( 2 == status )
		pai->udf = isnan(pai->val);

	return status;
}

static struct {
	long         number;
	DEVSUPFUN    report;
	DEVSUPFUN    init;
	DEVSUPFUN    init_record;
	DEVSUPFUN    get_ioint_info;
	DEVSUPFUN    read_record;
	DEVSUPFUN    special_linconv;
} devAiGenVar = {
	6,
	NULL,
	devGenVarInitDevSup,
	init_rec_ai,
	devGenVarGetIointInfo,
	devGenVarGet,
	read_ai
};
epicsExportAddress(dset, devAiGenVar);

#include <longinRecord.h>

static long init_rec_li(longinRecord *prec)
{
long status;

	status = devGenVarInitInpRec( &prec->inp, (dbCommon*)prec, -1, -1 );
	if ( status ) {
		recGblRecordError(status, (void*)prec, "devGenVar(longin): init_record failed\n");
		return status;
	}
	return 0;
}

static struct {
	long         number;
	DEVSUPFUN    report;
	DEVSUPFUN    init;
	DEVSUPFUN    init_record;
	DEVSUPFUN    get_ioint_info;
	DEVSUPFUN    read_record;
} devLiGenVar = {
	5,
	NULL,
	devGenVarInitDevSup,
	init_rec_li,
	devGenVarGetIointInfo,
	devGenVarGet
};
epicsExportAddress(dset, devLiGenVar);

#include <biRecord.h>

static long init_rec_bi(biRecord *prec)
{
long status;

	status = devGenVarInitInpRec( &prec->inp, (dbCommon*)prec, biRecordRVAL, biRecordVAL);
	if ( status ) {
		recGblRecordError(status, (void*)prec, "devGenVar(bi): init_record failed\n");
		return status;
	}
	return 0;
}

static long read_bi(biRecord *prec)
{
DevGenVar gv = ((DevGenVarPvt)prec->dpvt)->gv;
long      status;

	devGenVarLock( gv );

	status = devGenVarGet_nolock( (dbCommon*)prec );

	if ( status >= 0 && prec->mask )
		prec->rval &= prec->mask;
		
	devGenVarUnlock( gv );

	return status;
}

static struct {
	long         number;
	DEVSUPFUN    report;
	DEVSUPFUN    init;
	DEVSUPFUN    init_record;
	DEVSUPFUN    get_ioint_info;
	DEVSUPFUN    read_record;
} devBiGenVar = {
	5,
	NULL,
	devGenVarInitDevSup,
	init_rec_bi,
	devGenVarGetIointInfo,
	read_bi
};
epicsExportAddress(dset, devBiGenVar);

#include <mbbiRecord.h>

static long init_rec_mbbi(mbbiRecord *prec)
{
long status;

	status = devGenVarInitInpRec( &prec->inp, (dbCommon*)prec, mbbiRecordRVAL, mbbiRecordVAL);
	if ( status ) {
		recGblRecordError(status, (void*)prec, "devGenVar(mbbi): init_record failed\n");
		return status;
	}

	/* helper: let nobt == 0 means all bits */
	if ( 0 == prec->nobt ) prec->mask = -1;
	prec->mask <<= prec->shft;
	return 0;
}

static long read_mbbi(mbbiRecord *prec)
{
DevGenVar gv = ((DevGenVarPvt)prec->dpvt)->gv;
long      status;

	devGenVarLock( gv );

	status = devGenVarGet_nolock( (dbCommon*)prec );

	if ( status >= 0 && prec->mask )
		prec->rval &= prec->mask;

	devGenVarUnlock( gv );

	return status;
}

static struct {
	long         number;
	DEVSUPFUN    report;
	DEVSUPFUN    init;
	DEVSUPFUN    init_record;
	DEVSUPFUN    get_ioint_info;
	DEVSUPFUN    read_record;
} devMbbiGenVar = {
	5,
	NULL,
	devGenVarInitDevSup,
	init_rec_mbbi,
	devGenVarGetIointInfo,
	read_mbbi
};
epicsExportAddress(dset, devMbbiGenVar);

#include <aoRecord.h>

static long init_rec_ao(aoRecord *prec)
{
long status;

	status = devGenVarInitOutRec( &prec->out, (dbCommon*)prec, aoRecordRVAL, aoRecordVAL );

	if ( status && 2 != status ) {
		recGblRecordError(status, (void*)prec, "devGenVar(ao): init_record failed\n");
	}
	return status;
}

static struct {
	long         number;
	DEVSUPFUN    report;
	DEVSUPFUN    init;
	DEVSUPFUN    init_record;
	DEVSUPFUN    get_ioint_info;
	DEVSUPFUN    write_record;
	DEVSUPFUN    special_linconv;
} devAoGenVar = {
	6,
	NULL,
	devGenVarInitDevSup,
	init_rec_ao,
	devGenVarGetIointInfo,
	devGenVarPut,
	NULL
};
epicsExportAddress(dset, devAoGenVar);


#include <longoutRecord.h>

static long init_rec_lo(longoutRecord *prec)
{
long status;

	status = devGenVarInitOutRec( &prec->out, (dbCommon*)prec, -1, -1 );

	if ( status && 2 != status ) {
		recGblRecordError(status, (void*)prec, "devGenVar(longout): init_record failed\n");
	}

	return status;
}

static struct {
	long         number;
	DEVSUPFUN    report;
	DEVSUPFUN    init;
	DEVSUPFUN    init_record;
	DEVSUPFUN    get_ioint_info;
	DEVSUPFUN    write_record;
} devLoGenVar = {
	5,
	NULL,
	devGenVarInitDevSup,
	init_rec_lo,
	devGenVarGetIointInfo,
	devGenVarPut
};
epicsExportAddress(dset, devLoGenVar);

#include <boRecord.h>

static long init_rec_bo(boRecord *prec)
{
long status;

	status = devGenVarInitOutRec( &prec->out, (dbCommon*)prec, boRecordRVAL, -1 );

	if ( status && 2 != status ) {
		recGblRecordError(status, (void*)prec, "devGenVar(bo): init_record failed\n");
		return status;
	}

	/* Fixup the status. We don't support non-conversion in this
	 * devsup module but the bo record by itself does
	 */
	if ( prec->pini && 0 == status )
		status = 2;
		
	if ( 0 == status ) {
		/* boRecord conversion IMO doesn't convert MASK back
		 * correctly...
		 */
		if ( prec->mask )
			prec->rval &=  prec->mask;
	}

	return status;
}

static long write_bo(boRecord *prec)
{
long status;
DevGenVarPvt       p = prec->dpvt;
DevGenVar         gv = p->gv;
epicsUInt32 rv;

	devGenVarLock( gv );

	if ( prec->mask ) {
		/* Read value back into RVAL (caching original value) */
		rv = prec->rval;

		status = devGenVarReadback_nolock( (dbCommon*)prec );

		if ( status )
			goto bail;

		prec->rval &= ~prec->mask;
		prec->rval |= (rv & prec->mask);
	}

	status = devGenVarPut_nolock( (dbCommon*)prec );

bail:

	devGenVarUnlock( gv );

	return status;
}

static struct {
	long         number;
	DEVSUPFUN    report;
	DEVSUPFUN    init;
	DEVSUPFUN    init_record;
	DEVSUPFUN    get_ioint_info;
	DEVSUPFUN    write_record;
} devBoGenVar = {
	5,
	NULL,
	devGenVarInitDevSup,
	init_rec_bo,
	devGenVarGetIointInfo,
	write_bo
};
epicsExportAddress(dset, devBoGenVar);

#include <mbboRecord.h>

static long init_rec_mbbo(mbboRecord *prec)
{
long status;

	status = devGenVarInitOutRec( &prec->out, (dbCommon*)prec, mbboRecordRVAL, -1 );

	if ( status && 2 != status ) {
		recGblRecordError(status, (void*)prec, "devGenVar(mbbo): init_record failed\n");
		return status;
	}

	/* Fixup the status. We don't support non-conversion in this
	 * devsup module but the bo record by itself does
	 */
	if ( prec->pini && 0 == status )
		status = 2;
		
	/* helper: let nobt == 0 means all bits */
	if ( 0 == prec->nobt )
		prec->mask = -1;

	prec->mask <<= prec->shft;

	prec->rbv = prec->rval;

	if ( 0 == status ) {
		/* mbboRecord conversion IMO doesn't convert MASK back
		 * correctly...
		 */
		if ( prec->mask )
			prec->rval &=  prec->mask;
	}


	return status;
}

static long write_mbbo(mbboRecord *prec)
{
long status;
DevGenVarPvt       p = prec->dpvt;
DevGenVar         gv = p->gv;
epicsUInt32 rv;

	devGenVarLock( gv );

	/* Read value back into RVAL (caching original value) */
	rv = prec->rval;

	status = devGenVarReadback_nolock( (dbCommon*)prec );

	if ( 0 == status ) {

		prec->rbv = prec->rval;

		prec->rval &= ~prec->mask;
		prec->rval |= (rv & prec->mask);

		status = devGenVarPut_nolock( (dbCommon*)prec );

	}

	devGenVarUnlock( gv );

	return status;
}

static struct {
	long         number;
	DEVSUPFUN    report;
	DEVSUPFUN    init;
	DEVSUPFUN    init_record;
	DEVSUPFUN    get_ioint_info;
	DEVSUPFUN    write_record;
} devMbboGenVar = {
	5,
	NULL,
	devGenVarInitDevSup,
	init_rec_mbbo,
	devGenVarGetIointInfo,
	write_mbbo
};
epicsExportAddress(dset, devMbboGenVar);

static const iocshArg devGenVarConfigArg1 = {
	name:	"ld_table_size",
	type:   iocshArgInt,
};

static const iocshArg *devGenVarConfigArgs[] = {
	&devGenVarConfigArg1,
};

static iocshFuncDef devGenVarConfigDef = {
	name: "devGenVarConfig",
	nargs: sizeof(devGenVarConfigArgs)/sizeof(devGenVarConfigArgs[0]),
	arg:   devGenVarConfigArgs,
};

static void 
devGenVarConfigCall(const iocshArgBuf *argBuf)
{
	devGenVarConfig( argBuf->ival );
}

static void devGenVarRegistrar(void)
{
	iocshRegister( &devGenVarConfigDef, devGenVarConfigCall );
}

epicsExportRegistrar(devGenVarRegistrar);
