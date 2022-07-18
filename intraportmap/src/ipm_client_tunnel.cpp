#include "stdafx.h"

ipm_client_tunnel::ipm_client_tunnel(struct event_base* base, interface_ipm_client_tunnel* ptr_interface_p) : root_event_base(base), ptr_interface(ptr_interface_p)
{

}
