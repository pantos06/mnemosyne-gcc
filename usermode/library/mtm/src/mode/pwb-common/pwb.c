#include "mtm_i.h"
#include "beginend-bits.h"

void ITM_NORETURN
mtm_pwb_restart_transaction (mtm_tx_t *tx, mtm_restart_reason r)
{
	uint32_t actions;

#if (!defined(ALLOW_ABORTS))
	if (tx->mode == MTM_MODE_pwbnl) {
		assert(0 && "Aborts disabled for current mode pwbnl but want to abort.\n");
	}	
#endif

	rollback_transaction(tx);
	cm_delay(tx);
	mtm_decide_retry_strategy (r); 

	/* Reset field to restart transaction */
	pwb_prepare_transaction(tx);

	actions = a_runInstrumentedCode | a_restoreLiveVariables;
	//if ((tx->prop & pr_uninstrumentedCode) && (tx->status & TX_IRREVOCABLE)) {
	//	actions = a_runUninstrumentedCode | a_restoreLiveVariables;
	//}	
	
	mtm_longjmp (&tx->jb, actions);
}

