#include "stdafx.h"

void intraportmap_signal_event_callback(evutil_socket_t sig, short events, void* user_data);

intraportmap::intraportmap()
{
	reset();
}

bool intraportmap::init(int argc, char* argv[])
{
	bool ret = false;

	if (init_config(argc, argv) != true)
		goto end;

	if ((root_event_base = event_base_new()) == NULL)
	{
		slog_error("event_base_new error");
		goto end;
	}

	if (register_signal_event() != true)
		goto end;

	if (is_server)
	{
		slog_info("run as server");
		sp_ipm_server = std::make_shared<ipm_server>(root_event_base, this);
		if (sp_ipm_server->init(server_name.c_str(), server_port_name.c_str(), (size_t)max_buffer) != true)
		{
			slog_error("sp_ipm_server init error");
			goto end;
		}
	}
	else
	{
		slog_info("run as client");
		sp_ipm_client = std::make_shared<ipm_client>(root_event_base, this);
		if (sp_ipm_client->init(server_name.c_str(), server_port_name.c_str(), to_server_name.c_str(), to_server_port_name.c_str(), from_server_name.c_str(), from_server_port_name.c_str(), client_reconn_time, (size_t)max_buffer) != true)
		{
			slog_error("sp_ipm_client init error");
			goto end;
		}
	}


	ret = true;
end:
	if (ret == true)
	{
		is_state_init = true;
	}
	else
	{
		exit();
		reset();
	}
	return ret;
}

bool intraportmap::is_init()
{
	return is_state_init;
}

bool intraportmap::exit()
{
	bool ret = false;

	if (signal_event)
	{
		event_free(signal_event);
	}

	if (sp_ipm_client)
	{
		if (sp_ipm_client->is_init())
			sp_ipm_client->exit();
	}

	if (sp_ipm_server)
	{
		if (sp_ipm_server->is_init())
			sp_ipm_server->exit();
	}

	if (root_event_base)
		event_base_free(root_event_base);

	is_state_init = false;

	ret = true;
//end:
	return ret;
}

void intraportmap::reset()
{
	is_server = true;
	is_state_init = false;
	client_reconn_time = 15;
	max_buffer = 1024 * 1024;
	root_event_base = NULL;
	signal_event = NULL;
	if (sp_ipm_client)
	{
		sp_ipm_client->reset();
	}
	sp_ipm_client.reset();
	if (sp_ipm_server)
	{
		sp_ipm_server->reset();
	}
	sp_ipm_server.reset();
}

void intraportmap::exec()
{
	event_base_dispatch(root_event_base);
}

void intraportmap::on_interface_ipm_client_fail()
{
	slog_error("on_interface_ipm_client_fail");
	event_base_loopbreak(root_event_base);
}

void intraportmap::on_interface_ipm_server_fail()
{
	slog_error("on_interface_ipm_server_fail");
	event_base_loopbreak(root_event_base);
}

void intraportmap::on_signal_event(evutil_socket_t sig, short events)
{
	event_base_loopbreak(root_event_base);
}

bool intraportmap::init_config(int argc, char* argv[])
{
	bool ret = false;
	std::string server_full;
	std::string to_full;
	std::string from_full;
	int opt = 0;

	while ((opt = getopt(argc, argv, "cs:t:f:w:b:")) != -1) {
		switch (opt) {
		case 'c':
			is_server = false;
			break;
		case 's':
			server_full = optarg;
			break;
		case 't':
			to_full = optarg;
			break;
		case 'f':
			from_full = optarg;
			break;
		case 'w':
			if (sscanf(optarg, "%u", &client_reconn_time) <= 0)
			{
				slog_error("error option w %s", optarg);
				goto end;
			}
			break;
		case 'b':
			if (sscanf(optarg, "%llu", &max_buffer) <= 0)
			{
				slog_error("error option w %s", optarg);
				goto end;
			}
			break;
		default:
			slog_error("Unknown option %c", opt);
			goto end;
		}
	}

	if (util::split_addr_string(server_full.c_str(), server_name, server_port_name) != true)
	{
		slog_error("server_full error");
		goto end;
	}

	if (!is_server)
	{
		if (util::split_addr_string(to_full.c_str(), to_server_name, to_server_port_name) != true)
		{
			slog_error("to_full error");
			goto end;
		}
		if (util::split_addr_string(from_full.c_str(), from_server_name, from_server_port_name) != true)
		{
			slog_error("from_full error");
			goto end;
		}
	}

	ret = true;
end:
	return ret;
}

bool intraportmap::register_signal_event()
{
	bool ret = false;

	if ((signal_event = evsignal_new(root_event_base, SIGINT, intraportmap_signal_event_callback, this)) == NULL)
	{
		slog_error("evsignal_new error");
		goto end;
	}

	if (event_add(signal_event, NULL) != 0)
	{
		slog_error("event_add error");
		goto end;
	}

	ret = true;
end:
	if (signal_event)
	{
		event_free(signal_event);
		signal_event = NULL;
	}
	return ret;
}

void intraportmap_signal_event_callback(evutil_socket_t sig, short events, void* user_data)
{
	if (user_data)
		((intraportmap*)user_data)->on_signal_event(sig, events);
}
