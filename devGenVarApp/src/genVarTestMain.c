#include <epicsExit.h>
#include <epicsThread.h>
#include <iocsh.h>
#include <dbScan.h>
#include <errlog.h>
#include <alarm.h>

#include <devGenVar.h>

#include <dbFldTypes.h>
#include <epicsTypes.h>

epicsUInt16  genTestS     = 0xffff;
epicsInt32   genTestL     = -1;
epicsUInt32  genTestL1    = -2;

epicsUInt32  genAsyncL    = 0;

static IOSCANPVT    listS;
static IOSCANPVT    listL;

static DevGenVarRec testS[] = {
	DEV_GEN_VAR_INIT( &listS, 0, 0, &genTestS, DBR_USHORT )
};

static DevGenVarRec testL[] = {
	DEV_GEN_VAR_INIT( &listL, 0, 0, &genTestL,  DBR_LONG ),
	DEV_GEN_VAR_INIT( &listL, 0, 0, &genTestL1, DBR_ULONG )
};

static DevGenVarRec asyncL[] = {
	DEV_GEN_VAR_INIT( 0, 0, 0, &genAsyncL, DBR_ULONG )
};

static void
asyncT(void *arg)
{
	asyncL[0].ts.nsec = 44444444;
	while ( 1 ) {
		epicsEventWait( asyncL[0].evt );
		{
			devGenVarLock( asyncL );
				asyncL[0].ts.nsec++;
				asyncL[0].stat    = WRITE_ALARM;
				asyncL[0].sevr    = MINOR_ALARM;
				devGenVarProcComplete( asyncL );
			devGenVarUnlock( asyncL );
		}
	}
}

static void
ioscnT(void *arg)
{
	while ( 1 ) {
		epicsThreadSleep(2.0);
		devGenVarLock( & testS[0] );
		if ( 0xffff != genTestS ) {
			genTestS++;
			scanIoRequest( listS );
		}
		devGenVarUnlock( & testS[0] );
	}
}

int
main(int argc, char **argv)
{

	scanIoInit( &listS );
	devGenVarLockCreate( &testS[0] );
	if ( devGenVarRegister( "testS", testS, sizeof(testS)/sizeof(testS[0]) ) ) {
		errlogPrintf("devGenVarRegister(testS) failed\n");
	}

	scanIoInit( &listL );
	devGenVarLockCreate( &testL[0] );
	if ( devGenVarRegister( "testL", testL, sizeof(testL)/sizeof(testL[0])) ) {
		errlogPrintf("devGenVarRegister(testL) failed\n");
	}

	devGenVarLockCreate( &asyncL[0] );
	devGenVarEvtCreate(  &asyncL[0] );
	if ( devGenVarRegister( "asyncL", asyncL, sizeof(asyncL)/sizeof(asyncL[0])) ) {
		errlogPrintf("devGenVarRegister(asyncL) failed\n");
	}

	epicsThreadMustCreate("asyncThread",
	                      epicsThreadPriorityLow,
	                      epicsThreadGetStackSize(epicsThreadStackMedium),
	                      asyncT,
	                      0 );

	epicsThreadMustCreate("ioscnThread",
	                      epicsThreadPriorityLow,
	                      epicsThreadGetStackSize(epicsThreadStackMedium),
	                      ioscnT,
	                      0 );
	if ( argc >= 0 ) {
		iocsh( argv[1] );
		epicsThreadSleep(0.2);
	}

	testL[1].stat    = READ_ALARM;
	testL[1].sevr    = MINOR_ALARM;
	testL[1].ts.nsec = 12345678;
	scanIoRequest( listL );
	scanIoRequest( listS );
	iocsh( 0 );
	epicsExit( 0 );
	return( 0 );
}
