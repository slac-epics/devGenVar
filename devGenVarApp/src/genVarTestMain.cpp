#include <epicsExit.h>
#include <epicsThread.h>
#include <iocsh.h>
#include <dbScan.h>
#include <errlog.h>

#include <devGenVar.h>

#include <dbFldTypes.h>
#include <epicsTypes.h>

epicsUInt16  genTestS     = 0xffff;
epicsInt32   genTestL     = -1;
epicsUInt32  genTestL1    = -2;

static IOSCANPVT    listS;
static IOSCANPVT    listL;

static DevGenVarRec testS[] = {
	DEV_GEN_VAR_INIT( &listS, 0, 0, &genTestS, DBR_USHORT )
};

static DevGenVarRec testL[] = {
	DEV_GEN_VAR_INIT( &listL, 0, 0, &genTestL,  DBR_LONG ),
	DEV_GEN_VAR_INIT( &listL, 0, 0, &genTestL1, DBR_ULONG )
};


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
	if ( argc >= 0 ) {
		iocsh( argv[1] );
		epicsThreadSleep(0.2);
	}
	scanIoRequest( listL );
	scanIoRequest( listS );
	iocsh( 0 );
	epicsExit( 0 );
	return( 0 );
}
