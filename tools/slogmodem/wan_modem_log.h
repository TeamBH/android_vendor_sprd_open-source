/*
 *  wan_modem_log.h - The WAN MODEM log and dump handler class declaration.
 *
 *  Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 *  History:
 *  2015-7-13 Zhang Ziyi
 *  Initial version.
 */
#ifndef WAN_MODEM_LOG_H_
#define WAN_MODEM_LOG_H_

#include "log_pipe_hdl.h"

class WanModemLogHandler : public LogPipeHandler
{
public:
	WanModemLogHandler(LogController* ctrl,
			   Multiplexer* multi,
			   const LogConfig::ConfigEntry* conf,
			   StorageManager& stor_mgr);

private:
	/*  start_dump - override the virtual function to implement WAN
	 *               MODEM memory dump.
	 *  Return Value:
	 *    Return 0 if the dump transaction is started successfully,
	 *    return -1 if the dump transaction can not be started,
	 *    return 1 if the dump transaction is finished.
	 */
	int start_dump(const struct tm& lt);

	/*  diag_transaction_notify - Diagnosis port transaction result
	 *                            notification function.
	 *  @client: client parameter. It's the WanModemLogHandler* pointer.
	 *  @result: the transaction result.
	 *
	 *  This function is called by current DataConsumer object.
	 */
	static void diag_transaction_notify(void* client,
					    DataConsumer::LogProcResult res);
};

#endif  // !WAN_MODEM_LOG_H_

