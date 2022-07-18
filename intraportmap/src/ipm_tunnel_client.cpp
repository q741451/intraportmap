#include "stdafx.h"

ipm_tunnel_client::ipm_tunnel_client(struct event_base* base, interface_ipm_tunnel_client* ptr_interface_p) : root_event_base(base), ptr_interface(ptr_interface_p)
{

}
