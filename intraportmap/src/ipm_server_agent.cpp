#include "stdafx.h"


bool ipm_server_agent::init()
{
	bool ret = false;





	ret = true;
//end:
	if (ret == true)
		is_state_init = true;
	else
	{
		exit();
		reset();
	}
	return ret;
}

bool ipm_server_agent::is_init()
{
	return is_state_init;
}

bool ipm_server_agent::exit()
{
	bool ret = false;




	is_state_init = false;
	ret = true;
//end:
	return ret;
}

void ipm_server_agent::reset()
{
	is_state_init = false;
}
